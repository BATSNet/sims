/**
 * WiFi Service Implementation - ESP-IDF Port
 */

#include "network/wifi_service.h"
#include "storage/nvs_storage.h"
#include "config.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "lwip/inet.h"

static const char *TAG = "WiFi";

// Event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static WiFiService* s_instance = nullptr;  // For event callbacks

WiFiService::WiFiService() :
    state(WIFI_STATE_DISCONNECTED),
    lastReconnectAttempt(0),
    reconnectInterval(WIFI_RECONNECT_INTERVAL_MS),
    reconnectAttempts(0),
    retryCount(0),
    scanResults(nullptr),
    scanResultCount(0)
{
    memset(currentSSID, 0, sizeof(currentSSID));
    memset(localIP, 0, sizeof(localIP));
    s_instance = this;
}

WiFiService::~WiFiService() {
    end();
    s_instance = nullptr;
}

bool WiFiService::begin() {
    ESP_LOGI(TAG, "Initializing WiFi service...");

    // Create event group
    s_wifi_event_group = xEventGroupCreate();

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &ip_event_handler, this));

    // Set WiFi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi service initialized");

    // Try to connect to hardcoded credentials first
    if (connect(WIFI_SSID, WIFI_PASSWORD, true)) {
        ESP_LOGI(TAG, "Connected using hardcoded credentials");
        return true;
    }

    // Try to connect to stored networks
    if (tryStoredNetworks()) {
        ESP_LOGI(TAG, "Connected to stored network");
        return true;
    }

    ESP_LOGW(TAG, "No networks available, will retry in background");
    return false;
}

void WiFiService::update() {
    // Handle auto-reconnect (only when fully disconnected, not while connecting)
    if (state == WIFI_STATE_DISCONNECTED || state == WIFI_STATE_FAILED) {
        handleReconnect();
    }
}

void WiFiService::end() {
    if (scanResults) {
        free(scanResults);
        scanResults = nullptr;
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler);

    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = nullptr;
    }

    state = WIFI_STATE_DISCONNECTED;
}

bool WiFiService::connect(const char* ssid, const char* password, bool saveCredentials) {
    ESP_LOGI(TAG, "Connecting to %s...", ssid);

    state = WIFI_STATE_CONNECTING;
    retryCount = 0;

    esp_err_t err = connectInternal(ssid, password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start connection: %s", esp_err_to_name(err));
        state = WIFI_STATE_FAILED;
        return false;
    }

    // Wait for connection (blocking)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected! IP: %s", localIP);
        state = WIFI_STATE_CONNECTED;
        reconnectAttempts = 0;
        strncpy(currentSSID, ssid, sizeof(currentSSID) - 1);

        if (saveCredentials) {
            this->saveCredentials(ssid, password);
        }

        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Connection failed");
        state = WIFI_STATE_FAILED;
        return false;
    } else {
        ESP_LOGE(TAG, "Connection timeout");
        state = WIFI_STATE_FAILED;
        return false;
    }
}

esp_err_t WiFiService::connectInternal(const char* ssid, const char* password) {
    // Disconnect first if already connecting/connected to avoid ESP_ERR_WIFI_STATE
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(err));
        return err;
    }
    return esp_wifi_connect();
}

bool WiFiService::disconnect() {
    esp_wifi_disconnect();
    state = WIFI_STATE_DISCONNECTED;
    return true;
}

bool WiFiService::isConnected() {
    return state == WIFI_STATE_CONNECTED;
}

int WiFiService::scanNetworks() {
    ESP_LOGI(TAG, "Scanning for networks...");

    // Free previous scan results
    if (scanResults) {
        free(scanResults);
        scanResults = nullptr;
        scanResultCount = 0;
    }

    // Start scan
    wifi_scan_config_t scan_config = {
        .ssid = nullptr,
        .bssid = nullptr,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 100,
                .max = 300
            }
        }
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(err));
        return 0;
    }

    // Get scan results
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&scanResultCount));
    if (scanResultCount == 0) {
        ESP_LOGW(TAG, "No networks found");
        return 0;
    }

    scanResults = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * scanResultCount);
    if (!scanResults) {
        ESP_LOGE(TAG, "Failed to allocate scan results");
        scanResultCount = 0;
        return 0;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&scanResultCount, scanResults));
    ESP_LOGI(TAG, "Found %d networks", scanResultCount);

    return scanResultCount;
}

