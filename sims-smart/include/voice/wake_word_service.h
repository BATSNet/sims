#ifndef WAKE_WORD_SERVICE_H
#define WAKE_WORD_SERVICE_H

#include <Arduino.h>

// Forward declaration for ESP32-SR model data
// Will be properly defined when ESP32-SR is included in the .cpp file
struct model_iface_data_t;

class WakeWordService {
public:
    WakeWordService();
    ~WakeWordService();

    // Initialize wake word detection
    bool begin(const char* wakeWord = "SIMS Alert");

    // Stop wake word detection
    void end();

    // Check if wake word was detected (non-blocking)
    bool isAwake();

    // Reset wake word state (return to listening)
    void reset();

    // Get detection confidence (0-100)
    uint8_t getConfidence();

    // Process audio buffer (called from audio task)
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

private:
    State _state;
    bool _enabled;
    bool _detected;
    uint8_t _confidence;
    const char* _wakeWord;

    // ESP32-SR model handles
    model_iface_data_t* _modelData;

    // Internal methods
    bool initModel();
    void cleanupModel();
    void handleDetection();
};

#endif // WAKE_WORD_SERVICE_H
