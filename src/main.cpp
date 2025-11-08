/*
 * ESP32 BLOCKCHAIN TELEMETRY SYSTEM - SPIFFS VERSION v1.3
 * 
 * Features:
 * - Distributed sensor data storage
 * - Proof of Authority consensus
 * - ESP-NOW mesh network
 * - Immutable telemetry records
 * - SPIFFS persistent storage for blockchain and transactions
 * - Multi-sensor support
 * 
 * Hardware: ESP32 DevKit
 * Network: ESP-NOW for peer-to-peer
 * Storage: SPIFFS filesystem
 * 
 * NEW IN v1.3:
 * - SPIFFS integration for persistent storage
 * - Blockchain saves to /blockchain.dat
 * - Transaction pool saves to /txpool.dat
 * - Automatic load on startup
 * - Periodic saves to prevent data loss
 */

#include <esp_now.h>
#include <WiFi.h>
#include <mbedtls/md.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <FS.h>

// ==================== CONFIGURATION ====================
#define MAX_BLOCKS 50           // Increased for SPIFFS storage
#define MAX_PEERS 10            // Maximum peer nodes
#define BLOCK_TIME_MS 30000     // 30 seconds per block
#define MAX_TX_PER_BLOCK 4      // Transactions per block
#define TX_POOL_SIZE 20         // Transaction pool size
#define PEER_ANNOUNCE_INTERVAL 60000  // Announce every 60s
#define SAVE_INTERVAL 60000     // Save to SPIFFS every 60s

// Storage paths
#define BLOCKCHAIN_FILE "/blockchain.dat"
#define TXPOOL_FILE "/txpool.dat"
#define METADATA_FILE "/metadata.dat"

// Node role
enum NodeRole {
    SENSOR_NODE,      // Collects data, broadcasts
    VALIDATOR_NODE,   // Mines blocks, validates
    ARCHIVE_NODE      // Stores full history
};

// Role assignment strategy
enum RoleStrategy {
    STRATEGY_MAC_BASED,      // Based on MAC address hash
    STRATEGY_FIRST_COME,     // First nodes become validators
    STRATEGY_RUNTIME_ELECT,  // Network election (future)
    STRATEGY_ALL_VALIDATOR   // All nodes validate (testing)
};

RoleStrategy ROLE_STRATEGY = STRATEGY_MAC_BASED;
NodeRole MY_ROLE = SENSOR_NODE;

// ==================== DATA STRUCTURES ====================

typedef uint8_t Hash32[32];

struct TelemetryData {
    char sensorId[16];
    float temperature;
    float humidity;
    float pressure;
    float batteryVoltage;
    uint32_t timestamp;
    int16_t rssi;
    uint8_t dataQuality;
} __attribute__((packed));

struct Transaction {
    Hash32 txHash;
    TelemetryData data;
    uint8_t signature[32];
    uint8_t verified;
} __attribute__((packed));

struct Block {
    uint32_t index;
    uint32_t timestamp;
    Hash32 txHashes[MAX_TX_PER_BLOCK];
    uint8_t txCount;
    Hash32 previousHash;
    Hash32 blockHash;
    char validator[17];
    uint32_t nonce;
} __attribute__((packed));

struct BlockHeader {
    uint32_t index;
    uint32_t timestamp;
    uint8_t txCount;
    Hash32 blockHash;
    Hash32 previousHash;
    char validator[17];
} __attribute__((packed));

// Metadata structure for storage
struct ChainMetadata {
    uint32_t blockCount;
    uint32_t totalBlocks;
    uint32_t lastSaveTime;
    char lastValidator[17];
} __attribute__((packed));

enum MessageType {
    MSG_NEW_TELEMETRY,
    MSG_NEW_BLOCK,
    MSG_REQUEST_CHAIN,
    MSG_CHAIN_DATA,
    MSG_PEER_ANNOUNCE,
    MSG_VALIDATOR_HEARTBEAT
};

struct NetworkPacket {
    MessageType type;
    uint8_t data[200];
    uint16_t dataLen;
    char sender[17];
} __attribute__((packed));

// ==================== FORWARD DECLARATIONS ====================

void bin2hex(const uint8_t* bin, size_t len, char* outHex);
void calculateSHA256Binary(const uint8_t* data, size_t len, uint8_t* out32);
void calculateTxHash(Transaction* tx);
void calculateBlockHash(Block* block);
void signTransaction(Transaction* tx);

