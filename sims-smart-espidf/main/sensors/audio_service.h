/**
 * Audio Service - ESP-IDF Port
 *
 * I2S PDM microphone driver for SPM1423 on XIAO ESP32S3 Sense
 * Uses new ESP-IDF v5.x I2S PDM RX driver (not legacy)
 */

#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include "driver/i2s_pdm.h"
#include "config.h"

class AudioService {
public:
    AudioService();
    ~AudioService();

    // Initialize I2S PDM microphone
    bool begin();

    // Stop I2S driver
    void end();

    // Read audio samples (for voice recognition - small chunks)
    // Returns number of samples actually read
    size_t read(int16_t *buffer, size_t maxSamples);

    // Record audio with silence detection (for voice message upload)
    // Stops early when silence detected after speech starts.
    // Allocates buffer in PSRAM, caller must free.
    uint8_t* record(uint32_t maxDurationMs, size_t *outSize, uint32_t silenceTimeoutMs = 4000);

    // Get sample rate
    int getSampleRate() const { return AUDIO_SAMPLE_RATE; }

    // Check if initialized
    bool isInitialized() const { return initialized; }

private:
    bool initialized;
    i2s_chan_handle_t rxHandle;
};

#endif // AUDIO_SERVICE_H
