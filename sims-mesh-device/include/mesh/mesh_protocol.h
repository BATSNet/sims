/**
 * Mesh Protocol Layer
 * Implements flood routing, message prioritization, and TDMA scheduling
 */

#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

#include <Arduino.h>
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

    // Initialize mesh protocol
    bool begin(LoRaTransport* transport);

    // Update (call in main loop)
    void update();

    // Send incident report
    bool sendIncident(const IncidentReport& incident);

    // Send generic message
    bool sendMessage(const MeshMessage& msg);

    // Receive message (non-blocking)
    bool hasMessage();
    MeshMessage receiveMessage();

    // Device ID management
    uint32_t getDeviceId();
    void setDeviceId(uint32_t id);

    // Network status
    int getConnectedNodes();
    bool isConnected();

    // Statistics
    void getStats(uint32_t& sent, uint32_t& received, uint32_t& relayed);

private:
    LoRaTransport* transport;
    uint32_t deviceId;
    uint32_t sequenceNumber;

    // Routing table
    std::map<uint32_t, RouteEntry> routingTable;

    // Message queues (priority-based)
    std::priority_queue<MessageQueueEntry> messageQueue;
    std::queue<MeshMessage> receivedQueue;

    // Seen messages (for loop prevention)
    std::map<uint32_t, unsigned long> seenMessages;  // sequenceNumber -> timestamp

    // Statistics
    uint32_t messagesSent;
    uint32_t messagesReceived;
    uint32_t messagesRelayed;

    // Timers
    unsigned long lastHeartbeat;
    unsigned long lastCleanup;

    // Internal methods
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
