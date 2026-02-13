/**
 * NVS Storage Implementation
 */

#include "storage/nvs_storage.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "NVS";

// WiFi credential management

bool NVSStorage::saveWiFiCredentials(const char* ssid, const char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    // Find empty slot or check if SSID already exists
    int slot = -1;
    char stored_ssid[33];
    char key[16];

    for (int i = 0; i < WIFI_MAX_STORED_NETWORKS; i++) {
        getSSIDKey(i, key, sizeof(key));
        size_t len = sizeof(stored_ssid);
        err = nvs_get_str(nvs_handle, key, stored_ssid, &len);

        if (err == ESP_ERR_NVS_NOT_FOUND) {
            // Empty slot found
            if (slot == -1) {
                slot = i;
            }
        } else if (err == ESP_OK && strcmp(stored_ssid, ssid) == 0) {
            // SSID already exists, update it
            slot = i;
            break;
        }
    }

    // If no empty slot, overwrite oldest (slot 0)
    if (slot == -1) {
        slot = 0;
    }

    // Save credentials
    getSSIDKey(slot, key, sizeof(key));
    err = nvs_set_str(nvs_handle, key, ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    getPasswordKey(slot, key, sizeof(key));
    err = nvs_set_str(nvs_handle, key, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Credentials saved to slot %d", slot);
    return true;
}

bool NVSStorage::loadWiFiCredentials(int index, char* ssid, char* password, size_t max_len) {
    if (index < 0 || index >= WIFI_MAX_STORED_NETWORKS) {
        return false;
    }

    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }

    // Load SSID
    char key[16];
    getSSIDKey(index, key, sizeof(key));
    size_t len = max_len;
    err = nvs_get_str(nvs_handle, key, ssid, &len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return false;
    }

    // Load password
    getPasswordKey(index, key, sizeof(key));
    len = max_len;
    err = nvs_get_str(nvs_handle, key, password, &len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);
    return true;
}

int NVSStorage::getStoredNetworkCount() {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return 0;
    }

    int count = 0;
    char ssid[33];
    char key[16];

    for (int i = 0; i < WIFI_MAX_STORED_NETWORKS; i++) {
        getSSIDKey(i, key, sizeof(key));
        size_t len = sizeof(ssid);
        err = nvs_get_str(nvs_handle, key, ssid, &len);
        if (err == ESP_OK) {
            count++;
        }
    }

    nvs_close(nvs_handle);
    return count;
}

bool NVSStorage::clearWiFiCredentials() {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    // Erase all entries
    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // Commit changes
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "All WiFi credentials cleared");
    return (err == ESP_OK);
}

// Generic key-value storage

bool NVSStorage::putString(const char* namespace_name, const char* key, const char* value) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set string: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return (err == ESP_OK);
}

bool NVSStorage::getString(const char* namespace_name, const char* key, char* value, size_t max_len) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(namespace_name, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t len = max_len;
    err = nvs_get_str(nvs_handle, key, value, &len);
    nvs_close(nvs_handle);

    return (err == ESP_OK);
}

bool NVSStorage::putInt(const char* namespace_name, const char* key, int32_t value) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_i32(nvs_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set int: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return (err == ESP_OK);
}

int32_t NVSStorage::getInt(const char* namespace_name, const char* key, int32_t default_value) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(namespace_name, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return default_value;
    }

    int32_t value;
    err = nvs_get_i32(nvs_handle, key, &value);
    nvs_close(nvs_handle);

    return (err == ESP_OK) ? value : default_value;
}

bool NVSStorage::eraseKey(const char* namespace_name, const char* key) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_erase_key(nvs_handle, key);
    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return (err == ESP_OK);
}

bool NVSStorage::eraseNamespace(const char* namespace_name) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(namespace_name, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }

    err = nvs_erase_all(nvs_handle);
    if (err == ESP_OK) {
        nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return (err == ESP_OK);
}

// Private helper methods

void NVSStorage::getSSIDKey(int index, char* key, size_t max_len) {
    snprintf(key, max_len, "ssid%d", index);
}

void NVSStorage::getPasswordKey(int index, char* key, size_t max_len) {
    snprintf(key, max_len, "pass%d", index);
}
