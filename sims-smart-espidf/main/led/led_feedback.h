/**
 * LED Feedback Service - ESP-IDF Port
 *
 * Manages WS2812B RGB LED for status indication
 * Uses RMT (Remote Control) driver instead of FastLED
 */

#ifndef LED_FEEDBACK_H
#define LED_FEEDBACK_H

#include "driver/rmt.h"
#include "config.h"

class LEDFeedback {
public:
    enum State {
        STATE_IDLE,          // Green pulse - ready for wake word
        STATE_LISTENING,     // Blue pulse - listening for command
        STATE_PROCESSING,    // Yellow solid - processing
        STATE_RECORDING,     // Red pulse - recording audio
        STATE_UPLOADING,     // Cyan pulse - uploading to backend
        STATE_SUCCESS,       // Green flash - success
        STATE_ERROR,         // Red flash - error
        STATE_QUEUED         // Orange pulse - queued incidents pending
    };

    LEDFeedback();
    ~LEDFeedback();

    // Initialize LED
    bool begin();

    // Update LED animation (call in loop)
    void update();

    // Set LED state
    void setState(State state);

    // Get current state
    State getState() const { return currentState; }

    // Direct color control
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void off();

private:
    State currentState;
    unsigned long lastUpdate;
    uint8_t brightness;
    bool pulseDirection;  // true = increasing, false = decreasing
    bool rmtInitialized;
    rmt_channel_t rmtChannel;

    // Current color values
    uint8_t currentR, currentG, currentB;

    // WS2812B timing via RMT
    void sendPixel(uint8_t r, uint8_t g, uint8_t b);

    // Animation helpers
    void animateIdle();
    void animateListening();
    void animateProcessing();
    void animateRecording();
    void animateUploading();
    void animateSuccess();
    void animateError();
    void animateQueued();

    // Pulse animation with color
    void updatePulse(uint8_t r, uint8_t g, uint8_t b);
};

#endif // LED_FEEDBACK_H
