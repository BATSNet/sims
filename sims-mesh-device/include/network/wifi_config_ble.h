/**
 * WiFi Configuration via BLE
 *
 * Temporary BLE service for initial WiFi setup
 * Auto-disables once WiFi is connected (power saving)
 */

#ifndef WIFI_CONFIG_BLE_H
#define WIFI_CONFIG_BLE_H

#include <NimBLEDevice.h>
#include "network/wifi_service.h"
#include "config.h"

class WiFiConfigBLE {
public:
    WiFiConfigBLE(WiFiService* wifiService);
    ~WiFiConfigBLE();

    // Lifecycle
    bool begin();
    void end();
    void update();  // Call in main loop

    // Status
    bool isActive();
    bool isClientConnected();

private:
    WiFiService* wifiService;
    bool active;
    bool clientConnected;

    // BLE objects
    NimBLEServer* pServer;
    NimBLEService* pService;
    NimBLECharacteristic* pSSIDCharacteristic;
    NimBLECharacteristic* pPasswordCharacteristic;
    NimBLECharacteristic* pStatusCharacteristic;

    // Callbacks
    friend class WiFiConfigServerCallbacks;
    friend class WiFiConfigCharacteristicCallbacks;

    void handleSSIDWrite(const std::string& value);
    void handlePasswordWrite(const std::string& value);
    void updateStatus(const std::string& status);

    String pendingSSID;
    String pendingPassword;
    bool credentialsReceived;
};

// BLE Server callbacks
class WiFiConfigServerCallbacks : public NimBLEServerCallbacks {
public:
    WiFiConfigServerCallbacks(WiFiConfigBLE* service) : bleService(service) {}

    void onConnect(NimBLEServer* pServer) override;
    void onDisconnect(NimBLEServer* pServer) override;

private:
    WiFiConfigBLE* bleService;
};

// BLE Characteristic callbacks
class WiFiConfigCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
public:
    WiFiConfigCharacteristicCallbacks(WiFiConfigBLE* service) : bleService(service) {}

    void onWrite(NimBLECharacteristic* pCharacteristic) override;

private:
    WiFiConfigBLE* bleService;
};

#endif // WIFI_CONFIG_BLE_H
