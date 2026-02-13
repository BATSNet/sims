/**
 * MQTT Client Service Implementation
 */

#include "network/mqtt_client.h"
#include <ArduinoJson.h>

// Static instance for callback
MQTTClientService* MQTTClientService::instance = nullptr;

MQTTClientService::MQTTClientService() :
    mqttClient(wifiClient),
    brokerPort(MQTT_PORT),
    lastReconnectAttempt(0),
    reconnectAttempts(0),
    messageCallback(nullptr)
{
    instance = this;
}

MQTTClientService::~MQTTClientService() {
    end();
    instance = nullptr;
}

bool MQTTClientService::begin(const char* broker, int port, const char* clientId) {
    this->brokerHost = String(broker);
    this->brokerPort = port;
    this->clientId = String(clientId);

    Serial.printf("[MQTT] Initializing MQTT client (broker=%s:%d, clientId=%s)\n",
                 broker, port, clientId);

    mqttClient.setServer(broker, port);
    mqttClient.setCallback(staticCallback);
    mqttClient.setKeepAlive(SIMS_MQTT_KEEPALIVE);

    // Try initial connection
    if (connect()) {
        Serial.println("[MQTT] Connected to broker");
        return true;
    } else {
        Serial.println("[MQTT] Initial connection failed, will retry");
        return false;
    }
}

void MQTTClientService::end() {
    if (mqttClient.connected()) {
        mqttClient.disconnect();
    }
}

void MQTTClientService::update() {
    if (mqttClient.connected()) {
        mqttClient.loop();
    } else {
        handleReconnect();
    }
}

bool MQTTClientService::isConnected() {
    return mqttClient.connected();
}

bool MQTTClientService::publishIncident(const uint8_t* protobufData, size_t dataSize, uint8_t priority) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Cannot publish incident - not connected");
        return false;
    }

    // Publish raw Protobuf data to incidents/in topic
    // Backend mesh_gateway_service.py will decode the Protobuf
    bool success = mqttClient.publish(
        MQTT_TOPIC_INCIDENTS_IN,
        protobufData,
        dataSize,
        false  // Not retained
    );

    if (success) {
        Serial.printf("[MQTT] Incident published (%d bytes, priority=%d)\n", dataSize, priority);
    } else {
        Serial.println("[MQTT] ERROR: Failed to publish incident");
    }

    return success;
}

bool MQTTClientService::publishStatus(const NetworkStatus& status) {
    if (!mqttClient.connected()) {
        return false;
    }

    // Build JSON status payload
    JsonDocument doc;
    doc["nodeCount"] = status.nodeCount;
    doc["rssi"] = status.rssi;
    doc["hopCount"] = status.hopCount;
    doc["pendingMessages"] = status.pendingMessages;
    doc["timestamp"] = millis();
    doc["deviceId"] = String((uint32_t)ESP.getEfuseMac(), HEX);

    String payload;
    serializeJson(doc, payload);

    bool success = mqttClient.publish(
        MQTT_TOPIC_STATUS,
        payload.c_str(),
        false  // Not retained
    );

    if (success) {
        Serial.printf("[MQTT] Status published: %d nodes, RSSI=%d\n", status.nodeCount, status.rssi);
    }

    return success;
}

bool MQTTClientService::publishNodeList(const uint32_t* nodeIds, int count) {
    if (!mqttClient.connected()) {
        return false;
    }

    // Build JSON node list
    JsonDocument doc;
    JsonArray nodes = doc["nodes"].to<JsonArray>();

    for (int i = 0; i < count; i++) {
        nodes.add(String(nodeIds[i], HEX));
    }

    doc["timestamp"] = millis();
    doc["deviceId"] = String((uint32_t)ESP.getEfuseMac(), HEX);

    String payload;
    serializeJson(doc, payload);

    bool success = mqttClient.publish(
        MQTT_TOPIC_NODES,
        payload.c_str(),
        false  // Not retained
    );

    if (success) {
        Serial.printf("[MQTT] Node list published: %d nodes\n", count);
    }

    return success;
}

void MQTTClientService::setMessageCallback(MessageCallback callback) {
    this->messageCallback = callback;
}

bool MQTTClientService::connect() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MQTT] Cannot connect - WiFi not connected");
        return false;
    }

    Serial.printf("[MQTT] Connecting to broker %s:%d...\n", brokerHost.c_str(), brokerPort);

    // Attempt connection with QoS 1, clean session = false (persistent)
    bool connected = mqttClient.connect(
        clientId.c_str(),
        nullptr,  // No username
        nullptr,  // No password
        nullptr,  // No will topic
        0,        // Will QoS
        false,    // Will retain
        nullptr,  // Will message
        false     // Clean session (persistent)
    );

    if (connected) {
        Serial.println("[MQTT] Connected to broker");

        // Subscribe to backend → mesh topic
        mqttClient.subscribe(MQTT_TOPIC_INCIDENTS_OUT, MQTT_QOS);
        Serial.printf("[MQTT] Subscribed to %s\n", MQTT_TOPIC_INCIDENTS_OUT);

        reconnectAttempts = 0;
        return true;
    } else {
        int state = mqttClient.state();
        Serial.printf("[MQTT] Connection failed (state=%d)\n", state);
        return false;
    }
}

void MQTTClientService::handleReconnect() {
    unsigned long now = millis();

    if (now - lastReconnectAttempt >= getBackoffInterval()) {
        lastReconnectAttempt = now;
        reconnectAttempts++;

        Serial.printf("[MQTT] Reconnect attempt %d...\n", reconnectAttempts);
        connect();
    }
}

unsigned long MQTTClientService::getBackoffInterval() {
    // Exponential backoff: 5s, 10s, 20s, 40s, max 60s
    unsigned long interval = 5000 * (1 << min(reconnectAttempts, 4));
    return min(interval, 60000UL);
}

void MQTTClientService::staticCallback(char* topic, uint8_t* payload, unsigned int length) {
    if (instance != nullptr) {
        instance->handleMessage(topic, payload, length);
    }
}

void MQTTClientService::handleMessage(const char* topic, const uint8_t* payload, unsigned int length) {
    Serial.printf("[MQTT] Message received on %s (%d bytes)\n", topic, length);

    // Forward to user callback if set
    if (messageCallback != nullptr) {
        messageCallback(topic, payload, length);
    }
}
