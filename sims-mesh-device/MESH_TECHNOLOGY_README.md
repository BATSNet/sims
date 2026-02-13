# SIMS Mesh Technology Implementation

## Overview

This implementation adds WiFi, Bluetooth, and Meshtastic integration to the SIMS mesh device, enabling:

- WiFi connectivity for direct backend upload and gateway mode
- Bluetooth BLE bridge for phones to use ESP32 as LoRa modem
- MQTT gateway mode for forwarding mesh packets to backend
- Meshtastic protocol support for interoperability
- Intelligent transport routing (WiFi > LoRa)
- Offline queue with auto-sync

## Implementation Status

### Phase 0: Hardware Configuration Fix
**Status:** Complete

- GPS pins moved from GPIO43/44 to GPIO39/40 (avoids USB serial conflict)
- Vext power control documented (GPIO36 for external peripherals)
- All pin definitions centralized in config.h

**Files:**
- `include/config.h` - Updated pin definitions
- `src/main.cpp` - Updated to use config.h pins
- `QUICK_START.md` - Updated hardware setup guide

### Phase 1: WiFi Connectivity
**Status:** Complete

- WiFi connection management with auto-reconnect
- NVS storage for up to 5 network credentials
- BLE-based WiFi configuration (for initial setup)
- RSSI monitoring and network scanning
- Auto-disable BLE when WiFi connected (power saving)

**Files:**
- `include/network/wifi_service.h`
- `src/network/wifi_service.cpp`
- `include/network/wifi_config_ble.h`
- `src/network/wifi_config_ble.cpp`

**Usage:**
```cpp
WiFiService wifiService;
wifiService.begin();

// Or configure via BLE:
WiFiConfigBLE wifiConfigBLE(&wifiService);
wifiConfigBLE.begin();
```

### Phase 2: HTTP Direct Upload
**Status:** Complete

- POST to `/api/incidents` endpoint (JSON format)
- Chunked upload for images/audio
- Transport manager for intelligent routing (WiFi > LoRa)
- Offline queue integration with MessageStorage
- Retry logic with exponential backoff

**Files:**
- `include/network/http_client.h`
- `src/network/http_client.cpp`
- `include/network/transport_manager.h`
- `src/network/transport_manager.cpp`

**Usage:**
```cpp
HTTPClientService httpClient;
httpClient.begin(BACKEND_HOST, BACKEND_PORT);

auto result = httpClient.uploadIncident(
    latitude, longitude, altitude,
    priority, category, description,
    imageData, imageSize,
    audioData, audioSize
);
```

### Phase 3: MQTT Gateway Mode
**Status:** Complete

- Connect to MQTT broker (same host as backend)
- Publish mesh incidents to `sims/mesh/incidents/in` (Protobuf format)
- Publish network status to `sims/mesh/status`
- Publish node list to `sims/mesh/nodes`
- Subscribe to `sims/mesh/incidents/out` for backend commands
- QoS 1 with persistent session

**Files:**
- `include/network/mqtt_client.h`
- `src/network/mqtt_client.cpp`
- `platformio.ini` - Added PubSubClient library

**Backend Integration:**
- Works with existing `sims-backend/services/mesh_gateway_service.py`
- Works with existing `sims-backend/plugins/mesh_plugin.py`

**Usage:**
```cpp
MQTTClientService mqttClient;
String clientId = MQTT_CLIENT_ID_PREFIX + String((uint32_t)ESP.getEfuseMac(), HEX);
mqttClient.begin(MQTT_BROKER, MQTT_PORT, clientId.c_str());

// Publish incident (raw Protobuf)
mqttClient.publishIncident(protobufData, dataSize, priority);

// Publish status
MQTTClientService::NetworkStatus status;
status.nodeCount = 5;
status.rssi = -65;
mqttClient.publishStatus(status);
```

### Phase 4: Bluetooth BLE Service
**Status:** Complete

- 5 GATT characteristics:
  1. Incident TX (write) - Receive incidents from app
  2. Mesh RX (notify) - Forward mesh messages to app
  3. Status (read/notify) - GPS, mesh nodes, battery status
  4. Config (read/write) - Device settings
  5. Media Transfer (write) - Chunked image/audio data
- Multi-connection support (up to 9 devices)
- BLE → LoRa → Backend bridging
- Media chunking protocol (8-byte header + 504-byte payload)

**Files:**
- `include/ble/ble_service.h`
- `src/ble/ble_service.cpp`

