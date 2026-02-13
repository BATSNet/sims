/**
 * NVS Storage Service
 *
 * Wrapper for ESP-IDF NVS (Non-Volatile Storage)
 * Manages WiFi credentials and device configuration
 */

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

class NVSStorage {
public:
    // WiFi credential management
    static bool saveWiFiCredentials(const char* ssid, const char* password);
    static bool loadWiFiCredentials(int index, char* ssid, char* password, size_t max_len);
    static int getStoredNetworkCount();
    static bool clearWiFiCredentials();

    // Generic key-value storage
    static bool putString(const char* namespace_name, const char* key, const char* value);
    static bool getString(const char* namespace_name, const char* key, char* value, size_t max_len);
    static bool putInt(const char* namespace_name, const char* key, int32_t value);
    static int32_t getInt(const char* namespace_name, const char* key, int32_t default_value);
    static bool eraseKey(const char* namespace_name, const char* key);
    static bool eraseNamespace(const char* namespace_name);

private:
    // Helper to generate NVS keys for WiFi credentials
    static void getSSIDKey(int index, char* key, size_t max_len);
    static void getPasswordKey(int index, char* key, size_t max_len);
};

#ifdef __cplusplus
}
#endif

#endif // NVS_STORAGE_H
