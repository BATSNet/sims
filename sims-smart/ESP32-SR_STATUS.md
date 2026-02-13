# ESP32-SR Integration Status

**Date:** 2026-02-13
**Device:** SIMS-SMART (XIAO ESP32S3 Sense)
**Goal:** Integrate ESP32-SR voice recognition library for hands-free operation

## Summary

ESP32-SR integration has been **partially completed** with significant progress made on the code integration, but **linker-level compatibility issues** prevent successful compilation in the Arduino framework. The library is designed for ESP-IDF and requires infrastructure functions not available in Arduino.

## Completed Steps

### 1. ✅ ESP32-SR Library Installation
- Successfully cloned ESP32-SR repository to `lib/esp-sr/`
- Verified model files present (WakeNet9, MultiNet6/7)
- Library structure: `lib/esp-sr/include/esp32s3/`, `lib/esp-sr/lib/esp32s3/`

### 2. ✅ Build System Configuration
- Added ESP32-S3 specific include paths: `-Ilib/esp-sr/include/esp32s3`
- Added model and source includes: `-Ilib/esp-sr/model -Ilib/esp-sr/src/include`
- Linked required libraries: wakenet, multinet, esp_audio_front_end, esp_audio_processor, dl_lib, c_speech_features, fst, hufzip, nsnet, vadnet

### 3. ✅ Source Code Integration
**Wake Word Service (`wake_word_service.cpp`):**
- Integrated WakeNet9 API using `esp_wn_handle_from_name()`
- Implemented wake word detection with `wn9_hiesp` model ("Hi ESP")
- Added audio processing pipeline
- Proper model lifecycle management (create/destroy)

**Command Parser (`command_parser.cpp`):**
- Integrated MultiNet6 API using `esp_mn_handle_from_name()`
- Implemented linked-list command structure required by ESP32-SR
- Added 5 commands: take photo, record voice, send incident, cancel, status check
- Proper command ID mapping

**Main State Machine (`main.cpp`):**
- Added audio buffer management infrastructure
- Integrated voice processing hooks in STATE_IDLE and STATE_LISTENING_COMMAND
- LED feedback already integrated

### 4. ✅ Code Compilation
All source files compile successfully without errors. Type conflicts resolved, API calls corrected.

## Current Blocker: Linker Errors

### Missing ESP-IDF Infrastructure Functions

The ESP32-SR library expects these functions (provided by ESP-IDF, not Arduino):

