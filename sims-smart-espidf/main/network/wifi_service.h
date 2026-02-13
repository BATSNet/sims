/**
 * WiFi Service - ESP-IDF Port
 *
 * Event-driven WiFi management with:
 * - Auto-reconnect with exponential backoff
 * - NVS storage for credentials
 * - RSSI monitoring
 * - Network scanning
 */

#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi connection state
typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

// WiFi service API
class WiFiService {
public:
    WiFiService();
    ~WiFiService();

    // Lifecycle
    bool begin();
    void update();  // Call in main loop for status monitoring
    void end();

    // Connection management
    bool connect(const char* ssid, const char* password, bool saveCredentials = true);
    bool disconnect();
    bool isConnected();

    // Network scanning
    int scanNetworks();  // Returns number of networks found
    const char* getScannedSSID(int index);
    int getScannedRSSI(int index);
    bool getScannedEncryption(int index);

    // Status
    const char* getLocalIP();
    int getRSSI();  // Current network RSSI
    const char* getSSID();  // Current network SSID
    wifi_state_t getState();

    // Credential management (NVS)
    bool saveCredentials(const char* ssid, const char* password);
    bool loadCredentials(int index, char* ssid, char* password, size_t max_len);
    int getStoredNetworkCount();
    bool clearCredentials();
    bool tryStoredNetworks();  // Try all stored networks until one connects

private:
    // Connection state
    wifi_state_t state;
    unsigned long lastReconnectAttempt;
    unsigned long reconnectInterval;
    int reconnectAttempts;
    int retryCount;

    // Current network info
    char currentSSID[33];
    char localIP[16];

    // Scan results
    wifi_ap_record_t* scanResults;
    uint16_t scanResultCount;

    // Auto-reconnect logic
    void handleReconnect();
    unsigned long getBackoffInterval();  // Exponential backoff

    // Event handling
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
    static void ip_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

    // Internal connection
    esp_err_t connectInternal(const char* ssid, const char* password);
};

#ifdef __cplusplus
}
#endif

#endif // WIFI_SERVICE_H
