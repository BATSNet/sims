/**
 * GPS Service Implementation
 * ESP-IDF version using uart_driver
 */

#include "sensors/gps_service.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "GPS";

static inline unsigned long millis_now() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

GPSService::GPSService()
    : initialized(false), lastUpdate(0), uartNum(1) {
    currentLocation.valid = false;
}

bool GPSService::begin(int rxPin, int txPin) {
    ESP_LOGI(TAG, "Initializing GPS service...");

    uart_config_t uart_config = {};
    uart_config.baud_rate = GPS_BAUD_RATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_param_config((uart_port_t)uartNum, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_set_pin((uart_port_t)uartNum, txPin, rxPin, -1, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART pin config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_driver_install((uart_port_t)uartNum, 1024, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "GPS service initialized (RX=%d, TX=%d)", rxPin, txPin);
    ESP_LOGI(TAG, "Waiting for GPS fix...");

    return true;
}

void GPSService::update() {
    if (!initialized) return;

    // Read available bytes from UART
    uint8_t data[128];
    int len = uart_read_bytes((uart_port_t)uartNum, data, sizeof(data), 0);

    if (len > 0) {
        for (int i = 0; i < len; i++) {
            gps.encode((char)data[i]);
        }
    }

    // Update location data if we have a fix
    if (gps.location.isValid()) {
        currentLocation.latitude = gps.location.lat();
        currentLocation.longitude = gps.location.lng();
        currentLocation.altitude = gps.altitude.meters();
        currentLocation.speed = gps.speed.kmph();
        currentLocation.bearing = gps.course.deg();
        currentLocation.valid = true;
        currentLocation.timestamp = millis_now();
        lastUpdate = millis_now();

        static bool firstFix = true;
        if (firstFix) {
            ESP_LOGI(TAG, "First fix acquired: %.6f, %.6f",
                     currentLocation.latitude, currentLocation.longitude);
            firstFix = false;
        }
    } else {
        if (millis_now() - lastUpdate > GPS_TIMEOUT) {
            currentLocation.valid = false;
        }
    }

    // Periodic status log
    static unsigned long lastLog = 0;
    if (millis_now() - lastLog > 30000) {
        ESP_LOGI(TAG, "Status: %s, Satellites: %d, HDOP: %.2f",
                 hasFix() ? "FIX" : "NO FIX",
                 gps.satellites.value(),
                 gps.hdop.hdop());
        lastLog = millis_now();
    }
}

GPSLocation GPSService::getLocation() {
    return currentLocation;
}

bool GPSService::hasFix() {
    return currentLocation.valid && gps.location.isValid();
}

int GPSService::getSatellites() {
    return gps.satellites.value();
}

void GPSService::getStats(int& satellites, float& hdop, unsigned long& age) {
    satellites = gps.satellites.value();
    hdop = gps.hdop.hdop();
    age = gps.location.age();
}