**Usage:**
```cpp
BLEService bleService;
bleService.begin(&loraTransport, &meshProtocol);
bleService.setGPSLocation(&gpsLocation);

// Forward mesh message to BLE clients
bleService.notifyMeshMessage(data, dataSize);

// Update status
BLEService::DeviceStatus status;
status.latitude = 52.520008;
status.longitude = 13.404954;
status.meshNodes = 5;
status.batteryPercent = 85;
status.gpsValid = true;
bleService.updateStatus(status);
```

### Phase 5: Meshtastic Integration
**Status:** Framework Complete (Protobuf integration pending)

- Meshtastic adapter for protocol conversion
- Protocol manager for dual-protocol support
- Operating modes:
  - `PROTOCOL_MODE_SIMS_ONLY` (default)
  - `PROTOCOL_MODE_MESHTASTIC_ONLY`
  - `PROTOCOL_MODE_DUAL_HYBRID` (intelligent routing)
  - `PROTOCOL_MODE_BRIDGE` (send via both)

**Files:**
- `include/meshtastic_adapter.h`
- `src/meshtastic_adapter.cpp`
- `include/protocol_manager.h`
- `src/protocol_manager.cpp`

**Note:** This is a simplified adapter. Full Meshtastic integration requires:
1. Downloading Meshtastic .proto files from https://github.com/meshtastic/protobufs
2. Running nanopb generator to create .pb.h/.pb.c files
3. Updating adapter to use actual Meshtastic protobuf structures

**Usage:**
```cpp
MeshtasticAdapter meshtasticAdapter;
ProtocolManager protocolManager(&loraTransport);
protocolManager.begin(PROTOCOL_MODE_DUAL_HYBRID);
protocolManager.setMeshtasticAdapter(&meshtasticAdapter);

// Send incident (auto-routes based on mode)
protocolManager.sendIncident(latitude, longitude, description, priority, hasMedia);
```

### Phase 6: Integration
**Status:** Complete

- Comprehensive integration guide in `src/main.cpp`
- All services can work together
- Display manager updated with WiFi/BLE status
- Full end-to-end flow documented

## Architecture

### Data Flow: Device → Backend

**Option 1: WiFi Direct Upload**
```
Device → HTTP POST → Backend API → Database
```

**Option 2: WiFi Gateway Mode**
```
Device (LoRa) → Gateway (LoRa → MQTT) → Backend → Database
```

**Option 3: BLE Bridge**
```
Phone (BLE) → Device (LoRa) → Gateway (MQTT) → Backend → Database
```

**Option 4: Offline Queue**
```
Device → LittleFS Queue → [WiFi reconnects] → Auto-sync → Backend
```

### Transport Priority

1. WiFi (if connected) - Fastest, most reliable
2. LoRa mesh - Fallback when no WiFi
3. Offline queue - When no connectivity available

### Bandwidth Optimization

**Image Compression:**
- JPEG quality: 20% (configurable 10/20/30)
- Resolution: 640x480 VGA (down from 1600x1200)
- Target size: ~10-15 KB per image

**Audio Compression:**
- Codec: Opus (8 kbps)
- VAD (Voice Activity Detection) enabled
- Target size: ~10 KB for 10s audio

**Protobuf vs JSON:**
- LoRa mesh: Protobuf (3-5x smaller than JSON)
- MQTT gateway: Protobuf (no conversion)
- HTTP REST API: JSON (backend standard)

## Configuration

### WiFi Settings (`config.h`)

```cpp
#define WIFI_CONNECT_TIMEOUT 10000
#define WIFI_RECONNECT_INTERVAL 30000
#define BACKEND_HOST "192.168.1.100"
#define BACKEND_PORT 8000
```

### MQTT Settings (`config.h`)

```cpp
#define GATEWAY_MODE_ENABLED false  // Set true to enable
#define MQTT_BROKER "192.168.1.100"
#define MQTT_PORT 1883
#define MQTT_USE_TLS false          // Set true for production
```

### BLE Settings (`config.h`)

```cpp
#define BLE_DEVICE_NAME "SIMS-MESH"
#define BLE_MAX_CONNECTIONS 9
#define BLE_MTU_SIZE 512
```

### Protocol Mode (`config.h`)

```cpp
#define PROTOCOL_MODE PROTOCOL_MODE_SIMS_ONLY
```

