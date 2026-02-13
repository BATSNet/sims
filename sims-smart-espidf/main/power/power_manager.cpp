/**
 * Power Manager Implementation - ESP-IDF
 */

#include "power/power_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "Power";

PowerManager::PowerManager()
    : pmConfigured(false) {
}

bool PowerManager::begin() {
    ESP_LOGI(TAG, "Initializing power manager...");

    // Configure ADC for battery monitoring
#ifdef BATTERY_ENABLED
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);  // GPIO1
    ESP_LOGI(TAG, "Battery ADC configured on GPIO%d", BATTERY_ADC_PIN);
#endif

    ESP_LOGI(TAG, "Power manager initialized");
    return true;
}

bool PowerManager::enableLightSleep() {
#if CONFIG_PM_ENABLE
    ESP_LOGI(TAG, "Enabling automatic light sleep...");

    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = true
    };

    esp_err_t err = esp_pm_configure(&pm_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PM: %s", esp_err_to_name(err));
        return false;
    }

    pmConfigured = true;
    ESP_LOGI(TAG, "Light sleep enabled (240/80 MHz DFS)");
    return true;
#else
    ESP_LOGW(TAG, "Power management not enabled in sdkconfig");
    return false;
#endif
}

void PowerManager::disableLightSleep() {
#if CONFIG_PM_ENABLE
    if (pmConfigured) {
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 240,
            .min_freq_mhz = 240,
            .light_sleep_enable = false
        };
        esp_pm_configure(&pm_config);
        pmConfigured = false;
        ESP_LOGI(TAG, "Light sleep disabled");
    }
#endif
}

int PowerManager::getBatteryPercent() {
    int mv = getBatteryVoltage();
    if (mv <= BATTERY_MIN_MV) return 0;
    if (mv >= BATTERY_MAX_MV) return 100;

    return ((mv - BATTERY_MIN_MV) * 100) / (BATTERY_MAX_MV - BATTERY_MIN_MV);
}

int PowerManager::getBatteryVoltage() {
#ifdef BATTERY_ENABLED
    int raw = readBatteryADC();
    // Convert ADC reading to millivolts
    // ADC with 11dB attenuation: ~0-3.3V range, 12-bit resolution
    // With voltage divider on XIAO (typically 2:1), multiply by 2
    int mv = (raw * 3300 * 2) / 4095;
    return mv;
#else
    return BATTERY_MAX_MV;  // Assume full if no battery monitoring
#endif
}

bool PowerManager::isLowBattery() {
    return getBatteryPercent() < LOW_BATTERY_PERCENT;
}

void PowerManager::enableWiFiPower() {
    esp_wifi_start();
}

void PowerManager::disableWiFiPower() {
    esp_wifi_stop();
}

void PowerManager::enableCameraPower() {
    // Camera power is managed by CameraService::begin()/sleep()/wake()
    // On XIAO ESP32S3, there's no separate power control pin
}

void PowerManager::disableCameraPower() {
    // Camera power is managed by CameraService::sleep()
}

void PowerManager::enterDeepSleep(uint64_t sleepTimeUs) {
    ESP_LOGI(TAG, "Entering deep sleep for %llu us...", sleepTimeUs);

    // Configure wake-up source
    esp_sleep_enable_timer_wakeup(sleepTimeUs);

    // Enter deep sleep
    esp_deep_sleep_start();
    // Code execution stops here until wake-up
}

int PowerManager::readBatteryADC() {
#ifdef BATTERY_ENABLED
    // Average multiple readings for stability
    int total = 0;
    for (int i = 0; i < 16; i++) {
        total += adc1_get_raw(ADC1_CHANNEL_0);
    }
    return total / 16;
#else
    return 0;
#endif
}
