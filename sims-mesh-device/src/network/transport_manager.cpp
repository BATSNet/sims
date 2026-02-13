/**
 * Transport Manager Implementation
 */

#include "network/transport_manager.h"
#include "lora_transport.h"
#include "mesh/mesh_protocol.h"

TransportManager::TransportManager(
    WiFiService* wifiService,
    HTTPClientService* httpClient,
    MessageStorage* messageStorage,
    LoRaTransport* loraTransport,
    MeshProtocol* meshProtocol
) :
    wifiService(wifiService),
    httpClient(httpClient),
    messageStorage(messageStorage),
    loraTransport(loraTransport),
    meshProtocol(meshProtocol),
    successCountWiFi(0),
    successCountLoRa(0),
    failedCount(0),
    lastQueueProcessTime(0),
    queueProcessInterval(QUEUE_RETRY_INTERVAL),
    queueRetryAttempts(0)
{
}

TransportManager::~TransportManager() {
}

TransportManager::SendStatus TransportManager::sendIncident(
    float latitude,
    float longitude,
    float altitude,
    uint8_t priority,
    uint8_t category,
    const char* description,
    uint8_t* imageData,
    size_t imageSize,
    uint8_t* audioData,
    size_t audioSize
) {
    SendStatus status;
    status.result = SEND_FAILED;
    status.message = "Unknown error";

    size_t totalSize = imageSize + audioSize;

    Serial.printf("[Transport] Sending incident (priority=%d, size=%d bytes)\n", priority, totalSize);

    // Decision: WiFi or LoRa?
    if (shouldUseWiFi(priority, totalSize)) {
        Serial.println("[Transport] Attempting WiFi upload...");

        if (sendViaWiFi(latitude, longitude, altitude, priority, category, description,
                        imageData, imageSize, audioData, audioSize)) {
            status.result = SEND_SUCCESS_WIFI;
            status.message = "Sent via WiFi";
            successCountWiFi++;
            Serial.println("[Transport] WiFi upload successful");
            return status;
        } else {
            Serial.println("[Transport] WiFi upload failed, trying LoRa fallback...");
        }
    }

    // Fallback to LoRa mesh (only for text incidents, no media)
    if (loraTransport != nullptr && meshProtocol != nullptr) {
        if (imageData == nullptr && audioData == nullptr) {
            Serial.println("[Transport] Attempting LoRa transmission...");

            if (sendViaLoRa(latitude, longitude, altitude, priority, category, description)) {
                status.result = SEND_SUCCESS_LORA;
                status.message = "Sent via LoRa mesh";
                successCountLoRa++;
                Serial.println("[Transport] LoRa transmission successful");
                return status;
            } else {
                Serial.println("[Transport] LoRa transmission failed");
            }
        } else {
            Serial.println("[Transport] LoRa doesn't support media, queueing...");
        }
    }

    // Queue for later retry
    Serial.println("[Transport] No transport available, queueing incident...");
    if (queueIncident(latitude, longitude, altitude, priority, category, description,
                     imageData, imageSize, audioData, audioSize)) {
        status.result = SEND_QUEUED;
        status.message = "Queued for retry";
        Serial.printf("[Transport] Incident queued (%d in queue)\n", getQueuedCount());
    } else {
        status.result = SEND_FAILED;
        status.message = "Queue full";
        failedCount++;
        Serial.println("[Transport] ERROR: Failed to queue incident");
    }

    return status;
}

