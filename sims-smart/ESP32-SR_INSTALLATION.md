# ESP32-SR Installation Guide

The ESP32-SR library provides wake word detection (WakeNet) and voice command recognition (MultiNet) for ESP32-S3.

## Installation Steps

### 1. Clone ESP32-SR Library

```bash
cd sims-smart/lib
git clone --depth 1 https://github.com/espressif/esp-sr.git
```

This will download the ESP32-SR library into `lib/esp-sr/`.

### 2. Verify Model Files

The library includes pre-trained models in `lib/esp-sr/model/`:
- **WakeNet9** - Wake word detection (recommended for ESP32-S3)
- **MultiNet6** - Command recognition (supports English and Chinese)

Check that these files exist:
```
lib/esp-sr/model/wn9_hiesp.model
lib/esp-sr/model/mn6_en.model
```

### 3. Update Firmware Code

Once ESP32-SR is installed, update the voice service files:

**In `src/voice/wake_word_service.cpp`:**
1. Uncomment the ESP32-SR includes at the top
2. Uncomment the initialization code in `begin()`
3. Uncomment the audio processing code in `processAudio()`

**In `src/voice/command_parser.cpp`:**
1. Uncomment the ESP32-SR includes at the top
2. Uncomment the initialization code in `begin()`
3. Uncomment the command parsing code in `parseCommand()`

### 4. Build and Test

```bash
# Clean build to pick up new library
pio run --target clean

# Build firmware
pio run

# Upload to device
pio run --target upload --target monitor
```

## Configuration

### Wake Word Configuration

Edit `include/config.h` to change the wake word:
```cpp
#define WAKE_WORD "SIMS Alert"  // Change to your preferred wake word
```

Supported wake words depend on the WakeNet model used. WakeNet9 supports:
- "Hi ESP" (default)
- Custom wake words (requires training)

### Command Configuration

Add custom commands in `src/voice/command_parser.cpp`:
```cpp
// In begin() method
multinet->add_command(model_data, "your custom command");
```

Update `mapCommandId()` to handle the new command.

## Troubleshooting

### Library Not Found

**Error:** `fatal error: esp_wn_iface.h: No such file or directory`

**Solution:** Make sure ESP32-SR is cloned into `lib/esp-sr/` and rebuild.

### Model Load Failed

**Error:** `Failed to create AFE instance` or `Failed to create MultiNet instance`

**Solution:**
- Check PSRAM is enabled in `platformio.ini`
- Verify model files exist in `lib/esp-sr/model/`
- Ensure ESP32-S3 has 8MB PSRAM

### Wake Word Not Detecting

**Possible causes:**
1. Microphone not working - check PDM pins (GPIO41/42)
2. Background noise too high - test in quiet environment
3. Wrong wake word - use supported wake words
4. Audio gain too low - increase mic gain in AFE config

### High False Positive Rate

**Solutions:**
- Use `DET_MODE_2CH_95` instead of `DET_MODE_2CH_90` (more strict)
- Adjust VAD (Voice Activity Detection) sensitivity
- Use `VAD_MODE_4` for stricter VAD

## Performance

### Memory Usage
- **WakeNet9**: ~1-2MB RAM
- **MultiNet6**: ~3-5MB RAM
- **Total**: ~5-7MB (fits in 8MB PSRAM)

### Latency
- **Wake word detection**: <500ms typical
- **Command recognition**: 1-2s typical
- **End-to-end**: <3s from wake word to action

### Accuracy
- **Wake word**: >95% in quiet environments
- **Commands**: >90% in quiet environments
- **Noise robustness**: Good with AFE preprocessing

## Additional Resources

- [ESP32-SR GitHub](https://github.com/espressif/esp-sr)
- [ESP32-SR Documentation](https://docs.espressif.com/projects/esp-sr/)
- [WakeNet Models](https://github.com/espressif/esp-sr/blob/master/docs/wake_word_engine/README.md)
- [MultiNet Models](https://github.com/espressif/esp-sr/blob/master/docs/speech_command_recognition/README.md)
