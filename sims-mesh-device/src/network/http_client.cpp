/**
 * HTTP Client Service Implementation
 */

#include "network/http_client.h"

HTTPClientService::HTTPClientService() {
}

HTTPClientService::~HTTPClientService() {
    end();
}

bool HTTPClientService::begin(const char* backendHost, int backendPort) {
    backendBaseURL = "http://" + String(backendHost) + ":" + String(backendPort);
    Serial.printf("[HTTP] Backend URL: %s\n", backendBaseURL.c_str());
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
    uint8_t category,
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

    String imageURL = "";
    String audioURL = "";

    // Upload media files first (if present)
    if (imageData != nullptr && imageSize > 0) {
        Serial.printf("[HTTP] Uploading image (%d bytes)...\n", imageSize);
        if (!uploadMedia("/api/media/upload", imageData, imageSize, "image/jpeg", imageURL)) {
            result.message = "Image upload failed";
            Serial.println("[HTTP] ERROR: Image upload failed");
            // Continue anyway - incident without image is better than no incident
        } else {
            Serial.printf("[HTTP] Image uploaded: %s\n", imageURL.c_str());
        }
    }

    if (audioData != nullptr && audioSize > 0) {
        Serial.printf("[HTTP] Uploading audio (%d bytes)...\n", audioSize);
        if (!uploadMedia("/api/media/upload", audioData, audioSize, "audio/opus", audioURL)) {
            result.message = "Audio upload failed";
            Serial.println("[HTTP] ERROR: Audio upload failed");
            // Continue anyway
        } else {
            Serial.printf("[HTTP] Audio uploaded: %s\n", audioURL.c_str());
        }
    }

    // Build JSON payload
    String jsonPayload = buildIncidentJSON(
        latitude, longitude, altitude,
        priority, category, description,
        imageURL.isEmpty() ? nullptr : imageURL.c_str(),
        audioURL.isEmpty() ? nullptr : audioURL.c_str()
    );

    Serial.printf("[HTTP] Uploading incident JSON (%d bytes)...\n", jsonPayload.length());

    // POST to /api/incidents
    String url = backendBaseURL + "/api/incidents";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
        result.httpCode = httpCode;

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = http.getString();
            Serial.printf("[HTTP] Incident uploaded successfully (HTTP %d)\n", httpCode);
            Serial.printf("[HTTP] Response: %s\n", response.c_str());

            // Parse incident ID from response
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);
            if (!error && doc.containsKey("id")) {
                result.incidentId = doc["id"].as<String>();
            }

            result.success = true;
            result.message = "Incident uploaded";
        } else {
            String response = http.getString();
            result.message = "HTTP error: " + String(httpCode);
            Serial.printf("[HTTP] ERROR: HTTP %d - %s\n", httpCode, response.c_str());
        }
    } else {
        result.message = "Connection failed: " + http.errorToString(httpCode);
        Serial.printf("[HTTP] ERROR: Connection failed - %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return result;
}

HTTPClientService::IncidentUploadResult HTTPClientService::uploadIncidentJSON(
    float latitude,
    float longitude,
    uint8_t priority,
    uint8_t category,
    const char* description
) {
    return uploadIncident(latitude, longitude, 0.0, priority, category, description, nullptr, 0, nullptr, 0);
}

bool HTTPClientService::ping() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    String url = backendBaseURL + "/api/health";
    http.begin(url);
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
    uint8_t category,
    const char* description,
    const char* imageURL,
    const char* audioURL
) {
    JsonDocument doc;

    // Location data
    doc["latitude"] = latitude;
    doc["longitude"] = longitude;
    doc["altitude"] = altitude;

    // Incident metadata
    doc["priority"] = getPriorityString(priority);
    doc["category"] = getCategoryString(category);
    doc["description"] = description;
    doc["timestamp"] = millis();  // Device timestamp

    // Device info
    doc["deviceId"] = String((uint32_t)ESP.getEfuseMac(), HEX);
    doc["source"] = "mesh_device";

    // Media URLs (if uploaded)
    if (imageURL != nullptr) {
        doc["imageURL"] = imageURL;
    }
    if (audioURL != nullptr) {
        doc["audioURL"] = audioURL;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

bool HTTPClientService::uploadMedia(
    const char* endpoint,
    uint8_t* data,
    size_t dataSize,
    const char* contentType,
    String& mediaURL
) {
    String url = backendBaseURL + endpoint;
    http.begin(url);
    http.addHeader("Content-Type", contentType);

    int httpCode = http.POST(data, dataSize);

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String response = http.getString();

        // Parse media URL from response
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, response);
        if (!error && doc.containsKey("url")) {
            mediaURL = doc["url"].as<String>();
            http.end();
            return true;
        }
    }

    http.end();
    return false;
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

String HTTPClientService::getCategoryString(uint8_t category) {
    // Map category codes to strings (customize based on your schema)
    switch (category) {
        case 0: return "unknown";
        case 1: return "vehicle";
        case 2: return "drone";
        case 3: return "person";
        case 4: return "natural_disaster";
        case 5: return "fire";
        case 6: return "medical";
        default: return "other";
    }
}
