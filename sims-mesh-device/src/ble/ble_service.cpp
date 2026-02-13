/**
 * BLE Service Implementation
 */

#include "ble/ble_service.h"
#include "lora_transport.h"
#include "mesh/mesh_protocol.h"

SIMSBLEService::SIMSBLEService() :
    active(false),
    connectedClients(0),
    loraTransport(nullptr),
    meshProtocol(nullptr),
    gpsLocation(nullptr),
    pServer(nullptr),
    pService(nullptr),
    pIncidentTxChar(nullptr),
    pMeshRxChar(nullptr),
    pStatusChar(nullptr),
    pConfigChar(nullptr),
    pMediaChar(nullptr),
    expectedChunks(0),
    receivedChunks(0)
{
}

SIMSBLEService::~SIMSBLEService() {
    end();
}

bool SIMSBLEService::begin(LoRaTransport* loraTransport, MeshProtocol* meshProtocol) {
    this->loraTransport = loraTransport;
    this->meshProtocol = meshProtocol;

    Serial.println("[BLE] Starting BLE GATT service...");

    // Initialize NimBLE
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setMTU(BLE_MTU_SIZE);

    // Create BLE server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new SIMSBLEServiceServerCallbacks(this));

    // Create main GATT service
    pService = pServer->createService(BLE_SERVICE_UUID);

    // Characteristic 1: Incident TX (write) - Receive incidents from app
    pIncidentTxChar = pService->createCharacteristic(
        BLE_CHAR_INCIDENT_TX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pIncidentTxChar->setCallbacks(new SIMSBLEServiceCharacteristicCallbacks(this));

    // Characteristic 2: Mesh RX (notify) - Forward mesh messages to app
    pMeshRxChar = pService->createCharacteristic(
        BLE_CHAR_MESH_RX_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Characteristic 3: Status (read/notify) - Device status
    pStatusChar = pService->createCharacteristic(
        BLE_CHAR_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Characteristic 4: Config (read/write) - Device configuration
    pConfigChar = pService->createCharacteristic(
        BLE_CHAR_CONFIG_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    pConfigChar->setCallbacks(new SIMSBLEServiceCharacteristicCallbacks(this));

    // Characteristic 5: Media Transfer (write) - Chunked media data
    pMediaChar = pService->createCharacteristic(
        BLE_CHAR_MEDIA_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pMediaChar->setCallbacks(new SIMSBLEServiceCharacteristicCallbacks(this));

    // Start service
    pService->start();

    // Start advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();

    active = true;

    Serial.println("[BLE] BLE service started, advertising...");
    return true;
}

void SIMSBLEService::end() {
    if (!active) return;

    Serial.println("[BLE] Stopping BLE service...");

    if (pServer) {
        NimBLEDevice::getAdvertising()->stop();
        pServer->disconnect(0);
    }

    NimBLEDevice::deinit(true);
    active = false;
    connectedClients = 0;

    Serial.println("[BLE] BLE service stopped");
}

void SIMSBLEService::update() {
    // Nothing to do in main loop for now
    // NimBLE handles everything in its own task
}

void SIMSBLEService::setGPSLocation(const GPSLocation* location) {
    this->gpsLocation = location;
}

bool SIMSBLEService::notifyMeshMessage(const uint8_t* data, size_t dataSize) {
    if (!active || connectedClients == 0) {
        return false;
    }

    if (pMeshRxChar == nullptr) {
        return false;
    }

    // Notify all connected clients
    pMeshRxChar->setValue(data, dataSize);
    pMeshRxChar->notify();

    Serial.printf("[BLE] Mesh message forwarded to BLE clients (%d bytes)\n", dataSize);
    return true;
}

bool SIMSBLEService::isActive() {
    return active;
}

int SIMSBLEService::getConnectedClientCount() {
    return connectedClients;
}

void SIMSBLEService::updateStatus(const DeviceStatus& status) {
    if (!active || pStatusChar == nullptr) {
        return;
    }

    // Build JSON status
    JsonDocument doc;
    doc["latitude"] = status.latitude;
    doc["longitude"] = status.longitude;
    doc["meshNodes"] = status.meshNodes;
    doc["battery"] = status.batteryPercent;
    doc["gpsValid"] = status.gpsValid;
    doc["timestamp"] = millis();

    String statusJson;
    serializeJson(doc, statusJson);

    pStatusChar->setValue(statusJson.c_str());

    // Notify if clients are connected
    if (connectedClients > 0) {
        pStatusChar->notify();
    }
}

void SIMSBLEService::handleIncidentTx(const std::string& value) {
    Serial.printf("[BLE] Incident received (%d bytes)\n", value.length());

    // Parse JSON incident
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, value);

    if (error) {
        Serial.printf("[BLE] ERROR: Failed to parse incident JSON: %s\n", error.c_str());
        return;
    }

    // Enrich with GPS if not provided
    if (!doc.containsKey("latitude") || !doc.containsKey("longitude")) {
        if (gpsLocation != nullptr && gpsLocation->valid) {
            doc["latitude"] = gpsLocation->latitude;
            doc["longitude"] = gpsLocation->longitude;
            doc["altitude"] = gpsLocation->altitude;
            Serial.println("[BLE] Incident enriched with device GPS");
        } else {
            Serial.println("[BLE] WARNING: No GPS available for incident");
        }
    }

    // Add device ID and timestamp if not present
    if (!doc.containsKey("deviceId")) {
        doc["deviceId"] = String((uint32_t)ESP.getEfuseMac(), HEX);
    }
    if (!doc.containsKey("timestamp")) {
        doc["timestamp"] = millis();
    }

    // Forward to mesh network
    if (sendIncidentToMesh(doc)) {
        Serial.println("[BLE] Incident forwarded to mesh network");
    } else {
        Serial.println("[BLE] ERROR: Failed to forward incident to mesh");
    }
}

void SIMSBLEService::handleConfigWrite(const std::string& value) {
    Serial.printf("[BLE] Config write received (%d bytes)\n", value.length());

    // Parse JSON config
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, value);

    if (error) {
        Serial.printf("[BLE] ERROR: Failed to parse config JSON: %s\n", error.c_str());
        return;
    }

    // Handle configuration changes
    // TODO: Implement config persistence and application
    // For now, just log what was received

    if (doc.containsKey("meshEnabled")) {
        Serial.printf("[BLE] Config: meshEnabled = %d\n", doc["meshEnabled"].as<bool>());
    }
    if (doc.containsKey("gatewayMode")) {
        Serial.printf("[BLE] Config: gatewayMode = %d\n", doc["gatewayMode"].as<bool>());
    }
}

void SIMSBLEService::handleMediaChunk(const std::string& value) {
    if (value.length() < 8) {
        Serial.println("[BLE] ERROR: Media chunk too short");
        return;
    }

    // Parse chunk header (8 bytes)
    const uint8_t* data = (const uint8_t*)value.data();
    uint8_t seqNum = data[0];
    uint8_t totalChunks = data[1];
    uint32_t totalSize = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];

    Serial.printf("[BLE] Media chunk %d/%d (total size=%d)\n", seqNum + 1, totalChunks, totalSize);

    // First chunk - initialize buffer
    if (seqNum == 0) {
        mediaBuffer.clear();
        mediaBuffer.reserve(totalSize);
        expectedChunks = totalChunks;
        receivedChunks = 0;
    }

    // Append chunk data (skip 8-byte header)
    size_t chunkDataSize = value.length() - 8;
    for (size_t i = 0; i < chunkDataSize; i++) {
        mediaBuffer.push_back(data[8 + i]);
    }

    receivedChunks++;

    // All chunks received?
    if (receivedChunks >= expectedChunks) {
        Serial.printf("[BLE] Media transfer complete (%d bytes)\n", mediaBuffer.size());

        // TODO: Process complete media file
        // For now, just clear the buffer
        mediaBuffer.clear();
        expectedChunks = 0;
        receivedChunks = 0;
    }
}

bool SIMSBLEService::sendIncidentToMesh(const JsonDocument& incidentDoc) {
    if (meshProtocol == nullptr || loraTransport == nullptr) {
        Serial.println("[BLE] ERROR: Mesh protocol not available");
        return false;
    }

    // TODO: Convert JSON incident to Protobuf MeshPacket and send via mesh
    // This requires integration with MeshProtocol.sendIncident() method
    // For now, just log that we would send it

    Serial.println("[BLE] Would send incident to mesh (not yet integrated)");
    return false;
}

// Server callbacks
void SIMSBLEServiceServerCallbacks::onConnect(NimBLEServer* pServer) {
    bleService->connectedClients++;
    Serial.printf("[BLE] Client connected (total: %d)\n", bleService->connectedClients);

    // Don't stop advertising - allow multiple connections
    if (bleService->connectedClients < BLE_MAX_CONNECTIONS) {
        NimBLEDevice::getAdvertising()->start();
    }
}

void SIMSBLEServiceServerCallbacks::onDisconnect(NimBLEServer* pServer) {
    bleService->connectedClients--;
    Serial.printf("[BLE] Client disconnected (total: %d)\n", bleService->connectedClients);

    // Restart advertising
    NimBLEDevice::getAdvertising()->start();
}

// Characteristic callbacks
void SIMSBLEServiceCharacteristicCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (pCharacteristic->getUUID().toString() == BLE_CHAR_INCIDENT_TX_UUID) {
        bleService->handleIncidentTx(value);
    } else if (pCharacteristic->getUUID().toString() == BLE_CHAR_CONFIG_UUID) {
        bleService->handleConfigWrite(value);
    } else if (pCharacteristic->getUUID().toString() == BLE_CHAR_MEDIA_UUID) {
        bleService->handleMediaChunk(value);
    }
}
