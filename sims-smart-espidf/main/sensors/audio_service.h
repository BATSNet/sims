/**
 * Audio Service - ESP-IDF Port
 *
 * I2S PDM microphone driver for SPM1423 on XIAO ESP32S3 Sense
 */

#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include "driver/i2s.h"
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

    // Record audio for specified duration (for voice message upload)
    // Allocates buffer in PSRAM, caller must free
    uint8_t* record(uint32_t durationMs, size_t *outSize);

    // Get sample rate
    int getSampleRate() const { return AUDIO_SAMPLE_RATE; }

    // Check if initialized
    bool isInitialized() const { return initialized; }

private:
    bool initialized;
    i2s_port_t i2sPort;
};

#endif // AUDIO_SERVICE_H