// ==================== GLOBAL STATE ====================

Block blockchain[MAX_BLOCKS];
uint32_t blockCount = 0;
uint32_t totalBlocks = 0;

Transaction txPool[TX_POOL_SIZE];
uint8_t txPoolCount = 0;

uint8_t peerList[MAX_PEERS][6];
uint8_t peerCount = 0;
bool broadcastPeerAdded = false;

char myAddress[17];
Preferences preferences;

unsigned long lastBlockTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long lastAnnounceTime = 0;
unsigned long lastSaveTime = 0;

bool spiffsInitialized = false;

// ==================== SPIFFS FUNCTIONS ====================

// Initialize SPIFFS
bool initSPIFFS() {
    Serial.println("\n📁 Initializing SPIFFS...");
    
    if(!SPIFFS.begin(true)) {  // true = format on fail
        Serial.println("✗ SPIFFS mount failed");
        return false;
    }
    
    size_t totalBytes = SPIFFS.totalBytes();
    size_t usedBytes = SPIFFS.usedBytes();
    
    Serial.printf("✓ SPIFFS mounted\n");
    Serial.printf("  Total: %u bytes\n", totalBytes);
    Serial.printf("  Used: %u bytes\n", usedBytes);
    Serial.printf("  Free: %u bytes\n", totalBytes - usedBytes);
    
    spiffsInitialized = true;
    return true;
}

// Save metadata
bool saveMetadata() {
    if(!spiffsInitialized) return false;
    
    File file = SPIFFS.open(METADATA_FILE, FILE_WRITE);
    if(!file) {
        Serial.println("✗ Failed to open metadata file for writing");
        return false;
    }
    
    ChainMetadata meta;
    meta.blockCount = blockCount;
    meta.totalBlocks = totalBlocks;
    meta.lastSaveTime = millis() / 1000;
    
    if(blockCount > 0) {
        Block* lastBlock = &blockchain[(blockCount - 1) % MAX_BLOCKS];
        strcpy(meta.lastValidator, lastBlock->validator);
    } else {
        strcpy(meta.lastValidator, myAddress);
    }
    
    size_t written = file.write((uint8_t*)&meta, sizeof(meta));
    file.close();
    
    return (written == sizeof(meta));
}

// Load metadata
bool loadMetadata() {
    if(!spiffsInitialized) return false;
    
    if(!SPIFFS.exists(METADATA_FILE)) {
        Serial.println("ℹ️  No metadata file found");
        return false;
    }
    
    File file = SPIFFS.open(METADATA_FILE, FILE_READ);
    if(!file) {
        Serial.println("✗ Failed to open metadata file");
        return false;
    }
    
    ChainMetadata meta;
    size_t bytesRead = file.read((uint8_t*)&meta, sizeof(meta));
    file.close();
    
    if(bytesRead != sizeof(meta)) {
        Serial.println("✗ Metadata file corrupted");
        return false;
    }
    
    blockCount = meta.blockCount;
    totalBlocks = meta.totalBlocks;
    
    Serial.printf("✓ Metadata loaded: %u blocks\n", blockCount);
    return true;
}

// Save blockchain to SPIFFS
bool saveBlockchain() {
    if(!spiffsInitialized) return false;
    
    Serial.println("💾 Saving blockchain to SPIFFS...");
    
    File file = SPIFFS.open(BLOCKCHAIN_FILE, FILE_WRITE);
    if(!file) {
        Serial.println("✗ Failed to open blockchain file for writing");
        return false;
    }
    
    // Write block count first
    file.write((uint8_t*)&blockCount, sizeof(blockCount));
    
    // Write all blocks
    for(uint32_t i = 0; i < blockCount && i < MAX_BLOCKS; i++) {
        size_t written = file.write((uint8_t*)&blockchain[i], sizeof(Block));
        if(written != sizeof(Block)) {
            Serial.printf("✗ Failed to write block %u\n", i);
            file.close();
            return false;
        }
    }
    
    file.close();
    
    Serial.printf("✓ Saved %u blocks to SPIFFS\n", blockCount);
    return saveMetadata();
}

