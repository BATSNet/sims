/**
 * HTTP Client Service Implementation - ESP-IDF Port
 *
 * Uses esp_http_client API and mbedtls for base64 encoding.
 */

#include "network/http_client.h"
#include "network/wifi_service.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_crt_bundle.h"
#include "mbedtls/base64.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "HTTP";

// Response buffer for HTTP client
static char responseBuffer[1024];
static int responseLen = 0;

// HTTP event handler to capture response body
static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (responseLen + evt->data_len < (int)sizeof(responseBuffer) - 1) {
                memcpy(responseBuffer + responseLen, evt->data, evt->data_len);
                responseLen += evt->data_len;
                responseBuffer[responseLen] = 0;
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

HTTPClientService::HTTPClientService() {
    memset(backendURL, 0, sizeof(backendURL));
}

HTTPClientService::~HTTPClientService() {
    end();
}

bool HTTPClientService::begin(const char* url) {
    strncpy(backendURL, url, sizeof(backendURL) - 1);
    ESP_LOGI(TAG, "Backend URL: %s", backendURL);
    return true;
}

void HTTPClientService::end() {
    // Nothing to clean up
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
    IncidentUploadResult result = {};
    result.success = false;
    result.httpCode = 0;
    strncpy(result.message, "Unknown error", sizeof(result.message) - 1);
    result.incidentId[0] = 0;

    // Encode media as base64 if present
    char* imageBase64 = nullptr;
    char* audioBase64 = nullptr;
    size_t imageB64Len = 0;
    size_t audioB64Len = 0;

    if (imageData && imageSize > 0) {
        ESP_LOGI(TAG, "Encoding image (%d bytes)...", imageSize);
        imageBase64 = base64Encode(imageData, imageSize, &imageB64Len);
        if (imageBase64) {
            ESP_LOGI(TAG, "Image encoded (%d bytes base64)", imageB64Len);
        }
    }

    if (audioData && audioSize > 0) {
        ESP_LOGI(TAG, "Encoding audio (%d bytes)...", audioSize);
        audioBase64 = base64Encode(audioData, audioSize, &audioB64Len);
        if (audioBase64) {
            ESP_LOGI(TAG, "Audio encoded (%d bytes base64)", audioB64Len);
        }
    }

    // Build JSON payload
    size_t jsonLen = 0;
    char* jsonPayload = buildIncidentJSON(
        latitude, longitude, altitude,
        priority, voiceCommand, description,
        imageBase64, audioBase64, &jsonLen
    );

    // Free base64 buffers (no longer needed after JSON build)
    if (imageBase64) free(imageBase64);
    if (audioBase64) free(audioBase64);

    if (!jsonPayload) {
        strncpy(result.message, "Failed to build JSON payload", sizeof(result.message) - 1);
        ESP_LOGE(TAG, "%s", result.message);
        return result;
    }

    ESP_LOGI(TAG, "Uploading incident (%d bytes JSON)...", jsonLen);

    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = backendURL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = API_TIMEOUT_MS;
    config.event_handler = http_event_handler;
    // Only attach TLS cert bundle for HTTPS URLs
    if (strncmp(backendURL, "https://", 8) == 0) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(jsonPayload);
        strncpy(result.message, "Failed to init HTTP client", sizeof(result.message) - 1);
        ESP_LOGE(TAG, "%s", result.message);
        return result;
    }

    // Set headers and body
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, jsonPayload, jsonLen);

    // Clear response buffer
    responseLen = 0;
    responseBuffer[0] = 0;

    // Perform request
    esp_err_t err = esp_http_client_perform(client);
    result.httpCode = esp_http_client_get_status_code(client);

    if (err == ESP_OK) {
        if (result.httpCode == 200 || result.httpCode == 201) {
            ESP_LOGI(TAG, "Success (HTTP %d)", result.httpCode);

            // Parse incident ID from response
            if (responseLen > 0) {
                cJSON* root = cJSON_Parse(responseBuffer);
                if (root) {
                    cJSON* id = cJSON_GetObjectItem(root, "id");
                    if (id && cJSON_IsString(id)) {
                        strncpy(result.incidentId, id->valuestring, sizeof(result.incidentId) - 1);
                        ESP_LOGI(TAG, "Incident ID: %s", result.incidentId);
                    }
                    cJSON_Delete(root);
                }
            }

            result.success = true;
            strncpy(result.message, "Uploaded successfully", sizeof(result.message) - 1);
        } else {
            snprintf(result.message, sizeof(result.message), "HTTP error: %d", result.httpCode);
            ESP_LOGE(TAG, "HTTP %d - %s", result.httpCode, responseBuffer);
        }
    } else {
        snprintf(result.message, sizeof(result.message), "Connection failed: %s",
                 esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", result.message);
    }

    esp_http_client_cleanup(client);
    free(jsonPayload);
    return result;
}

