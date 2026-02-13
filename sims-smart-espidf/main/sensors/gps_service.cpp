/**
 * GPS Service Implementation - ESP-IDF Port
 *
 * Port UART from Arduino HardwareSerial to ESP-IDF uart_driver.
 * TinyGPS++ library used as-is (pure C++, no Arduino dependency).
 */

#include "sensors/gps_service.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "GPS";

GPSService::GPSService()
    : initialized(false), lastUpdate(0), uartPort(GPS_UART_NUM) {
    memset(&currentLocation, 0, sizeof(currentLocation));
    currentLocation.valid = false;
}

bool GPSService::begin(int rxPin, int txPin, int baud) {
    ESP_LOGI(TAG, "Initializing GPS service...");

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(uartPort, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_set_pin(uartPort, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        return false;
    }

    // Install UART driver with RX buffer only (no TX needed for GPS)
    err = uart_driver_install(uartPort, 1024, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "GPS service initialized on UART%d (RX=%d, TX=%d, %d baud)",
             uartPort, rxPin, txPin, baud);
    ESP_LOGI(TAG, "Waiting for GPS fix...");

    return true;
}

void GPSService::update() {
    if (!initialized) {
        return;
    }

    // Read available data from UART
    uint8_t data[256];
    int len = uart_read_bytes(uartPort, data, sizeof(data), pdMS_TO_TICKS(10));

    // Feed data to TinyGPS++ parser
    for (int i = 0; i < len; i++) {
        gps.encode((char)data[i]);
    }

    // Update location data if we have a fix
    if (gps.location.isValid()) {
        currentLocation.latitude = gps.location.lat();
        currentLocation.longitude = gps.location.lng();
        currentLocation.altitude = gps.altitude.meters();
        currentLocation.speed = gps.speed.kmph();
        currentLocation.bearing = gps.course.deg();
        currentLocation.valid = true;
        currentLocation.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
        lastUpdate = currentLocation.timestamp;

        // Log GPS fix on first acquisition
        static bool firstFix = true;
        if (firstFix) {
            ESP_LOGI(TAG, "First fix acquired: %.6f, %.6f (%d sats)",
                     currentLocation.latitude, currentLocation.longitude,
                     gps.satellites.value());
            firstFix = false;
        }
    } else {
        // Check if data is stale
        unsigned long now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (lastUpdate > 0 && (now - lastUpdate > GPS_FIX_TIMEOUT_MS)) {
            if (!GPS_USE_CACHED) {
                currentLocation.valid = false;
            }
        }
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
