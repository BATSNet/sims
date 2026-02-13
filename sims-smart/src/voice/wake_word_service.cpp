/**
 * Wake Word Service Implementation
 *
 * TODO: Integrate ESP32-SR library
 * - Install: cd lib && git clone https://github.com/espressif/esp-sr.git
 * - Include: esp_wn_iface.h, esp_wn_models.h
 * - Models: WakeNet9 (recommended for ESP32-S3)
 */

#include "voice/wake_word_service.h"

#ifdef ESP32_SR_ENABLED
// TODO: Uncomment when ESP32-SR is installed
// #include "esp_wn_iface.h"
// #include "esp_wn_models.h"
// #include "esp_afe_sr_iface.h"
// #include "esp_afe_sr_models.h"
#endif

WakeWordService::WakeWordService()
    : _state(STATE_UNINITIALIZED),
      _enabled(false),
      _detected(false),
      _confidence(0),
      _wakeWord(nullptr),
      _modelData(nullptr) {
}

WakeWordService::~WakeWordService() {
    end();
}

bool WakeWordService::begin(const char* wakeWord) {
    Serial.println("[WakeWord] Initializing wake word service...");
    _wakeWord = wakeWord;

#ifdef ESP32_SR_ENABLED
    // TODO: Initialize ESP32-SR when library is installed
    // Example initialization code:
    /*
    // Get AFE (Acoustic Front End) handle
    afe_config_t afe_config = {
        .aec_init = true,
        .se_init = true,
        .vad_init = true,
        .wakenet_init = true,
        .voice_communication_init = false,
        .voice_communication_agc_init = false,
        .voice_communication_agc_gain = 15,
        .vad_mode = VAD_MODE_3,
        .wakenet_model_name = "wn9_hiesp",  // WakeNet9
        .wakenet_mode = DET_MODE_2CH_90,
        .afe_mode = SR_MODE_LOW_COST,
        .afe_perferred_core = 0,
        .afe_perferred_priority = 5,
        .afe_ringbuf_size = 50,
        .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM,
        .agc_mode = AFE_MN_PEAK_AGC_MODE_2,
    };

    // Create AFE instance
    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    _modelData = afe_handle->create_from_config(&afe_config);

    if (_modelData == nullptr) {
        Serial.println("[WakeWord] ERROR: Failed to create AFE instance");
        _state = STATE_ERROR;
        return false;
    }
    */

    // For now, just indicate success
    _state = STATE_IDLE;
    _enabled = true;
    Serial.println("[WakeWord] Initialized (ESP32-SR stub)");
    Serial.printf("[WakeWord] Wake word: \"%s\"\n", _wakeWord);
    return true;
#else
    Serial.println("[WakeWord] ESP32-SR not enabled - using stub mode");
    _state = STATE_IDLE;
    _enabled = true;
    return true;
#endif
}

void WakeWordService::end() {
    if (_modelData != nullptr) {
        // TODO: Cleanup ESP32-SR resources
        // afe_handle->destroy(_modelData);
        _modelData = nullptr;
    }
    _state = STATE_UNINITIALIZED;
    _enabled = false;
}

bool WakeWordService::isAwake() {
    return _detected;
}

void WakeWordService::reset() {
    _detected = false;
    _confidence = 0;
    _state = STATE_IDLE;
}

uint8_t WakeWordService::getConfidence() {
    return _confidence;
}

void WakeWordService::processAudio(int16_t* audioBuffer, size_t samples) {
    if (!_enabled || _state != STATE_LISTENING) {
        return;
    }

#ifdef ESP32_SR_ENABLED
    // TODO: Process audio through ESP32-SR when library is installed
    /*
    // Feed audio to AFE
    int afe_chunksize = afe_handle->get_feed_chunksize(_modelData);
    int audio_chunksize = AFE_READ_SAMPLES;

    // Feed audio data
    afe_handle->feed(_modelData, audioBuffer);

    // Check for wake word detection
    int res = afe_handle->fetch(_modelData);
    if (res == AFE_FETCH_CHANNEL_VERIFIED) {
        // Wake word detected
        _detected = true;
        _confidence = 95;  // ESP32-SR doesn't provide confidence, use high value
        _state = STATE_DETECTED;
        handleDetection();
    }
    */
#else
    // Stub mode - no actual detection
    // In real implementation, this would process audio through WakeNet9
#endif
}

void WakeWordService::enable() {
    _enabled = true;
    _state = STATE_LISTENING;
    Serial.println("[WakeWord] Enabled");
}

void WakeWordService::disable() {
    _enabled = false;
    _state = STATE_IDLE;
    Serial.println("[WakeWord] Disabled");
}

bool WakeWordService::initModel() {
    // TODO: Load WakeNet9 model
    // Model files should be in lib/esp-sr/model/
    return true;
}

void WakeWordService::cleanupModel() {
    // TODO: Cleanup model resources
}

void WakeWordService::handleDetection() {
    Serial.printf("[WakeWord] DETECTED! Wake word: \"%s\", Confidence: %d%%\n",
                  _wakeWord, _confidence);
}
