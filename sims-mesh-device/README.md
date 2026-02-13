# SIMS Mesh Device

ESP32-S3 LoRa mesh network firmware for off-grid incident reporting.

## Hardware

**Heltec LoRa 32 V3** (primary):
- ESP32-S3 + SX1262 LoRa + 128x64 OLED
- CP2102 USB-UART bridge

**Seeeduino XIAO ESP32S3 Sense** (camera variant):
- ESP32-S3 + OV2640 camera
- Requires external SX1262 module

## Setup

### 1. Install CP2102 Driver (Heltec only)

**Windows:**
1. Connect board via USB
2. Open Device Manager
3. Find device with yellow warning
4. Right-click > Update driver > Search automatically
5. Verify COM port appears (e.g., COM7)

**Manual:** https://www.silabs.com/interface/usb-bridges/classic/device.cp2102

### 2. Install PlatformIO

In VS Code:
1. Extensions (Ctrl+Shift+X)
2. Search "PlatformIO IDE"
3. Install and restart

### 3. Build & Upload

```bash
# Build
platformio run -e heltec_wifi_lora_32_V3

# Upload (replace COM7 with your port)
platformio run -e heltec_wifi_lora_32_V3 -t upload --upload-port COM7

# Monitor serial
platformio device monitor --port COM7 --baud 115200
```

### 4. Configuration

Edit `include/config.h`:
- `LORA_FREQUENCY`: 868.0 (EU) or 915.0 (US)
- `LORA_TX_POWER`: 14 (dBm)
- `LORA_SPREADING_FACTOR`: 7-12 (range vs speed)

## Pin Definitions (Heltec)

**LoRa:** SCK=9, MISO=11, MOSI=10, CS=8, RST=12, DIO1=14, BUSY=13
**OLED:** SDA=17, SCL=18, RST=21
**GPS:** RX=44, TX=43
**Controls:** PTT=GPIO0, LED=GPIO35

## Troubleshooting

**COM port locked/busy:**
```bash
tasklist | grep platformio
taskkill /F /PID <pid>
```

**Upload fails:**
- Check USB cable (must support data)
- Verify CP2102 driver installed
- Try different USB port
- Manual boot: Hold PRG, press RST, release PRG

**OLED blank:**
- Check serial output: `platformio device monitor`
- Verify I2C pins (SDA=17, SCL=18)
- Press RST button to reboot

**LoRa init fails:**
- Check antenna connected
- Verify frequency (868 EU, 915 US)
- Check SPI pins match config

**GPS no fix:**
- Requires clear sky view
- Wait 30-60s for cold start
- Indoor may not work
