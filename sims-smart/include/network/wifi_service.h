/**
 * WiFi Service - Connection Management
 *
 * Manages WiFi connectivity with:
 * - Auto-reconnect with exponential backoff
 * - NVS storage for up to 5 network credentials
 * - RSSI monitoring
 * - Network scanning
 */

#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <WiFi.h>
#include <Preferences.h>
#include "../config.h"

class WiFiService {
public:
    WiFiService();
    ~WiFiService();

    // Lifecycle
    bool begin();
    void update();  // Call in main loop for auto-reconnect
    void end();

    // Connection management
    bool connect(const char* ssid, const char* password, bool saveCredentials = true);
    bool disconnect();
    bool isConnected();

    // Network scanning
    int scanNetworks();  // Returns number of networks found
    String getScannedSSID(int index);
    int getScannedRSSI(int index);
    bool getScannedEncryption(int index);

    // Status
    IPAddress getLocalIP();
    int getRSSI();  // Current network RSSI
    const char* getSSID();  // Current network SSID

    // Credential management (NVS)
    bool saveCredentials(const char* ssid, const char* password);
    bool loadCredentials(int index, String& ssid, String& password);
    int getStoredNetworkCount();
    bool clearCredentials();
    bool tryStoredNetworks();  // Try all stored networks until one connects

private:
    Preferences preferences;

    // Connection state
    bool connected;
    unsigned long lastReconnectAttempt;
    unsigned long reconnectInterval;
    int reconnectAttempts;

    // Auto-reconnect logic
    void handleReconnect();
    unsigned long getBackoffInterval();  // Exponential backoff

    // NVS key generation
    String getSSIDKey(int index);
    String getPasswordKey(int index);
};

#endif // WIFI_SERVICE_H
