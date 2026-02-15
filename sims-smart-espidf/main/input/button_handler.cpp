/**
 * Button Handler Implementation for SIMS-SMART Device
 *
 * Handles debounce and long-press detection for 3 GPIO buttons.
 * Active LOW with internal pull-up resistors.
 */

#include "input/button_handler.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "Buttons";

static unsigned long millis_now() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

ButtonHandler::ButtonHandler() : initialized_(false) {
    buttons_[0] = { BTN_ACTION_PIN, true, true, false, 0, 0, false };
    buttons_[1] = { BTN_CANCEL_PIN, true, true, false, 0, 0, false };
    buttons_[2] = { BTN_MODE_PIN,   true, true, false, 0, 0, false };
}

ButtonHandler::~ButtonHandler() {}

bool ButtonHandler::begin() {
    for (int i = 0; i < 3; i++) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << buttons_[i].pin);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;

        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure GPIO %d: %s",
                     buttons_[i].pin, esp_err_to_name(err));
            return false;
        }
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Buttons initialized (ACTION=%d, CANCEL=%d, MODE=%d)",
             BTN_ACTION_PIN, BTN_CANCEL_PIN, BTN_MODE_PIN);
    return true;
}

ButtonHandler::Event ButtonHandler::poll() {
    if (!initialized_) return EVENT_NONE;

    unsigned long now = millis_now();

    for (int i = 0; i < 3; i++) {
        ButtonState& btn = buttons_[i];
        bool reading = gpio_get_level((gpio_num_t)btn.pin);

        // Debounce
        if (reading != btn.lastReading) {
            btn.lastDebounceTime = now;
        }
        btn.lastReading = reading;

        if ((now - btn.lastDebounceTime) < BTN_DEBOUNCE_MS) {
            continue;
        }

        bool pressed = !reading; // Active LOW

        // State change: not pressed -> pressed
        if (pressed && !btn.stableState) {
            btn.stableState = true;
            btn.wasPressed = true;
            btn.pressStartTime = now;
            btn.longPressReported = false;
        }

        // State change: pressed -> released
        if (!pressed && btn.stableState) {
            btn.stableState = false;

            // Only report short press if long press wasn't already reported
            if (btn.wasPressed && !btn.longPressReported) {
                btn.wasPressed = false;

                switch (i) {
                    case 0: return EVENT_ACTION_PRESS;
                    case 1: return EVENT_CANCEL_PRESS;
                    case 2: return EVENT_MODE_PRESS;
                }
            }
            btn.wasPressed = false;
        }

        // Long press detection (only for action button, index 0)
        if (i == 0 && btn.stableState && btn.wasPressed && !btn.longPressReported) {
            if ((now - btn.pressStartTime) >= BTN_LONG_PRESS_MS) {
                btn.longPressReported = true;
                return EVENT_ACTION_LONG_PRESS;
            }
        }
    }

    return EVENT_NONE;
}
