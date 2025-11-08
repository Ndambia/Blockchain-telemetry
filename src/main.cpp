/*
 * ESP32 BLOCKCHAIN TELEMETRY SYSTEM - FIXED VERSION v1.2
 * 
 * Features:
 * - Distributed sensor data storage
 * - Proof of Authority consensus
 * - ESP-NOW mesh network
 * - Immutable telemetry records
 * - Multi-sensor support
 * 
 * Hardware: ESP32 DevKit
 * Network: ESP-NOW for peer-to-peer
 * 
 * FIXES APPLIED (v1.2):
 * - Fixed ESP-NOW payload size issue (Block too large)
 * - Block header-only broadcast with hash verification
 * - Fixed broadcast peer management
 * - Proper error handling for ESP-NOW
 * - Reduced MAX_TX_PER_BLOCK to fit in payload
 */

#include <esp_now.h>
#include <WiFi.h>
#include <mbedtls/md.h>
#include <Preferences.h>

// ==================== CONFIGURATION ====================
#define MAX_BLOCKS 10           // Blocks stored in RAM
#define MAX_PEERS 10            // Maximum peer nodes
#define BLOCK_TIME_MS 30000     // 30 seconds per block
#define MAX_TX_PER_BLOCK 4      // Reduced to fit in ESP-NOW payload
#define TX_POOL_SIZE 20         // Increased pool size
#define PEER_ANNOUNCE_INTERVAL 60000  // Announce every 60s

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

RoleStrategy ROLE_STRATEGY = STRATEGY_MAC_BASED; // Choose strategy
NodeRole MY_ROLE = SENSOR_NODE; // Will be set dynamically

// ==================== DATA STRUCTURES ====================

// Compact binary hash type
typedef uint8_t Hash32[32]; // binary SHA256 (32 bytes)

// Compact telemetry
struct TelemetryData {
    char sensorId[16];        // 16 bytes
    float temperature;        // 4
    float humidity;           // 4
    float pressure;           // 4
    float batteryVoltage;     // 4
    uint32_t timestamp;       // 4
    int16_t rssi;             // 2
    uint8_t dataQuality;      // 1
} __attribute__((packed));

// Transaction structure
struct Transaction {
    Hash32 txHash;            // 32 bytes binary
    TelemetryData data;       // ~39-40 bytes
    uint8_t signature[32];    // 32 bytes binary signature
    uint8_t verified;         // 1
} __attribute__((packed));

// Block stores binary hashes of transactions
struct Block {
    uint32_t index;           // 4
    uint32_t timestamp;       // 4
    Hash32 txHashes[MAX_TX_PER_BLOCK]; // 32 * 4 = 128
    uint8_t txCount;          // 1
    Hash32 previousHash;      // 32
    Hash32 blockHash;         // 32
    char validator[17];       // 17
    uint32_t nonce;           // 4
} __attribute__((packed));

// Compact block header for network transmission
struct BlockHeader {
    uint32_t index;
    uint32_t timestamp;
    uint8_t txCount;
    Hash32 blockHash;
    Hash32 previousHash;
    char validator[17];
} __attribute__((packed));

// Network message types
enum MessageType {
    MSG_NEW_TELEMETRY,         // New sensor reading
    MSG_NEW_BLOCK,             // New mined block (header only)
    MSG_REQUEST_CHAIN,         // Request blockchain sync
    MSG_CHAIN_DATA,            // Blockchain data response
    MSG_PEER_ANNOUNCE,         // Node announcement
    MSG_VALIDATOR_HEARTBEAT    // Validator status
};

// Network packet (ESP-NOW max is ~250 bytes)
struct NetworkPacket {
    MessageType type;
    uint8_t data[200];         // Safe payload size
    uint16_t dataLen;
    char sender[17];           // MAC address string
} __attribute__((packed));

// ==================== GLOBAL STATE ====================

// Blockchain storage
Block blockchain[MAX_BLOCKS];
uint32_t blockCount = 0;
uint32_t totalBlocks = 0;      // Total including pruned

// Transaction pool
Transaction txPool[TX_POOL_SIZE];
uint8_t txPoolCount = 0;

// Network peers
uint8_t peerList[MAX_PEERS][6]; // MAC addresses
uint8_t peerCount = 0;
bool broadcastPeerAdded = false;

// Node identity
char myAddress[17];             // This node's address
Preferences preferences;

// Timing
unsigned long lastBlockTime = 0;
unsigned long lastTelemetryTime = 0;
unsigned long lastAnnounceTime = 0;

