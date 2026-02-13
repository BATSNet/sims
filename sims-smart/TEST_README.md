# Camera and Audio Test App

Simple test application to verify the XIAO ESP32S3 Sense camera and microphone work correctly.

## Quick Test

### 1. Build and Upload

```bash
cd sims-smart

# Build test app
pio run -e test

# Upload to device
pio run -e test --target upload

# Open serial monitor
pio device monitor --baud 115200
```

Or all in one command:
```bash
pio run -e test --target upload && pio device monitor --baud 115200
```

### 2. Serial Commands

Once the device is connected, you'll see:
```
========================================
XIAO ESP32S3 Sense - Camera & Audio Test
========================================

[Camera] Initializing OV2640...
[Camera] OV2640 initialized successfully
[Microphone] Initializing PDM microphone...
[Microphone] PDM microphone initialized successfully

Initialization complete!
```

**Available Commands:**

Type in the serial monitor:

- **`p`** or **`photo`** - Take a photo
  - Captures JPEG image
  - Shows resolution, size, and capture time
  - Displays JPEG header bytes

- **`a`** or **`audio`** - Record 10 seconds of audio
  - Records from built-in PDM microphone
  - Shows progress percentage
  - Displays audio statistics (amplitude, min/max)

- **`s`** or **`status`** - Print system status
  - Shows CPU, memory, uptime
  - Confirms camera and mic status

- **`h`** or **`help`** - Show help

## Expected Output

### Photo Capture

```
[Camera] Capturing photo...
[Camera] Photo captured in 234 ms
[Camera] Resolution: 800x600
[Camera] Format: JPEG
[Camera] Size: 45678 bytes
[Camera] JPEG Header: FF D8 FF E0 00 10 4A 46 49 46 ...
[Camera] Photo capture complete
```

### Audio Recording

```
[Microphone] Recording 10 seconds of audio...
[Microphone] Progress: 10%
[Microphone] Progress: 20%
...
[Microphone] Progress: 100%
[Microphone] Recording complete in 10045 ms
[Microphone] Bytes recorded: 320000 / 320000
[Microphone] Samples: 160000
[Microphone] Sample rate: 16000 Hz
[Microphone] Duration: 10.00 seconds
[Microphone] Average amplitude: 2345
[Microphone] Min: -8192, Max: 8191, Range: 16383
[Microphone] Audio level good
```

## Troubleshooting

### Camera Init Failed

**Error:** `Camera init failed with error 0x20001`

**Solutions:**
- Verify PSRAM is enabled (should be by default)
- Check camera ribbon cable is connected
- Reset the board (press reset button)

### Microphone Silent

**Warning:** `Audio level very low - check microphone!`

**Solutions:**
- Speak closer to the board during recording
- The microphone is on the top side of the board
- Verify PDM microphone is soldered (it's built-in on Sense variant)

### Board Not Detected

**Solutions:**
- Install CH340/CP2102 USB driver
- Try different USB cable (must support data)
- Check USB port in Device Manager (Windows) or `ls /dev/tty*` (Linux/Mac)

### Upload Failed

**Solutions:**
- Hold BOOT button while connecting USB
- Try: `pio run -e test --target upload --upload-port COM7` (replace COM7)

## What This Tests

- ✅ **Camera**: OV2640 initialization and JPEG capture
- ✅ **Microphone**: PDM audio recording at 16kHz
- ✅ **PSRAM**: Memory allocation for buffers
- ✅ **I2S**: PDM audio interface
- ✅ **Serial**: USB CDC communication

## Next Steps

Once both camera and audio work:

1. Integrate camera service into main firmware:
   - Copy camera init code to `src/sensors/camera_service.cpp`

2. Integrate audio service into main firmware:
   - Copy microphone init code to `src/sensors/audio_service.cpp`

3. Connect to voice recognition:
   - Feed audio samples to wake word detector
   - Feed audio samples to command parser

4. Test full workflow:
   - Wake word → command → photo/audio → upload

## Code Location

Test app source: `test/camera_audio_test.cpp`

This is a standalone test - it doesn't use the services from `src/`.
