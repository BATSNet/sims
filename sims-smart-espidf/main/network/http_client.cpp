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

#if USE_BINARY_FORMAT
    // Build compact binary payload (no base64 overhead)
    size_t payloadLen = 0;
    uint8_t* payload = buildIncidentBinary(
        latitude, longitude, altitude,
        priority, description,
        imageData, imageSize,
        audioData, audioSize,
        &payloadLen
    );

    if (!payload) {
        strncpy(result.message, "Failed to build binary payload", sizeof(result.message) - 1);
        ESP_LOGE(TAG, "%s", result.message);
        return result;
    }

    ESP_LOGI(TAG, "Uploading incident (%d bytes binary)...", payloadLen);
    const char* contentType = "application/octet-stream";
#else
    // Legacy JSON+base64 path
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

    size_t payloadLen = 0;
    char* payload = buildIncidentJSON(
        latitude, longitude, altitude,
        priority, voiceCommand, description,
        imageBase64, audioBase64, &payloadLen
    );

    if (imageBase64) free(imageBase64);
    if (audioBase64) free(audioBase64);

    if (!payload) {
        strncpy(result.message, "Failed to build JSON payload", sizeof(result.message) - 1);
        ESP_LOGE(TAG, "%s", result.message);
        return result;
    }

    ESP_LOGI(TAG, "Uploading incident (%d bytes JSON)...", payloadLen);
    const char* contentType = "application/json";
