# ESP32 Blockchain Telemetry System with SPIFFS Storage

![Version](https://img.shields.io/badge/version-1.3-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)

A distributed blockchain-based telemetry system for ESP32 microcontrollers with persistent storage, mesh networking, and dynamic role assignment.



## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    CLOUD BACKEND                            │
│  Flask API + Pusher (Real-time)                             │
│  URL: http://your-server:5000/api                           │
└──────────────────────┬──────────────────────────────────────┘
                       │ HTTP/REST
                       │
┌──────────────────────▼──────────────────────────────────────┐
│              BRIDGE NODE (ESP32 with WiFi)                  │
│  - WiFi: Connects to backend                                │
│  - ESP-NOW: Participates in mesh network                    │
│  - Role: VALIDATOR/SENSOR/ARCHIVE                           │
│  - Forwards blockchain data to cloud                        │
└──────────────────────┬──────────────────────────────────────┘
                       │ ESP-NOW Mesh
         ┌─────────────┼─────────────┐
         │             │             │
┌────────▼────┐  ┌────▼─────┐  ┌───▼──────┐
│  ESP32 #2   │  │ ESP32 #3 │  │ ESP32 #4 │
│  ESP-NOW    │  │ ESP-NOW  │  │ ESP-NOW  │
│  SENSOR     │  │ SENSOR   │  │ VALIDATOR│
└─────────────┘  └──────────┘  └──────────┘
```




# ESP32 Blockchain System Setup Guide



# UI coming soon

The ui code for dash board is been cooked Cooming soon

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Architecture](#architecture)
- [Configuration](#configuration)
- [Usage](#usage)
- [Serial Commands](#serial-commands)
- [Storage Management](#storage-management)
- [Troubleshooting](#troubleshooting)
- [API Reference](#api-reference)
- [Contributing](#contributing)
- [License](#license)

## 🎯 Overview

This project implements a decentralized blockchain network on ESP32 devices for collecting, validating, and storing sensor telemetry data. Each node can act as a sensor, validator, or archive node, creating a resilient mesh network with immutable data storage.

### Key Capabilities

- **Decentralized Architecture**: No single point of failure
- **Mesh Networking**: ESP-NOW peer-to-peer communication
- **Persistent Storage**: SPIFFS-based blockchain persistence
- **Dynamic Roles**: Automatic or manual node role assignment
- **Proof of Authority**: Efficient consensus mechanism
- **Immutable Records**: Cryptographically secured telemetry data

## ✨ Features

### Core Features

- ✅ **Blockchain Implementation**

  - SHA-256 cryptographic hashing
  - Block validation and chain integrity
  - Transaction pool management
  - Genesis block creation
- ✅ **Network Communication**

  - ESP-NOW mesh networking
  - Broadcast peer discovery
  - Block header propagation
  - Transaction broadcasting
- ✅ **Persistent Storage**

  - SPIFFS filesystem integration
  - Automatic blockchain saves
  - Transaction pool persistence
  - Metadata tracking
- ✅ **Role Management**

  - Sensor nodes (data collection)
  - Validator nodes (block mining)
  - Archive nodes (long-term storage)
  - Runtime role switching
- ✅ **Telemetry Data**

  - Temperature monitoring
  - Humidity tracking
  - Pressure readings
  - Battery voltage
  - RSSI signal strength
  - Data quality metrics

## 🔧 Hardware Requirements

### Minimum Requirements

- **Microcontroller**: ESP32 DevKit (any variant)
- **Flash Memory**: 4MB minimum (for SPIFFS)
- **RAM**: 520KB (standard ESP32)
- **Power Supply**: 3.3V regulated

### Recommended Hardware

- ESP32-WROOM-32 or ESP32-WROVER module
- USB cable for programming
- External sensors (optional):
  - DHT22 (temperature/humidity)
  - BME280 (temperature/humidity/pressure)
  - Battery monitoring circuit

### Network Setup

- **Minimum**: 1 ESP32 (single-node mode)
- **Recommended**: 3+ ESP32 devices for mesh network
- **Maximum**: 10 peers per node (configurable)

## 💻 Software Requirements

### Development Environment

- **PlatformIO** (recommended) or Arduino IDE
- **ESP32 Arduino Core** v2.0.0 or later
- **Python 3.7+** (for PlatformIO)

### Required Libraries

All libraries are included in ESP32 Arduino Core:

- `esp_now.h` - ESP-NOW protocol
- `WiFi.h` - WiFi functionality
- `mbedtls/md.h` - Cryptographic functions
- `SPIFFS.h` - Filesystem
- `Preferences.h` - Non-volatile storage

## 📦 Installation

### Method 1: PlatformIO (Recommended)

```bash
# 1. Install PlatformIO Core
pip install platformio

# 2. Create new project
mkdir esp32-blockchain
cd esp32-blockchain
pio project init --board esp32dev

# 3. Copy files
# - Copy main code to src/main.cpp
# - Copy platformio.ini to project root
# - (Optional) Copy custom_partitions.csv to project root

# 4. Build and upload
pio run --target upload

# 5. Open serial monitor
pio device monitor
```

### Method 2: Arduino IDE

```bash
# 1. Install ESP32 board support
# Tools → Board → Boards Manager → Search "ESP32" → Install

# 2. Create new sketch
# Copy the main code into the sketch

# 3. Configure board
# Tools → Board → ESP32 Dev Module
# Tools → Partition Scheme → Default 4MB with spiffs

# 4. Upload
# Click Upload button
```

### Project Structure

```
esp32-blockchain/
├── platformio.ini              # PlatformIO configuration
├── custom_partitions.csv       # Optional: Extended SPIFFS
├── README.md                   # This file
├── src/
│   └── main.cpp               # Main application code
└── .gitignore                 # Git ignore file
```

## 🚀 Quick Start

### 1. First Upload

```bash
# Upload to your first ESP32
pio run --target upload --upload-port /dev/ttyUSB0

# Open serial monitor
pio device monitor --port /dev/ttyUSB0
```

Expected output:

```
╔════════════════════════════════════╗
║  ESP32 BLOCKCHAIN TELEMETRY v1.3   ║
║    WITH SPIFFS STORAGE             ║
╚════════════════════════════════════╝

Node Address: AA:BB:CC:DD:EE:FF
Role Strategy: MAC-based (deterministic)
✓ Role assigned: VALIDATOR

📁 Initializing SPIFFS...
✓ SPIFFS mounted
  Total: 1507328 bytes
  
✓ Genesis block created
  Hash: a3f5e8c9d2b4a1e7...
  
✓ System initialized
```

### 2. Add More Nodes

Upload the same code to additional ESP32 devices. They will:

1. Auto-discover each other via ESP-NOW
2. Assign roles based on MAC address
3. Start collecting and validating telemetry
4. Sync blockchain data

### 3. Monitor Operation

Watch the serial output for:

- Block mining events
- Peer discoveries
- Transaction broadcasts
- Storage operations

## 🏗️ Architecture

### System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Node                            │
├─────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐              │
│  │  Sensor  │  │Validator │  │ Archive  │              │
│  │   Task   │  │   Task   │  │   Task   │              │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘              │
│       │             │              │                     │
│  ┌────┴─────────────┴──────────────┴─────┐              │
│  │        Transaction Pool                │              │
│  └────────────────┬───────────────────────┘              │
│                   │                                      │
│  ┌────────────────┴───────────────────────┐              │
│  │          Blockchain Layer              │              │
│  │  ┌──────┐  ┌──────┐  ┌──────┐         │              │
│  │  │Block │→ │Block │→ │Block │→ ...    │              │
│  │  │  #0  │  │  #1  │  │  #2  │         │              │
│  │  └──────┘  └──────┘  └──────┘         │              │
│  └────────────────┬───────────────────────┘              │
│                   │                                      │
│  ┌────────────────┴───────────────────────┐              │
│  │         SPIFFS Storage                 │              │
│  │  • blockchain.dat                      │              │
│  │  • txpool.dat                          │              │
│  │  • metadata.dat                        │              │
│  └────────────────┬───────────────────────┘              │
│                   │                                      │
│  ┌────────────────┴───────────────────────┐              │
│  │       ESP-NOW Mesh Network             │              │
│  └────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────┘
           ↕                    ↕
    [Other ESP32]        [Other ESP32]
```

### Data Flow

1. **Sensor Collection**: Sensors read telemetry data
2. **Transaction Creation**: Data packaged into transactions
3. **Broadcasting**: Transactions broadcast to network
4. **Pool Management**: Transactions added to pool
5. **Block Mining**: Validators create blocks from pool
6. **Validation**: Blocks validated by network
7. **Storage**: Blockchain saved to SPIFFS
8. **Persistence**: Data survives reboots

### Node Roles

#### Sensor Node

- Collects telemetry data every 10 seconds
- Creates and broadcasts transactions
- Stores blockchain locally
- Does not mine blocks

#### Validator Node

- Performs all sensor functions
- Mines new blocks (Proof of Authority)
- Validates incoming blocks
- Round-robin block creation

#### Archive Node

- Stores complete blockchain history
- Provides data for network sync
- No active mining or sensing
- Long-term data retention

### Role Assignment Strategies

#### MAC-Based (Default)

```cpp
RoleStrategy ROLE_STRATEGY = STRATEGY_MAC_BASED;
```

- Deterministic role based on MAC address hash
- 30% become validators
- 65% become sensors
- 5% become archive nodes

#### First-Come Strategy

```cpp
RoleStrategy ROLE_STRATEGY = STRATEGY_FIRST_COME;
```

- First 3 nodes become validators
- Every 10th node becomes archive
- Rest become sensors

#### All-Validator (Testing)

```cpp
RoleStrategy ROLE_STRATEGY = STRATEGY_ALL_VALIDATOR;
```

- All nodes act as validators
- Useful for development and testing

## ⚙️ Configuration

### Key Constants

Edit these in the main code:

```cpp
// Storage Configuration
#define MAX_BLOCKS 50           // Blocks in RAM (circular buffer)
#define MAX_PEERS 10            // Maximum peer nodes
#define TX_POOL_SIZE 20         // Transaction pool size

// Timing Configuration
#define BLOCK_TIME_MS 30000     // Block creation interval (30s)
#define SAVE_INTERVAL 60000     // SPIFFS save interval (60s)
#define PEER_ANNOUNCE_INTERVAL 60000  // Peer discovery (60s)

// Blockchain Configuration
#define MAX_TX_PER_BLOCK 4      // Transactions per block

// Storage Paths
#define BLOCKCHAIN_FILE "/blockchain.dat"
#define TXPOOL_FILE "/txpool.dat"
#define METADATA_FILE "/metadata.dat"
```

### Partition Scheme

#### Default Partition (1.5MB SPIFFS)

```ini
# In platformio.ini
board_build.filesystem = spiffs
```

#### Custom Partition (Larger SPIFFS)

```ini
# In platformio.ini
board_build.partitions = custom_partitions.csv
```

Custom partition layout:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
app1,     app,  ota_1,   0x150000,0x140000,
spiffs,   data, spiffs,  0x290000,0x170000,
```

This provides **~1.5MB SPIFFS** for blockchain storage.

### Role Strategy Selection

Change role assignment strategy:

```cpp
// In main code, near top
RoleStrategy ROLE_STRATEGY = STRATEGY_MAC_BASED;  // Change this

// Options:
// STRATEGY_MAC_BASED      - Hash-based (default)
// STRATEGY_FIRST_COME     - Join order based
// STRATEGY_ALL_VALIDATOR  - All nodes validate
// STRATEGY_RUNTIME_ELECT  - Future: network voting
```

## 📖 Usage

### Serial Commands


| Command | Function  | Description           |
| ------- | --------- | --------------------- |
| `V`     | Validator | Set node as validator |
| `S`     | Sensor    | Set node as sensor    |
| `A`     | Archive   | Set node as archive   |
| `W`     | Write     | Manual SPIFFS save    |
| `C`     | Clear     | Delete all storage    |
| `L`     | List      | Show SPIFFS files     |
| `?`     | Help      | Show menu            |

### Example Usage Session

```bash
# 1. Start serial monitor
pio device monitor

# 2. System boots and loads blockchain
✓ Loaded 15 blocks from SPIFFS

# 3. Watch automatic operation
⛏️  Mining new block (4 txs pending) - Scheduled
✓ Block #16 mined and broadcast
💾 Saving blockchain to SPIFFS...
✓ Saved 16 blocks to SPIFFS

# 4. Check storage
> L
📂 SPIFFS Files:
  /blockchain.dat (4800 bytes)
  /txpool.dat (480 bytes)
  /metadata.dat (40 bytes)

# 5. Manual save
> W
💾 Manual save triggered
✓ Periodic save completed

# 6. Clear and restart
> C
🗑️  Clearing all stored data...
  ✓ Blockchain file removed
  ✓ Transaction pool file removed
  ✓ Metadata file removed
✓ Storage cleared

# 7. Reset ESP32 - creates new genesis
```

### Status Display

Every 30 seconds, status is printed:

```
╔════════════════════════════════════╗
║   BLOCKCHAIN TELEMETRY STATUS      ║
╚════════════════════════════════════╝
 Address: AA:BB:CC:DD:EE:FF
 Role: VALIDATOR
 Blocks: 15 (total: 15)
 TX Pool: 3 / 20
 Peers: 2 connected
 Last Block: #14 (4 tx)
 Last Hash: a3f5e8c9d2b4a1e7...
 SPIFFS: 15360 / 1507328 bytes
 Uptime: 456 seconds
 Free heap: 234567 bytes
```

## 💾 Storage Management

### File Structure


| File              | Content              | Size (approx)              |
| ----------------- | -------------------- | -------------------------- |
| `/blockchain.dat` | All blocks           | ~300 bytes × blocks       |
| `/txpool.dat`     | Pending transactions | ~120 bytes × transactions |
| `/metadata.dat`   | Chain metadata       | ~40 bytes                  |

### Storage Capacity

With default 1.5MB SPIFFS:

- **Maximum blocks**: ~5,000 blocks
- **With transactions**: ~1,000 blocks (4 tx each)
- **Actual limit**: Depends on data patterns

### Automatic Operations

1. **On Startup**: Loads blockchain and transaction pool
2. **After Mining**: Immediately saves new block
3. **Periodic**: Saves every 60 seconds
4. **On Genesis**: Saves initial block

### Manual Operations

```cpp
// Save immediately
> W

// Clear all data
> C

// List files and sizes
> L
```

### Data Persistence

The blockchain survives:

- ✅ Power loss
- ✅ Reboots
- ✅ Code updates (if SPIFFS preserved)
- ✅ Network disconnections

### Best Practices

1. **Regular Monitoring**: Check SPIFFS usage with 'L' command
2. **Backup Important Data**: Export via serial before clearing
3. **Adjust Save Interval**: Balance safety vs. flash wear
4. **Monitor Free Space**: Keep at least 20% free

## 🔍 Troubleshooting

### Common Issues

#### SPIFFS Mount Failed

```
✗ SPIFFS mount failed
```

**Solutions:**

1. Check partition table includes SPIFFS
2. First-time format is automatic
3. Try manual format:

```cpp
SPIFFS.format();  // Add to setup() temporarily
```

#### Blockchain Won't Load

```
ℹ️  No blockchain file found, starting fresh
```

**This is normal for first boot.** If unexpected:

1. Check files exist: Press 'L'
2. Verify file sizes are non-zero
3. Try clearing and restarting: Press 'C', then reset

#### Out of Memory

```
Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
```

**Solutions:**

1. Reduce `MAX_BLOCKS` (try 25 instead of 50)
2. Reduce `TX_POOL_SIZE` (try 10 instead of 20)
3. Check free heap in status display
4. Disable verbose logging

#### Peers Not Connecting

```
Peers: 0 connected
```

**Solutions:**

1. Ensure all devices on same WiFi channel
2. Check ESP-NOW initialization success
3. Verify broadcast peer added
4. Devices must be within ESP-NOW range (~100m)
5. Wait for peer announcement cycle (60s)

#### Blocks Not Mining

```
TX Pool: 20 / 20  (full but no mining)
```

**Check:**

1. Node role is VALIDATOR: Press 'V'
2. Check `isMyTurnToValidate()` logic
3. Verify BLOCK_TIME_MS interval passed
4. Look for mining messages in serial output

#### Storage Full

```
✗ Failed to open blockchain file for writing
```

**Solutions:**

1. Check usage: Press 'L'
2. Clear old data: Press 'C'
3. Increase SPIFFS partition size
4. Implement block pruning

### Debug Mode

Enable verbose debugging:

```cpp
// In platformio.ini
build_flags = 
    -D CORE_DEBUG_LEVEL=5  // Maximum debug output
```

### Performance Monitoring

Monitor key metrics:

```
Free heap: 234567 bytes     ← Should stay > 100KB
SPIFFS: 15360 / 1507328    ← Should stay < 80%
Uptime: 456 seconds         ← Track stability
Peers: 2 connected          ← Network health
```

## 📚 API Reference

### Core Functions

#### Blockchain Management

```cpp
void createGenesisBlock();
// Creates the first block in the chain

bool validateBlock(Block* block);
// Validates block integrity and chain linkage

bool addBlock(Block* newBlock);
// Adds validated block to chain and saves to SPIFFS

Block createBlock();
// Creates new block from transaction pool
```

#### Transaction Handling

```cpp
Transaction createTelemetryTransaction();
// Creates transaction from current sensor readings

bool addToTxPool(Transaction* tx);
// Adds transaction to pending pool

void calculateTxHash(Transaction* tx);
// Computes SHA-256 hash of transaction

void signTransaction(Transaction* tx);
// Signs transaction with node identity
```

#### Storage Operations

```cpp
bool initSPIFFS();
// Initializes SPIFFS filesystem

bool saveBlockchain();
// Saves entire blockchain to SPIFFS

bool loadBlockchain();
// Loads blockchain from SPIFFS on startup

bool saveTxPool();
// Saves pending transactions

bool loadTxPool();
// Loads pending transactions

void clearStorage();
// Deletes all SPIFFS files
```

#### Network Functions

```cpp
void broadcastTelemetry(Transaction* tx);
// Broadcasts transaction to all peers

void broadcastBlock(Block* block);
// Broadcasts block header to network

void onDataReceived(const uint8_t* mac, const uint8_t* data, int len);
// ESP-NOW receive callback

void setupBroadcastPeer();
// Configures ESP-NOW broadcast address
```

#### Role Management

```cpp
void assignNodeRole();
// Assigns role based on configured strategy

NodeRole assignRoleByMAC(const char* macAddr);
// Deterministic role from MAC hash

void checkRoleChangeCommand();
// Handles serial role change commands
```

### Data Structures

#### Block Structure

```cpp
struct Block {
    uint32_t index;                      // Block number
    uint32_t timestamp;                  // Creation time
    Hash32 txHashes[MAX_TX_PER_BLOCK];  // Transaction hashes
    uint8_t txCount;                     // Number of transactions
    Hash32 previousHash;                 // Previous block hash
    Hash32 blockHash;                    // This block's hash
    char validator[17];                  // Creator's MAC
    uint32_t nonce;                      // Random nonce
} __attribute__((packed));
```

#### Transaction Structure

```cpp
struct Transaction {
    Hash32 txHash;                // Transaction hash
    TelemetryData data;           // Sensor data payload
    uint8_t signature[32];        // Digital signature
    uint8_t verified;             // Verification flag
} __attribute__((packed));
```

#### Telemetry Data

```cpp
struct TelemetryData {
    char sensorId[16];           // Sensor identifier
    float temperature;           // Temperature (°C)
    float humidity;              // Humidity (%)
    float pressure;              // Pressure (hPa)
    float batteryVoltage;        // Battery (V)
    uint32_t timestamp;          // Unix timestamp
    int16_t rssi;                // WiFi signal strength
    uint8_t dataQuality;         // Data quality score
} __attribute__((packed));
```

## 🤝 Contributing

### Development Setup

```bash
# Clone repository
git clone https://github.com/yourusername/esp32-blockchain.git
cd esp32-blockchain

# Install dependencies
pio lib install

# Build
pio run

# Run tests (if available)
pio test
```

### Coding Standards

- Follow existing code style
- Comment complex logic
- Use meaningful variable names
- Test on real hardware before PR

### Contribution Areas

- 🐛 Bug fixes
- ✨ New features
- 📝 Documentation improvements
- 🧪 Test coverage
- 🎨 Code optimization

### Roadmap

- [ ]  Chain synchronization protocol
- [ ]  Web dashboard for monitoring
- [ ]  SD card archival support
- [ ]  Advanced consensus algorithms
- [ ]  Data compression
- [ ]  OTA updates
- [ ]  RESTful API
- [ ]  Mobile app integration

## 📄 License

MIT License

Copyright (c) 2024 ESP32 Blockchain Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## 🙏 Acknowledgments

- ESP32 Arduino Core developers
- PlatformIO team
- Blockchain community
- Open source contributors

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/esp32-blockchain/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/esp32-blockchain/discussions)
- **Email**: brianndambia6@gmail.com

## 🔗 Related Projects

- [ESP32 IoT Framework](https://github.com/espressif/esp-idf)
- [Arduino ESP32](https://github.com/espressif/arduino-esp32)
- [PlatformIO](https://platformio.org)

---

**Made with ❤️ for the IoT and Blockchain communities by Ndambia**

*Last Updated: November 2025*
