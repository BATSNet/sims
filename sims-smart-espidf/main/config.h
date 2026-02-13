#ifndef CONFIG_H
#define CONFIG_H

// Device identification
#define DEVICE_TYPE "sims-smart"
#define FIRMWARE_VERSION "2.0.0-espidf"

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
#define GPS_UART_NUM       UART_NUM_1

// Status LED (using built-in WS2812B)
#define STATUS_LED_PIN     21   // Built-in RGB LED
#define STATUS_LED_COUNT   1

// Battery monitoring
#define BATTERY_ENABLED
#define BATTERY_ADC_PIN    1    // GPIO1 (A0)
#define BATTERY_MIN_MV     3200 // Empty voltage
#define BATTERY_MAX_MV     4200 // Full voltage

// WiFi configuration (move to NVS in production)
#define WIFI_SSID          "YOUR_WIFI_SSID"
#define WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"
#define WIFI_CONNECT_TIMEOUT_MS    10000  // 10s timeout for connection
#define WIFI_RECONNECT_INTERVAL_MS 30000  // 30s between reconnect attempts
#define WIFI_MAX_RETRY     5
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
#define WAKE_WORD          "hiesp"  // WakeNet9 wake word
#define COMMAND_TIMEOUT_MS 5000  // 5 seconds to issue command after wake word
#define AUDIO_SAMPLE_RATE  16000 // 16kHz for voice
#define AUDIO_BUFFER_SIZE  4096
#define AUDIO_BUFFER_COUNT 4

// Camera settings (OV2640)
#define CAMERA_PIXEL_FORMAT  PIXFORMAT_JPEG
#define CAMERA_FRAME_SIZE    FRAMESIZE_SVGA  // 800x600
#define CAMERA_JPEG_QUALITY  20              // 0-63 (lower = better quality)
#define CAMERA_FB_COUNT      2

// OV2640 Camera Pins (XIAO ESP32S3 Sense)
#define CAM_PIN_PWDN       -1
#define CAM_PIN_RESET      -1
#define CAM_PIN_XCLK       10
#define CAM_PIN_SIOD       40  // SDA
#define CAM_PIN_SIOC       39  // SCL
#define CAM_PIN_D7         48
#define CAM_PIN_D6         11
#define CAM_PIN_D5         12
#define CAM_PIN_D4         14
#define CAM_PIN_D3         16
#define CAM_PIN_D2         18
#define CAM_PIN_D1         17
#define CAM_PIN_D0         15
#define CAM_PIN_VSYNC      38
#define CAM_PIN_HREF       47
#define CAM_PIN_PCLK       13

// Recording settings
#define MAX_VOICE_DURATION_MS  10000  // 10 seconds max voice recording
#define MAX_IMAGE_SIZE_KB      100    // Max JPEG size for upload

// GPS settings
#define GPS_FIX_TIMEOUT_MS     30000  // 30 seconds to get fix
#define GPS_MIN_SATELLITES     4      // Minimum satellites for good fix
#define GPS_USE_CACHED         1      // Use last known position if no fix

// Storage settings
#define ENABLE_OFFLINE_QUEUE   1
#define MAX_QUEUED_INCIDENTS   10
#define STORAGE_PARTITION      "/storage"
#define MODELS_PARTITION       "/models"

// Power management
#define LOW_BATTERY_PERCENT    20
#define ENABLE_SLEEP_MODE      0  // Future: deep sleep between incidents

// NVS namespaces
#define NVS_NAMESPACE_WIFI     "wifi"
#define NVS_NAMESPACE_CONFIG   "config"

// Task priorities
#define TASK_PRIORITY_MAIN     5
#define TASK_PRIORITY_VOICE    4
#define TASK_PRIORITY_GPS      3
#define TASK_PRIORITY_LED      2

// Task stack sizes (bytes)
#define TASK_STACK_MAIN        4096
#define TASK_STACK_VOICE       8192
#define TASK_STACK_GPS         2048
#define TASK_STACK_LED         2048

#endif // CONFIG_H
