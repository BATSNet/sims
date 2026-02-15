/**
 * Meshtastic BLE Service
 * Full Bluetooth LE GATT service compatible with Meshtastic app
 * ESP-IDF version using native NimBLE API
 *
 * Protocol flow:
 * 1. Phone connects and subscribes to FromNum
 * 2. Phone writes want_config_id to ToRadio
 * 3. Phone polls by reading FromRadio repeatedly
 * 4. Each read returns next config message from state machine
 * 5. After config_complete_id, steady state begins
 *
 * CRITICAL: FromRadio is READ-ONLY, NOT NOTIFY
 */

#ifndef MESHTASTIC_BLE_H
#define MESHTASTIC_BLE_H

#include <stdint.h>
#include <stddef.h>
#include "lora_transport.h"
#include "mesh/mesh_protocol.h"

// Config state machine - app assumes this exact sequence
enum ConfigState {
    STATE_SEND_NOTHING = 0,
    STATE_SEND_MY_INFO,
    STATE_SEND_OWN_NODEINFO,
    STATE_SEND_CHANNELS,
    STATE_SEND_COMPLETE_ID,
    STATE_SEND_PACKETS,
};

class MeshtasticBLE {
public:
    MeshtasticBLE();

    bool begin(const char* deviceName, LoRaTransport* lora, MeshProtocol* mesh);
    void update();

    void notifyFromNum();

    bool isConnected();
    int getConnectedCount();

    // Public for access from C callbacks
    size_t getFromRadio(uint8_t* buffer, size_t maxLen);
    void handleToRadio(const uint8_t* data, size_t len);

    void onConnect();
    void onDisconnect();
    void onFromNumSubscribe(bool subscribed);

    // Queue a received LoRa payload for the BLE client (legacy, calls queueRawMeshPacket)
    void queueReceivedPayload(const uint8_t* payload, size_t len, uint32_t fromNodeId);

    // Queue a raw Meshtastic MeshPacket for the BLE client to read via FromRadio.
    // Wraps in FromRadio { packet: <raw MeshPacket bytes> }
    void queueRawMeshPacket(const uint8_t* meshPacketData, size_t meshPacketLen);

    LoRaTransport* loraTransport;
    MeshProtocol* meshProtocol;

    char storedDeviceName[16];  // Unique BLE name (e.g. "SIMS-A4D3")
    char storedShortName[5];    // 4-char short name (e.g. "A4D3")

    bool initialized;
    int connectedClients;
    uint16_t connHandle;

    ConfigState configState;
    uint32_t configNonce;
    uint32_t fromNum;
    int channelIndex;  // Tracks which channel to send next

    // Message queue
    static const int MAX_QUEUE = 10;
    uint8_t messageQueue[MAX_QUEUE][256];
    size_t messageQueueLen[MAX_QUEUE];
    int queueHead;
    int queueTail;
    int queueCount;

    // GATT attribute handles
    uint16_t fromNumValHandle;
    uint16_t fromRadioValHandle;
};

#endif // MESHTASTIC_BLE_H