// Load blockchain from SPIFFS
bool loadBlockchain() {
    if(!spiffsInitialized) return false;
    
    if(!SPIFFS.exists(BLOCKCHAIN_FILE)) {
        Serial.println("ℹ️  No blockchain file found, starting fresh");
        return false;
    }
    
    Serial.println("📖 Loading blockchain from SPIFFS...");
    
    File file = SPIFFS.open(BLOCKCHAIN_FILE, FILE_READ);
    if(!file) {
        Serial.println("✗ Failed to open blockchain file");
        return false;
    }
    
    // Read block count
    uint32_t savedBlockCount;
    file.read((uint8_t*)&savedBlockCount, sizeof(savedBlockCount));
    
    Serial.printf("  Found %u blocks in storage\n", savedBlockCount);
    
    // Read blocks
    uint32_t blocksToLoad = (savedBlockCount < MAX_BLOCKS) ? savedBlockCount : MAX_BLOCKS;
    
    for(uint32_t i = 0; i < blocksToLoad; i++) {
        size_t bytesRead = file.read((uint8_t*)&blockchain[i], sizeof(Block));
        if(bytesRead != sizeof(Block)) {
            Serial.printf("✗ Failed to read block %u\n", i);
            file.close();
            return false;
        }
    }
    
    file.close();
    
    blockCount = blocksToLoad;
    totalBlocks = savedBlockCount;
    
    Serial.printf("✓ Loaded %u blocks from SPIFFS\n", blockCount);
    
    // Verify last block
    if(blockCount > 0) {
        Block* lastBlock = &blockchain[blockCount - 1];
        char hex[65];
        bin2hex(lastBlock->blockHash, 32, hex);
        Serial.printf("  Last block: #%u\n", lastBlock->index);
        Serial.printf("  Hash: %.16s...\n", hex);
    }
    
    return loadMetadata();
}

// Save transaction pool
bool saveTxPool() {
    if(!spiffsInitialized || txPoolCount == 0) return false;
    
    File file = SPIFFS.open(TXPOOL_FILE, FILE_WRITE);
    if(!file) {
        Serial.println("✗ Failed to open txpool file for writing");
        return false;
    }
    
    // Write transaction count
    file.write((uint8_t*)&txPoolCount, sizeof(txPoolCount));
    
    // Write transactions
    for(uint8_t i = 0; i < txPoolCount; i++) {
        file.write((uint8_t*)&txPool[i], sizeof(Transaction));
    }
    
    file.close();
    Serial.printf("✓ Saved %u transactions to SPIFFS\n", txPoolCount);
    return true;
}

// Load transaction pool
bool loadTxPool() {
    if(!spiffsInitialized) return false;
    
    if(!SPIFFS.exists(TXPOOL_FILE)) {
        Serial.println("ℹ️  No transaction pool file found");
        return false;
    }
    
    File file = SPIFFS.open(TXPOOL_FILE, FILE_READ);
    if(!file) {
        return false;
    }
    
    // Read transaction count
    uint8_t savedTxCount;
    file.read((uint8_t*)&savedTxCount, sizeof(savedTxCount));
    
    // Read transactions
    txPoolCount = (savedTxCount < TX_POOL_SIZE) ? savedTxCount : TX_POOL_SIZE;
    
    for(uint8_t i = 0; i < txPoolCount; i++) {
        file.read((uint8_t*)&txPool[i], sizeof(Transaction));
    }
    
    file.close();
    Serial.printf("✓ Loaded %u transactions from SPIFFS\n", txPoolCount);
    return true;
}

// Periodic save task
void periodicSaveTask() {
    unsigned long now = millis();
    
    if(now - lastSaveTime >= SAVE_INTERVAL) {
        Serial.println("\n⏱️  Periodic save triggered");
        
        bool success = true;
        
        if(blockCount > 0) {
            success = saveBlockchain() && success;
        }
        
        if(txPoolCount > 0) {
            success = saveTxPool() && success;
        }
        
        if(success) {
            Serial.println("✓ Periodic save completed\n");
        } else {
            Serial.println("⚠️  Some save operations failed\n");
        }
        
        lastSaveTime = now;
    }
}