HTTPClientService::IncidentUploadResult HTTPClientService::uploadIncidentJSON(
    float latitude,
    float longitude,
    uint8_t priority,
    const char* description
) {
    return uploadIncident(latitude, longitude, 0.0f, priority, "", description,
                          nullptr, 0, nullptr, 0);
}

bool HTTPClientService::ping() {
    // Extract base URL
    char healthURL[280];
    char baseURL[256];
    strncpy(baseURL, backendURL, sizeof(baseURL) - 1);
    baseURL[sizeof(baseURL) - 1] = 0;

    char* apiPos = strstr(baseURL, "/api/");
    if (apiPos) {
        *apiPos = 0;
    }
    snprintf(healthURL, sizeof(healthURL), "%s/api/health", baseURL);

    esp_http_client_config_t config = {};
    config.url = healthURL;
    config.timeout_ms = 5000;
    config.event_handler = http_event_handler;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;

    responseLen = 0;
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    bool success = (err == ESP_OK && status == 200);
    ESP_LOGI(TAG, "Health check: %s (HTTP %d)", success ? "OK" : "FAILED", status);
    return success;
}

char* HTTPClientService::buildIncidentJSON(
    float latitude,
    float longitude,
    float altitude,
    uint8_t priority,
    const char* voiceCommand,
    const char* description,
    const char* imageBase64,
    const char* audioBase64,
    size_t* outLen
) {
    cJSON* doc = cJSON_CreateObject();
    if (!doc) {
        *outLen = 0;
        return nullptr;
    }

    // Device identification
    char* deviceId = getDeviceId();
    cJSON_AddStringToObject(doc, "device_type", DEVICE_TYPE);
    cJSON_AddStringToObject(doc, "device_id", deviceId);

    // Title (required by backend)
    char title[128];
    snprintf(title, sizeof(title), "Voice report from %s", deviceId);
    cJSON_AddStringToObject(doc, "title", title);
    free(deviceId);

    // Location data
    cJSON_AddNumberToObject(doc, "latitude", latitude);
    cJSON_AddNumberToObject(doc, "longitude", longitude);
    if (altitude > 0) {
        cJSON_AddNumberToObject(doc, "altitude", altitude);
    }

    // Incident metadata
    cJSON_AddStringToObject(doc, "priority", getPriorityString(priority));
    cJSON_AddStringToObject(doc, "description", description ? description : "");

    // Voice command metadata
    if (voiceCommand && strlen(voiceCommand) > 0) {
        cJSON_AddStringToObject(doc, "voice_command", voiceCommand);
    }

    // Embedded media
    if (imageBase64) {
        cJSON_AddBoolToObject(doc, "has_image", true);
        cJSON_AddStringToObject(doc, "image", imageBase64);
    } else {
        cJSON_AddBoolToObject(doc, "has_image", false);
    }

    if (audioBase64) {
        cJSON_AddBoolToObject(doc, "has_audio", true);
        cJSON_AddStringToObject(doc, "audio", audioBase64);
    } else {
        cJSON_AddBoolToObject(doc, "has_audio", false);
    }

    // Serialize to string
    char* jsonStr = cJSON_PrintUnformatted(doc);
    cJSON_Delete(doc);

    if (jsonStr) {
        *outLen = strlen(jsonStr);
    } else {
        *outLen = 0;
    }
    return jsonStr;
}

char* HTTPClientService::base64Encode(const uint8_t* data, size_t length, size_t* outLen) {
    // Calculate output size (base64 expands ~4/3)
    size_t bufferSize = ((length + 2) / 3) * 4 + 1;
    char* output = (char*)malloc(bufferSize);
    if (!output) {
        ESP_LOGE(TAG, "Failed to allocate base64 buffer (%d bytes)", bufferSize);
        *outLen = 0;
        return nullptr;
    }

    size_t written = 0;
    int ret = mbedtls_base64_encode((unsigned char*)output, bufferSize,
                                    &written, data, length);
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 encode failed: %d", ret);
        free(output);
        *outLen = 0;
        return nullptr;
    }

    output[written] = 0;
    *outLen = written;
    return output;
}

const char* HTTPClientService::getPriorityString(uint8_t priority) {
    switch (priority) {
        case PRIORITY_CRITICAL: return "critical";
        case PRIORITY_HIGH:     return "high";
        case PRIORITY_MEDIUM:   return "medium";
        case PRIORITY_LOW:      return "low";
        default:                return "medium";
    }
}

char* HTTPClientService::getDeviceId() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);

    char* id = (char*)malloc(32);
    if (id) {
        snprintf(id, 32, "xiao-esp32s3-%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return id;
}
