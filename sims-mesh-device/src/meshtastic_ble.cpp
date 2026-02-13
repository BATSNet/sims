/**
 * Meshtastic BLE Service Implementation
 * Makes SIMS device appear as Meshtastic node in app
 *
 * Implements the official Meshtastic BLE protocol:
 * - Phone writes want_config_id to ToRadio
 * - Phone polls FromRadio repeatedly (READ only, no NOTIFY)
 * - State machine returns config messages in required order
 * - FromNum notifies phone when new data is available
 */

#include "meshtastic_ble.h"
#include "meshtastic_encoder.h"
#include "meshtastic_test.h"
#include <string.h>
#include <pb_decode.h>
#include "meshtastic/mesh.pb.h"

MeshtasticBLE::MeshtasticBLE()
    : bleServer(nullptr), meshtasticService(nullptr),
      toRadioChar(nullptr), fromRadioChar(nullptr), fromNumChar(nullptr),
      loraTransport(nullptr), meshProtocol(nullptr),
      initialized(false), connectedClients(0),
      configState(STATE_SEND_NOTHING), configNonce(0), fromNum(0),
      queueHead(0), queueTail(0), queueCount(0) {
    memset(messageQueue, 0, sizeof(messageQueue));
    memset(messageQueueLen, 0, sizeof(messageQueueLen));
}

bool MeshtasticBLE::begin(const char* deviceName, LoRaTransport* lora, MeshProtocol* mesh) {
    loraTransport = lora;
    meshProtocol = mesh;

    Serial.println("[Meshtastic BLE] Initializing...");

    // Initialize NimBLE
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    // Create BLE Server
    bleServer = NimBLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks(this));

    // Create Meshtastic Service
    meshtasticService = bleServer->createService(MESHTASTIC_SERVICE_UUID);

    // ToRadio characteristic - phone WRITES here
    toRadioChar = meshtasticService->createCharacteristic(
        TORADIO_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    toRadioChar->setCallbacks(new ToRadioCallbacks(this));

    // FromRadio characteristic - phone READS here (NOT NOTIFY!)
    // "Adding NIMBLE_PROPERTY::NOTIFY appears to break things"
    fromRadioChar = meshtasticService->createCharacteristic(
        FROMRADIO_UUID,
        NIMBLE_PROPERTY::READ
    );
    fromRadioChar->setCallbacks(new FromRadioCallbacks(this));

    // FromNum characteristic - NOTIFY to signal new data available
    fromNumChar = meshtasticService->createCharacteristic(
        FROMNUM_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    fromNumChar->setCallbacks(new FromNumCallbacks(this));

    // Start service
    meshtasticService->start();

    // Start advertising
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(MESHTASTIC_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->start();

    initialized = true;
    Serial.printf("[Meshtastic BLE] Service started, advertising as: %s\n", deviceName);

    return true;
}

void MeshtasticBLE::update() {
    // Nothing needed - everything is event-driven via BLE callbacks
}

void MeshtasticBLE::notifyFromNum() {
    if (!initialized || connectedClients == 0) {
        return;
    }

    fromNum++;
    uint8_t val[4];
    val[0] = fromNum & 0xFF;
    val[1] = (fromNum >> 8) & 0xFF;
    val[2] = (fromNum >> 16) & 0xFF;
    val[3] = (fromNum >> 24) & 0xFF;

    fromNumChar->setValue(val, 4);
    fromNumChar->notify();
    Serial.printf("[Meshtastic BLE] FromNum notify: %u\n", fromNum);
}

bool MeshtasticBLE::isConnected() {
    return connectedClients > 0;
}

int MeshtasticBLE::getConnectedCount() {
    return connectedClients;
}

// State machine: returns next FromRadio message based on config state
size_t MeshtasticBLE::getFromRadio(uint8_t* buffer, size_t maxLen) {
    uint32_t deviceId = meshProtocol ? meshProtocol->getDeviceId() : 0xed020f3c;
    size_t len = 0;

    switch (configState) {
        case STATE_SEND_MY_INFO:
            len = buildFromRadio_MyNodeInfo(buffer, maxLen, deviceId);
            if (len > 0) {
                configState = STATE_SEND_OWN_NODEINFO;
                Serial.printf("[State] MY_INFO sent (%zu bytes), next: OWN_NODEINFO\n", len);
            }
            break;

        case STATE_SEND_OWN_NODEINFO:
            len = buildFromRadio_NodeInfo(buffer, maxLen, deviceId, "SIMS-MESH", "SIMS");
            if (len > 0) {
                configState = STATE_SEND_COMPLETE_ID;
                Serial.printf("[State] OWN_NODEINFO sent (%zu bytes), next: COMPLETE_ID\n", len);
            }
            break;

        case STATE_SEND_COMPLETE_ID:
            len = buildFromRadio_ConfigComplete(buffer, maxLen);
            if (len > 0) {
                configState = STATE_SEND_PACKETS;
                Serial.printf("[State] CONFIG_COMPLETE sent (%zu bytes), entering steady state\n", len);
            }
            break;

        case STATE_SEND_PACKETS:
            // Steady state - return queued packets or empty
            if (queueCount > 0) {
                len = messageQueueLen[queueHead];
                memcpy(buffer, messageQueue[queueHead], len);
                queueHead = (queueHead + 1) % MAX_QUEUE;
                queueCount--;
                Serial.printf("[State] Packet sent (%zu bytes), %d remaining\n", len, queueCount);
            }
            // Return 0 if no packets - phone reads until empty
            break;

        case STATE_SEND_NOTHING:
        default:
            // Nothing to send yet
            break;
    }

    return len;
}

void MeshtasticBLE::handleToRadio(const uint8_t* data, size_t len) {
    if (len < 1) return;

    Serial.printf("[Meshtastic BLE] ToRadio: %d bytes, hex: ", len);
    for (size_t i = 0; i < len && i < 20; i++) {
        Serial.printf("%02x", data[i]);
    }
    Serial.println();

    // Decode ToRadio protobuf
    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data, len);
    bool decoded = pb_decode(&stream, meshtastic_ToRadio_fields, &toRadio);

    if (!decoded) {
        Serial.printf("[Meshtastic BLE] ToRadio decode failed: %s\n", PB_GET_ERROR(&stream));
        return;
    }

    if (toRadio.which_payload_variant == meshtastic_ToRadio_want_config_id_tag) {
        configNonce = toRadio.payload_variant.want_config_id;
        Serial.printf("[Meshtastic BLE] WANT_CONFIG received, nonce=%u\n", configNonce);

        // Start config state machine
        configState = STATE_SEND_MY_INFO;

        // Reset queue
        queueHead = 0;
        queueTail = 0;
        queueCount = 0;

        // Signal phone that data is ready to read
        notifyFromNum();

    } else if (toRadio.which_payload_variant == meshtastic_ToRadio_packet_tag) {
        Serial.println("[Meshtastic BLE] Mesh packet from app");
        // Forward to LoRa if needed
    } else {
        Serial.printf("[Meshtastic BLE] Unknown ToRadio variant: %d\n", toRadio.which_payload_variant);
    }
}

// --- BLE Callback implementations ---

void ToRadioCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        meshtasticBLE->handleToRadio((const uint8_t*)value.data(), value.length());
    }
}

