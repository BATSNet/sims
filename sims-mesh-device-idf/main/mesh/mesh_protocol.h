/**
 * Mesh Protocol Layer
 * Implements flood routing, message prioritization
 * ESP-IDF version using NVS instead of Preferences
 */

#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

#include <stdint.h>
#include "../config.h"
#include "../lora_transport.h"
#include <map>
#include <queue>

struct RouteEntry {
    uint32_t destinationId;
    uint32_t nextHop;
    uint8_t hopCount;
    unsigned long timestamp;
};

struct MessageQueueEntry {
    MeshMessage message;
    uint8_t retryCount;
    unsigned long nextRetryTime;
    bool needsAck;
};

class MeshProtocol {
public:
    MeshProtocol();

    bool begin(LoRaTransport* transport);
    void update();

    bool sendIncident(const IncidentReport& incident);
    bool sendMessage(const MeshMessage& msg);

    bool hasMessage();
    MeshMessage receiveMessage();

    uint32_t getDeviceId();
    void setDeviceId(uint32_t id);

    int getConnectedNodes();
    bool isConnected();

    void getStats(uint32_t& sent, uint32_t& received, uint32_t& relayed);

private:
    LoRaTransport* transport;
    uint32_t deviceId;
    uint32_t sequenceNumber;

    std::map<uint32_t, RouteEntry> routingTable;
    std::priority_queue<MessageQueueEntry> messageQueue;
    std::queue<MeshMessage> receivedQueue;
    std::map<uint32_t, unsigned long> seenMessages;

    uint32_t messagesSent;
    uint32_t messagesReceived;
    uint32_t messagesRelayed;

    unsigned long lastHeartbeat;
    unsigned long lastCleanup;

    void sendHeartbeat();
    void processIncomingMessage(const MeshMessage& msg);
    bool shouldRelay(const MeshMessage& msg);
    void relayMessage(const MeshMessage& msg);
    void updateRoute(uint32_t nodeId, uint32_t nextHop, uint8_t hopCount);
    void cleanupOldEntries();
    uint32_t generateDeviceId();
    bool isMessageSeen(uint32_t sequenceNumber);
    void markMessageAsSeen(uint32_t sequenceNumber);
};

#endif // MESH_PROTOCOL_H
