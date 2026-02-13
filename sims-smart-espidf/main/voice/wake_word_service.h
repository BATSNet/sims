/**
 * Wake Word Service - ESP-IDF Port
 *
 * ESP32-SR WakeNet9 integration for wake word detection.
 * This is the core reason for the ESP-IDF port - ESP32-SR requires
 * ESP-IDF infrastructure (SPIFFS, model loading APIs, DSP).
 */

#ifndef WAKE_WORD_SERVICE_H
#define WAKE_WORD_SERVICE_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration for ESP32-SR model data
struct model_iface_data_t;

class WakeWordService {
public:
    WakeWordService();
    ~WakeWordService();

    // Initialize wake word detection
    bool begin(const char* wakeWord = "hiesp");

    // Stop wake word detection
    void end();

    // Check if wake word was detected (non-blocking)
    bool isAwake();

    // Reset wake word state (return to listening)
    void reset();

    // Get detection confidence (0-100)
    uint8_t getConfidence();

    // Process audio buffer (called from voice task)
    void processAudio(int16_t* audioBuffer, size_t samples);

    // Get current state
    enum State {
        STATE_UNINITIALIZED,
        STATE_IDLE,
        STATE_LISTENING,
        STATE_DETECTED,
        STATE_ERROR
    };
    State getState() const { return _state; }

    // Enable/disable wake word detection
    void enable();
    void disable();
    bool isEnabled() const { return _enabled; }

    // Get required audio chunk size for WakeNet
    int getChunkSize() const { return _chunkSize; }

    // Get required sample rate
    int getSampleRate() const { return _sampleRate; }

private:
    State _state;
    bool _enabled;
    bool _detected;
    uint8_t _confidence;
    const char* _wakeWord;

    // ESP32-SR model handles (opaque pointers)
    void* _wakenetHandle;     // esp_wn_iface_t*
    model_iface_data_t* _modelData;

    // Audio parameters from model
    int _chunkSize;
    int _sampleRate;

    // Internal methods
    void handleDetection();
};

#endif // WAKE_WORD_SERVICE_H
