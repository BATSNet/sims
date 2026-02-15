/**
 * HTTP Client Service - ESP-IDF Port
 *
 * Direct incident upload to backend via WiFi
 * Uses esp_http_client instead of Arduino HTTPClient
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_http_client.h"
#include "config.h"
#include <stdint.h>
#include <stddef.h>

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
        char message[128];
        char incidentId[64];
    };

    // Transcription result
    struct TranscribeResult {
        bool success;
        int httpCode;
        char text[512];
        char error[128];
    };

    // Transcribe raw PCM audio via backend Whisper
    TranscribeResult transcribeAudio(const uint8_t* pcmData, size_t pcmSize);

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

    // Build binary payload (public - used by mesh send path too)
    // Caller must free() the returned buffer.
    uint8_t* buildIncidentBinary(
        float latitude,
        float longitude,
        float altitude,
        uint8_t priority,
        const char* description,
        uint8_t* imageData,
        size_t imageSize,
        uint8_t* audioData,
        size_t audioSize,
        size_t* outLen
    );

private:
    char backendURL[256];

    // Helper methods
    char* buildIncidentJSON(
        float latitude,
        float longitude,
        float altitude,
        uint8_t priority,
        const char* voiceCommand,
        const char* description,
        const char* imageBase64,
        const char* audioBase64,
        size_t* outLen
    );

    char* base64Encode(const uint8_t* data, size_t length, size_t* outLen);
    const char* getPriorityString(uint8_t priority);
    char* getDeviceId();
};

#endif // HTTP_CLIENT_H