Options:
- `PROTOCOL_MODE_SIMS_ONLY` - Use only SIMS protocol (default)
- `PROTOCOL_MODE_MESHTASTIC_ONLY` - Use only Meshtastic protocol
- `PROTOCOL_MODE_DUAL_HYBRID` - Intelligent routing (critical/media → SIMS, text → Meshtastic)
- `PROTOCOL_MODE_BRIDGE` - Send via both protocols

## Dependencies

### Libraries Added

All libraries auto-install via PlatformIO:

- `knolleary/PubSubClient@^2.8` - MQTT client
- `h2zero/NimBLE-Arduino@^1.4.1` - Bluetooth BLE (already present)
- `bblanchon/ArduinoJson@^7.0.4` - JSON parsing (already present)

### Hardware Requirements

- Heltec LoRa 32 V3 (ESP32-S3 + SX1262)
- External GPS module (connected to GPIO39/40)
- Optional: External camera/microphone for media

## Testing

### Unit Tests

1. WiFi connection with valid/invalid credentials
2. HTTP POST to backend with mock incident
3. MQTT publish/subscribe
4. BLE characteristic read/write
5. Protobuf encode/decode

### Integration Tests

1. **WiFi Direct Upload:** Connect WiFi → Capture incident → Verify backend receives
2. **Gateway Mode:** Receive LoRa packet → Forward via MQTT → Verify backend receives
3. **BLE Bridge:** Connect phone via BLE → Send incident → Verify LoRa transmission
4. **Transport Fallback:** WiFi connected = HTTP, WiFi disconnected = LoRa
5. **Offline Queue:** Queue incident when offline → WiFi reconnects → Auto-upload

### Field Tests

1. WiFi range and reconnection stability
2. BLE multi-device connectivity (3+ phones)
3. Mesh fallback in WiFi dead zones
4. Gateway relay performance
5. Battery impact (WiFi vs LoRa vs BLE)

## Security Considerations

**Implemented:**
- WiFi credentials encrypted in NVS
- BLE pairing recommended for config characteristic
- Mesh packets use AES-256 encryption (per architecture doc)

**TODO (Production):**
- MQTT over TLS (mqtts://)
- BLE bonding for trusted devices
- Input validation on all BLE characteristics
- Rate limiting on API endpoints

## Performance Metrics

**Expected Bandwidth (per incident):**
- Text only: ~100 bytes (Protobuf)
- Text + GPS: ~150 bytes
- Text + GPS + image: ~10 KB (with compression)
- Text + GPS + audio: ~10 KB (with Opus)
- Full incident (image + audio): ~20 KB

**Network Support:**
- LoRa mesh: 200 concurrent devices (TDMA)
- WiFi: Standard ESP32 limits
- BLE: Up to 9 simultaneous connections

## Troubleshooting

### WiFi Not Connecting

1. Check credentials in NVS or use BLE config
2. Verify BACKEND_HOST and BACKEND_PORT in config.h
3. Check WiFi signal strength (RSSI)
4. Review serial output for connection errors

### MQTT Not Publishing

1. Ensure GATEWAY_MODE_ENABLED is true
2. Check MQTT broker is running
3. Verify broker IP and port
4. Check WiFi is connected first

### BLE Not Visible

1. Ensure WiFi is not connected (BLE auto-disables)
2. Check Bluetooth is enabled on phone
3. Scan for device name "SIMS-MESH"
4. Review BLE service UUID in nRF Connect app

### Offline Queue Not Processing

1. Check WiFi reconnection status
2. Verify MessageStorage is initialized
3. Check LittleFS has space
4. Review queue backoff interval

## Future Enhancements

**Short-term:**
1. OTA firmware updates via WiFi
2. Web portal for WiFi configuration (alternative to BLE)
3. GPS delta encoding for tracking mode
4. On-device Whisper (voice-to-text)

**Long-term:**
1. Full Meshtastic protobuf integration
2. Mesh network health monitoring dashboard
3. Distributed consensus for critical messages
4. Edge AI for incident classification

## References

- Research: `research/mesh-network-architecture.md` - Full system architecture
- Research: `research/heltec-lora32-v3-pinout.md` - Hardware pins
- Research: `research/heltec-v3-vext-power-fix.md` - Power management
- Backend: `sims-backend/services/mesh_gateway_service.py` - MQTT subscriber
- Backend: `sims-backend/plugins/mesh_plugin.py` - Incident handler
- Main: `CLAUDE.md` - Project overview and guidelines

## Support

For implementation questions or issues:
1. Check serial monitor output for error messages
2. Review integration guide in `src/main.cpp`
3. Verify configuration in `include/config.h`
4. Consult research documentation in `research/` directory