const char* WiFiService::getScannedSSID(int index) {
    if (index < 0 || index >= scanResultCount || !scanResults) {
        return "";
    }
    return (const char*)scanResults[index].ssid;
}

int WiFiService::getScannedRSSI(int index) {
    if (index < 0 || index >= scanResultCount || !scanResults) {
        return 0;
    }
    return scanResults[index].rssi;
}

bool WiFiService::getScannedEncryption(int index) {
    if (index < 0 || index >= scanResultCount || !scanResults) {
        return false;
    }
    return scanResults[index].authmode != WIFI_AUTH_OPEN;
}

const char* WiFiService::getLocalIP() {
    return localIP;
}

int WiFiService::getRSSI() {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}

const char* WiFiService::getSSID() {
    return currentSSID;
}

wifi_state_t WiFiService::getState() {
    return state;
}

bool WiFiService::saveCredentials(const char* ssid, const char* password) {
    return NVSStorage::saveWiFiCredentials(ssid, password);
}

bool WiFiService::loadCredentials(int index, char* ssid, char* password, size_t max_len) {
    return NVSStorage::loadWiFiCredentials(index, ssid, password, max_len);
}

int WiFiService::getStoredNetworkCount() {
    return NVSStorage::getStoredNetworkCount();
}

bool WiFiService::clearCredentials() {
    return NVSStorage::clearWiFiCredentials();
}

bool WiFiService::tryStoredNetworks() {
    ESP_LOGI(TAG, "Trying stored networks...");

    int count = getStoredNetworkCount();
    if (count == 0) {
        ESP_LOGW(TAG, "No stored networks");
        return false;
    }

    char ssid[33];
    char password[65];

    for (int i = 0; i < WIFI_MAX_STORED_NETWORKS; i++) {
        if (loadCredentials(i, ssid, password, sizeof(ssid))) {
            ESP_LOGI(TAG, "Trying network %d: %s", i, ssid);
            if (connect(ssid, password, false)) {
                return true;
            }
        }
    }

    return false;
}

void WiFiService::handleReconnect() {
    unsigned long now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Check if it's time to attempt reconnection (non-blocking)
    if (now - lastReconnectAttempt >= getBackoffInterval()) {
        lastReconnectAttempt = now;
        reconnectAttempts++;

        ESP_LOGI(TAG, "Reconnect attempt %d (non-blocking)...", reconnectAttempts);

        // Just kick off a connect - the event handler will update state
        retryCount = 0;
        state = WIFI_STATE_CONNECTING;
        esp_wifi_connect();
    }
}

unsigned long WiFiService::getBackoffInterval() {
    // Exponential backoff: 30s, 60s, 120s, 240s, max 300s (5 min)
    unsigned long interval = reconnectInterval * (1 << (reconnectAttempts < 4 ? reconnectAttempts : 4));
    return interval < 300000 ? interval : 300000;  // Cap at 5 minutes
}

// Static event handlers

void WiFiService::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
    WiFiService* self = (WiFiService*)arg;

    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi started");
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Connected to AP");
        self->state = WIFI_STATE_CONNECTED;
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
        ESP_LOGW(TAG, "Disconnected from AP, reason: %d", event->reason);

        if (self->retryCount < WIFI_MAX_RETRY) {
            self->state = WIFI_STATE_CONNECTING;
            esp_wifi_connect();
            self->retryCount++;
            ESP_LOGI(TAG, "Retry connection %d/%d", self->retryCount, WIFI_MAX_RETRY);
        } else {
            self->state = WIFI_STATE_FAILED;
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Max retries reached, will backoff");
        }
    }
}

void WiFiService::ip_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data) {
    WiFiService* self = (WiFiService*)arg;

    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        snprintf(self->localIP, sizeof(self->localIP), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", self->localIP);

        self->state = WIFI_STATE_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