// Clear all stored data (useful for testing)
void clearStorage() {
    Serial.println("\n🗑️  Clearing all stored data...");
    
    if(SPIFFS.exists(BLOCKCHAIN_FILE)) {
        SPIFFS.remove(BLOCKCHAIN_FILE);
        Serial.println("  ✓ Blockchain file removed");
    }
    
    if(SPIFFS.exists(TXPOOL_FILE)) {
        SPIFFS.remove(TXPOOL_FILE);
        Serial.println("  ✓ Transaction pool file removed");
    }
    
    if(SPIFFS.exists(METADATA_FILE)) {
        SPIFFS.remove(METADATA_FILE);
        Serial.println("  ✓ Metadata file removed");
    }
    
    blockCount = 0;
    totalBlocks = 0;
    txPoolCount = 0;
    
    Serial.println("✓ Storage cleared\n");
}

// List all files in SPIFFS
void listSPIFFSFiles() {
    if(!spiffsInitialized) return;
    
    Serial.println("\n📂 SPIFFS Files:");
    
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    
    while(file) {
        Serial.printf("  %s (%u bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }
    Serial.println();
}

// ==================== ROLE ASSIGNMENT ====================

NodeRole assignRoleByMAC(const char* macAddr) {
    uint32_t hash = 0;
    for(int i = 0; macAddr[i] != '\0'; i++) {
        hash = hash * 31 + macAddr[i];
    }
    
    uint8_t roleValue = hash % 100;
    
    if(roleValue < 30) {
        return VALIDATOR_NODE;
    } else if(roleValue < 95) {
        return SENSOR_NODE;
    } else {
        return ARCHIVE_NODE;
    }
}

NodeRole assignRoleByJoinOrder(uint32_t nodeNumber) {
    if(nodeNumber <= 2) {
        return VALIDATOR_NODE;
    }
    else if(nodeNumber % 10 == 0) {
        return ARCHIVE_NODE;
    }
    else {
        return SENSOR_NODE;
    }
}

void assignNodeRole() {
    switch(ROLE_STRATEGY) {
        case STRATEGY_MAC_BASED:
            MY_ROLE = assignRoleByMAC(myAddress);
            Serial.println("Role Strategy: MAC-based (deterministic)");
            break;
            
        case STRATEGY_FIRST_COME: {
            if(!preferences.begin("blockchain", false)) {
                Serial.println("✗ Failed to open preferences");
                MY_ROLE = SENSOR_NODE;
                break;
            }
            
            uint32_t nodeId = preferences.getUInt("nodeId", 0);
            if(nodeId == 0) {
                nodeId = peerCount + 1;
                preferences.putUInt("nodeId", nodeId);
                Serial.printf("New node ID assigned: %u\n", nodeId);
            }
            preferences.end();
            
            MY_ROLE = assignRoleByJoinOrder(nodeId);
            Serial.printf("Role Strategy: First-come (Node #%u)\n", nodeId);
            break;
        }
            
        case STRATEGY_ALL_VALIDATOR:
            MY_ROLE = VALIDATOR_NODE;
            Serial.println("Role Strategy: All validators (testing mode)");
            break;
            
        case STRATEGY_RUNTIME_ELECT:
            MY_ROLE = assignRoleByMAC(myAddress);
            Serial.println("Role Strategy: Runtime election (not implemented, using MAC)");
            break;
    }
    
    const char* roleName = 
        MY_ROLE == SENSOR_NODE ? "SENSOR" : 
        MY_ROLE == VALIDATOR_NODE ? "VALIDATOR" : "ARCHIVE";
    Serial.printf("✓ Role assigned: %s\n", roleName);
}

void checkRoleChangeCommand() {
    if(Serial.available() > 0) {
        char cmd = Serial.read();
        
        switch(cmd) {
            case 'v':
            case 'V':
                MY_ROLE = VALIDATOR_NODE;
                Serial.println("\n✓ Role changed to: VALIDATOR");
                break;
            case 's':
            case 'S':
                MY_ROLE = SENSOR_NODE;
                Serial.println("\n✓ Role changed to: SENSOR");
                break;
            case 'a':
            case 'A':
                MY_ROLE = ARCHIVE_NODE;
                Serial.println("\n✓ Role changed to: ARCHIVE");
                break;
            case 'c':
            case 'C':
                clearStorage();
                break;
            case 'l':
            case 'L':
                listSPIFFSFiles();
                break;
            case 'w':
            case 'W':
                Serial.println("\n💾 Manual save triggered");
                saveBlockchain();
                saveTxPool();
                break;
            case '?':
                Serial.println("\n=== Commands ===");
                Serial.println("V - Set as VALIDATOR");
                Serial.println("S - Set as SENSOR");
                Serial.println("A - Set as ARCHIVE");
                Serial.println("C - Clear storage");
                Serial.println("L - List SPIFFS files");
                Serial.println("W - Write/save now");
                Serial.println("? - Show this help");
                break;
        }
        
        while(Serial.available()) Serial.read();
    }
}

// ==================== CRYPTOGRAPHIC FUNCTIONS ====================

void calculateSHA256Binary(const uint8_t* data, size_t len, uint8_t* out32) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, data, len);
    mbedtls_md_finish(&ctx, out32);
    mbedtls_md_free(&ctx);
}

