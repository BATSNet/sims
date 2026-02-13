# SIMS Mesh Device - Quick Start Guide

## Hardware Setup

### Heltec LoRa 32 V3

1. **Connect USB-C cable** to computer
2. **Install USB driver** (CP210x for Windows)
3. **Verify connection**: Check Device Manager for COM port

### Required External Components

Connect the following components to the Heltec board:

**GPS Module (NEO-6M/7M/8M)**:
- VCC → 3.3V (or use Vext via header)
- GND → GND
- RX → GPIO 39 (J3 pin 9) - UPDATED: was GPIO 44
- TX → GPIO 40 (J3 pin 10) - UPDATED: was GPIO 43
- Note: Previous pins (43/44) conflicted with USB serial debugging

**I2S Microphone (INMP441)**:
- VCC → 3.3V
- GND → GND
- SCK → GPIO 41
- WS → GPIO 42
- SD → GPIO 2

**Push-to-Talk Button**:
- One side → GPIO 0
- Other side → GND

## Software Setup

### 1. Install PlatformIO

**Option A: VS Code Extension**
```bash
# Install VS Code
# Open VS Code, install PlatformIO IDE extension
```

**Option B: PlatformIO Core (CLI)**
```bash
pip install platformio
```

### 2. Install Dependencies

Dependencies are automatically installed from `platformio.ini` when you build:
- RadioLib (LoRa)
- TinyGPSPlus (GPS)
- Adafruit SSD1306 (Display)
- Nanopb (Protobuf)
- NimBLE-Arduino (Bluetooth)

### 3. Build Firmware

**Using VS Code**:
1. Open `sims-mesh-device` folder in VS Code
2. Click PlatformIO icon in sidebar
3. Select `heltec_wifi_lora_32_V3` environment
4. Click "Build"

**Using CLI**:
```bash
cd sims-mesh-device
pio run -e heltec_wifi_lora_32_V3
```

### 4. Upload to Device

**Using VS Code**:
1. Connect device via USB
2. Click "Upload" in PlatformIO

**Using CLI**:
```bash
pio run -e heltec_wifi_lora_32_V3 -t upload
```

### 5. Monitor Serial Output

**Using VS Code**:
- Click "Monitor" in PlatformIO

**Using CLI**:
```bash
pio device monitor
```

Expected output:
```
SIMS Mesh Device Starting...
Version: 1.0.0
Board: ESP32-S3 + LoRa SX1262
[LoRa] Initializing SX1262... success!
[GPS] Initializing GPS service...
[Mesh] Device ID: 0x12345678
Initialization complete. Device ready.
```

## First Test: Two-Device Communication

### Device 1: Reporter

1. Flash firmware
2. Open serial monitor
3. Wait for GPS fix (may take 1-2 minutes)
4. **Short press PTT button** to capture image (if camera available)
5. **Long press PTT button** to record voice

Expected output:
```
PTT button pressed
Long press detected - starting voice recording...
Recording stopped: 10240 bytes, 5000 ms
[Mesh] Preparing incident message...
[Mesh] Incident payload: 128 bytes
[LoRa] Packet sent successfully (128 bytes)
```

### Device 2: Gateway/Receiver

1. Flash firmware
2. Open serial monitor
3. Wait for messages from Device 1

Expected output:
```
[LoRa] Received packet: 128 bytes, RSSI: -45 dBm, SNR: 8.5 dB
[Mesh] Received: from=0x12345678, seq=1, type=1, hops=0
GPS: lat=52.520008, lon=13.404954, alt=34.5
```

## Configuration

Edit `include/config.h` to customize:

### LoRa Frequency

```cpp
// For Europe (868 MHz)
#define LORA_FREQUENCY 868E6

// For US (915 MHz)
#define LORA_FREQUENCY 915E6
```

### Spreading Factor (Range vs Speed)

```cpp
// Fast transmission, shorter range
#define LORA_SPREADING_FACTOR 7

// Slow transmission, longer range
#define LORA_SPREADING_FACTOR 12
```

### Camera Quality (Bandwidth vs Image Quality)

```cpp
// Very low quality, small file size (10-20 KB)
#define CAMERA_JPEG_QUALITY 10

// Medium quality, larger file size (50-100 KB)
#define CAMERA_JPEG_QUALITY 30
```

## Troubleshooting

### LoRa Not Working

**Symptom**: "LoRa initialization failed!"

**Solutions**:
1. Check antenna is connected
2. Verify frequency setting (868 vs 915 MHz)
3. Check pin definitions match your board
4. Try reducing TX power: `#define LORA_TX_POWER 14`

### GPS No Fix

**Symptom**: "GPS Status: NO FIX"

**Solutions**:
1. Move device outdoors or near window
2. Wait 2-5 minutes for cold start
3. Check UART wiring (RX ↔ TX correct?)
4. Verify GPS module has power (LED blinking?)

### Camera Failed

**Symptom**: "Camera initialization failed"

**Solutions**:
1. Only XIAO ESP32S3 Sense has built-in camera
2. Check `HAS_CAMERA` is defined in platformio.ini
3. Verify PSRAM is enabled
4. Reduce JPEG quality if memory errors

### Upload Failed

**Symptom**: "Failed to connect to ESP32"

**Solutions**:
1. Check USB cable supports data (not power-only)
2. Install CP210x USB driver
3. Hold BOOT button during upload
4. Try different USB port

### Compilation Errors

**Symptom**: "undefined reference to X"

**Solutions**:
1. Clean build: `pio run -t clean`
2. Update libraries: `pio pkg update`
3. Check all required libraries are in platformio.ini

## Range Testing

### Urban Environment
- **Expected range**: 2-5 km
- **Best performance**: Line of sight, elevated position
- **Obstacles**: Buildings reduce range significantly

### Rural Environment
- **Expected range**: 10-15 km
- **Best performance**: Flat terrain, no obstacles
- **Maximum**: Up to 20 km with SF12 and high elevation

### Range Test Procedure

1. Flash both devices
2. Device 1: Stay at starting point
3. Device 2: Walk away with smartphone running serial monitor
4. Record RSSI and SNR at different distances:
   - 100m
   - 500m
   - 1km
   - 2km
   - 5km
   - Until packet loss >50%

5. Plot RSSI vs distance to find maximum range

## Next Steps

- [ ] Test basic LoRa communication (2 devices)
- [ ] Measure signal strength (RSSI/SNR) at various distances
- [ ] Connect GPS module and verify location capture
- [ ] Test incident reporting (image + voice + GPS)
- [ ] Set up backend mesh gateway (MQTT bridge)
- [ ] Configure backend mesh plugin
- [ ] Test end-to-end incident flow (device → mesh → backend → dashboard)

## Support

For issues:
1. Check serial monitor output for error messages
2. Verify hardware connections
3. Review main SIMS repository README
4. Check GitHub issues

## Additional Resources

- [RadioLib Documentation](https://github.com/jgromes/RadioLib)
- [LoRa Basics](https://lora.readthedocs.io/)
- [Meshtastic Project](https://meshtastic.org/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
