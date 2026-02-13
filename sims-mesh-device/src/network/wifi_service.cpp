/**
 * WiFi Service Implementation
 */

#include "network/wifi_service.h"

WiFiService::WiFiService() :
    connected(false),
    lastReconnectAttempt(0),
    reconnectInterval(WIFI_RECONNECT_INTERVAL),
    reconnectAttempts(0)
{
}

WiFiService::~WiFiService() {
    end();
}

bool WiFiService::begin() {
    Serial.println("[WiFi] Initializing WiFi service...");

    // Set WiFi mode to station (client)
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnect ourselves

    // Initialize NVS for credential storage
    if (!preferences.begin("wifi-creds", false)) {
        Serial.println("[WiFi] ERROR: Failed to initialize NVS");
        return false;
    }

    Serial.println("[WiFi] WiFi service initialized");

    // Try to connect to stored networks
    if (tryStoredNetworks()) {
        Serial.println("[WiFi] Connected to stored network");
        connected = true;
        return true;
    }

    Serial.println("[WiFi] No stored networks available");
    return false;  // No networks available, will need BLE config
}

void WiFiService::update() {
    // Check if we're still connected
    if (WiFi.status() == WL_CONNECTED) {
        if (!connected) {
            connected = true;
            reconnectAttempts = 0;
            Serial.printf("[WiFi] Connected to %s\n", WiFi.SSID().c_str());
            Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
        }
    } else {
        if (connected) {
            connected = false;
            Serial.println("[WiFi] Connection lost");
        }
        handleReconnect();
    }
}

void WiFiService::end() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    preferences.end();
    connected = false;
}

bool WiFiService::connect(const char* ssid, const char* password, bool saveCredentials) {
    Serial.printf("[WiFi] Connecting to %s...\n", ssid);

    WiFi.begin(ssid, password);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECT_TIMEOUT) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        connected = true;
        reconnectAttempts = 0;

        if (saveCredentials) {
            this->saveCredentials(ssid, password);
        }

        return true;
    } else {
        Serial.println("[WiFi] Connection failed");
        connected = false;
        return false;
    }
}

bool WiFiService::disconnect() {
    WiFi.disconnect();
    connected = false;
    return true;
}

bool WiFiService::isConnected() {
    return connected && (WiFi.status() == WL_CONNECTED);
}

int WiFiService::scanNetworks() {
    Serial.println("[WiFi] Scanning for networks...");
    int n = WiFi.scanNetworks();
    Serial.printf("[WiFi] Found %d networks\n", n);
    return n;
}

String WiFiService::getScannedSSID(int index) {
    return WiFi.SSID(index);
}

int WiFiService::getScannedRSSI(int index) {
    return WiFi.RSSI(index);
}

bool WiFiService::getScannedEncryption(int index) {
    return WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
}

IPAddress WiFiService::getLocalIP() {
    return WiFi.localIP();
}

int WiFiService::getRSSI() {
    return WiFi.RSSI();
}

const char* WiFiService::getSSID() {
    static String ssid;  // Static to keep the string alive
    ssid = WiFi.SSID();
    return ssid.c_str();
}

bool WiFiService::saveCredentials(const char* ssid, const char* password) {
    // Find empty slot or oldest entry
    int slot = getStoredNetworkCount();
    if (slot >= WIFI_MAX_STORED_NETWORKS) {
        slot = 0;  // Overwrite oldest
    }

    // Check if this SSID is already stored
    for (int i = 0; i < WIFI_MAX_STORED_NETWORKS; i++) {
        String storedSSID;
        String storedPassword;
        if (loadCredentials(i, storedSSID, storedPassword)) {
            if (storedSSID.equals(ssid)) {
                slot = i;  // Update existing entry
                break;
            }
        }
    }

    String ssidKey = getSSIDKey(slot);
    String passwordKey = getPasswordKey(slot);

    preferences.putString(ssidKey.c_str(), ssid);
    preferences.putString(passwordKey.c_str(), password);

    Serial.printf("[WiFi] Credentials saved to slot %d\n", slot);
    return true;
}

bool WiFiService::loadCredentials(int index, String& ssid, String& password) {
    if (index < 0 || index >= WIFI_MAX_STORED_NETWORKS) {
        return false;
    }

    String ssidKey = getSSIDKey(index);
    String passwordKey = getPasswordKey(index);

    ssid = preferences.getString(ssidKey.c_str(), "");
    password = preferences.getString(passwordKey.c_str(), "");

    return !ssid.isEmpty();
}

int WiFiService::getStoredNetworkCount() {
    int count = 0;
    for (int i = 0; i < WIFI_MAX_STORED_NETWORKS; i++) {
        String ssid;
        String password;
        if (loadCredentials(i, ssid, password)) {
            count++;
        }
    }
    return count;
}

bool WiFiService::clearCredentials() {
    preferences.clear();
    Serial.println("[WiFi] All credentials cleared");
    return true;
}

bool WiFiService::tryStoredNetworks() {
    Serial.println("[WiFi] Trying stored networks...");

    for (int i = 0; i < WIFI_MAX_STORED_NETWORKS; i++) {
        String ssid;
        String password;
        if (loadCredentials(i, ssid, password)) {
            Serial.printf("[WiFi] Trying network %d: %s\n", i, ssid.c_str());
            if (connect(ssid.c_str(), password.c_str(), false)) {
                return true;
            }
        }
    }

    return false;
}

void WiFiService::handleReconnect() {
    unsigned long now = millis();

    // Check if it's time to attempt reconnection
    if (now - lastReconnectAttempt >= getBackoffInterval()) {
        lastReconnectAttempt = now;
        reconnectAttempts++;

        Serial.printf("[WiFi] Reconnect attempt %d...\n", reconnectAttempts);

        if (!tryStoredNetworks()) {
            Serial.println("[WiFi] Reconnection failed");
        }
    }
}

unsigned long WiFiService::getBackoffInterval() {
    // Exponential backoff: 30s, 60s, 120s, 240s, max 300s (5 min)
    unsigned long interval = WIFI_RECONNECT_INTERVAL * (1 << min(reconnectAttempts, 4));
    return min(interval, 300000UL);  // Cap at 5 minutes
}

String WiFiService::getSSIDKey(int index) {
    return "ssid" + String(index);
}

String WiFiService::getPasswordKey(int index) {
    return "pass" + String(index);
}
