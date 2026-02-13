#ifndef CONFIG_H
#define CONFIG_H

// Device identification
#define DEVICE_TYPE "sims-smart"
#define FIRMWARE_VERSION "1.0.0"

// Hardware configuration - XIAO ESP32S3 Sense
#define BOARD_XIAO_ESP32S3_SENSE

// Built-in peripherals (fixed pins on XIAO ESP32S3 Sense)
#define HAS_CAMERA              // OV2640 on dedicated pins
#define HAS_MICROPHONE          // SPM1423 PDM microphone
#define HAS_BUILTIN_LED         // WS2812B RGB LED on GPIO21

// PDM Microphone pins (built-in)
#define MIC_PDM_CLK_PIN    42   // PDM clock
#define MIC_PDM_DATA_PIN   41   // PDM data

// External GPS module (UART)
#define GPS_ENABLED
#define GPS_RX_PIN         43   // GPIO43 (D6)
#define GPS_TX_PIN         44   // GPIO44 (D7)
#define GPS_BAUD           9600
#define GPS_UART           Serial1

// Status LED (using built-in WS2812B)
#define STATUS_LED_PIN     21   // Built-in RGB LED
#define STATUS_LED_COUNT   1

// Battery monitoring
#define BATTERY_ENABLED
#define BATTERY_ADC_PIN    1    // GPIO1 (A0)
#define BATTERY_MIN_MV     3200 // Empty voltage
#define BATTERY_MAX_MV     4200 // Full voltage

// WiFi configuration
#define WIFI_SSID          "YOUR_WIFI_SSID"
#define WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"
#define WIFI_CONNECT_TIMEOUT    10000  // 10s timeout for connection
#define WIFI_RECONNECT_INTERVAL 30000  // 30s between reconnect attempts
#define WIFI_MAX_STORED_NETWORKS 5     // Store up to 5 WiFi credentials in NVS

// Backend API configuration
#define BACKEND_URL        "http://192.168.1.100:8080/api/incidents"
#define API_TIMEOUT_MS     30000

// Incident priority levels
#define PRIORITY_CRITICAL  0  // SOS, urgent threats
#define PRIORITY_HIGH      1  // Active incidents
#define PRIORITY_MEDIUM    2  // Media, sensor data
#define PRIORITY_LOW       3  // Heartbeats, status

// Voice recognition settings
#define WAKE_WORD          "SIMS Alert"
#define COMMAND_TIMEOUT_MS 5000  // 5 seconds to issue command after wake word
#define AUDIO_SAMPLE_RATE  16000 // 16kHz for voice
#define AUDIO_BUFFER_SIZE  4096

// Camera settings
#define CAMERA_FRAMESIZE   FRAMESIZE_SVGA  // 800x600
#define CAMERA_QUALITY     10              // JPEG quality 0-63 (lower = better)

// Recording settings
#define MAX_VOICE_DURATION_MS  10000  // 10 seconds max voice recording
#define MAX_IMAGE_SIZE_KB      100    // Max JPEG size for upload

// GPS settings
#define GPS_FIX_TIMEOUT_MS     30000  // 30 seconds to get fix
#define GPS_MIN_SATELLITES     4      // Minimum satellites for good fix
#define GPS_USE_CACHED         true   // Use last known position if no fix

// Storage settings
#define ENABLE_OFFLINE_QUEUE   true
#define MAX_QUEUED_INCIDENTS   10
#define STORAGE_PARTITION      "/littlefs"

// Power management
#define LOW_BATTERY_PERCENT    20
#define ENABLE_SLEEP_MODE      false  // Future: deep sleep between incidents

// Debug settings
#define DEBUG_SERIAL
#define DEBUG_VOICE
// #define DEBUG_GPS
// #define DEBUG_CAMERA
// #define DEBUG_WIFI

// Serial configuration
#define SERIAL_BAUD        115200

#endif // CONFIG_H
