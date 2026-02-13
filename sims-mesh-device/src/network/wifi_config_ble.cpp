/**
 * WiFi Configuration via BLE Implementation
 */

#include "network/wifi_config_ble.h"

WiFiConfigBLE::WiFiConfigBLE(WiFiService* wifiService) :
    wifiService(wifiService),
    active(false),
    clientConnected(false),
    pServer(nullptr),
    pService(nullptr),
    pSSIDCharacteristic(nullptr),
    pPasswordCharacteristic(nullptr),
    pStatusCharacteristic(nullptr),
    credentialsReceived(false)
{
}

WiFiConfigBLE::~WiFiConfigBLE() {
    end();
}

bool WiFiConfigBLE::begin() {
    Serial.println("[BLE-WiFi] Starting WiFi configuration BLE service...");

    // Initialize NimBLE
    NimBLEDevice::init(BLE_DEVICE_NAME);

    // Create BLE server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new WiFiConfigServerCallbacks(this));

    // Create WiFi config service
    pService = pServer->createService(BLE_WIFI_CONFIG_SERVICE_UUID);

    // SSID characteristic (write)
    pSSIDCharacteristic = pService->createCharacteristic(
        BLE_WIFI_SSID_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pSSIDCharacteristic->setCallbacks(new WiFiConfigCharacteristicCallbacks(this));

    // Password characteristic (write)
    pPasswordCharacteristic = pService->createCharacteristic(
        BLE_WIFI_PASS_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pPasswordCharacteristic->setCallbacks(new WiFiConfigCharacteristicCallbacks(this));

    // Status characteristic (read/notify)
    pStatusCharacteristic = pService->createCharacteristic(
        BLE_WIFI_CONFIG_SERVICE_UUID,  // Reuse service UUID for status
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Start service
    pService->start();

    // Start advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_WIFI_CONFIG_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();

    active = true;
    updateStatus("Ready for WiFi config");

    Serial.println("[BLE-WiFi] BLE service started, advertising...");
    return true;
}

void WiFiConfigBLE::end() {
    if (!active) return;

    Serial.println("[BLE-WiFi] Stopping BLE service...");

    if (pServer) {
        NimBLEDevice::getAdvertising()->stop();
        pServer->disconnect(0);  // Disconnect all clients
    }

    NimBLEDevice::deinit(true);

    active = false;
    clientConnected = false;

    Serial.println("[BLE-WiFi] BLE service stopped");
}

void WiFiConfigBLE::update() {
    if (!active) return;

    // If credentials received, try to connect
    if (credentialsReceived) {
        credentialsReceived = false;

        Serial.printf("[BLE-WiFi] Attempting to connect to %s...\n", pendingSSID.c_str());
        updateStatus("Connecting...");

        if (wifiService->connect(pendingSSID.c_str(), pendingPassword.c_str(), true)) {
            updateStatus("Connected!");
            Serial.println("[BLE-WiFi] WiFi connected, will disable BLE in 5s...");
            delay(5000);  // Give client time to read status
            end();  // Auto-disable BLE to save power
        } else {
            updateStatus("Connection failed");
            Serial.println("[BLE-WiFi] WiFi connection failed");
        }

        // Clear credentials from memory
        pendingSSID = "";
        pendingPassword = "";
    }

    // Auto-disable if WiFi is connected (may have connected via stored credentials)
    if (wifiService->isConnected() && active) {
        Serial.println("[BLE-WiFi] WiFi connected, disabling BLE...");
        updateStatus("WiFi connected, BLE disabled");
        delay(2000);
        end();
    }
}

bool WiFiConfigBLE::isActive() {
    return active;
}

bool WiFiConfigBLE::isClientConnected() {
    return clientConnected;
}

void WiFiConfigBLE::handleSSIDWrite(const std::string& value) {
    pendingSSID = String(value.c_str());
    Serial.printf("[BLE-WiFi] SSID received: %s\n", pendingSSID.c_str());
}

void WiFiConfigBLE::handlePasswordWrite(const std::string& value) {
    pendingPassword = String(value.c_str());
    Serial.println("[BLE-WiFi] Password received");

    // Both SSID and password received
    if (!pendingSSID.isEmpty() && !pendingPassword.isEmpty()) {
        credentialsReceived = true;
    }
}

void WiFiConfigBLE::updateStatus(const std::string& status) {
    if (pStatusCharacteristic) {
        pStatusCharacteristic->setValue(status);
        pStatusCharacteristic->notify();
        Serial.printf("[BLE-WiFi] Status: %s\n", status.c_str());
    }
}

// Server callbacks
void WiFiConfigServerCallbacks::onConnect(NimBLEServer* pServer) {
    bleService->clientConnected = true;
    Serial.println("[BLE-WiFi] Client connected");
    bleService->updateStatus("Client connected");
}

void WiFiConfigServerCallbacks::onDisconnect(NimBLEServer* pServer) {
    bleService->clientConnected = false;
    Serial.println("[BLE-WiFi] Client disconnected");

    // Restart advertising
    NimBLEDevice::getAdvertising()->start();
}

// Characteristic callbacks
void WiFiConfigCharacteristicCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (pCharacteristic->getUUID().toString() == BLE_WIFI_SSID_UUID) {
        bleService->handleSSIDWrite(value);
    } else if (pCharacteristic->getUUID().toString() == BLE_WIFI_PASS_UUID) {
        bleService->handlePasswordWrite(value);
    }
}
