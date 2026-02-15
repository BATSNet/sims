/**
 * Button Handler for SIMS-SMART Device
 *
 * Handles 3 physical buttons with debounce and long-press detection.
 * Buttons are active LOW with internal pull-up resistors.
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>
#include "config.h"

class ButtonHandler {
public:
    // Button events
    enum Event {
        EVENT_NONE = 0,
        EVENT_ACTION_PRESS,       // Short press on action/PTT button
        EVENT_ACTION_LONG_PRESS,  // Long press on action button (flashlight toggle)
        EVENT_CANCEL_PRESS,       // Short press on cancel button
        EVENT_MODE_PRESS,         // Short press on mode/menu button
    };

    ButtonHandler();
    ~ButtonHandler();

    // Initialize GPIO pins with internal pull-up
    bool begin();

    // Poll buttons - call every loop iteration, returns event if any
    Event poll();

private:
    struct ButtonState {
        int pin;
        bool lastReading;
        bool stableState;
        bool wasPressed;      // tracks if button was pressed (for release detection)
        unsigned long lastDebounceTime;
        unsigned long pressStartTime;
        bool longPressReported;
    };

    ButtonState buttons_[3];
    bool initialized_;
};

#endif // BUTTON_HANDLER_H
