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
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
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
    // Initialize ESP32-SR WakeNet
    Serial.println("[WakeWord] Initializing ESP32-SR WakeNet...");

    // Get WakeNet interface handle
    const esp_wn_iface_t *wakenet = esp_wn_handle_from_name("wn9_hiesp");
    if (wakenet == nullptr) {
        Serial.println("[WakeWord] ERROR: Failed to get WakeNet handle");
        _state = STATE_ERROR;
        return false;
    }

    // Create WakeNet model instance (DET_MODE_90 = 90% confidence)
    _modelData = wakenet->create("wn9_hiesp", DET_MODE_90);
    if (_modelData == nullptr) {
        Serial.println("[WakeWord] ERROR: Failed to create WakeNet instance");
        _state = STATE_ERROR;
        return false;
    }

    _state = STATE_IDLE;
    _enabled = true;
    Serial.println("[WakeWord] ESP32-SR WakeNet initialized successfully");
    Serial.printf("[WakeWord] Wake word: \"Hi ESP\" (model: wn9_hiesp)\n");
    Serial.printf("[WakeWord] Sample rate: %d Hz\n", wakenet->get_samp_rate(_modelData));
    Serial.printf("[WakeWord] Chunk size: %d samples\n", wakenet->get_samp_chunksize(_modelData));
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
#ifdef ESP32_SR_ENABLED
        // Cleanup ESP32-SR resources
        const esp_wn_iface_t *wakenet = esp_wn_handle_from_name("wn9_hiesp");
        if (wakenet != nullptr) {
            wakenet->destroy(_modelData);
        }
#endif
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
    // Get WakeNet interface
    const esp_wn_iface_t *wakenet = esp_wn_handle_from_name("wn9_hiesp");
    if (wakenet == nullptr || _modelData == nullptr) {
        return;
    }

    // Detect wake word in audio buffer
    int result = wakenet->detect(_modelData, audioBuffer);
    if (result > 0) {
        // Wake word detected (result > 0 indicates detection)
        _detected = true;
        _confidence = 95;  // ESP32-SR doesn't provide confidence score
        _state = STATE_DETECTED;
        handleDetection();
    }
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
