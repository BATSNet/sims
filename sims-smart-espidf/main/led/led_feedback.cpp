/**
 * LED Feedback Service Implementation - ESP-IDF Port
 *
 * Uses the espressif/led_strip managed component with SPI backend.
 * The RMT TX backend produced no visible output on ESP-IDF v5.5,
 * so we switched to SPI which uses the MOSI line for signal generation.
 *
 * The led_strip component handles all WS2812B timing internally,
 * including the GRB byte order and reset pulse.
 */

#include "led/led_feedback.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "LED";

// RMT resolution for WS2812B - 10MHz gives 100ns per tick
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

LEDFeedback::LEDFeedback()
    : currentState(STATE_IDLE),
      lastUpdate(0),
      brightness(0),
      pulseDirection(true),
      stripInitialized(false),
      ledStrip(nullptr),
      currentR(0), currentG(0), currentB(0) {
}

LEDFeedback::~LEDFeedback() {
    if (stripInitialized && ledStrip) {
        off();
        led_strip_del(ledStrip);
        ledStrip = nullptr;
    }
}

bool LEDFeedback::begin() {
    ESP_LOGI(TAG, "Initializing LED feedback (led_strip RMT driver)...");

    // Configure the LED strip (common settings)
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = STATUS_LED_PIN;
    strip_config.max_leds = STATUS_LED_COUNT;
    strip_config.led_model = LED_MODEL_WS2812;
    // Set GRB format field-by-field (compound literal macros can fail in C++)
    strip_config.color_component_format.format.r_pos = 1;
    strip_config.color_component_format.format.g_pos = 0;
    strip_config.color_component_format.format.b_pos = 2;
    strip_config.color_component_format.format.w_pos = 3;
    strip_config.color_component_format.format.bytes_per_color = 1;
    strip_config.color_component_format.format.num_components = 3;
    strip_config.flags.invert_out = false;

    // Configure the RMT backend
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = LED_STRIP_RMT_RES_HZ;
    rmt_config.flags.with_dma = false;

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &ledStrip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LED strip RMT init failed: %s", esp_err_to_name(err));
        return false;
    }

    // Clear the LED on startup
    led_strip_clear(ledStrip);

    stripInitialized = true;
    ESP_LOGI(TAG, "LED feedback initialized on GPIO%d (led_strip component)", STATUS_LED_PIN);

    // Startup test: brief white flash to confirm LED works
    sendPixel(255, 255, 255);
    vTaskDelay(pdMS_TO_TICKS(200));
    sendPixel(0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Start in idle state
    setState(STATE_IDLE);
    return true;
}

void LEDFeedback::sendPixel(uint8_t r, uint8_t g, uint8_t b) {
    if (!stripInitialized || !ledStrip) return;

    // The led_strip component handles GRB ordering internally
    led_strip_set_pixel(ledStrip, 0, r, g, b);
    led_strip_refresh(ledStrip);
}

void LEDFeedback::update() {
    switch (currentState) {
        case STATE_IDLE:       animateIdle();       break;
        case STATE_LISTENING:  animateListening();  break;
        case STATE_PROCESSING: animateProcessing(); break;
        case STATE_RECORDING:  animateRecording();  break;
        case STATE_UPLOADING:  animateUploading();  break;
        case STATE_SUCCESS:    animateSuccess();    break;
        case STATE_ERROR:      animateError();      break;
        case STATE_QUEUED:     animateQueued();      break;
    }
}

void LEDFeedback::setState(State state) {
    if (currentState != state) {
        currentState = state;
        brightness = 0;
        pulseDirection = true;
        lastUpdate = xTaskGetTickCount() * portTICK_PERIOD_MS;

        static const char* stateNames[] = {
            "IDLE", "LISTENING", "PROCESSING", "RECORDING",
            "UPLOADING", "SUCCESS", "ERROR", "QUEUED"
        };
        ESP_LOGI(TAG, "State: %s", stateNames[state]);
    }
}

void LEDFeedback::setColor(uint8_t r, uint8_t g, uint8_t b) {
    currentR = r;
    currentG = g;
    currentB = b;
    sendPixel(r, g, b);
}

void LEDFeedback::off() {
    if (stripInitialized && ledStrip) {
        led_strip_clear(ledStrip);
    }
    currentR = 0;
    currentG = 0;
    currentB = 0;
}

void LEDFeedback::animateIdle() {
    // Green pulse (slow)
    updatePulse(0, 255, 0);
}

void LEDFeedback::animateListening() {
    // Blue pulse (medium)
    updatePulse(0, 0, 255);
}

void LEDFeedback::animateProcessing() {
    // Yellow solid
    setColor(255, 200, 0);
}

void LEDFeedback::animateRecording() {
    // Red pulse (fast)
    updatePulse(255, 0, 0);
}

void LEDFeedback::animateUploading() {
    // Cyan pulse (medium)
    updatePulse(0, 255, 255);
}

void LEDFeedback::animateSuccess() {
    // Green flash (3x)
    unsigned long now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    unsigned long elapsed = now - lastUpdate;
    if (elapsed < 1500) {  // 3 flashes over 1.5s
        int flashPhase = (elapsed / 250) % 2;
        if (flashPhase) {
            setColor(0, 255, 0);
        } else {
            off();
        }
    } else {
        setState(STATE_IDLE);  // Return to idle after flashing
    }
}

void LEDFeedback::animateError() {
    // Red flash (5x)
    unsigned long now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    unsigned long elapsed = now - lastUpdate;
    if (elapsed < 2500) {  // 5 flashes over 2.5s
        int flashPhase = (elapsed / 250) % 2;
        if (flashPhase) {
            setColor(255, 0, 0);
        } else {
            off();
        }
    } else {
        setState(STATE_IDLE);  // Return to idle after flashing
    }
}

void LEDFeedback::animateQueued() {
    // Orange pulse (slow)
    updatePulse(255, 100, 0);
}

void LEDFeedback::updatePulse(uint8_t r, uint8_t g, uint8_t b) {
    unsigned long now = xTaskGetTickCount() * portTICK_PERIOD_MS;
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
        uint8_t scaledR = (r * brightness) / 255;
        uint8_t scaledG = (g * brightness) / 255;
        uint8_t scaledB = (b * brightness) / 255;
        setColor(scaledR, scaledG, scaledB);
    }
}
