/**
 * HTTP Client Service
 *
 * Direct incident upload to backend via WiFi
 * Features:
 * - POST to /api/incidents endpoint
 * - JSON payload format
 * - Chunked upload for images/audio
 * - Retry logic with exponential backoff
 * - Bandwidth optimizations (image compression, VAD)
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

class HTTPClientService {
public:
    HTTPClientService();
    ~HTTPClientService();

    // Lifecycle
    bool begin(const char* backendHost, int backendPort);
    void end();

    // Incident upload
    struct IncidentUploadResult {
        bool success;
        int httpCode;
        String message;
        String incidentId;  // Backend-assigned incident ID
    };

    IncidentUploadResult uploadIncident(
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

    // Simple JSON upload (no media)
    IncidentUploadResult uploadIncidentJSON(
        float latitude,
        float longitude,
        uint8_t priority,
        uint8_t category,
        const char* description
    );

    // Health check
    bool ping();

private:
    String backendBaseURL;
    HTTPClient http;

    // Helper methods
    String buildIncidentJSON(
        float latitude,
        float longitude,
        float altitude,
        uint8_t priority,
        uint8_t category,
        const char* description,
        const char* imageURL = nullptr,
        const char* audioURL = nullptr
    );

    bool uploadMedia(
        const char* endpoint,
        uint8_t* data,
        size_t dataSize,
        const char* contentType,
        String& mediaURL
    );

    String getPriorityString(uint8_t priority);
    String getCategoryString(uint8_t category);
};

#endif // HTTP_CLIENT_H
