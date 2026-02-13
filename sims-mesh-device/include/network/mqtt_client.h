/**
 * MQTT Client Service
 *
 * Gateway mode: Forward LoRa mesh packets to backend via MQTT
 * Features:
 * - Publish mesh incidents to backend (Protobuf format)
 * - Subscribe to backend commands
 * - Publish network status and node list
 * - QoS 1 with persistent session
 * - Auto-reconnect
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// Forward declarations
struct MeshMessage;

class MQTTClientService {
public:
    MQTTClientService();
    ~MQTTClientService();

    // Lifecycle
    bool begin(const char* broker, int port, const char* clientId);
    void end();
    void update();  // Call in main loop for reconnect and message processing

    // Connection status
    bool isConnected();

    // Publish mesh incidents (Protobuf format - raw bytes)
    bool publishIncident(const uint8_t* protobufData, size_t dataSize, uint8_t priority);

    // Publish network status
    struct NetworkStatus {
        int nodeCount;
        int rssi;
        int hopCount;
        int pendingMessages;
    };
    bool publishStatus(const NetworkStatus& status);

    // Publish node list
    bool publishNodeList(const uint32_t* nodeIds, int count);

    // Set callback for incoming messages from backend
    typedef void (*MessageCallback)(const char* topic, const uint8_t* payload, unsigned int length);
    void setMessageCallback(MessageCallback callback);

private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;

    String clientId;
    String brokerHost;
    int brokerPort;

    unsigned long lastReconnectAttempt;
    int reconnectAttempts;

    MessageCallback messageCallback;

    // Internal callback for PubSubClient
    static MQTTClientService* instance;  // For static callback
    static void staticCallback(char* topic, uint8_t* payload, unsigned int length);
    void handleMessage(const char* topic, const uint8_t* payload, unsigned int length);

    // Connection management
    bool connect();
    void handleReconnect();
    unsigned long getBackoffInterval();
};

#endif // MQTT_CLIENT_H
