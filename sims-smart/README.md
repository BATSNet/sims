# SIMS-SMART Device

Voice-controlled hands-free incident reporting device based on Seeed Studio XIAO ESP32S3 Sense.

## Hardware

- **Board**: Seeed Studio XIAO ESP32S3 Sense
- **Size**: Ultra-compact (21x17.5mm)
- **Built-in**: OV2640 camera, digital PDM microphone, 8MB PSRAM
- **External**: GPS module (NEO-6M/7M), WS2812B RGB LED, LiPo battery

## Features

- Wake word detection ("SIMS Alert")
- Voice command recognition (take photo, record voice, send incident)
- Automatic GPS capture
- Offline queue for failed uploads
- LED status feedback
- 24+ hour battery life

## Differentiators vs sims-mesh-device

| Aspect | sims-mesh-device | sims-smart |
|--------|------------------|------------|
| Hardware | Heltec LoRa 32 V3 | XIAO ESP32S3 Sense |
| Size | Standard board | Ultra-compact |
| Connectivity | LoRa mesh + WiFi | WiFi + BLE only |
| Interface | OLED + button | Voice commands + LED |
| Form Factor | Field device | Wearable device |

## Quick Start

```bash
cd sims-smart

# Build firmware
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

## Voice Commands

After wake word "SIMS Alert":
- "Take photo" - Capture image
- "Record voice" - Record audio message
- "Send incident" - Upload to backend
- "Cancel" - Abort current operation
- "Status check" - Battery and connection status

## LED Status Indicators

- **Green pulse**: Idle (ready)
- **Blue pulse**: Listening for command
- **Yellow solid**: Processing
- **Red pulse**: Recording audio
- **Cyan pulse**: Uploading
- **Green flash**: Success
- **Red flash**: Error
- **Orange pulse**: Queued incidents pending

## Configuration

Edit `include/config.h` to configure:
- WiFi credentials
- Backend server URL
- GPS UART pins
- LED pin
- Wake word settings

## Dependencies

ESP32-SR library (manual installation):
```bash
cd lib
git clone --depth 1 https://github.com/espressif/esp-sr.git
```

## Power Management

- Low-power wake word mode: ~15mA
- Normal operation: ~100-200mA
- Target battery life: 24+ hours on 1000mAh

## Backend Integration

Uses existing SIMS backend API endpoint:
- `POST /api/incidents`

Payload includes device_type="sims-smart" for differentiation.

## Development

See `CLAUDE.md` for detailed build instructions and development guidelines.