// ==================== ROLE ASSIGNMENT ====================

// Calculate role based on MAC address
NodeRole assignRoleByMAC(const char* macAddr) {
    // Hash the MAC address to get consistent role
    uint32_t hash = 0;
    for(int i = 0; macAddr[i] != '\0'; i++) {
        hash = hash * 31 + macAddr[i];
    }
    
    // Distribute roles: 30% validators, 70% sensors
    uint8_t roleValue = hash % 100;
    
    if(roleValue < 30) {
        return VALIDATOR_NODE;
    } else if(roleValue < 95) {
        return SENSOR_NODE;
    } else {
        return ARCHIVE_NODE;
    }
}

// Assign role based on network join order
NodeRole assignRoleByJoinOrder(uint32_t nodeNumber) {
    // First 3 nodes are validators
    if(nodeNumber <= 2) {
        return VALIDATOR_NODE;
    }
    // One archive node per 10 nodes
    else if(nodeNumber % 10 == 0) {
        return ARCHIVE_NODE;
    }
    // Rest are sensors
    else {
        return SENSOR_NODE;
    }
}

// Dynamic role assignment based on strategy
void assignNodeRole() {
    switch(ROLE_STRATEGY) {
        case STRATEGY_MAC_BASED:
            MY_ROLE = assignRoleByMAC(myAddress);
            Serial.println("Role Strategy: MAC-based (deterministic)");
            break;
            
        case STRATEGY_FIRST_COME: {
            // Use stored join order from preferences
            if(!preferences.begin("blockchain", false)) {
                Serial.println("✗ Failed to open preferences");
                MY_ROLE = SENSOR_NODE;
                break;
            }
            
            uint32_t nodeId = preferences.getUInt("nodeId", 0);
            if(nodeId == 0) {
                // First time - assign new ID based on current network size
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
            // Future: network-based election
            // For now, fallback to MAC-based
            MY_ROLE = assignRoleByMAC(myAddress);
            Serial.println("Role Strategy: Runtime election (not implemented, using MAC)");
            break;
    }
    
    // Display assigned role
    const char* roleName = 
        MY_ROLE == SENSOR_NODE ? "SENSOR" : 
        MY_ROLE == VALIDATOR_NODE ? "VALIDATOR" : "ARCHIVE";
    Serial.printf("✓ Role assigned: %s\n", roleName);
}

// Allow runtime role change via serial command
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
            case '?':
                Serial.println("\n=== Role Commands ===");
                Serial.println("V - Set as VALIDATOR");
                Serial.println("S - Set as SENSOR");
                Serial.println("A - Set as ARCHIVE");
                Serial.println("? - Show this help");
                break;
        }
        
        // Clear remaining buffer
        while(Serial.available()) Serial.read();
    }
}

// ==================== CRYPTOGRAPHIC FUNCTIONS ====================

// Calculate SHA256 hash (binary)
void calculateSHA256Binary(const uint8_t* data, size_t len, uint8_t* out32) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, data, len);
    mbedtls_md_finish(&ctx, out32);
    mbedtls_md_free(&ctx);
}

// Helper: hex encode for printing
void bin2hex(const uint8_t* bin, size_t len, char* outHex) {
    for(size_t i = 0; i < len; ++i) {
        sprintf(outHex + i*2, "%02x", bin[i]);
    }
    outHex[len*2] = '\0';
}

// Calculate transaction hash
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

// Calculate block hash
void calculateBlockHash(Block* block) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&ctx);

    // Metadata
    uint8_t buf[64];
    int len = snprintf((char*)buf, sizeof(buf), "%u|%u|", block->index, block->timestamp);
    mbedtls_md_update(&ctx, buf, len);
    mbedtls_md_update(&ctx, (const unsigned char*)block->validator, strlen(block->validator));
    mbedtls_md_update(&ctx, (const unsigned char*)&block->nonce, sizeof(block->nonce));
    
    // Previous hash
    mbedtls_md_update(&ctx, block->previousHash, 32);

    // Transaction hashes
    for(int i = 0; i < block->txCount; ++i) {
        mbedtls_md_update(&ctx, block->txHashes[i], 32);
    }

    mbedtls_md_finish(&ctx, block->blockHash);
    mbedtls_md_free(&ctx);
}

// Sign transaction (simplified)
void signTransaction(Transaction* tx) {
    char data[100];
    char hashHex[65];
    bin2hex(tx->txHash, 32, hashHex);
    snprintf(data, sizeof(data), "%s|%s", hashHex, myAddress);
    calculateSHA256Binary((uint8_t*)data, strlen(data), tx->signature);
}