void ServerCallbacks::onConnect(NimBLEServer* pServer) {
    meshtasticBLE->connectedClients++;
    Serial.printf("[Meshtastic BLE] Client connected (total: %d)\n",
                  meshtasticBLE->connectedClients);

    // Reset state on new connection
    meshtasticBLE->configState = STATE_SEND_NOTHING;
    meshtasticBLE->configNonce = 0;
}

void ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
    meshtasticBLE->connectedClients--;
    Serial.printf("[Meshtastic BLE] Client disconnected (total: %d)\n",
                  meshtasticBLE->connectedClients);

    // Reset state
    meshtasticBLE->configState = STATE_SEND_NOTHING;

    // Restart advertising
    NimBLEDevice::getAdvertising()->start();
}

void FromNumCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic,
                                    ble_gap_conn_desc* desc, uint16_t subValue) {
    if (subValue > 0) {
        Serial.println("[Meshtastic BLE] Client subscribed to FromNum - waiting for want_config_id");
        // Don't send anything - wait for phone to write want_config_id to ToRadio
    } else {
        Serial.println("[Meshtastic BLE] Client unsubscribed from FromNum");
    }
}

void FromRadioCallbacks::onRead(NimBLECharacteristic* pCharacteristic,
                                 ble_gap_conn_desc* desc) {
    uint8_t buffer[256];
    size_t len = meshtasticBLE->getFromRadio(buffer, sizeof(buffer));

    if (len > 0) {
        pCharacteristic->setValue(buffer, len);
        Serial.printf("[Meshtastic BLE] FromRadio read: %zu bytes\n", len);
    } else {
        // Return empty - signals "no more data" to phone
        pCharacteristic->setValue((uint8_t*)"", 0);
        Serial.println("[Meshtastic BLE] FromRadio read: empty (no data)");
    }
}
