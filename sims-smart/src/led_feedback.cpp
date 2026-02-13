/**
 * LED Feedback Service Implementation
 */

#include "led_feedback.h"

LEDFeedback::LEDFeedback()
    : currentState(STATE_IDLE), lastUpdate(0), brightness(0), pulseDirection(true) {
}

bool LEDFeedback::begin() {
    FastLED.addLeds<WS2812B, STATUS_LED_PIN, GRB>(leds, STATUS_LED_COUNT);
    FastLED.setBrightness(255);

    Serial.println("[LED] LED feedback initialized");
    setState(STATE_IDLE);
    return true;
}

void LEDFeedback::update() {
    switch (currentState) {
        case STATE_IDLE:
            animateIdle();
            break;
        case STATE_LISTENING:
            animateListening();
            break;
        case STATE_PROCESSING:
            animateProcessing();
            break;
        case STATE_RECORDING:
            animateRecording();
            break;
        case STATE_UPLOADING:
            animateUploading();
            break;
        case STATE_SUCCESS:
            animateSuccess();
            break;
        case STATE_ERROR:
            animateError();
            break;
        case STATE_QUEUED:
            animateQueued();
            break;
    }
}

void LEDFeedback::setState(State state) {
    if (currentState != state) {
        currentState = state;
        brightness = 0;
        pulseDirection = true;
        lastUpdate = millis();
#ifdef DEBUG_SERIAL
        const char* stateNames[] = {
            "IDLE", "LISTENING", "PROCESSING", "RECORDING",
            "UPLOADING", "SUCCESS", "ERROR", "QUEUED"
        };
        Serial.printf("[LED] State: %s\n", stateNames[state]);
#endif
    }
}

void LEDFeedback::setColor(uint8_t r, uint8_t g, uint8_t b) {
    leds[0] = CRGB(r, g, b);
    FastLED.show();
}

void LEDFeedback::off() {
    leds[0] = CRGB::Black;
    FastLED.show();
}

void LEDFeedback::animateIdle() {
    // Green pulse (slow)
    updatePulse(CRGB::Green);
}

void LEDFeedback::animateListening() {
    // Blue pulse (medium)
    updatePulse(CRGB::Blue);
}

void LEDFeedback::animateProcessing() {
    // Yellow solid
    leds[0] = CRGB::Yellow;
    FastLED.show();
}

void LEDFeedback::animateRecording() {
    // Red pulse (fast)
    updatePulse(CRGB::Red);
}

void LEDFeedback::animateUploading() {
    // Cyan pulse (medium)
    updatePulse(CRGB::Cyan);
}

void LEDFeedback::animateSuccess() {
    // Green flash (3x)
    unsigned long elapsed = millis() - lastUpdate;
    if (elapsed < 1500) {  // 3 flashes over 1.5s
        int flashNum = (elapsed / 250) % 2;
        leds[0] = flashNum ? CRGB::Green : CRGB::Black;
        FastLED.show();
    } else {
        setState(STATE_IDLE);  // Return to idle after flashing
    }
}

void LEDFeedback::animateError() {
    // Red flash (5x)
    unsigned long elapsed = millis() - lastUpdate;
    if (elapsed < 2500) {  // 5 flashes over 2.5s
        int flashNum = (elapsed / 250) % 2;
        leds[0] = flashNum ? CRGB::Red : CRGB::Black;
        FastLED.show();
    } else {
        setState(STATE_IDLE);  // Return to idle after flashing
    }
}

void LEDFeedback::animateQueued() {
    // Orange pulse (slow)
    updatePulse(CRGB::Orange);
}

void LEDFeedback::updatePulse(CRGB color) {
    unsigned long now = millis();
    if (now - lastUpdate > 20) {  // Update every 20ms
        lastUpdate = now;

        // Update brightness for pulse effect
        if (pulseDirection) {
            brightness += 5;
            if (brightness >= 250) {
                pulseDirection = false;
            }
        } else {
            brightness -= 5;
            if (brightness <= 5) {
                pulseDirection = true;
            }
        }

        // Apply brightness to color
        leds[0] = color;
        leds[0].fadeLightBy(255 - brightness);
        FastLED.show();
    }
}