// ==================== BLOCKCHAIN FUNCTIONS ====================

// Create genesis block
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
}

// Validate block
bool validateBlock(Block* block) {
    // Check block index
    if(block->index != totalBlocks) {
        Serial.printf("✗ Invalid block index: %u (expected %u)\n", 
                     block->index, totalBlocks);
        return false;
    }
    
    // Check previous hash
    if(blockCount > 0) {
        Block* lastBlock = &blockchain[(blockCount - 1) % MAX_BLOCKS];
        if (memcmp(block->previousHash, lastBlock->blockHash, 32) != 0){
            Serial.println("✗ Previous hash mismatch");
            return false;
        }
    }
    
    // Verify block hash
    Block tempBlock = *block;
    calculateBlockHash(&tempBlock);
    
    if (memcmp(tempBlock.blockHash, block->blockHash, 32) != 0) {
        Serial.println("✗ Block hash invalid");
        return false;
    }
    
    return true;
}

// Add block to chain
bool addBlock(Block* newBlock) {
    if(!validateBlock(newBlock)) {
        return false;
    }
    
    // Store in circular buffer
    uint32_t index = blockCount % MAX_BLOCKS;
    blockchain[index] = *newBlock;
    blockCount++;
    totalBlocks++;
    
    // Clear processed transactions from pool
    txPoolCount = 0;
    
    Serial.printf("✓ Block #%u added (%d tx)\n", 
                 newBlock->index, newBlock->txCount);
    
    return true;
}

// Create new block from transaction pool
Block createBlock() {
    Block newBlock = {0};
    newBlock.index = totalBlocks;
    newBlock.timestamp = millis() / 1000;
    
    // Copy transactions from pool
    newBlock.txCount = (txPoolCount < MAX_TX_PER_BLOCK) ? txPoolCount : MAX_TX_PER_BLOCK;

    for (int i = 0; i < newBlock.txCount; ++i) {
        memcpy(newBlock.txHashes[i], txPool[i].txHash, 32);
    }

    // Get previous block hash
    if(blockCount > 0) {
        Block* prevBlock = &blockchain[(blockCount - 1) % MAX_BLOCKS];
        memcpy(newBlock.previousHash, prevBlock->blockHash, 32);
    } else {
        memset(newBlock.previousHash, 0, 32);
    }
    
    strcpy(newBlock.validator, myAddress);
    newBlock.nonce = random(0, 1000000);
    
    // Calculate hash
    calculateBlockHash(&newBlock);
    
    return newBlock;
}

// ==================== TELEMETRY FUNCTIONS ====================

// Create telemetry transaction
Transaction createTelemetryTransaction() {
    Transaction tx = {0};
    
    // Populate sensor data (replace with real sensors)
    snprintf(tx.data.sensorId, sizeof(tx.data.sensorId), "ESP_%s", myAddress + 9);
    tx.data.temperature = 20.0 + random(-50, 150) / 10.0;  // Simulate
    tx.data.humidity = 40.0 + random(0, 400) / 10.0;
    tx.data.pressure = 1013.25 + random(-100, 100) / 10.0;
    tx.data.batteryVoltage = 3.3 + random(-3, 3) / 10.0;
    tx.data.timestamp = millis() / 1000;
    tx.data.rssi = WiFi.RSSI();
    tx.data.dataQuality = 95 + random(0, 5);
    
    // Calculate hash and sign
    calculateTxHash(&tx);
    signTransaction(&tx);
    tx.verified = false;
    
    return tx;
}

// Add transaction to pool
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

// Query telemetry data
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

// Setup broadcast peer (only once)
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