void bin2hex(const uint8_t* bin, size_t len, char* outHex) {
    for(size_t i = 0; i < len; ++i) {
        sprintf(outHex + i*2, "%02x", bin[i]);
    }
    outHex[len*2] = '\0';
}

void calculateTxHash(Transaction* tx) {
    char data[200];
    int n = snprintf(data, sizeof(data), "%s|%.2f|%.2f|%.2f|%u",
                     tx->data.sensorId,
                     tx->data.temperature,
                     tx->data.humidity,
                     tx->data.pressure,
                     tx->data.timestamp);
    calculateSHA256Binary((const uint8_t*)data, n, tx->txHash);
}

void calculateBlockHash(Block* block) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);

    uint8_t buf[64];
    int len = snprintf((char*)buf, sizeof(buf), "%u|%u|", block->index, block->timestamp);
    mbedtls_md_update(&ctx, buf, len);
    mbedtls_md_update(&ctx, (const unsigned char*)block->validator, strlen(block->validator));
    mbedtls_md_update(&ctx, (const unsigned char*)&block->nonce, sizeof(block->nonce));
    
    mbedtls_md_update(&ctx, block->previousHash, 32);

    for(int i = 0; i < block->txCount; ++i) {
        mbedtls_md_update(&ctx, block->txHashes[i], 32);
    }

    mbedtls_md_finish(&ctx, block->blockHash);
    mbedtls_md_free(&ctx);
}

void signTransaction(Transaction* tx) {
    char data[100];
    char hashHex[65];
    bin2hex(tx->txHash, 32, hashHex);
    snprintf(data, sizeof(data), "%s|%s", hashHex, myAddress);
    calculateSHA256Binary((uint8_t*)data, strlen(data), tx->signature);
}

// ==================== BLOCKCHAIN FUNCTIONS ====================

void createGenesisBlock() {
    Block genesis = {0};
    genesis.index = 0;
    genesis.timestamp = millis() / 1000;
    genesis.txCount = 0;
    memset(genesis.previousHash, 0, 32);
    strcpy(genesis.validator, myAddress);
    genesis.nonce = 0;
    
    calculateBlockHash(&genesis);
    
    blockchain[0] = genesis;
    blockCount = 1;
    totalBlocks = 1;
    
    Serial.println("✓ Genesis block created");
    char hex[65];
    bin2hex(genesis.blockHash, 32, hex);
    Serial.printf("  Hash: %s\n", hex);
    
    // Save genesis block immediately
    saveBlockchain();
}

bool validateBlock(Block* block) {
    if(block->index != totalBlocks) {
        Serial.printf("✗ Invalid block index: %u (expected %u)\n", 
                     block->index, totalBlocks);
        return false;
    }
    
    if(blockCount > 0) {
        Block* lastBlock = &blockchain[(blockCount - 1) % MAX_BLOCKS];
        if (memcmp(block->previousHash, lastBlock->blockHash, 32) != 0){
            Serial.println("✗ Previous hash mismatch");
            return false;
        }
    }
    
    Block tempBlock = *block;
    calculateBlockHash(&tempBlock);
    
    if (memcmp(tempBlock.blockHash, block->blockHash, 32) != 0) {
        Serial.println("✗ Block hash invalid");
        return false;
    }
    
    return true;
}

bool addBlock(Block* newBlock) {
    if(!validateBlock(newBlock)) {
        return false;
    }
    
    uint32_t index = blockCount % MAX_BLOCKS;
    blockchain[index] = *newBlock;
    blockCount++;
    totalBlocks++;
    
    txPoolCount = 0;
    
    Serial.printf("✓ Block #%u added (%d tx)\n", 
                 newBlock->index, newBlock->txCount);
    
    // Save to SPIFFS after adding block
    saveBlockchain();
    
    return true;
}

