/**
 * Transport Manager
 *
 * Intelligent routing between WiFi and LoRa mesh
 * Priority: WiFi (if connected) > LoRa mesh
 * Features:
 * - Automatic fallback on WiFi failure
 * - Message queueing when offline
 * - Priority-based routing
 * - Retry logic with exponential backoff
 */

#ifndef TRANSPORT_MANAGER_H
#define TRANSPORT_MANAGER_H

#include "network/wifi_service.h"
#include "network/http_client.h"
#include "storage/message_storage.h"
#include "config.h"

// Forward declarations (to avoid circular dependencies)
class LoRaTransport;
class MeshProtocol;

class TransportManager {
public:
    TransportManager(
        WiFiService* wifiService,
        HTTPClientService* httpClient,
        MessageStorage* messageStorage,
        LoRaTransport* loraTransport = nullptr,
        MeshProtocol* meshProtocol = nullptr
    );

    ~TransportManager();

    // Send incident report
    enum SendResult {
        SEND_SUCCESS_WIFI,
        SEND_SUCCESS_LORA,
        SEND_QUEUED,
        SEND_FAILED
    };

    struct SendStatus {
        SendResult result;
        String message;
        String incidentId;  // Backend-assigned ID (WiFi only)
    };

    SendStatus sendIncident(
        float latitude,
        float longitude,
        float altitude,
        uint8_t priority,
        uint8_t category,
        const char* description,
        uint8_t* imageData = nullptr,
        size_t imageSize = 0,
        uint8_t* audioData = nullptr,
        size_t audioSize = 0
    );

    // Queue processing (call in main loop)
    void processQueue();

    // Statistics
    int getQueuedCount();
    int getSuccessCountWiFi();
    int getSuccessCountLoRa();
    int getFailedCount();

private:
    WiFiService* wifiService;
    HTTPClientService* httpClient;
    MessageStorage* messageStorage;
    LoRaTransport* loraTransport;
    MeshProtocol* meshProtocol;

    // Statistics
    int successCountWiFi;
    int successCountLoRa;
    int failedCount;

    // Queue processing
    unsigned long lastQueueProcessTime;
    unsigned long queueProcessInterval;
    int queueRetryAttempts;

    // Helper methods
    bool shouldUseWiFi(uint8_t priority, size_t totalSize);
    bool sendViaWiFi(
        float latitude, float longitude, float altitude,
        uint8_t priority, uint8_t category, const char* description,
        uint8_t* imageData, size_t imageSize,
        uint8_t* audioData, size_t audioSize
    );
    bool sendViaLoRa(
        float latitude, float longitude, float altitude,
        uint8_t priority, uint8_t category, const char* description
    );
    bool queueIncident(
        float latitude, float longitude, float altitude,
        uint8_t priority, uint8_t category, const char* description,
        uint8_t* imageData, size_t imageSize,
        uint8_t* audioData, size_t audioSize
    );

    unsigned long getQueueBackoffInterval();
};

#endif // TRANSPORT_MANAGER_H
