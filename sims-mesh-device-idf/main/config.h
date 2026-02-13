/**
 * SIMS Mesh Device Configuration
 * ESP-IDF version
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

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

// GPS Module (External) - GPIO39/40 to avoid USB serial conflict
#define GPS_RX      39  // J3 pin 9
#define GPS_TX      40  // J3 pin 10
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
#define VEXT_CTRL   36

// Audio configuration
#define AUDIO_SAMPLE_RATE 16000   // 16 kHz
#define AUDIO_BITS_PER_SAMPLE 16
#define AUDIO_MAX_DURATION 30000  // Max 30s recording
#define AUDIO_BUFFER_SIZE 4096

// Power management
#define SLEEP_TIMEOUT_MS 300000   // Sleep after 5 min idle
#define BATTERY_CHECK_INTERVAL 60000
#define LOW_BATTERY_THRESHOLD 3.3

// Battery ADC (GPIO 1 / ADC1_CH0 on Heltec V3)
#define BATTERY_ADC_PIN      1
#define BATTERY_ADC_CTRL     37       // GPIO 37 must be LOW to enable battery voltage divider
#define BATTERY_DIVIDER      4.9f     // Heltec V3: 390K/100K divider = (390+100)/100
#define BATTERY_FULL_V       4.2f
#define BATTERY_EMPTY_V      3.0f
#define BATTERY_SAMPLES      16

// Button configuration (GPIO 0 - active low with pull-up)
#define BUTTON_SHORT_PRESS_MAX_MS  500
#define BUTTON_LONG_PRESS_MS       2000
#define BUTTON_DEBOUNCE_MS         50

// Idle/display timeout
#define IDLE_SCREEN_TIMEOUT_MS     120000  // 2 minutes

// Storage configuration
#define MAX_STORED_MESSAGES 100
#define STORAGE_PATH "/spiffs/messages"
#define SPIFFS_MOUNT_POINT "/spiffs"

// BLE Configuration
#define BLE_DEVICE_NAME "SIMS-MESH"
#define BLE_MAX_CONNECTIONS 3
#define BLE_MTU_SIZE 512

// FreeRTOS Task Configuration
#define MAIN_TASK_STACK_SIZE 8192
#define MAIN_TASK_PRIORITY 5
#define MAIN_TASK_CORE 0

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
