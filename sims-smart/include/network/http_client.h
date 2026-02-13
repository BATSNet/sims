/**
 * HTTP Client Service
 *
 * Direct incident upload to backend via WiFi
 * Features:
 * - POST to /api/incidents endpoint
 * - JSON payload with base64-encoded media
 * - Voice command metadata
 * - Retry logic
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../config.h"

class HTTPClientService {
public:
    HTTPClientService();
    ~HTTPClientService();

    // Lifecycle
    bool begin(const char* backendURL);
    void end();

    // Incident upload result
    struct IncidentUploadResult {
        bool success;
        int httpCode;
        String message;
        String incidentId;  // Backend-assigned incident ID
    };

    // Upload incident with voice command metadata
    IncidentUploadResult uploadIncident(
        float latitude,
        float longitude,
        float altitude,
        uint8_t priority,
        const char* voiceCommand,
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
        const char* description
    );

    // Health check
    bool ping();

private:
    String backendURL;
    HTTPClient http;

    // Helper methods
    String buildIncidentJSON(
        float latitude,
        float longitude,
        float altitude,
        uint8_t priority,
        const char* voiceCommand,
        const char* description,
        const char* imageBase64 = nullptr,
        const char* audioBase64 = nullptr
    );

    String base64Encode(uint8_t* data, size_t length);
    String getPriorityString(uint8_t priority);
};

#endif // HTTP_CLIENT_H
