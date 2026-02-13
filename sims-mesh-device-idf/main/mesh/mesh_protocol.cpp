/**
 * Mesh Protocol Implementation
 * ESP-IDF version using NVS instead of Preferences
 */

#include "mesh/mesh_protocol.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>

static const char* TAG = "Mesh";

static inline unsigned long millis_now() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

MeshProtocol::MeshProtocol()
    : transport(nullptr), deviceId(0), sequenceNumber(0),
      messagesSent(0), messagesReceived(0), messagesRelayed(0),
      lastHeartbeat(0), lastCleanup(0) {
}

bool MeshProtocol::begin(LoRaTransport* lora) {
    transport = lora;

    if (!transport) {
        ESP_LOGE(TAG, "LoRa transport is null");
        return false;
    }

    deviceId = generateDeviceId();
    ESP_LOGI(TAG, "Device ID: 0x%08X", (unsigned int)deviceId);

    sequenceNumber = 1;

    ESP_LOGI(TAG, "Mesh protocol initialized");
    return true;
}

void MeshProtocol::update() {
    unsigned long now = millis_now();

    if (now - lastHeartbeat > MESH_HEARTBEAT_INTERVAL) {
        sendHeartbeat();
        lastHeartbeat = now;
    }

    if (transport && transport->available()) {
        uint8_t buffer[MAX_PACKET_SIZE];
        size_t length = 0;

        if (transport->receive(buffer, &length)) {
            MeshMessage msg;
            size_t copyLen = length < sizeof(MeshMessage) ? length : sizeof(MeshMessage);
            memcpy(&msg, buffer, copyLen);
            processIncomingMessage(msg);
        }
    }

    if (now - lastCleanup > 60000) {
        cleanupOldEntries();
        lastCleanup = now;
    }
}

bool MeshProtocol::sendIncident(const IncidentReport& incident) {
    ESP_LOGI(TAG, "Preparing incident message...");

    MeshMessage msg;
    msg.sourceId = deviceId;
    msg.destinationId = 0xFFFFFFFF;
    msg.sequenceNumber = sequenceNumber++;
    msg.messageType = MSG_TYPE_INCIDENT;
    msg.priority = incident.priority;
    msg.hopCount = 0;
    msg.ttl = 60;
    msg.timestamp = millis_now();

    uint8_t* payload = msg.payload;
    size_t offset = 0;

    memcpy(payload + offset, &incident.latitude, sizeof(float));
    offset += sizeof(float);
    memcpy(payload + offset, &incident.longitude, sizeof(float));
    offset += sizeof(float);
    memcpy(payload + offset, &incident.altitude, sizeof(float));
    offset += sizeof(float);

    size_t descLen = strlen(incident.description);
    if (descLen > (size_t)(MAX_PAYLOAD_SIZE - offset - 1)) {
        descLen = MAX_PAYLOAD_SIZE - offset - 1;
    }
    memcpy(payload + offset, incident.description, descLen);
    offset += descLen;
    payload[offset] = '\0';
    offset++;

    msg.payloadSize = offset;

    ESP_LOGI(TAG, "Incident payload: %d bytes", msg.payloadSize);

    if (incident.hasImage) {
        ESP_LOGW(TAG, "Image chunking not yet implemented");
    }
    if (incident.hasAudio) {
        ESP_LOGW(TAG, "Audio chunking not yet implemented");
    }

    return sendMessage(msg);
}

bool MeshProtocol::sendMessage(const MeshMessage& msg) {
    if (!transport) {
        return false;
    }

    markMessageAsSeen(msg.sequenceNumber);

    bool success = transport->send((uint8_t*)&msg, sizeof(MeshMessage));

    if (success) {
        messagesSent++;
        ESP_LOGI(TAG, "Message sent: seq=%lu, type=%d, pri=%d",
                 (unsigned long)msg.sequenceNumber, msg.messageType, msg.priority);
    } else {
        ESP_LOGE(TAG, "Failed to send message");
    }

    return success;
}

void MeshProtocol::sendHeartbeat() {
    MeshMessage msg;
    msg.sourceId = deviceId;
    msg.destinationId = 0xFFFFFFFF;
    msg.sequenceNumber = sequenceNumber++;
    msg.messageType = MSG_TYPE_HEARTBEAT;
    msg.priority = PRIORITY_LOW;
    msg.hopCount = 0;
    msg.ttl = 60;
    msg.timestamp = millis_now();
    msg.payloadSize = 0;

    ESP_LOGI(TAG, "Sending heartbeat");
    sendMessage(msg);
}

bool MeshProtocol::hasMessage() {
    return !receivedQueue.empty();
}

MeshMessage MeshProtocol::receiveMessage() {
    if (receivedQueue.empty()) {
        MeshMessage empty;
        memset(&empty, 0, sizeof(MeshMessage));
        return empty;
    }

    MeshMessage msg = receivedQueue.front();
    receivedQueue.pop();
    return msg;
}

