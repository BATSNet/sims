/**
 * Audio Service
 * Handles I2S microphone recording
 */

#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include <Arduino.h>
#include <driver/i2s.h>
#include "../config.h"

class AudioService {
public:
    AudioService();
    ~AudioService();

    // Initialize I2S microphone
    bool begin();

    // Start recording
    bool startRecording();

    // Stop recording
    bool stopRecording();

    // Check if currently recording
    bool isRecording();

    // Get recorded audio data
    uint8_t* getAudioData();
    size_t getAudioSize();

    // Check if audio is available
    bool hasAudio();

    // Clear audio buffer
    void clearAudio();

private:
    bool initialized;
    bool recording;
    uint8_t* audioBuffer;
    size_t audioBufferSize;
    size_t audioDataSize;
    unsigned long recordingStartTime;

    void allocateBuffer();
    void freeBuffer();
};

#endif // AUDIO_SERVICE_H
