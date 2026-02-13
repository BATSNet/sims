/**
 * LED Feedback Service Implementation - ESP-IDF Port
 *
 * Replaces FastLED with RMT (Remote Control) driver for WS2812B LED.
 * RMT is ESP-IDF's peripheral for precise timing signals.
 */

#include "led/led_feedback.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "LED";

// WS2812B timing constants (in RMT ticks at 10MHz = 100ns per tick)
// T0H = 400ns (4 ticks), T0L = 850ns (8.5 -> 9 ticks)
// T1H = 800ns (8 ticks), T1L = 450ns (4.5 -> 5 ticks)
#define WS2812_T0H  4
#define WS2812_T0L  9
#define WS2812_T1H  8
#define WS2812_T1L  5

LEDFeedback::LEDFeedback()
    : currentState(STATE_IDLE),
      lastUpdate(0),
      brightness(0),
      pulseDirection(true),
      rmtInitialized(false),
      rmtChannel(RMT_CHANNEL_0),
      currentR(0), currentG(0), currentB(0) {
}

LEDFeedback::~LEDFeedback() {
    if (rmtInitialized) {
        off();
        rmt_driver_uninstall(rmtChannel);
    }
}

bool LEDFeedback::begin() {
    ESP_LOGI(TAG, "Initializing LED feedback (RMT driver)...");

    rmt_config_t rmt_cfg = {};
    rmt_cfg.rmt_mode = RMT_MODE_TX;
    rmt_cfg.channel = rmtChannel;
    rmt_cfg.gpio_num = (gpio_num_t)STATUS_LED_PIN;
    rmt_cfg.clk_div = 8;  // 80MHz / 8 = 10MHz -> 100ns per tick
    rmt_cfg.mem_block_num = 1;
    rmt_cfg.tx_config.loop_en = false;
    rmt_cfg.tx_config.carrier_en = false;
    rmt_cfg.tx_config.idle_output_en = true;
    rmt_cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

    esp_err_t err = rmt_config(&rmt_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = rmt_driver_install(rmtChannel, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    rmtInitialized = true;
    ESP_LOGI(TAG, "LED feedback initialized on GPIO%d", STATUS_LED_PIN);

    // Start in idle state
    setState(STATE_IDLE);
    return true;
}

void LEDFeedback::sendPixel(uint8_t r, uint8_t g, uint8_t b) {
    if (!rmtInitialized) return;

    // WS2812B expects GRB order
    rmt_item32_t items[24];
    uint32_t color = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;

    for (int i = 0; i < 24; i++) {
        uint8_t bit = (color >> (23 - i)) & 1;
        if (bit) {
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T1H;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T1L;
        } else {
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T0H;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T0L;
        }
    }

    rmt_write_items(rmtChannel, items, 24, true);
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
    sendPixel(0, 0, 0);
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
