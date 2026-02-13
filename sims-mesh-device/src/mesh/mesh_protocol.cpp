/**
 * Mesh Protocol Implementation
 * Flood routing with hop limit, message prioritization, and TDMA scheduling
 */

#include "mesh/mesh_protocol.h"
#include <Preferences.h>

MeshProtocol::MeshProtocol()
    : transport(nullptr), deviceId(0), sequenceNumber(0),
      messagesSent(0), messagesReceived(0), messagesRelayed(0),
      lastHeartbeat(0), lastCleanup(0) {
}

bool MeshProtocol::begin(LoRaTransport* lora) {
    transport = lora;

    if (!transport) {
        Serial.println("[Mesh] ERROR: LoRa transport is null");
        return false;
    }

    // Load or generate device ID
    deviceId = generateDeviceId();
    Serial.printf("[Mesh] Device ID: 0x%08X\n", deviceId);

    // Initialize sequence number
    sequenceNumber = 1;

    Serial.println("[Mesh] Mesh protocol initialized");
    return true;
}

void MeshProtocol::update() {
    // Send periodic heartbeat
    unsigned long now = millis();
    if (now - lastHeartbeat > MESH_HEARTBEAT_INTERVAL) {
        sendHeartbeat();
        lastHeartbeat = now;
    }

    // Process incoming messages
    if (transport && transport->available()) {
        uint8_t buffer[MAX_PACKET_SIZE];
        size_t length = 0;

        if (transport->receive(buffer, &length)) {
            // Parse mesh message
            MeshMessage msg;
            memcpy(&msg, buffer, min(length, sizeof(MeshMessage)));

            processIncomingMessage(msg);
        }
    }

    // Cleanup old routing table entries and seen messages
    if (now - lastCleanup > 60000) {  // Every 60 seconds
        cleanupOldEntries();
        lastCleanup = now;
    }
}

bool MeshProtocol::sendIncident(const IncidentReport& incident) {
    Serial.println("[Mesh] Preparing incident message...");

    // Create mesh message
    MeshMessage msg;
    msg.sourceId = deviceId;
    msg.destinationId = 0xFFFFFFFF;  // Broadcast
    msg.sequenceNumber = sequenceNumber++;
    msg.messageType = MSG_TYPE_INCIDENT;
    msg.priority = incident.priority;
    msg.hopCount = 0;
    msg.ttl = 60;  // TTL in hops (not time)
    msg.timestamp = millis();

    // Encode incident data into payload (simplified)
    // TODO: Use Protobuf for proper encoding
    uint8_t* payload = msg.payload;
    size_t offset = 0;

    // Pack GPS data
    memcpy(payload + offset, &incident.latitude, sizeof(float));
    offset += sizeof(float);
    memcpy(payload + offset, &incident.longitude, sizeof(float));
    offset += sizeof(float);
    memcpy(payload + offset, &incident.altitude, sizeof(float));
    offset += sizeof(float);

    // Pack description
    size_t descLen = min(strlen(incident.description), (size_t)(MAX_PAYLOAD_SIZE - offset - 1));
    memcpy(payload + offset, incident.description, descLen);
    offset += descLen;
    payload[offset] = '\0';
    offset++;

    msg.payloadSize = offset;

    Serial.printf("[Mesh] Incident payload: %d bytes\n", msg.payloadSize);

    // TODO: Handle image and audio data with chunking
    if (incident.hasImage) {
        Serial.println("[Mesh] WARNING: Image chunking not yet implemented");
    }
    if (incident.hasAudio) {
        Serial.println("[Mesh] WARNING: Audio chunking not yet implemented");
    }

    // Send message
    return sendMessage(msg);
}