```
undefined reference to `get_model_base_path'          - Model file path management
undefined reference to `get_static_srmodels'          - Static model loading
undefined reference to `esp_sr_get_debug_mode'        - Debug configuration
undefined reference to `dl_matrixq8_copy_to_psram_spiffs'  - SPIFFS file system
undefined reference to `sp_processor_alloc'           - Speech processor
undefined reference to `alphabet_from_spiffs'         - Character mapping from SPIFFS
undefined reference to `dl_rfft_f32_init'             - DSP (FFT operations)
undefined reference to `fst_command_list_alloc'       - FST graph allocation
undefined reference to `hufzip_model_free_data'       - Model compression
```

### Root Cause

ESP32-SR is designed for **ESP-IDF framework**, which provides:
1. **SPIFFS file system** - For loading models from flash
2. **Model infrastructure** - Path management and model loading
3. **DSP library functions** - FFT, matrix operations
4. **Speech processor** - Audio feature extraction

Arduino ESP32 framework (used by PlatformIO) is a simplified wrapper that **does not include** these ESP-IDF components by default.

## Options to Resolve

### Option 1: Stub Out Missing Functions (Quick, Limited Functionality)
**Effort:** 2-4 hours
**Approach:** Create stub implementations of missing functions
- Implement `get_model_base_path()` to return model directory
- Implement `get_static_srmodels()` to load models from embedded data
- Stub out SPIFFS functions (may break model loading)
- Link ESP32 Arduino DSP library if available

**Pros:** Quick solution, may allow basic compilation
**Cons:** Models may not load properly, limited functionality, unstable

### Option 2: Port to ESP-IDF Framework (Full Functionality)
**Effort:** 1-2 days
**Approach:** Migrate sims-smart from Arduino to ESP-IDF
- Use ESP-IDF with PlatformIO (platform = espressif32, framework = espidf)
- Rewrite Arduino-specific code (WiFi, LED, etc.) using ESP-IDF APIs
- Use ESP-IDF component system for ESP32-SR
- Full access to all ESP-IDF features

**Pros:** Official support, full functionality, better performance
**Cons:** Major rewrite, steeper learning curve, more complex build system

### Option 3: Alternative Voice Library (Recommended)
**Effort:** 1-2 days
**Approach:** Use Arduino-compatible voice recognition
- **TensorFlow Lite Micro** - Lightweight ML for Arduino
- **Edge Impulse** - Pre-trained models, Arduino library
- **Porcupine (PicoVoice)** - Wake word detection, Arduino support
- **Custom keyword spotting** - Simpler, DIY approach

**Pros:** Arduino native, easier integration, community support
**Cons:** May have lower accuracy than ESP32-SR, limited features

### Option 4: Disable Voice Recognition (Fallback)
**Effort:** <1 hour
**Approach:** Keep voice services as stubs, use button trigger
- Compile with `-UESP32_SR_ENABLED` (disable voice code)
- Use physical button to trigger incident reporting
- Keep GPS, camera, WiFi, LED functionality

**Pros:** Immediate working device, simple operation
**Cons:** Not voice-controlled (defeats original purpose)

## Recommended Path Forward

### Short-term (Next 24 hours)
**Option 4: Disable voice recognition** and get basic device working
- Validates all other components (GPS, WiFi, upload, LED)
- Provides working baseline
- Builds confidence in hardware/firmware

### Medium-term (Next week)
**Option 3: Evaluate alternative voice library**
- Test TensorFlow Lite Micro with simple wake word
- Check Edge Impulse Arduino support
- Compare accuracy and integration effort

### Long-term (If voice is critical)
**Option 2: Port to ESP-IDF**
- Only if voice recognition is absolutely required
- Provides full ESP32-SR capability
- More maintainable long-term

## Technical Details

### Code Structure (Ready for ESP-IDF port)
```
sims-smart/
├── src/voice/
│   ├── wake_word_service.cpp    ✅ API integration complete
│   └── command_parser.cpp       ✅ API integration complete
├── platformio.ini                ✅ Libraries configured
└── lib/esp-sr/                   ✅ Installed and structured
```

### What Works (Compiles)
- Wake word service API calls
- Command parser API calls
- Linked list command structure
- Model lifecycle management
- Main state machine integration

### What Doesn't Work (Linker)
- Model loading from filesystem
- DSP operations (FFT, matrix math)
- Speech processor initialization
- SPIFFS file access

## Lessons Learned

1. **ESP32-SR requires ESP-IDF** - Not designed for Arduino framework
2. **Library documentation unclear** - Examples assume ESP-IDF knowledge
3. **Arduino ESP32 limitations** - Simplified framework missing advanced features
4. **Complexity underestimated** - "Uncomment and it works" was overly optimistic

## Files Modified

### Successfully Integrated
- `src/voice/wake_word_service.cpp` - WakeNet9 API
- `src/voice/command_parser.cpp` - MultiNet6 API with linked list
- `src/main.cpp` - Audio buffer management
- `platformio.ini` - Build configuration
- `include/voice/*.h` - Type definitions fixed

### No Changes Needed
- `src/led_feedback.cpp` - Already integrated
- `src/network/wifi_service.cpp` - Reused from sims-mesh-device
- `src/sensors/gps_service.cpp` - Reused from sims-mesh-device

## Next Steps (User Decision Required)

**QUESTION FOR USER:** Which path should we take?

1. **Quick validation** - Disable voice, get device working with button trigger?
2. **Alternative approach** - Try TensorFlow Lite or Edge Impulse?
3. **Full commitment** - Port to ESP-IDF for complete ESP32-SR support?

Please advise on priority:
- Is voice recognition critical, or can we start with button trigger?
- Is time-to-working-device more important than voice features?
- Are you comfortable with ESP-IDF complexity if we go that route?