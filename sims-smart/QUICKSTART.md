# SIMS-SMART Quick Start Guide

Get your SIMS-SMART device up and running in 5 minutes.

## Hardware Requirements

- **Board**: Seeed Studio XIAO ESP32S3 Sense
- **GPS Module**: NEO-6M or NEO-7M (optional but recommended)
- **Battery**: 3.7V LiPo (500-1000mAh recommended)
- **USB Cable**: USB-C for programming and power

## Step 1: Hardware Setup

### Connect GPS Module (Optional)

| GPS Pin | XIAO Pin | GPIO |
|---------|----------|------|
| VCC     | 3V3      | -    |
| GND     | GND      | -    |
| TX      | D6       | 43   |
| RX      | D7       | 44   |

### Connect Battery (Optional)

Connect LiPo battery to BAT+ and BAT- pads on underside of board.

## Step 2: Configure WiFi

Edit `include/config.h`:
```cpp
#define WIFI_SSID          "YourWiFiNetwork"
#define WIFI_PASSWORD      "YourPassword"
#define BACKEND_URL        "http://192.168.1.100:8080/api/incidents"
```

Replace with your actual WiFi credentials and backend server URL.

## Step 3: Build and Upload

### Install PlatformIO

If not already installed:
```bash
pip install platformio
```

### Build and Upload Firmware

```bash
cd sims-smart

# Build
pio run

# Upload to connected device
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

## Step 4: Test Basic Functions

### LED Indicators

After upload, you should see:
- **Green pulse**: Device is ready and listening for wake word
- **Blue pulse**: Processing voice command
- **Cyan pulse**: Uploading to backend
- **Green flash**: Success
- **Red flash**: Error

### Serial Monitor

Watch the serial output for status messages:
```
========================================
SIMS-SMART Device
Version: 1.0.0
Device ID: xiao-esp32s3-12345678
========================================

[WiFi] Connecting to YourNetwork...
[WiFi] Connected! IP: 192.168.1.50
[GPS] Initializing GPS service...
[GPS] Waiting for GPS fix...
[MAIN] System initialized successfully
[MAIN] Ready for voice commands
[MAIN] Wake word: "SIMS Alert"
```

### Test GPS (if connected)

Wait 30-60 seconds for GPS fix. You should see:
```
[GPS] First fix acquired: 52.520008, 13.404954
```

### Test WiFi Upload

The device will attempt to connect to WiFi on boot. If successful:
```
[WiFi] Connected to YourNetwork
[WiFi] IP: 192.168.1.50
[WiFi] RSSI: -45 dBm
```

## Step 5: Voice Control (ESP32-SR Required)

Voice control requires installing ESP32-SR library (see `ESP32-SR_INSTALLATION.md`).

### Without ESP32-SR (Testing Mode)

The device runs in stub mode without ESP32-SR. You can still:
- Test WiFi connectivity
- Test GPS
- Test HTTP uploads
- Monitor system status

### With ESP32-SR (Full Functionality)

After installing ESP32-SR:

1. **Say wake word**: "SIMS Alert"
   - LED turns blue
   - Device listens for command

2. **Give command**:
   - "Take photo" - Capture image
   - "Record voice" - Record audio message
   - "Send incident" - Upload to backend
   - "Cancel" - Abort
   - "Status check" - Print system status

3. **Wait for feedback**:
   - Cyan pulse: Uploading
   - Green flash: Success
   - Red flash: Error

## Troubleshooting

### Device Not Detected

- Install CH340/CP2102 USB driver
- Try different USB cable (must support data, not just charging)
- Try different USB port

### WiFi Connection Failed

- Double-check SSID and password in `config.h`
- Ensure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Check WiFi signal strength

### GPS No Fix

- Move near window or outdoors
- Wait 30-60 seconds for cold start
- Check GPS module power (3.3V)
- Verify UART connections

### Upload Failed

- Hold BOOT button while connecting USB
- Check COM port in Device Manager
- Try `pio run --target upload --upload-port COM7` (replace COM7 with your port)

### LED Not Working

- Check FastLED library is installed: `pio pkg install`
- Verify STATUS_LED_PIN (should be 21 for built-in LED)

## Next Steps

1. **Install ESP32-SR**: See `ESP32-SR_INSTALLATION.md` for voice recognition
2. **Test Backend**: Ensure SIMS backend is running and accessible
3. **Customize**: Edit commands, wake word, and behavior in `config.h`
4. **Deploy**: Add enclosure and mount in desired location

## Getting Help

- Check `README.md` for overview
- Read `CLAUDE.md` for development details
- Check serial monitor output for error messages
- Review `ESP32-SR_INSTALLATION.md` for voice setup