#endif

    // Configure HTTP client
    esp_http_client_config_t config = {};
    config.url = backendURL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = API_TIMEOUT_MS;
    config.event_handler = http_event_handler;
    if (strncmp(backendURL, "https://", 8) == 0) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(payload);
        strncpy(result.message, "Failed to init HTTP client", sizeof(result.message) - 1);
        ESP_LOGE(TAG, "%s", result.message);
        return result;
    }

    esp_http_client_set_header(client, "Content-Type", contentType);
    esp_http_client_set_post_field(client, (const char*)payload, payloadLen);

    responseLen = 0;
    responseBuffer[0] = 0;

    esp_err_t err = esp_http_client_perform(client);
    result.httpCode = esp_http_client_get_status_code(client);

    if (err == ESP_OK) {
        if (result.httpCode == 200 || result.httpCode == 201) {
            ESP_LOGI(TAG, "Success (HTTP %d)", result.httpCode);

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
    free(payload);
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

HTTPClientService::TranscribeResult HTTPClientService::transcribeAudio(
    const uint8_t* pcmData, size_t pcmSize
) {
    TranscribeResult result = {};
    result.success = false;
    result.httpCode = 0;
    result.text[0] = 0;
    strncpy(result.error, "Unknown error", sizeof(result.error) - 1);

    if (!pcmData || pcmSize == 0) {
        strncpy(result.error, "No audio data", sizeof(result.error) - 1);
        return result;
    }

    // Build transcribe URL from backend base URL
    char transcribeURL[280];
    char baseURL[256];
    strncpy(baseURL, backendURL, sizeof(baseURL) - 1);
    baseURL[sizeof(baseURL) - 1] = 0;

    // Strip trailing path to get base (e.g. http://host:8000/api/lora/incident -> http://host:8000)
    char* apiPos = strstr(baseURL, "/api/");
    if (apiPos) {
        *apiPos = 0;
    }
    snprintf(transcribeURL, sizeof(transcribeURL), "%s/api/lora/transcribe", baseURL);

    ESP_LOGI(TAG, "Transcribing %d bytes of PCM audio via %s", (int)pcmSize, transcribeURL);

    esp_http_client_config_t config = {};
    config.url = transcribeURL;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = AUDIO_TRANSCRIBE_TIMEOUT_MS;
    config.event_handler = http_event_handler;
    if (strncmp(transcribeURL, "https://", 8) == 0) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        strncpy(result.error, "Failed to init HTTP client", sizeof(result.error) - 1);
        ESP_LOGE(TAG, "%s", result.error);
        return result;
    }

    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field(client, (const char*)pcmData, pcmSize);

    responseLen = 0;
    responseBuffer[0] = 0;

    esp_err_t err = esp_http_client_perform(client);
    result.httpCode = esp_http_client_get_status_code(client);

    if (err == ESP_OK) {
        if (result.httpCode == 200) {
            // Parse JSON response: {"text": "transcribed text"}
            if (responseLen > 0) {
                cJSON* root = cJSON_Parse(responseBuffer);
                if (root) {
                    cJSON* textItem = cJSON_GetObjectItem(root, "text");
                    if (textItem && cJSON_IsString(textItem)) {
                        strncpy(result.text, textItem->valuestring, sizeof(result.text) - 1);
                        result.success = true;
                        result.error[0] = 0;
                        ESP_LOGI(TAG, "Transcription: \"%s\"", result.text);
                    } else {
                        strncpy(result.error, "No 'text' field in response", sizeof(result.error) - 1);
                    }
                    cJSON_Delete(root);
                } else {
                    strncpy(result.error, "Failed to parse JSON response", sizeof(result.error) - 1);
                }
            } else {
                strncpy(result.error, "Empty response body", sizeof(result.error) - 1);
            }
        } else {
            snprintf(result.error, sizeof(result.error), "HTTP error: %d", result.httpCode);
            ESP_LOGE(TAG, "Transcribe HTTP %d - %s", result.httpCode, responseBuffer);
        }
    } else {
        snprintf(result.error, sizeof(result.error), "Connection failed: %s",
                 esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", result.error);
    }

    esp_http_client_cleanup(client);
    return result;
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

uint8_t* HTTPClientService::buildIncidentBinary(
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
) {
    // Cap description at 255 bytes
    size_t descLen = description ? strlen(description) : 0;
    if (descLen > 255) descLen = 255;

    // Total: 19 fixed + desc + 2 (img_len) + img + 4 (aud_len uint32) + aud
    size_t totalSize = 19 + descLen + 2 + imageSize + 4 + audioSize;

    uint8_t* buf = (uint8_t*)malloc(totalSize);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate binary buffer (%d bytes)", totalSize);
        *outLen = 0;
        return nullptr;
    }

    size_t pos = 0;

    // [0] Version
    buf[pos++] = BINARY_FORMAT_VERSION;

    // [1] Flags: bit0=has_image, bit1=has_audio, bit2-3=priority, bit4=raw 8kHz PCM
    uint8_t flags = 0;
    if (imageData && imageSize > 0) flags |= 0x01;
    if (audioData && audioSize > 0) {
        flags |= 0x02;
        flags |= 0x10;  // raw 8kHz 16-bit PCM audio
    }
    flags |= (priority & 0x03) << 2;
    buf[pos++] = flags;

    // [2-5] Latitude as int32 LE (lat * 1e7)
    int32_t latInt = (int32_t)(latitude * 1e7);
    memcpy(buf + pos, &latInt, 4);
    pos += 4;

    // [6-9] Longitude as int32 LE (lon * 1e7)
    int32_t lonInt = (int32_t)(longitude * 1e7);
    memcpy(buf + pos, &lonInt, 4);
    pos += 4;

    // [10-11] Altitude as int16 LE (meters)
    int16_t altInt = (int16_t)altitude;
    memcpy(buf + pos, &altInt, 2);
    pos += 2;

    // [12-17] Device MAC (6 bytes)
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    memcpy(buf + pos, mac, 6);
    pos += 6;

    // [18] Description length
    buf[pos++] = (uint8_t)descLen;

    // [19..] Description UTF-8
    if (descLen > 0) {
        memcpy(buf + pos, description, descLen);
        pos += descLen;
    }

    // Image length (uint16 LE) + data
    uint16_t imgLen = (imageData && imageSize > 0) ? (uint16_t)imageSize : 0;
    memcpy(buf + pos, &imgLen, 2);
    pos += 2;
    if (imgLen > 0) {
        memcpy(buf + pos, imageData, imgLen);
        pos += imgLen;
    }

    // Audio length (uint32 LE) + data
    uint32_t audLen = (audioData && audioSize > 0) ? (uint32_t)audioSize : 0;
    memcpy(buf + pos, &audLen, 4);
    pos += 4;
    if (audLen > 0) {
        memcpy(buf + pos, audioData, audLen);
        pos += audLen;
    }

    *outLen = pos;
    ESP_LOGI(TAG, "Binary payload: %d bytes (desc=%d, img=%d, aud=%d)",
             pos, descLen, imgLen, audLen);
    return buf;
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