void TransportManager::processQueue() {
    unsigned long now = millis();

    // Check if it's time to process queue
    if (now - lastQueueProcessTime < getQueueBackoffInterval()) {
        return;
    }

    lastQueueProcessTime = now;

    // Only process queue if WiFi is connected
    if (!wifiService->isConnected()) {
        return;
    }

    if (messageStorage == nullptr) {
        return;
    }

    int pendingCount = messageStorage->getPendingCount();
    if (pendingCount == 0) {
        return;
    }

    Serial.printf("[Transport] Processing offline queue (%d pending incidents)...\n", pendingCount);

    // Process pending messages one at a time
    IncidentReport incident;
    while (messageStorage->getNextPending(incident)) {
        Serial.printf("[Transport] Retrying queued incident (timestamp=%lu, priority=%d)\n",
                     incident.timestamp, incident.priority);

        // Try to send via WiFi (no media data in queue)
        if (sendViaWiFi(incident.latitude, incident.longitude, incident.altitude,
                        incident.priority, incident.category, incident.description,
                        nullptr, 0, nullptr, 0)) {
            messageStorage->markAsSent(incident.timestamp);
            Serial.println("[Transport] Queued incident sent successfully");
            successCountWiFi++;
        } else {
            Serial.println("[Transport] Failed to send queued incident, will retry later");
            break;  // Stop processing queue if one fails
        }
    }

    queueRetryAttempts++;
}

int TransportManager::getQueuedCount() {
    if (messageStorage == nullptr) {
        return 0;
    }
    return messageStorage->getPendingCount();
}

int TransportManager::getSuccessCountWiFi() {
    return successCountWiFi;
}

int TransportManager::getSuccessCountLoRa() {
    return successCountLoRa;
}

int TransportManager::getFailedCount() {
    return failedCount;
}

bool TransportManager::shouldUseWiFi(uint8_t priority, size_t totalSize) {
    // Always use WiFi if connected and available
    if (!wifiService->isConnected()) {
        return false;
    }

    // WiFi is available, use it for:
    // - Any message with media (images/audio)
    // - All priority levels (WiFi is faster and more reliable)
    return true;
}

bool TransportManager::sendViaWiFi(
    float latitude, float longitude, float altitude,
    uint8_t priority, uint8_t category, const char* description,
    uint8_t* imageData, size_t imageSize,
    uint8_t* audioData, size_t audioSize
) {
    auto result = httpClient->uploadIncident(
        latitude, longitude, altitude,
        priority, category, description,
        imageData, imageSize,
        audioData, audioSize
    );

    return result.success;
}

bool TransportManager::sendViaLoRa(
    float latitude, float longitude, float altitude,
    uint8_t priority, uint8_t category, const char* description
) {
    if (loraTransport == nullptr || meshProtocol == nullptr) {
        return false;
    }

    // TODO: Implement LoRa transmission via MeshProtocol
    // This requires MeshProtocol to have a sendIncident() method
    // For now, return false (not implemented)

    Serial.println("[Transport] LoRa transmission not yet integrated");
    return false;
}

bool TransportManager::queueIncident(
    float latitude, float longitude, float altitude,
    uint8_t priority, uint8_t category, const char* description,
    uint8_t* imageData, size_t imageSize,
    uint8_t* audioData, size_t audioSize
) {
    if (messageStorage == nullptr) {
        return false;
    }

    // Build incident report
    IncidentReport incident;
    incident.deviceId = (uint32_t)ESP.getEfuseMac();
    incident.latitude = latitude;
    incident.longitude = longitude;
    incident.altitude = altitude;
    incident.timestamp = millis();
    incident.priority = priority;
    incident.category = category;
    strncpy(incident.description, description, sizeof(incident.description) - 1);
    incident.description[sizeof(incident.description) - 1] = '\0';

    // Note: Media data is not stored (MessageStorage saves space by only storing metadata)
    incident.hasImage = false;
    incident.hasAudio = false;

    // Store to LittleFS queue
    return messageStorage->storeMessage(incident);
}

unsigned long TransportManager::getQueueBackoffInterval() {
    // Exponential backoff: 60s, 120s, 240s, max 1 hour
    if (!QUEUE_EXPONENTIAL_BACKOFF) {
        return queueProcessInterval;
    }

    unsigned long interval = queueProcessInterval * (1 << min(queueRetryAttempts, 6));
    return min(interval, (unsigned long)QUEUE_MAX_BACKOFF);
}
