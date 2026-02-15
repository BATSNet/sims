/**
 * Raw PCM Speech Recorder Implementation
 *
 * Accepts 16kHz 16-bit PCM, applies 4x software gain, and stores raw int16
 * samples directly into PSRAM at native 16kHz. No downsampling or encoding.
 */

#include "codec/lpc10_codec.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char* TAG = "PCM";

LPC10Encoder::LPC10Encoder()
    : _sampleCount(0),
      _buf(nullptr),
      _bufPos(0) {
}

LPC10Encoder::~LPC10Encoder() {
    if (_buf) {
        heap_caps_free(_buf);
        _buf = nullptr;
    }
}

void LPC10Encoder::reset() {
    if (!_buf) {
        _buf = (uint8_t*)heap_caps_malloc(RAW_PCM_MAX_BYTES, MALLOC_CAP_SPIRAM);
        if (!_buf) {
            _buf = (uint8_t*)malloc(RAW_PCM_MAX_BYTES);
        }
        if (!_buf) {
            ESP_LOGE(TAG, "Failed to allocate PCM buffer (%d bytes)", RAW_PCM_MAX_BYTES);
            return;
        }
    }

    _bufPos = 0;
    _sampleCount = 0;

    ESP_LOGI(TAG, "PCM recorder reset, buffer at %p (%d bytes max)",
             _buf, RAW_PCM_MAX_BYTES);
}

uint32_t LPC10Encoder::getDurationMs() const {
    // Each sample is at 16kHz
    return (_sampleCount * 1000) / 16000;
}

void LPC10Encoder::feedSamples(const int16_t* samples, size_t count) {
    if (!_buf || isFull()) return;

    for (size_t i = 0; i < count && _bufPos + 2 <= RAW_PCM_MAX_BYTES; i++) {
        // Apply 4x software gain (matches Seeed's VOLUME_GAIN=2 example)
        int32_t amplified = (int32_t)samples[i] << 2;
        // Clamp to int16 range
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        int16_t sample = (int16_t)amplified;

        // Store raw 16-bit sample (little-endian)
        _buf[_bufPos++] = (uint8_t)(sample & 0xFF);
        _buf[_bufPos++] = (uint8_t)((sample >> 8) & 0xFF);
        _sampleCount++;
    }
}
