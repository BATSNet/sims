/**
 * BLE GATT Service - Mesh Bridge
 *
 * Allows phones/devices to connect via BLE and send data through LoRa mesh
 * Features:
 * - 5 GATT characteristics (Incident TX, Mesh RX, Status, Config, Media)
 * - Multi-connection support (up to 9 devices)
 * - BLE ↔ LoRa bridging
 * - Media chunking protocol
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "config.h"

// Forward declarations
class LoRaTransport;
class MeshProtocol;
struct GPSLocation;

class SIMSBLEService {
public:
    SIMSBLEService();
    ~SIMSBLEService();

    // Lifecycle
    bool begin(LoRaTransport* loraTransport = nullptr, MeshProtocol* meshProtocol = nullptr);
    void end();
    void update();  // Call in main loop

    // Set GPS service for incident enrichment
    void setGPSLocation(const GPSLocation* location);

    // Forward mesh messages to BLE clients
    bool notifyMeshMessage(const uint8_t* data, size_t dataSize);

    // Status
    bool isActive();
    int getConnectedClientCount();

    // Status update for characteristic
    struct DeviceStatus {
        float latitude;
        float longitude;
        int meshNodes;
        int batteryPercent;
        bool gpsValid;
    };
    void updateStatus(const DeviceStatus& status);

private:
    bool active;
    int connectedClients;

    LoRaTransport* loraTransport;
    MeshProtocol* meshProtocol;
    const GPSLocation* gpsLocation;

    // BLE objects
    NimBLEServer* pServer;
    NimBLEService* pService;
    NimBLECharacteristic* pIncidentTxChar;   // Write - receive incidents from app
    NimBLECharacteristic* pMeshRxChar;       // Notify - forward mesh messages to app
    NimBLECharacteristic* pStatusChar;       // Read/Notify - device status
    NimBLECharacteristic* pConfigChar;       // Read/Write - device configuration
    NimBLECharacteristic* pMediaChar;        // Write - chunked media transfer

    // Media chunking state
    struct MediaChunk {
        uint8_t sequenceNumber;
        uint8_t totalChunks;
        uint32_t totalSize;
        uint8_t data[504];  // 512 - 8 byte header
    };

    std::vector<uint8_t> mediaBuffer;
    int expectedChunks;
    int receivedChunks;

    // Callbacks
    friend class SIMSBLEServiceServerCallbacks;
    friend class SIMSBLEServiceCharacteristicCallbacks;

    void handleIncidentTx(const std::string& value);
    void handleConfigWrite(const std::string& value);
    void handleMediaChunk(const std::string& value);

    bool sendIncidentToMesh(const JsonDocument& incidentDoc);
};

// BLE Server callbacks
class SIMSBLEServiceServerCallbacks : public NimBLEServerCallbacks {
public:
    SIMSBLEServiceServerCallbacks(SIMSBLEService* service) : bleService(service) {}

    void onConnect(NimBLEServer* pServer) override;
    void onDisconnect(NimBLEServer* pServer) override;

private:
    SIMSBLEService* bleService;
};

// BLE Characteristic callbacks
class SIMSBLEServiceCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
public:
    SIMSBLEServiceCharacteristicCallbacks(SIMSBLEService* service) : bleService(service) {}

    void onWrite(NimBLECharacteristic* pCharacteristic) override;

private:
    SIMSBLEService* bleService;
};

#endif // BLE_SERVICE_H
