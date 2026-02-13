/**
 * Wake Word Service Implementation - ESP-IDF Port
 *
 * Now with direct ESP32-SR support (the whole reason for this port).
 * ESP32-SR WakeNet9 model for "Hi ESP" wake word detection.
 */

#include "voice/wake_word_service.h"
#include "esp_log.h"

// ESP32-SR includes (only available in ESP-IDF)
#include "esp_wn_iface.h"
#include "esp_wn_models.h"

static const char *TAG = "WakeWord";

WakeWordService::WakeWordService()
    : _state(STATE_UNINITIALIZED),
      _enabled(false),
      _detected(false),
      _confidence(0),
      _wakeWord(nullptr),
      _wakenetHandle(nullptr),
      _modelData(nullptr),
      _chunkSize(0),
      _sampleRate(16000) {
}

WakeWordService::~WakeWordService() {
    end();
}

bool WakeWordService::begin(const char* wakeWord) {
    ESP_LOGI(TAG, "Initializing wake word service...");
    _wakeWord = wakeWord;

    // Get WakeNet interface handle
    const esp_wn_iface_t *wakenet = esp_wn_handle_from_name("wn9_hiesp");
    if (wakenet == nullptr) {
        ESP_LOGE(TAG, "Failed to get WakeNet handle for 'wn9_hiesp'");
        _state = STATE_ERROR;
        return false;
    }

    // Create WakeNet model instance
    // DET_MODE_90 = 90% detection threshold (good balance of accuracy vs false positives)
    _modelData = wakenet->create("wn9_hiesp", DET_MODE_90);
    if (_modelData == nullptr) {
        ESP_LOGE(TAG, "Failed to create WakeNet instance");
        _state = STATE_ERROR;
        return false;
    }

    // Store handle for later use
    _wakenetHandle = (void*)wakenet;

    // Get audio parameters from model
    _sampleRate = wakenet->get_samp_rate(_modelData);
    _chunkSize = wakenet->get_samp_chunksize(_modelData);

    _state = STATE_IDLE;
    _enabled = true;

    ESP_LOGI(TAG, "WakeNet9 initialized successfully");
    ESP_LOGI(TAG, "Wake word: \"Hi ESP\" (model: wn9_hiesp)");
    ESP_LOGI(TAG, "Sample rate: %d Hz, Chunk size: %d samples", _sampleRate, _chunkSize);

    return true;
}

void WakeWordService::end() {
    if (_modelData != nullptr && _wakenetHandle != nullptr) {
        const esp_wn_iface_t *wakenet = (const esp_wn_iface_t*)_wakenetHandle;
        wakenet->destroy(_modelData);
        _modelData = nullptr;
        _wakenetHandle = nullptr;
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

    if (_wakenetHandle == nullptr || _modelData == nullptr) {
        return;
    }

    const esp_wn_iface_t *wakenet = (const esp_wn_iface_t*)_wakenetHandle;

    // Detect wake word in audio buffer
    // WakeNet expects chunks of exactly _chunkSize samples
    int result = wakenet->detect(_modelData, audioBuffer);
    if (result > 0) {
        // Wake word detected (result > 0 indicates detection index)
        _detected = true;
        _confidence = 95;  // WakeNet doesn't provide a confidence score
        _state = STATE_DETECTED;
        handleDetection();
    }
}

void WakeWordService::enable() {
    if (_state != STATE_UNINITIALIZED && _state != STATE_ERROR) {
        _enabled = true;
        _state = STATE_LISTENING;
    }
}

void WakeWordService::disable() {
    _enabled = false;
    if (_state == STATE_LISTENING) {
        _state = STATE_IDLE;
    }
}

void WakeWordService::handleDetection() {
    ESP_LOGI(TAG, "DETECTED! Wake word: \"%s\", Confidence: %d%%",
             _wakeWord ? _wakeWord : "Hi ESP", _confidence);
}