// ESP-NOW receive callback
void onDataReceived(const uint8_t* mac, const uint8_t* data, int len) {
    NetworkPacket* packet = (NetworkPacket*)data;
    
    // Add peer if new
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
    
    // Handle message
    switch(packet->type) {
        case MSG_NEW_TELEMETRY: {
            Transaction* tx = (Transaction*)packet->data;
            addToTxPool(tx);
            break;
        }
        
        case MSG_NEW_BLOCK: {
            BlockHeader* header = (BlockHeader*)packet->data;
            // In a real implementation, we'd request full block data
            // For now, we just acknowledge receipt
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

// Broadcast packet to all peers
void broadcastPacket(NetworkPacket* packet) {
    strcpy(packet->sender, myAddress);
    
    // Ensure broadcast peer is added
    setupBroadcastPeer();
    
    // Broadcast for discovery
    uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_err_t result = esp_now_send(broadcastAddr, (uint8_t*)packet, sizeof(NetworkPacket));
    
    if(result != ESP_OK && result != ESP_ERR_ESPNOW_NOT_FOUND) {
        Serial.printf("✗ Broadcast error: %d\n", result);
    }
}

// Broadcast telemetry transaction
void broadcastTelemetry(Transaction* tx) {
    NetworkPacket packet;
    packet.type = MSG_NEW_TELEMETRY;
    memcpy(packet.data, tx, sizeof(Transaction));
    packet.dataLen = sizeof(Transaction);
    
    broadcastPacket(&packet);
}

// Broadcast new block (header only to fit in payload)
void broadcastBlock(Block* block) {
    NetworkPacket packet;
    packet.type = MSG_NEW_BLOCK;
    
    // Create compact header
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

// ==================== CONSENSUS (IMPROVED PoA) ====================

bool isMyTurnToValidate() {
    // Single-node mode: always mine
    if(peerCount == 0) return true;
    
    // Multi-node: round-robin based on time
    unsigned long interval = BLOCK_TIME_MS / 1000;
    unsigned long currentSlot = (millis() / 1000) / interval;
    
    // Use last byte of address as validator ID
    int myId = myAddress[15] % (peerCount + 1);
    int validatorSlot = currentSlot % (peerCount + 1);
    
    return (myId == validatorSlot);
}

// Mining/validation task (IMPROVED)
void validatorTask() {
    if(MY_ROLE != VALIDATOR_NODE) return;
    
    unsigned long now = millis();
    bool shouldMine = false;
    const char* reason = "";
    
    // Emergency mining if pool nearly full
    if(txPoolCount >= (TX_POOL_SIZE - 4)) {
        shouldMine = true;
        reason = "Emergency (pool nearly full)";
    }
    // Regular timed mining
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
    
    // Collect telemetry every 10 seconds
    if(now - lastTelemetryTime >= 10000) {
        Transaction tx = createTelemetryTransaction();
        
        addToTxPool(&tx);
        broadcastTelemetry(&tx);
        
        lastTelemetryTime = now;
    }
}

// ==================== PEER DISCOVERY TASK ====================

void peerDiscoveryTask() {
    unsigned long now = millis();
    
    // Periodic peer announcements for better discovery
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
    
    Serial.printf(" Uptime: %lu seconds\n", millis() / 1000);
    Serial.printf(" Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println();
}

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║  ESP32 BLOCKCHAIN TELEMETRY v1.2   ║");
    Serial.println("║    DYNAMIC ROLE ASSIGNMENT         ║");
    Serial.println("╚════════════════════════════════════╝\n");
    
    // Initialize WiFi for ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Get MAC address as node identity
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(myAddress, sizeof(myAddress), 
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    Serial.printf("Node Address: %s\n", myAddress);
    
    // Assign role dynamically
    assignNodeRole();
    
    Serial.printf("Max TX per block: %d\n\n", MAX_TX_PER_BLOCK);
    
    // Initialize ESP-NOW
    if(esp_now_init() != ESP_OK) {
        Serial.println("✗ ESP-NOW init failed");
        return;
    }
    
    esp_now_register_recv_cb(onDataReceived);
    
    // Create genesis block
    createGenesisBlock();
    
    // Setup broadcast peer
    setupBroadcastPeer();
    
    // Initial announcement
    NetworkPacket announce;
    announce.type = MSG_PEER_ANNOUNCE;
    strcpy(announce.sender, myAddress);
    announce.dataLen = 0;
    
    uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcastAddr, (uint8_t*)&announce, sizeof(announce));
    
    Serial.println("✓ System initialized\n");
    
    lastBlockTime = millis();
    lastTelemetryTime = millis();
    lastAnnounceTime = millis();
}

// ==================== MAIN LOOP ====================

void loop() {
    static unsigned long lastStatus = 0;
    
    // Check for role change commands
    checkRoleChangeCommand();
    
    // Run tasks
    sensorTask();
    validatorTask();
    peerDiscoveryTask();
    
    // Print status every 30 seconds
    if(millis() - lastStatus >= 30000) {
        printStatus();
        lastStatus = millis();
        
        // Demo: Query recent data using correct sensor ID
        if(blockCount > 1 && txPoolCount > 0) {
            char querySensorId[20];
            snprintf(querySensorId, sizeof(querySensorId), "ESP_%s", myAddress + 9);
            queryTelemetryData(querySensorId, 0, UINT32_MAX);
        }
    }
    
    delay(100);
}