void MeshProtocol::processIncomingMessage(const MeshMessage& msg) {
    messagesReceived++;

    ESP_LOGI(TAG, "Received: from=0x%08X, seq=%lu, type=%d, hops=%d",
             (unsigned int)msg.sourceId, (unsigned long)msg.sequenceNumber,
             msg.messageType, msg.hopCount);

    bool isForUs = (msg.destinationId == deviceId || msg.destinationId == 0xFFFFFFFF);

    if (isMessageSeen(msg.sequenceNumber)) {
        ESP_LOGI(TAG, "Message already seen, discarding");
        return;
    }

    markMessageAsSeen(msg.sequenceNumber);
    updateRoute(msg.sourceId, msg.sourceId, msg.hopCount);

    if (isForUs) {
        receivedQueue.push(msg);

        if (msg.destinationId != 0xFFFFFFFF) {
            MeshMessage ack;
            ack.sourceId = deviceId;
            ack.destinationId = msg.sourceId;
            ack.sequenceNumber = sequenceNumber++;
            ack.messageType = MSG_TYPE_ACK;
            ack.priority = PRIORITY_HIGH;
            ack.hopCount = 0;
            ack.ttl = 30;
            ack.timestamp = millis_now();
            ack.payloadSize = sizeof(uint32_t);
            memcpy(ack.payload, &msg.sequenceNumber, sizeof(uint32_t));

            ESP_LOGI(TAG, "Sending ACK");
            sendMessage(ack);
        }
    }

    if (shouldRelay(msg)) {
        relayMessage(msg);
    }
}

bool MeshProtocol::shouldRelay(const MeshMessage& msg) {
    if (msg.hopCount >= MESH_MAX_HOPS) {
        ESP_LOGI(TAG, "Max hops reached, not relaying");
        return false;
    }

    unsigned long age = (millis_now() - msg.timestamp) / 1000;
    if (age > msg.ttl) {
        ESP_LOGI(TAG, "TTL expired, not relaying");
        return false;
    }

    if (msg.messageType == MSG_TYPE_ACK || msg.messageType == MSG_TYPE_NACK) {
        return false;
    }

    if (msg.destinationId != 0xFFFFFFFF && msg.destinationId != deviceId) {
        return false;
    }

    return true;
}

void MeshProtocol::relayMessage(const MeshMessage& msg) {
    MeshMessage relay = msg;
    relay.hopCount++;

    ESP_LOGI(TAG, "Relaying message (hop %d)", relay.hopCount);

    sendMessage(relay);
    messagesRelayed++;
}

void MeshProtocol::updateRoute(uint32_t nodeId, uint32_t nextHop, uint8_t hopCount) {
    RouteEntry entry;
    entry.destinationId = nodeId;
    entry.nextHop = nextHop;
    entry.hopCount = hopCount;
    entry.timestamp = millis_now();

    routingTable[nodeId] = entry;
}

void MeshProtocol::cleanupOldEntries() {
    unsigned long now = millis_now();
    unsigned long maxAge = 300000;

    for (auto it = routingTable.begin(); it != routingTable.end(); ) {
        if (now - it->second.timestamp > maxAge) {
            ESP_LOGI(TAG, "Removing stale route: 0x%08X", (unsigned int)it->first);
            it = routingTable.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = seenMessages.begin(); it != seenMessages.end(); ) {
        if (now - it->second > maxAge) {
            it = seenMessages.erase(it);
        } else {
            ++it;
        }
    }
}

uint32_t MeshProtocol::generateDeviceId() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("sims-mesh", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        // Fallback to MAC-based ID
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
    }

    uint32_t id = 0;
    err = nvs_get_u32(handle, "deviceId", &id);

    if (err == ESP_ERR_NVS_NOT_FOUND || id == 0) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        id = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];

        nvs_set_u32(handle, "deviceId", id);
        nvs_commit(handle);
        ESP_LOGI(TAG, "Generated new device ID: 0x%08X", (unsigned int)id);
    } else {
        ESP_LOGI(TAG, "Loaded device ID from storage: 0x%08X", (unsigned int)id);
    }

    nvs_close(handle);
    return id;
}

bool MeshProtocol::isMessageSeen(uint32_t seqNum) {
    return seenMessages.find(seqNum) != seenMessages.end();
}

void MeshProtocol::markMessageAsSeen(uint32_t seqNum) {
    seenMessages[seqNum] = millis_now();
}

uint32_t MeshProtocol::getDeviceId() {
    return deviceId;
}

void MeshProtocol::setDeviceId(uint32_t id) {
    deviceId = id;

    nvs_handle_t handle;
    esp_err_t err = nvs_open("sims-mesh", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u32(handle, "deviceId", id);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

int MeshProtocol::getConnectedNodes() {
    return routingTable.size();
}

bool MeshProtocol::isConnected() {
    return routingTable.size() > 0;
}

void MeshProtocol::getStats(uint32_t& sent, uint32_t& received, uint32_t& relayed) {
    sent = messagesSent;
    received = messagesReceived;
    relayed = messagesRelayed;
}
