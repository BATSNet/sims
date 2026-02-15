/**
 * Meshtastic BLE Service
 * Full Bluetooth LE GATT service compatible with Meshtastic app
 * Makes SIMS device appear as a Meshtastic node in the app
 *
 * Protocol flow (from Meshtastic firmware source):
 * 1. Phone connects and subscribes to FromNum
 * 2. Phone writes want_config_id to ToRadio
 * 3. Phone polls by reading FromRadio repeatedly
 * 4. Each read returns next config message from state machine
 * 5. After config_complete_id, steady state begins
 *
 * CRITICAL: FromRadio is READ-ONLY, NOT NOTIFY
 * "Adding NIMBLE_PROPERTY::NOTIFY to FromRadioCharacteristic appears to break things"
 */

#ifndef MESHTASTIC_BLE_H
#define MESHTASTIC_BLE_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "lora_transport.h"
#include "mesh/mesh_protocol.h"

// Meshtastic BLE Service UUIDs (from official firmware)
#define MESHTASTIC_SERVICE_UUID "6ba1b218-15a8-461f-9fa8-5dcae273eafd"
#define TORADIO_UUID            "f75c76d2-129e-4dad-a1dd-7866124401e7"
#define FROMRADIO_UUID          "2c55e69e-4993-11ed-b878-0242ac120002"
#define FROMNUM_UUID            "ed9da18c-a800-4f66-a670-aa7547e34453"

// Config state machine - app assumes this exact sequence
enum ConfigState {
    STATE_SEND_NOTHING = 0,     // Waiting for want_config_id
    STATE_SEND_MY_INFO,         // Send MyNodeInfo
    STATE_SEND_OWN_NODEINFO,    // Send our NodeInfo
    STATE_SEND_COMPLETE_ID,     // Send config_complete_id
    STATE_SEND_PACKETS,         // Steady state - real packets
};

class MeshtasticBLE {
public:
    MeshtasticBLE();

    bool begin(const char* deviceName, LoRaTransport* lora, MeshProtocol* mesh);
    void update();

    // Notify app of new data via FromNum
    void notifyFromNum();

    // Connection status
    bool isConnected();
    int getConnectedCount();

private:
    NimBLEServer* bleServer;
    NimBLEService* meshtasticService;
    NimBLECharacteristic* toRadioChar;
    NimBLECharacteristic* fromRadioChar;
    NimBLECharacteristic* fromNumChar;

    LoRaTransport* loraTransport;
    MeshProtocol* meshProtocol;

    char storedDeviceName[16];  // Unique BLE name (e.g. "SIMS-A4D3")
    char storedShortName[5];    // 4-char short name (e.g. "A4D3")

    bool initialized;
    int connectedClients;

    // Config state machine
    ConfigState configState;
    uint32_t configNonce;       // Nonce from want_config_id
    uint32_t fromNum;           // Monotonically incrementing counter

    // Message queue for steady-state packets
    static const int MAX_QUEUE = 10;
    uint8_t messageQueue[MAX_QUEUE][256];
    size_t messageQueueLen[MAX_QUEUE];
    int queueHead;
    int queueTail;
    int queueCount;

    // Get next FromRadio message based on state machine
    // Returns encoded bytes in buffer, or 0 if nothing to send
    size_t getFromRadio(uint8_t* buffer, size_t maxLen);

    // Handle incoming ToRadio write from app
    void handleToRadio(const uint8_t* data, size_t len);

    friend class ToRadioCallbacks;
    friend class ServerCallbacks;
    friend class FromNumCallbacks;
    friend class FromRadioCallbacks;
};

// BLE Callbacks
class ToRadioCallbacks : public NimBLECharacteristicCallbacks {
public:
    ToRadioCallbacks(MeshtasticBLE* parent) : meshtasticBLE(parent) {}
    void onWrite(NimBLECharacteristic* pCharacteristic) override;
private:
    MeshtasticBLE* meshtasticBLE;
};

class ServerCallbacks : public NimBLEServerCallbacks {
public:
    ServerCallbacks(MeshtasticBLE* parent) : meshtasticBLE(parent) {}
    void onConnect(NimBLEServer* pServer) override;
    void onDisconnect(NimBLEServer* pServer) override;
private:
    MeshtasticBLE* meshtasticBLE;
};

class FromNumCallbacks : public NimBLECharacteristicCallbacks {
public:
    FromNumCallbacks(MeshtasticBLE* parent) : meshtasticBLE(parent) {}
    void onSubscribe(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc, uint16_t subValue) override;
private:
    MeshtasticBLE* meshtasticBLE;
};

class FromRadioCallbacks : public NimBLECharacteristicCallbacks {
public:
    FromRadioCallbacks(MeshtasticBLE* parent) : meshtasticBLE(parent) {}
    void onRead(NimBLECharacteristic* pCharacteristic, ble_gap_conn_desc* desc) override;
private:
    MeshtasticBLE* meshtasticBLE;
};

#endif // MESHTASTIC_BLE_H
