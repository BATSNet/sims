/**
 * SIMS Mesh Device Configuration
 */

#ifndef CONFIG_H
#define CONFIG_H

// Device configuration
#define DEVICE_NAME "SIMS-MESH"
#define FIRMWARE_VERSION "1.0.0"

// LoRa configuration
#ifndef LORA_FREQUENCY
#define LORA_FREQUENCY 868E6  // 868 MHz for EU (915E6 for US)
#endif

#define LORA_BANDWIDTH 125.0    // 125 kHz
#define LORA_SPREADING_FACTOR 7 // SF7 (fast) to SF12 (slow, long range)
#define LORA_CODING_RATE 5      // 4/5

// Quick Test Mode: Uncomment to enable Meshtastic compatibility testing
#define MESHTASTIC_TEST_MODE

#ifdef MESHTASTIC_TEST_MODE
#define LORA_SYNC_WORD 0x2B     // Meshtastic sync word for compatibility testing
#else
#define LORA_SYNC_WORD 0x12     // SIMS private network sync word
#endif

#define LORA_TX_POWER 20        // dBm (max 20 for SX1262)
#define LORA_PREAMBLE_LENGTH 8

// Mesh network configuration
#define MESH_MAX_HOPS 5         // Maximum hop count for flood routing
#define MESH_TTL 300000         // Message TTL in ms (5 minutes)
#define MESH_HEARTBEAT_INTERVAL 60000  // Heartbeat every 60s
#define MESH_MAX_RETRIES 3      // Retry failed transmissions
#define MESH_ACK_TIMEOUT 5000   // Wait 5s for ACK

// Message prioritization
#define PRIORITY_CRITICAL 0     // SOS, urgent threats
#define PRIORITY_HIGH 1         // Active incidents
#define PRIORITY_MEDIUM 2       // Media, sensor data
#define PRIORITY_LOW 3          // Heartbeats, status

// TDMA configuration (for 200 device support)
#define TDMA_SLOT_DURATION 500  // ms per slot
#define TDMA_TOTAL_SLOTS 200    // Support 200 devices
#define TDMA_CYCLE_TIME (TDMA_SLOT_DURATION * TDMA_TOTAL_SLOTS)  // 100s

// Packet configuration
#define MAX_PACKET_SIZE 255     // LoRa maximum
#define MAX_PAYLOAD_SIZE 200    // After headers
#define CHUNK_SIZE 180          // For large file transfers

// Hardware Pin Definitions (Heltec LoRa 32 V3)
// LoRa SX1262
#define LORA_SCK    9
#define LORA_MISO   11
#define LORA_MOSI   10
#define LORA_CS     8
#define LORA_RST    12
#define LORA_DIO1   14
#define LORA_BUSY   13

// GPS Module (External) - UPDATED to avoid USB serial conflict
// Previously used GPIO43/44 which conflicts with CP2102 (USB serial TX/RX)
// Now using GPIO39/40 which are available on header J3
#define GPS_RX      39  // J3 pin 9 (was 44 - conflicted with CP2102_RX)
#define GPS_TX      40  // J3 pin 10 (was 43 - conflicted with CP2102_TX)
#define GPS_BAUD_RATE 9600
#define GPS_UPDATE_INTERVAL 1000  // Update every 1s
#define GPS_TIMEOUT 10000         // 10s timeout for fix

// User Interface
#define PTT_BUTTON  0   // Push-to-talk button
#define STATUS_LED  35  // Status LED

// OLED Display (128x64 SSD1306)
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21

// Vext Power Control - CRITICAL for external peripherals (GPS, camera modules)
// GPIO36 controls Vext via P-channel FET: LOW = power ON, HIGH = power OFF
// Must be set LOW in setup() to enable power to external devices
#define VEXT_CTRL   36

// Camera configuration
#ifdef HAS_CAMERA
#define CAMERA_JPEG_QUALITY 20    // Very low quality for bandwidth (10-30)
#define CAMERA_FRAME_SIZE FRAMESIZE_VGA  // 640x480
#define CAMERA_PIXEL_FORMAT PIXFORMAT_JPEG
#endif

// Audio configuration
#define AUDIO_SAMPLE_RATE 16000   // 16 kHz (adequate for speech)
#define AUDIO_BITS_PER_SAMPLE 16
#define AUDIO_MAX_DURATION 30000  // Max 30s recording
#define AUDIO_BUFFER_SIZE 4096

// Power management
#define SLEEP_TIMEOUT_MS 300000   // Sleep after 5 min idle
#define BATTERY_CHECK_INTERVAL 60000  // Check battery every 60s
#define LOW_BATTERY_THRESHOLD 3.3     // Volts

// Storage configuration
#define MAX_STORED_MESSAGES 100   // Queue up to 100 pending messages
#define STORAGE_PATH "/messages"

// AI processing
#define ENABLE_VOICE_TO_TEXT false  // Requires Whisper model (large)
#define ENABLE_OBJECT_DETECTION false  // Requires YOLO model
#define ENABLE_VAD true  // Voice Activity Detection (lightweight)

// WiFi Configuration
#define WIFI_CONNECT_TIMEOUT 10000      // 10s timeout for connection
#define WIFI_RECONNECT_INTERVAL 30000   // 30s between reconnect attempts
#define WIFI_MAX_STORED_NETWORKS 5      // Store up to 5 WiFi credentials in NVS
#define BACKEND_HOST "192.168.1.100"    // Backend server IP (change as needed)
#define BACKEND_PORT 8000               // Backend server port