Block createBlock() {
    Block newBlock = {0};
    newBlock.index = totalBlocks;
    newBlock.timestamp = millis() / 1000;
    
    newBlock.txCount = (txPoolCount < MAX_TX_PER_BLOCK) ? txPoolCount : MAX_TX_PER_BLOCK;

    for (int i = 0; i < newBlock.txCount; ++i) {
        memcpy(newBlock.txHashes[i], txPool[i].txHash, 32);
    }

    if(blockCount > 0) {
        Block* prevBlock = &blockchain[(blockCount - 1) % MAX_BLOCKS];
        memcpy(newBlock.previousHash, prevBlock->blockHash, 32);
    } else {
        memset(newBlock.previousHash, 0, 32);
    }
    
    strcpy(newBlock.validator, myAddress);
    newBlock.nonce = random(0, 1000000);
    
    calculateBlockHash(&newBlock);
    
    return newBlock;
}

// ==================== TELEMETRY FUNCTIONS ====================

Transaction createTelemetryTransaction() {
    Transaction tx = {0};
    
    snprintf(tx.data.sensorId, sizeof(tx.data.sensorId), "ESP_%s", myAddress + 9);
    tx.data.temperature = 20.0 + random(-50, 150) / 10.0;
    tx.data.humidity = 40.0 + random(0, 400) / 10.0;
    tx.data.pressure = 1013.25 + random(-100, 100) / 10.0;
    tx.data.batteryVoltage = 3.3 + random(-3, 3) / 10.0;
    tx.data.timestamp = millis() / 1000;
    tx.data.rssi = WiFi.RSSI();
    tx.data.dataQuality = 95 + random(0, 5);
    
    calculateTxHash(&tx);
    signTransaction(&tx);
    tx.verified = false;
    
    return tx;
}

bool addToTxPool(Transaction* tx) {
    if(txPoolCount >= TX_POOL_SIZE) {
        Serial.println("✗ Transaction pool full");
        return false;
    }
    
    txPool[txPoolCount++] = *tx;
    
    Serial.printf("✓ TX added to pool: %s (%.1f°C)\n", 
                 tx->data.sensorId, tx->data.temperature);
    
    return true;
}

void queryTelemetryData(const char* sensorId, uint32_t startTime, uint32_t endTime) {
    Serial.printf("\n=== Telemetry Query: %s ===\n", sensorId);
    int count = 0;

    for (int i = 0; i < txPoolCount; i++) {
        Transaction* tx = &txPool[i];

        if(strcmp(tx->data.sensorId, sensorId) == 0 &&
           tx->data.timestamp >= startTime &&
           tx->data.timestamp <= endTime)
        {
            Serial.printf(" Temp: %.1f°C | Humidity: %.1f%% | Time: %u\n",
                          tx->data.temperature,
                          tx->data.humidity,
                          tx->data.timestamp);
            count++;
        }
    }

    Serial.printf("Found %d readings\n\n", count);
}

// ==================== NETWORK FUNCTIONS ====================

