/**
 * HTTP Client Service Implementation
 */

#include "network/http_client.h"
#include <base64.h>

HTTPClientService::HTTPClientService() {
}

HTTPClientService::~HTTPClientService() {
    end();
}

bool HTTPClientService::begin(const char* url) {
    backendURL = String(url);
    Serial.printf("[HTTP] Backend URL: %s\n", backendURL.c_str());
    return true;
}

void HTTPClientService::end() {
    http.end();
}

HTTPClientService::IncidentUploadResult HTTPClientService::uploadIncident(
    float latitude,
    float longitude,
    float altitude,
    uint8_t priority,
    const char* voiceCommand,
    const char* description,
    uint8_t* imageData,
    size_t imageSize,
    uint8_t* audioData,
    size_t audioSize
) {
    IncidentUploadResult result = {false, 0, "", ""};

    if (WiFi.status() != WL_CONNECTED) {
        result.message = "WiFi not connected";
        Serial.println("[HTTP] ERROR: WiFi not connected");
        return result;
    }

    // Encode media as base64 if present
    String imageBase64 = "";
    String audioBase64 = "";

    if (imageData != nullptr && imageSize > 0) {
        Serial.printf("[HTTP] Encoding image (%d bytes)...\n", imageSize);
        imageBase64 = base64Encode(imageData, imageSize);
        Serial.printf("[HTTP] Image encoded (%d bytes base64)\n", imageBase64.length());
    }

    if (audioData != nullptr && audioSize > 0) {
        Serial.printf("[HTTP] Encoding audio (%d bytes)...\n", audioSize);
        audioBase64 = base64Encode(audioData, audioSize);
        Serial.printf("[HTTP] Audio encoded (%d bytes base64)\n", audioBase64.length());
    }

    // Build JSON payload with embedded media
    String jsonPayload = buildIncidentJSON(
        latitude, longitude, altitude,
        priority, voiceCommand, description,
        imageBase64.isEmpty() ? nullptr : imageBase64.c_str(),
        audioBase64.isEmpty() ? nullptr : audioBase64.c_str()
    );

    Serial.printf("[HTTP] Uploading incident (%d bytes JSON)...\n", jsonPayload.length());

    // POST to backend
    http.begin(backendURL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(API_TIMEOUT_MS);

    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
        result.httpCode = httpCode;

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = http.getString();
            Serial.printf("[HTTP] Success (HTTP %d)\n", httpCode);

            // Parse incident ID from response
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);
            if (!error && doc.containsKey("id")) {
                result.incidentId = doc["id"].as<String>();
                Serial.printf("[HTTP] Incident ID: %s\n", result.incidentId.c_str());
            }

            result.success = true;
            result.message = "Uploaded successfully";
        } else {
            String response = http.getString();
            result.message = "HTTP error: " + String(httpCode);
            Serial.printf("[HTTP] ERROR: HTTP %d - %s\n", httpCode, response.c_str());
        }
    } else {
        result.message = "Connection failed: " + http.errorToString(httpCode);
        Serial.printf("[HTTP] ERROR: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return result;
}

HTTPClientService::IncidentUploadResult HTTPClientService::uploadIncidentJSON(
    float latitude,
    float longitude,
    uint8_t priority,
    const char* description
) {
    return uploadIncident(latitude, longitude, 0.0, priority, "", description, nullptr, 0, nullptr, 0);
}

bool HTTPClientService::ping() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    // Extract base URL (remove /api/incidents)
    String baseURL = backendURL;
    int idx = baseURL.indexOf("/api/");
    if (idx > 0) {
        baseURL = baseURL.substring(0, idx);
    }

    String healthURL = baseURL + "/api/health";
    http.begin(healthURL);
    http.setTimeout(5000);  // 5s timeout

    int httpCode = http.GET();
    bool success = (httpCode == HTTP_CODE_OK);

    http.end();

    Serial.printf("[HTTP] Health check: %s (HTTP %d)\n", success ? "OK" : "FAILED", httpCode);
    return success;
}

String HTTPClientService::buildIncidentJSON(
    float latitude,
    float longitude,
    float altitude,
    uint8_t priority,
    const char* voiceCommand,
    const char* description,
    const char* imageBase64,
    const char* audioBase64
) {
    JsonDocument doc;

    // Device identification
    doc["device_type"] = "sims-smart";
    doc["device_id"] = "xiao-esp32s3-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    // Location data
    doc["latitude"] = latitude;
    doc["longitude"] = longitude;
    if (altitude > 0) {
        doc["altitude"] = altitude;
    }

    // Incident metadata
    doc["priority"] = getPriorityString(priority);
    doc["timestamp"] = millis();  // Device uptime timestamp
    doc["description"] = description;

    // Voice command metadata (unique to sims-smart)
    if (voiceCommand != nullptr && strlen(voiceCommand) > 0) {
        doc["voice_command"] = voiceCommand;
    }

    // Embedded media (base64)
    if (imageBase64 != nullptr) {
        doc["has_image"] = true;
        doc["image"] = imageBase64;
    } else {
        doc["has_image"] = false;
    }

    if (audioBase64 != nullptr) {
        doc["has_audio"] = true;
        doc["audio"] = audioBase64;
    } else {
        doc["has_audio"] = false;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

String HTTPClientService::base64Encode(uint8_t* data, size_t length) {
    return base64::encode(data, length);
}

String HTTPClientService::getPriorityString(uint8_t priority) {
    switch (priority) {
        case PRIORITY_CRITICAL: return "critical";
        case PRIORITY_HIGH: return "high";
        case PRIORITY_MEDIUM: return "medium";
        case PRIORITY_LOW: return "low";
        default: return "medium";
    }
}