// MQTT Configuration (Gateway Mode)
#define GATEWAY_MODE_ENABLED false      // Set true to enable MQTT gateway mode
#define MQTT_BROKER "192.168.1.100"     // MQTT broker IP (same as backend)
#define MQTT_PORT 1883                  // MQTT port (use 8883 for TLS)
#define MQTT_USE_TLS false              // Set true for production (mqtts://)
#define MQTT_CLIENT_ID_PREFIX "sims-mesh-"
#define MQTT_QOS 1                      // QoS 1 for at-least-once delivery
#define SIMS_MQTT_KEEPALIVE 60          // Keepalive interval in seconds

// MQTT Topics (per architecture doc)
#define MQTT_TOPIC_INCIDENTS_IN "sims/mesh/incidents/in"    // Mesh to backend
#define MQTT_TOPIC_INCIDENTS_OUT "sims/mesh/incidents/out"  // Backend to mesh
#define MQTT_TOPIC_STATUS "sims/mesh/status"                // Network health
#define MQTT_TOPIC_NODES "sims/mesh/nodes"                  // Active nodes list

// BLE Configuration
#define BLE_DEVICE_NAME "SIMS-MESH"
#define BLE_MAX_CONNECTIONS 9           // Support up to 9 simultaneous connections
#define BLE_MTU_SIZE 512                // Maximum MTU for faster transfers

// BLE GATT Service UUIDs
#define BLE_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHAR_INCIDENT_TX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHAR_MESH_RX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHAR_STATUS_UUID "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHAR_CONFIG_UUID "6E400005-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHAR_MEDIA_UUID "6E400006-B5A3-F393-E0A9-E50E24DCCA9E"

// WiFi Config via BLE (temporary service for initial setup)
#define BLE_WIFI_CONFIG_SERVICE_UUID "0000aaaa-0000-1000-8000-00805f9b34fb"
#define BLE_WIFI_SSID_UUID "0000aaab-0000-1000-8000-00805f9b34fb"
#define BLE_WIFI_PASS_UUID "0000aaac-0000-1000-8000-00805f9b34fb"

// Bandwidth Optimization Settings (per architecture doc)
#define IMAGE_QUALITY 20                // JPEG quality (10/20/30) - target 10KB per image
#define IMAGE_WIDTH 640                 // VGA width
#define IMAGE_HEIGHT 480                // VGA height
#define AUDIO_CODEC_OPUS true           // Use Opus codec for audio compression
#define AUDIO_BITRATE 8000              // 8 kbps (target ~10KB for 10s audio)
#define GPS_DELTA_ENCODING false        // Future enhancement for tracking mode

// Offline Queue Settings (MessageStorage integration)
#define QUEUE_MAX_SIZE 100              // Maximum queued incidents
#define QUEUE_RETRY_INTERVAL 60000      // 60s between retry attempts
#define QUEUE_EXPONENTIAL_BACKOFF true  // Enable exponential backoff
#define QUEUE_MAX_BACKOFF 3600000       // 1 hour maximum backoff

// Protocol Mode (Meshtastic Integration)
#define PROTOCOL_MODE_SIMS_ONLY 0
#define PROTOCOL_MODE_MESHTASTIC_ONLY 1
#define PROTOCOL_MODE_DUAL_HYBRID 2
#define PROTOCOL_MODE_BRIDGE 3
#define PROTOCOL_MODE PROTOCOL_MODE_SIMS_ONLY  // Default: no changes to existing behavior

// Meshtastic Configuration (when enabled)
#define MESHTASTIC_SYNC_WORD 0x2B       // Standard Meshtastic sync word
#define ROUTE_CRITICAL_VIA_SIMS true    // Use SIMS protocol for critical messages
#define ROUTE_MEDIA_VIA_SIMS true       // Use SIMS protocol for media
#define ROUTE_TEXT_VIA_MESHTASTIC false // Use Meshtastic for text (when dual mode)

// Message types
enum MessageType {
    MSG_TYPE_HEARTBEAT = 0,
    MSG_TYPE_INCIDENT = 1,
    MSG_TYPE_LOCATION = 2,
    MSG_TYPE_ACK = 3,
    MSG_TYPE_NACK = 4,
    MSG_TYPE_ROUTE_REQUEST = 5,
    MSG_TYPE_ROUTE_REPLY = 6,
    MSG_TYPE_DATA_CHUNK = 7
};

// Data structures
struct GPSLocation {
    float latitude;
    float longitude;
    float altitude;
    float speed;
    float bearing;
    bool valid;
    unsigned long timestamp;
};

struct IncidentReport {
    uint32_t deviceId;
    float latitude;
    float longitude;
    float altitude;
    unsigned long timestamp;

    bool hasImage;
    uint8_t* imageData;
    size_t imageSize;

    bool hasAudio;
    uint8_t* audioData;
    size_t audioSize;

    char description[256];
    uint8_t priority;
    uint8_t category;
};

struct MeshMessage {
    uint32_t sourceId;
    uint32_t destinationId;  // 0xFFFFFFFF for broadcast
    uint32_t sequenceNumber;
    uint8_t messageType;
    uint8_t priority;
    uint8_t hopCount;
    uint8_t ttl;
    uint16_t payloadSize;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    unsigned long timestamp;
};

#endif // CONFIG_H