bool MeshProtocol::sendMessage(const MeshMessage& msg) {
    if (!transport) {
        return false;
    }

    // Mark message as seen (don't relay our own messages)
    markMessageAsSeen(msg.sequenceNumber);

    // Transmit
    bool success = transport->send((uint8_t*)&msg, sizeof(MeshMessage));

    if (success) {
        messagesSent++;
        Serial.printf("[Mesh] Message sent: seq=%lu, type=%d, pri=%d\n",
                     msg.sequenceNumber, msg.messageType, msg.priority);
    } else {
        Serial.println("[Mesh] ERROR: Failed to send message");
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
    msg.ttl = 60;  // 60 seconds
    msg.timestamp = millis();
    msg.payloadSize = 0;

    Serial.println("[Mesh] Sending heartbeat");
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

    Serial.printf("[Mesh] Received: from=0x%08X, seq=%lu, type=%d, hops=%d\n",
                 msg.sourceId, msg.sequenceNumber, msg.messageType, msg.hopCount);

    // Check if message is for us or broadcast
    bool isForUs = (msg.destinationId == deviceId || msg.destinationId == 0xFFFFFFFF);

    // Check if we've seen this message before (loop prevention)
    if (isMessageSeen(msg.sequenceNumber)) {
        Serial.println("[Mesh] Message already seen, discarding");
        return;
    }

    // Mark as seen
    markMessageAsSeen(msg.sequenceNumber);

    // Update routing table
    updateRoute(msg.sourceId, msg.sourceId, msg.hopCount);

    // Process message if it's for us
    if (isForUs) {
        receivedQueue.push(msg);

        // Send ACK for non-broadcast messages
        if (msg.destinationId != 0xFFFFFFFF) {
            MeshMessage ack;
            ack.sourceId = deviceId;
            ack.destinationId = msg.sourceId;
            ack.sequenceNumber = sequenceNumber++;
            ack.messageType = MSG_TYPE_ACK;
            ack.priority = PRIORITY_HIGH;
            ack.hopCount = 0;
            ack.ttl = 30;
            ack.timestamp = millis();
            ack.payloadSize = sizeof(uint32_t);
            memcpy(ack.payload, &msg.sequenceNumber, sizeof(uint32_t));

            Serial.println("[Mesh] Sending ACK");
            sendMessage(ack);
        }
    }

    // Relay message if needed
    if (shouldRelay(msg)) {
        relayMessage(msg);
    }
}

bool MeshProtocol::shouldRelay(const MeshMessage& msg) {
    // Don't relay if hop count exceeded
    if (msg.hopCount >= MESH_MAX_HOPS) {
        Serial.println("[Mesh] Max hops reached, not relaying");
        return false;
    }

    // Don't relay if TTL expired
    unsigned long age = (millis() - msg.timestamp) / 1000;
    if (age > msg.ttl) {
        Serial.println("[Mesh] TTL expired, not relaying");
        return false;
    }

    // Don't relay ACKs (they're unicast)
    if (msg.messageType == MSG_TYPE_ACK || msg.messageType == MSG_TYPE_NACK) {
        return false;
    }

    // Don't relay if message is directly for another device (not broadcast)
    if (msg.destinationId != 0xFFFFFFFF && msg.destinationId != deviceId) {
        return false;
    }

    return true;
}

void MeshProtocol::relayMessage(const MeshMessage& msg) {
    MeshMessage relay = msg;
    relay.hopCount++;

    Serial.printf("[Mesh] Relaying message (hop %d)\n", relay.hopCount);

    sendMessage(relay);
    messagesRelayed++;
}

void MeshProtocol::updateRoute(uint32_t nodeId, uint32_t nextHop, uint8_t hopCount) {
    RouteEntry entry;
    entry.destinationId = nodeId;
    entry.nextHop = nextHop;
    entry.hopCount = hopCount;
    entry.timestamp = millis();

    routingTable[nodeId] = entry;
}

void MeshProtocol::cleanupOldEntries() {
    unsigned long now = millis();
    unsigned long maxAge = 300000;  // 5 minutes

    // Clean up routing table
    for (auto it = routingTable.begin(); it != routingTable.end(); ) {
        if (now - it->second.timestamp > maxAge) {
            Serial.printf("[Mesh] Removing stale route: 0x%08X\n", it->first);
            it = routingTable.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up seen messages
    for (auto it = seenMessages.begin(); it != seenMessages.end(); ) {
        if (now - it->second > maxAge) {
            it = seenMessages.erase(it);
        } else {
            ++it;
        }
    }
}

uint32_t MeshProtocol::generateDeviceId() {
    // Try to load from NVS storage
    Preferences prefs;
    prefs.begin("sims-mesh", false);

    uint32_t id = prefs.getUInt("deviceId", 0);

    if (id == 0) {
        // Generate new ID from MAC address
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);

        id = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];

        // Store for next boot
        prefs.putUInt("deviceId", id);
        Serial.printf("[Mesh] Generated new device ID: 0x%08X\n", id);
    } else {
        Serial.printf("[Mesh] Loaded device ID from storage: 0x%08X\n", id);
    }

    prefs.end();
    return id;
}

bool MeshProtocol::isMessageSeen(uint32_t sequenceNumber) {
    return seenMessages.find(sequenceNumber) != seenMessages.end();
}

void MeshProtocol::markMessageAsSeen(uint32_t sequenceNumber) {
    seenMessages[sequenceNumber] = millis();
}

uint32_t MeshProtocol::getDeviceId() {
    return deviceId;
}

void MeshProtocol::setDeviceId(uint32_t id) {
    deviceId = id;

    // Save to NVS
    Preferences prefs;
    prefs.begin("sims-mesh", false);
    prefs.putUInt("deviceId", id);
    prefs.end();
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