void setupBroadcastPeer() {
    if(broadcastPeerAdded) return;
    
    uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    
    esp_err_t result = esp_now_add_peer(&peerInfo);
    if(result == ESP_OK) {
        broadcastPeerAdded = true;
        Serial.println("✓ Broadcast peer added");
    } else if(result != ESP_ERR_ESPNOW_EXIST) {
        Serial.printf("✗ Failed to add broadcast peer: %d\n", result);
    }
}

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    NetworkPacket* packet = (NetworkPacket*)data;
    
    bool peerExists = false;
    for(int i = 0; i < peerCount; i++) {
        if(memcmp(peerList[i], mac, 6) == 0) {
            peerExists = true;
            break;
        }
    }
    if(!peerExists && peerCount < MAX_PEERS) {
        memcpy(peerList[peerCount++], mac, 6);
        Serial.printf("✓ New peer added: %02X:%02X:%02X:%02X:%02X:%02X\n",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    
    switch(packet->type) {
        case MSG_NEW_TELEMETRY: {
            Transaction* tx = (Transaction*)packet->data;
            addToTxPool(tx);
            break;
        }
        
        case MSG_NEW_BLOCK: {
            BlockHeader* header = (BlockHeader*)packet->data;
            Serial.printf("✓ Block header received: #%u\n", header->index);
            break;
        }
        
        case MSG_REQUEST_CHAIN: {
            Serial.println("Chain sync requested");
            break;
        }
        
        case MSG_PEER_ANNOUNCE: {
            Serial.printf("Peer announced: %s\n", packet->sender);
            break;
        }
    }
}

void broadcastPacket(NetworkPacket* packet) {
    strcpy(packet->sender, myAddress);
    setupBroadcastPeer();
    
    uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_err_t result = esp_now_send(broadcastAddr, (uint8_t*)packet, sizeof(NetworkPacket));
    
    if(result != ESP_OK && result != ESP_ERR_ESPNOW_NOT_FOUND) {
        Serial.printf("✗ Broadcast error: %d\n", result);
    }
}

void broadcastTelemetry(Transaction* tx) {
    NetworkPacket packet;
    packet.type = MSG_NEW_TELEMETRY;
    memcpy(packet.data, tx, sizeof(Transaction));
    packet.dataLen = sizeof(Transaction);
    
    broadcastPacket(&packet);
}

void broadcastBlock(Block* block) {
    NetworkPacket packet;
    packet.type = MSG_NEW_BLOCK;
    
    BlockHeader header;
    header.index = block->index;
    header.timestamp = block->timestamp;
    header.txCount = block->txCount;
    memcpy(header.blockHash, block->blockHash, 32);
    memcpy(header.previousHash, block->previousHash, 32);
    strcpy(header.validator, block->validator);
    
    memcpy(packet.data, &header, sizeof(BlockHeader));
    packet.dataLen = sizeof(BlockHeader);
    
    broadcastPacket(&packet);
    
    Serial.println("✓ Block header broadcast");
}

// ==================== CONSENSUS ====================

bool isMyTurnToValidate() {
    if(peerCount == 0) return true;
    
    unsigned long interval = BLOCK_TIME_MS / 1000;
    unsigned long currentSlot = (millis() / 1000) / interval;
    
    int myId = myAddress[15] % (peerCount + 1);
    int validatorSlot = currentSlot % (peerCount + 1);
    
    return (myId == validatorSlot);
}

void validatorTask() {
    if(MY_ROLE != VALIDATOR_NODE) return;
    
    unsigned long now = millis();
    bool shouldMine = false;
    const char* reason = "";
    
    if(txPoolCount >= (TX_POOL_SIZE - 4)) {
        shouldMine = true;
        reason = "Emergency (pool nearly full)";
    }
    else if(now - lastBlockTime >= BLOCK_TIME_MS) {
        if(txPoolCount > 0 && isMyTurnToValidate()) {
            shouldMine = true;
            reason = "Scheduled";
        }
    }
    
    if(shouldMine && txPoolCount > 0) {
        Serial.printf("\n⛏️  Mining new block (%d txs pending) - %s\n", txPoolCount, reason);
        
        Block newBlock = createBlock();
        
        if(addBlock(&newBlock)) {
            broadcastBlock(&newBlock);
            lastBlockTime = now;
            
            Serial.printf("✓ Block #%u mined and broadcast\n", newBlock.index);
        }
    }
}

// ==================== SENSOR TASK ====================

void sensorTask() {
    if(MY_ROLE != SENSOR_NODE && MY_ROLE != VALIDATOR_NODE) return;
    
    unsigned long now = millis();
    
    if(now - lastTelemetryTime >= 10000) {
        Transaction tx = createTelemetryTransaction();
        
        addToTxPool(&tx);
        broadcastTelemetry(&tx);
        
        lastTelemetryTime = now;
    }
}

// ==================== PEER DISCOVERY ====================

void peerDiscoveryTask() {
    unsigned long now = millis();
    
    if(now - lastAnnounceTime >= PEER_ANNOUNCE_INTERVAL) {
        NetworkPacket announce;
        announce.type = MSG_PEER_ANNOUNCE;
        strcpy(announce.sender, myAddress);
        announce.dataLen = 0;
        
        setupBroadcastPeer();
        
        uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_now_send(broadcastAddr, (uint8_t*)&announce, sizeof(announce));
        
        Serial.printf("📡 Peer announcement sent (peers: %d)\n", peerCount);
        lastAnnounceTime = now;
    }
}

// ==================== STATUS DISPLAY ====================

void printStatus() {
    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║   BLOCKCHAIN TELEMETRY STATUS      ║");
    Serial.println("╚════════════════════════════════════╝");
    Serial.printf(" Address: %s\n", myAddress);
    Serial.printf(" Role: %s\n", 
                 MY_ROLE == SENSOR_NODE ? "SENSOR" : 
                 MY_ROLE == VALIDATOR_NODE ? "VALIDATOR" : "ARCHIVE");
    Serial.printf(" Blocks: %u (total: %u)\n", blockCount, totalBlocks);
    Serial.printf(" TX Pool: %u / %d\n", txPoolCount, TX_POOL_SIZE);
    Serial.printf(" Peers: %u connected\n", peerCount);
    
    if(blockCount > 0) {
        Block* lastBlock = &blockchain[(blockCount - 1) % MAX_BLOCKS];
        Serial.printf(" Last Block: #%u (%d tx)\n", 
                     lastBlock->index, lastBlock->txCount);
        char hex[65];
        bin2hex(lastBlock->blockHash, 32, hex);
        Serial.printf(" Last Hash: %.16s...\n", hex);
    }
    
    if(spiffsInitialized) {
        Serial.printf(" SPIFFS: %u / %u bytes\n", 
                     SPIFFS.usedBytes(), SPIFFS.totalBytes());
    }
    
    Serial.printf(" Uptime: %lu seconds\n", millis() / 1000);
    Serial.printf(" Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println();
}

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║  ESP32 BLOCKCHAIN TELEMETRY v1.3   ║");
    Serial.println("║    WITH SPIFFS STORAGE             ║");
    Serial.println("╚════════════════════════════════════╝\n");
    
    // Initialize SPIFFS first
    if(!initSPIFFS()) {
        Serial.println("⚠️  Continuing without SPIFFS");
    }
    
    // Initialize WiFi for ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Get MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(myAddress, sizeof(myAddress), 
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    Serial.printf("Node Address: %s\n", myAddress);
    
    // Assign role
    assignNodeRole();
    
    Serial.printf("Max TX per block: %d\n\n", MAX_TX_PER_BLOCK);
    
    // Initialize ESP-NOW
    if(esp_now_init() != ESP_OK) {
        Serial.println("✗ ESP-NOW init failed");
        return;
    }
    
    esp_now_register_recv_cb(onDataReceived);
    
    // Try to load existing blockchain from SPIFFS
    bool loaded = false;
    if(spiffsInitialized) {
        loaded = loadBlockchain();
        if(loaded) {
            loadTxPool();  // Also load pending transactions
        }
    }
    
    // Create genesis if no blockchain exists
    if(!loaded || blockCount == 0) {
        createGenesisBlock();
    }
    
    // Setup broadcast peer
    setupBroadcastPeer();
    
    // Initial announcement
    NetworkPacket announce;
    announce.type = MSG_PEER_ANNOUNCE;
    strcpy(announce.sender, myAddress);
    announce.dataLen = 0;
    
    uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastAddr, (uint8_t*)&announce, sizeof(announce));
    
    Serial.println("✓ System initialized");
    Serial.println("\nCommands: V=Validator, S=Sensor, A=Archive");
    Serial.println("          C=Clear storage, L=List files, W=Save now, ?=Help\n");
    
    lastBlockTime = millis();
    lastTelemetryTime = millis();
    lastAnnounceTime = millis();
    lastSaveTime = millis();
}

// ==================== MAIN LOOP ====================

void loop() {
    static unsigned long lastStatus = 0;
    
    // Check for commands
    checkRoleChangeCommand();
    
    // Run tasks
    sensorTask();
    validatorTask();
    peerDiscoveryTask();
    periodicSaveTask();  // NEW: Periodic SPIFFS saves
    
    // Print status every 30 seconds
    if(millis() - lastStatus >= 30000) {
        printStatus();
        lastStatus = millis();
        
        // Demo query
        if(blockCount > 1 && txPoolCount > 0) {
            char querySensorId[20];
            snprintf(querySensorId, sizeof(querySensorId), "ESP_%s", myAddress + 9);
            queryTelemetryData(querySensorId, 0, UINT32_MAX);
        }
    }
    
    delay(100);
}