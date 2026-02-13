# SIMS-SMART ESP-IDF Port

Voice-controlled hands-free incident reporting device using ESP-IDF framework.

## Why ESP-IDF?

The Arduino framework cannot support ESP32-SR (Espressif's voice recognition library) due to missing ESP-IDF infrastructure (SPIFFS, model loading APIs, DSP functions). This port moves the entire firmware to native ESP-IDF to enable full voice recognition capabilities.

## Hardware

- **Board**: Seeed Studio XIAO ESP32S3 Sense
- **CPU**: ESP32-S3 dual-core @ 240MHz
- **RAM**: 512KB SRAM + 8MB PSRAM
- **Flash**: 8MB
- **Peripherals**: OV2640 camera, PDM microphone, WS2812B LED, GPS module

## Build Instructions

### Prerequisites

Install ESP-IDF v5.x: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/

### Build

```bash
cd sims-smart-espidf
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

### Clean Build

```bash
idf.py fullclean
idf.py build
```

## Project Structure

```
sims-smart-espidf/
├── CMakeLists.txt              # Root build config
├── sdkconfig.defaults          # ESP-IDF defaults (PSRAM, flash, WiFi)
├── partitions.csv              # Flash layout (2MB app, 3MB models, 2MB storage)
├── main/
│   ├── CMakeLists.txt          # Component build config
│   ├── idf_component.yml      # Managed dependencies (esp-sr, esp-camera)
│   ├── config.h               # Hardware pins, WiFi, API configuration
│   ├── main.cpp               # Entry point, state machine, FreeRTOS tasks
│   ├── network/
│   │   ├── wifi_service.cpp/h  # Event-driven WiFi (esp_wifi API)
│   │   └── http_client.cpp/h  # HTTP POST (esp_http_client + cJSON)
│   ├── storage/
│   │   └── nvs_storage.cpp/h  # NVS credential storage
│   ├── sensors/
│   │   ├── gps_service.cpp/h  # UART + TinyGPS++
│   │   ├── camera_service.cpp/h # esp_camera (OV2640)
│   │   └── audio_service.cpp/h  # I2S PDM microphone
│   ├── voice/
│   │   ├── wake_word_service.cpp/h # WakeNet9 "Hi ESP"
│   │   └── command_parser.cpp/h    # MultiNet6 commands
│   ├── led/
│   │   └── led_feedback.cpp/h # RMT driver for WS2812B
│   └── power/
│       └── power_manager.cpp/h # Battery, light sleep, DFS
└── components/                 # External components (TinyGPSPlus)
```

## Architecture

### Dual-Core Task Design

- **Core 0 - main_task**: State machine, WiFi, HTTP uploads, GPS, LED
- **Core 1 - voice_task**: Continuous audio capture, WakeNet9, MultiNet6

### State Machine

```
IDLE (green pulse) -> "Hi ESP" -> WAKE_DETECTED (flash)
  -> LISTENING_COMMAND (blue pulse) -> command recognized
    -> CAPTURING_IMAGE / RECORDING_VOICE
    -> PROCESSING (GPS + payload)
    -> UPLOADING (HTTP POST)
    -> SUCCESS / ERROR -> back to IDLE
```

### Voice Commands

| Command | Action |
|---------|--------|
| "take photo" | Capture JPEG from OV2640 |
| "record voice" | Record 10s audio message |
| "send incident" | Upload with GPS data |
| "cancel" | Return to idle |
| "status check" | Print system status |

## Configuration

Edit `main/config.h`:
```cpp
#define WIFI_SSID      "YOUR_WIFI_SSID"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"
#define BACKEND_URL    "http://192.168.1.100:8080/api/incidents"
```

## Original Arduino Code

The original Arduino firmware is in `../sims-smart/`. This ESP-IDF port preserves the same functionality and state machine logic while using native ESP-IDF APIs for full ESP32-SR voice recognition support.
