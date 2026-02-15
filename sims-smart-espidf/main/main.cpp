/**
 * SIMS-SMART Device - ESP-IDF Port
 *
 * Voice-controlled hands-free incident reporting
 * Hardware: Seeed Studio XIAO ESP32S3 Sense
 *
 * Architecture:
 * - main_task: State machine, service coordination, BLE/HTTP uploads (Core 0)
 * - voice_task: Continuous audio capture, WakeNet9, MultiNet7 (Core 1)
 * - GPS updated periodically from main_task
 * - LED updated from main_task
 *
 * Flow: "Hi ESP" -> MultiNet command words -> [optional photo] -> send via BLE mesh (WiFi fallback)
 */

#include <stdio.h>
#include <string.h>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_mac.h"
#include "esp_heap_caps.h"

#include "model_path.h"
#include "config.h"
#include "mesh/mesh_ble_client.h"
#include "network/wifi_service.h"
#include "network/http_client.h"
#include "sensors/gps_service.h"
#include "sensors/camera_service.h"
#include "sensors/audio_service.h"
#include "voice/wake_word_service.h"
#include "voice/command_parser.h"
#include "led/led_feedback.h"
#include "power/power_manager.h"
#include "display/display_manager.h"
#include "input/button_handler.h"
#include "driver/gpio.h"

static const char *TAG = "MAIN";

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

typedef enum {
    STATE_INIT,
    STATE_IDLE,                  // Listening for wake word
    STATE_WAKE_DETECTED,         // Wake word detected, brief feedback
    STATE_LISTENING_COMMAND,     // MultiNet7 listening for command words
    STATE_RECORDING_VOICE,       // Dedicated clean audio recording
    STATE_CAPTURING_IMAGE,       // Taking photo
    STATE_PROCESSING,            // Getting GPS, building payload
    STATE_SENDING,               // Sending via BLE mesh (WiFi fallback)
    STATE_SUCCESS,               // Send successful
    STATE_ERROR                  // Error occurred
} system_state_t;

static const char* stateToString(system_state_t s) {
    switch (s) {
        case STATE_INIT:             return "INIT";
        case STATE_IDLE:             return "IDLE";
        case STATE_WAKE_DETECTED:    return "WAKE_DETECTED";
        case STATE_LISTENING_COMMAND:return "LISTENING_COMMAND";
        case STATE_RECORDING_VOICE:  return "RECORDING_VOICE";
        case STATE_CAPTURING_IMAGE:  return "CAPTURING_IMAGE";
        case STATE_PROCESSING:       return "PROCESSING";
        case STATE_SENDING:          return "SENDING";
        case STATE_SUCCESS:          return "SUCCESS";
        case STATE_ERROR:            return "ERROR";
        default:                     return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Voice events for inter-task communication
// ---------------------------------------------------------------------------

typedef enum {
    EVENT_NONE,
    EVENT_WAKE_DETECTED,
    EVENT_COMMAND_RECOGNIZED
} voice_event_type_t;

typedef struct {
    voice_event_type_t type;
    int command_id;  // word ID for EVENT_COMMAND_RECOGNIZED
} voice_event_t;

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

static system_state_t current_state = STATE_INIT;

// Description buffer (built from MultiNet7 command words)
static char commandDescription[512];
static bool wantPhoto = false;  // set true if user says photo/picture/snapshot

// Timing
static unsigned long stateEntryTime = 0;

// ---------------------------------------------------------------------------
// Inter-task communication
// ---------------------------------------------------------------------------

static QueueHandle_t voice_event_queue = NULL;

// Shared flag: main task tells voice task whether to listen for wake word
static volatile bool voiceListenForWakeWord = true;

// Shared flag: main task tells voice task to listen for command words
static volatile bool voiceListenForCommand = false;

// Shared flag: pause voice task entirely (during audio recording in main task)
static volatile bool voiceTaskPaused = false;

// ---------------------------------------------------------------------------
// Task handles
// ---------------------------------------------------------------------------

static TaskHandle_t main_task_handle = NULL;
static TaskHandle_t voice_task_handle = NULL;

// Camera background capture
static volatile bool cameraCaptureRunning = false;
static volatile bool cameraCaptureSuccess = false;
static SemaphoreHandle_t cameraCaptureDone = NULL;

// ---------------------------------------------------------------------------
// Global services
// ---------------------------------------------------------------------------

static MeshBleClient   meshBle;
static WiFiService      wifi;
static HTTPClientService httpClient;
static GPSService       gps;
static CameraService    camera;
static AudioService     audio;
static WakeWordService  wakeWord;
static CommandParser    cmdParser;
static LEDFeedback      led;
static PowerManager     power;
static DisplayManager   display;
static ButtonHandler    buttons;
static bool             flashlightOn = false;

// Voice recording buffer (clean PCM from dedicated recording phase)
static uint8_t* voiceRecording = nullptr;
static size_t voiceRecordingSize = 0;

// Offline incident buffer - stores one pending payload in PSRAM for retry
static uint8_t*         pendingPayload = nullptr;
static size_t           pendingPayloadLen = 0;
static unsigned long    pendingRetryTime = 0;
#define PENDING_RETRY_INTERVAL_MS 10000  // retry every 10 seconds

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static void main_task(void *pvParameters);
static void voice_task(void *pvParameters);
static void handle_state_transition(system_state_t newState);
static void handle_error(const char* message);
static void print_system_info(void);
static void print_status(void);
static void initialize_nvs(void);
static unsigned long millis_now(void);
static void savePendingPayload(const uint8_t* payload, size_t len);
static bool trySendPendingPayload(void);
// Helper: milliseconds since boot
// ---------------------------------------------------------------------------

static unsigned long millis_now(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

// Escape double quotes in a string for JSON embedding (e.g. nested JSON values)
static size_t escapeJsonQuotes(const char* src, char* dst, size_t dstSize) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dstSize - 1; i++) {
        if (src[i] == '"') {
            if (j + 2 >= dstSize) break;
            dst[j++] = '\\';
            dst[j++] = '"';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
    return j;
}

// Apply 4x software gain to PCM buffer (compensate for quiet PDM mic)
static void applyGain(uint8_t* pcmBuffer, size_t byteLen) {
    int16_t* samples = (int16_t*)pcmBuffer;
    size_t count = byteLen / sizeof(int16_t);
    for (size_t i = 0; i < count; i++) {
        int32_t amplified = (int32_t)samples[i] << 2;
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        samples[i] = (int16_t)amplified;
    }
}

// ---------------------------------------------------------------------------
// Offline incident buffer - save and retry
// ---------------------------------------------------------------------------

static void savePendingPayload(const uint8_t* payload, size_t len) {
    // Free any existing pending payload
    if (pendingPayload) {
        heap_caps_free(pendingPayload);
        pendingPayload = nullptr;
        pendingPayloadLen = 0;
    }

    // Allocate in PSRAM
    pendingPayload = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (!pendingPayload) {
        pendingPayload = (uint8_t*)malloc(len);
    }
    if (!pendingPayload) {
        ESP_LOGE(TAG, "Failed to allocate pending payload buffer (%d bytes)", (int)len);
        return;
    }

    memcpy(pendingPayload, payload, len);
    pendingPayloadLen = len;
    pendingRetryTime = millis_now() + PENDING_RETRY_INTERVAL_MS;
    ESP_LOGW(TAG, "Saved %d bytes as pending payload (will retry when connected)", (int)len);
}

static bool trySendPendingPayload(void) {
    if (!pendingPayload || pendingPayloadLen == 0) return false;
    if (millis_now() < pendingRetryTime) return false;

    // Check if we have connectivity
    bool haveMesh = meshBle.isReady();
    bool haveWifi = wifi.isConnected();

    if (!haveMesh && !haveWifi) {
        pendingRetryTime = millis_now() + PENDING_RETRY_INTERVAL_MS;
        return false;
    }

    ESP_LOGI(TAG, "Retrying pending payload (%d bytes)...", (int)pendingPayloadLen);
    bool sent = false;

    if (haveMesh) {
        sent = meshBle.sendPayload(pendingPayload, pendingPayloadLen);
        if (sent) {
            ESP_LOGI(TAG, "Pending payload sent via BLE mesh");
        }
    }

    if (!sent && haveWifi) {
        // Direct HTTP POST of the binary payload
        esp_http_client_config_t config = {};
        config.url = BACKEND_URL;
        config.method = HTTP_METHOD_POST;
        config.timeout_ms = API_TIMEOUT_MS;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client) {
            esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
            esp_http_client_set_post_field(client, (const char*)pendingPayload, pendingPayloadLen);

            esp_err_t err = esp_http_client_perform(client);
            int status = esp_http_client_get_status_code(client);
            esp_http_client_cleanup(client);

            if (err == ESP_OK && (status == 200 || status == 201)) {
                sent = true;
                ESP_LOGI(TAG, "Pending payload sent via WiFi (HTTP %d)", status);
            } else {
                ESP_LOGW(TAG, "Pending payload WiFi retry failed (HTTP %d, err=%s)",
                         status, esp_err_to_name(err));
            }
        }
    }

    if (sent) {
        heap_caps_free(pendingPayload);
        pendingPayload = nullptr;
        pendingPayloadLen = 0;
        ESP_LOGI(TAG, "Pending payload cleared after successful send");
        // Brief LED feedback
        led.setState(LEDFeedback::STATE_SUCCESS);
        display.showScreen(DisplayManager::SCREEN_SUCCESS);
    } else {
        pendingRetryTime = millis_now() + PENDING_RETRY_INTERVAL_MS;
        ESP_LOGW(TAG, "Pending payload retry failed, will try again in %d ms", PENDING_RETRY_INTERVAL_MS);
    }

    return sent;
}

// ---------------------------------------------------------------------------
// Camera background capture task
// ---------------------------------------------------------------------------

static void camera_capture_task(void *pvParameters) {
    cameraCaptureSuccess = false;

#ifdef HAS_CAMERA
    // Lazy init camera on first capture
    if (!camera.isInitialized()) {
        ESP_LOGI(TAG, "Camera task: initializing camera...");
        if (!camera.begin()) {
            ESP_LOGE(TAG, "Camera task: init failed");
            cameraCaptureRunning = false;
            xSemaphoreGive(cameraCaptureDone);
            vTaskDelete(NULL);
            return;
        }
    }

    if (camera.capture()) {
        ESP_LOGI(TAG, "Camera task: captured %d bytes", camera.getImageSize());
        cameraCaptureSuccess = true;
    } else {
        ESP_LOGE(TAG, "Camera task: capture failed");
    }
#else
    ESP_LOGW(TAG, "Camera task: camera not available");
#endif

    cameraCaptureRunning = false;
    xSemaphoreGive(cameraCaptureDone);
    vTaskDelete(NULL);
}

static void startCameraCapture() {
    if (cameraCaptureRunning) return;  // already running

    cameraCaptureRunning = true;
    cameraCaptureSuccess = false;
    xSemaphoreTake(cameraCaptureDone, 0);  // reset semaphore

    xTaskCreatePinnedToCore(
        camera_capture_task, "cam_task", 16384, NULL,
        5, NULL, 0);  // Core 0, lower priority than voice
    ESP_LOGI(TAG, "Camera capture started in background");
}

// Track last word recognition time for timeout reset
static unsigned long lastWordTime = 0;

// ---------------------------------------------------------------------------
// app_main - entry point
// ---------------------------------------------------------------------------

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SIMS-SMART Device - ESP-IDF Port");
    ESP_LOGI(TAG, "Version: %s", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "========================================");

    print_system_info();
    initialize_nvs();

    // Create inter-task queue
    voice_event_queue = xQueueCreate(10, sizeof(voice_event_t));
    if (voice_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create voice event queue");
        return;
    }

    // Create camera capture semaphore
    cameraCaptureDone = xSemaphoreCreateBinary();
    xSemaphoreGive(cameraCaptureDone);  // start in "done" state

    // Main task on Core 0 (networking, state machine)
    BaseType_t ret = xTaskCreatePinnedToCore(
        main_task, "main_task", TASK_STACK_MAIN, NULL,
        TASK_PRIORITY_MAIN, &main_task_handle, 0);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create main task");
        return;
    }

    ESP_LOGI(TAG, "System boot complete");
}

// ---------------------------------------------------------------------------
// Main task - service init + state machine
// ---------------------------------------------------------------------------

static void main_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Main task started on core %d", xPortGetCoreID());

    // ---- Initialize services ----

    // LED first for visual feedback during init
    if (!led.begin()) {
        ESP_LOGE(TAG, "LED initialization failed");
    }
    led.setState(LEDFeedback::STATE_PROCESSING);

    // BLE mesh client (primary transport - init before WiFi)
    ESP_LOGI(TAG, "Initializing BLE mesh client...");
    if (!meshBle.begin()) {
        ESP_LOGW(TAG, "BLE mesh init failed - WiFi will be primary");
    } else {
        ESP_LOGI(TAG, "BLE mesh client initialized, will scan for mesh devices");
    }

    // WiFi (fallback transport)
    ESP_LOGI(TAG, "Initializing WiFi...");
    if (!wifi.begin()) {
        ESP_LOGW(TAG, "WiFi init failed - will retry in background");
    }

    // HTTP client
    httpClient.begin(BACKEND_URL);

    // GPS
#ifdef GPS_ENABLED
    ESP_LOGI(TAG, "Initializing GPS...");
    if (!gps.begin(GPS_RX_PIN, GPS_TX_PIN)) {
        ESP_LOGE(TAG, "GPS initialization failed");
    }
#endif

    // Camera - lazy init on first capture to avoid FB-OVF spam
    ESP_LOGI(TAG, "Camera: deferred init (will init on first capture)");

    // Audio (I2S PDM microphone)
#ifdef HAS_MICROPHONE
    ESP_LOGI(TAG, "Initializing microphone...");
    if (!audio.begin()) {
        ESP_LOGE(TAG, "Audio initialization failed");
    }
#endif

    // Load ESP32-SR models from flash partition
    ESP_LOGI(TAG, "Loading voice models from flash...");
    srmodel_list_t *models = esp_srmodel_init("model");
    if (models == NULL) {
        ESP_LOGE(TAG, "Failed to load SR models from 'model' partition");
    } else {
        ESP_LOGI(TAG, "Loaded %d SR models", models->num);
    }

    // Voice recognition
    ESP_LOGI(TAG, "Initializing wake word detection...");
    if (!wakeWord.begin(WAKE_WORD)) {
        ESP_LOGW(TAG, "Wake word init failed");
    }

    ESP_LOGI(TAG, "Initializing command parser...");
    if (!cmdParser.begin()) {
        ESP_LOGW(TAG, "Command parser init failed");
    }

    // Power manager
    power.begin();

    // OLED Display
    ESP_LOGI(TAG, "Initializing OLED display...");
    if (!display.begin(OLED_SDA_PIN, OLED_SCL_PIN, OLED_I2C_ADDR)) {
        ESP_LOGW(TAG, "OLED display not connected (will work without it)");
    }

    // Physical buttons
    ESP_LOGI(TAG, "Initializing buttons...");
    if (!buttons.begin()) {
        ESP_LOGW(TAG, "Button init failed (will work without buttons)");
    }

    // Flashlight LED output
    gpio_config_t flash_conf = {};
    flash_conf.pin_bit_mask = (1ULL << FLASHLIGHT_PIN);
    flash_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&flash_conf);
    gpio_set_level((gpio_num_t)FLASHLIGHT_PIN, 0);

    // Start voice task on Core 1 (dedicated to audio processing)
    BaseType_t ret = xTaskCreatePinnedToCore(
        voice_task, "voice_task", TASK_STACK_VOICE, NULL,
        TASK_PRIORITY_VOICE, &voice_task_handle, 1);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create voice task");
    }

    ESP_LOGI(TAG, "All services initialized");
    ESP_LOGI(TAG, "Ready for voice commands - say \"%s\"", WAKE_WORD);
    handle_state_transition(STATE_IDLE);

    // ---- Main loop: state machine ----

    unsigned long lastStatusPrint = 0;

    while (1) {
        // Update background services
        meshBle.update();
        wifi.update();
        led.update();

#ifdef GPS_ENABLED
        gps.update();
#endif

        // State machine
        switch (current_state) {

        case STATE_IDLE: {
            // Listen for wake word
            voiceListenForWakeWord = true;
            voiceListenForCommand = false;

            // Retry sending pending payload if we have one
            trySendPendingPayload();

            // Check for wake word event from voice task
            voice_event_t event;
            if (xQueueReceive(voice_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (event.type == EVENT_WAKE_DETECTED) {
                    ESP_LOGI(TAG, "Wake word detected!");
                    handle_state_transition(STATE_WAKE_DETECTED);
                }
            }
            break;
        }

        case STATE_WAKE_DETECTED: {
            // Brief pause for user feedback, then start listening for commands
            vTaskDelay(pdMS_TO_TICKS(300));

            // Clear state
            commandDescription[0] = '\0';
            wantPhoto = false;
            lastWordTime = 0;
            if (voiceRecording) {
                heap_caps_free(voiceRecording);
                voiceRecording = nullptr;
                voiceRecordingSize = 0;
            }

            // Switch voice task from wake word to command recognition
            voiceListenForWakeWord = false;
            voiceListenForCommand = true;
            voiceTaskPaused = false;

            // Enable MultiNet command parser
            cmdParser.enable();

            handle_state_transition(STATE_LISTENING_COMMAND);
            break;
        }

        case STATE_LISTENING_COMMAND: {
            // MultiNet7 processes audio in real-time on Core 1
            // Recognizes multi-word phrases, each maps to a full description

            voice_event_t event;
            if (xQueueReceive(voice_event_queue, &event, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (event.type == EVENT_COMMAND_RECOGNIZED) {
                    int cmdId = event.command_id;
                    const char* phrase = CommandParser::getWordString(cmdId);
                    ESP_LOGI(TAG, "Recognized: \"%s\" (id=%d)", phrase, cmdId);

                    // Reset timeout on every recognized phrase
                    lastWordTime = millis_now();

                    if (cmdId == WORD_CANCEL) {
                        ESP_LOGI(TAG, "Command: CANCEL");
                        cmdParser.disable();
                        voiceListenForCommand = false;
                        handle_state_transition(STATE_IDLE);
                        break;
                    }

                    if (cmdId == WORD_SEND) {
                        ESP_LOGI(TAG, "Command: SEND");
                        cmdParser.disable();
                        voiceListenForCommand = false;
                        if (wantPhoto) {
                            handle_state_transition(STATE_CAPTURING_IMAGE);
                        } else {
                            handle_state_transition(STATE_PROCESSING);
                        }
                        break;
                    }

                    if (CommandParser::isPhotoWord(cmdId)) {
                        wantPhoto = true;
                        ESP_LOGI(TAG, "Photo queued");
                    }

                    // Descriptive phrase - append to description
                    if (CommandParser::isDescriptiveWord(cmdId)) {
                        const char* desc = CommandParser::getDescription(cmdId);
                        size_t curLen = strlen(commandDescription);
                        if (curLen > 0 && curLen < sizeof(commandDescription) - 3) {
                            strncat(commandDescription, ". ",
                                    sizeof(commandDescription) - curLen - 1);
                        }
                        size_t newLen = strlen(commandDescription);
                        strncat(commandDescription, desc,
                                sizeof(commandDescription) - newLen - 1);
                        ESP_LOGI(TAG, "Description so far: \"%s\"", commandDescription);
                    }
                }
            }

            // Auto-send after timeout since last recognized phrase
            unsigned long refTime = (lastWordTime > 0) ? lastWordTime : stateEntryTime;
            if (millis_now() - refTime > COMMAND_TIMEOUT_MS) {
                ESP_LOGI(TAG, "Command timeout - auto-sending");
                cmdParser.disable();
                voiceListenForCommand = false;

                if (strlen(commandDescription) == 0) {
                    strncpy(commandDescription, "voice report",
                            sizeof(commandDescription) - 1);
                }

                if (wantPhoto) {
                    handle_state_transition(STATE_CAPTURING_IMAGE);
                } else {
                    handle_state_transition(STATE_PROCESSING);
                }
            }
            break;
        }

        // STATE_RECORDING_VOICE is currently disabled - using on-device word
        // recognition instead. Kept for future use when audio recording is fixed.
        // case STATE_RECORDING_VOICE: {
        //     voiceTaskPaused = true;
        //     vTaskDelay(pdMS_TO_TICKS(100));
        //     voiceRecording = audio.record(5000, &voiceRecordingSize, 2000);
        //     if (voiceRecording && voiceRecordingSize > 0) {
        //         applyGain(voiceRecording, voiceRecordingSize);
        //         ESP_LOGI(TAG, "Voice recording: %d bytes (%.1fs)",
        //                  (int)voiceRecordingSize,
        //                  (float)voiceRecordingSize / (16000.0f * 2));
        //     }
        //     voiceTaskPaused = false;
        //     if (wantPhoto) {
        //         handle_state_transition(STATE_CAPTURING_IMAGE);
        //     } else {
        //         handle_state_transition(STATE_PROCESSING);
        //     }
        //     break;
        // }

        case STATE_CAPTURING_IMAGE: {
            // Stop I2S and voice before camera to avoid GDMA conflicts
            voiceTaskPaused = true;
            vTaskDelay(pdMS_TO_TICKS(100));  // Let voice task reach pause point
            audio.end();                     // Stop I2S DMA completely
            ESP_LOGI(TAG, "I2S stopped for camera capture");

            // Do camera capture directly in main task (avoids separate stack alloc)
            bool captureOk = false;
#ifdef HAS_CAMERA
            if (!camera.isInitialized()) {
                ESP_LOGI(TAG, "Initializing camera...");
                if (!camera.begin()) {
                    ESP_LOGE(TAG, "Camera init failed");
                }
            }
            if (camera.isInitialized()) {
                if (camera.capture()) {
                    ESP_LOGI(TAG, "Image captured: %d bytes", camera.getImageSize());
                    captureOk = true;
                } else {
                    ESP_LOGE(TAG, "Camera capture failed");
                }
            }
#endif

            // Restart I2S and resume voice
            audio.begin();
            voiceTaskPaused = false;
            ESP_LOGI(TAG, "I2S restarted after camera capture");

            if (captureOk) {
                handle_state_transition(STATE_PROCESSING);
            } else {
                ESP_LOGW(TAG, "Sending without image");
                wantPhoto = false;
                handle_state_transition(STATE_PROCESSING);
            }
            break;
        }

        case STATE_PROCESSING: {
            ESP_LOGI(TAG, "Processing incident...");

            // Get GPS location
            GPSLocation location = gps.getLocation();
            if (!location.valid) {
                ESP_LOGW(TAG, "No GPS fix");
#if GPS_USE_CACHED
                ESP_LOGI(TAG, "Using cached GPS location");
#else
                handle_error("No GPS fix available");
                break;
#endif
            }

            handle_state_transition(STATE_SENDING);
            break;
        }

        case STATE_SENDING: {
            ESP_LOGI(TAG, "Sending incident...");

            GPSLocation location = gps.getLocation();

            const char* description = (strlen(commandDescription) > 0) ?
                commandDescription : "voice report";

            ESP_LOGI(TAG, "Description: \"%s\"", description);
            ESP_LOGI(TAG, "Has image: %s", wantPhoto ? "yes" : "no");

            // Build binary payload
            uint8_t* imgData = nullptr;
            size_t imgSize = 0;
            if (wantPhoto) {
                imgData = camera.getImageBuffer();
                imgSize = camera.getImageSize();
            }

            // No audio recording - description built from MultiNet word recognition
            const uint8_t* pcmData = nullptr;
            size_t pcmSize = 0;

            bool sent = false;
            bool meshSent = false;

            // Step 1: Try WiFi first (full payload with image/audio)
            if (wifi.isConnected()) {
                display.showScreen(DisplayManager::SCREEN_SENDING);

                HTTPClientService::IncidentUploadResult result = httpClient.uploadIncident(
                    location.latitude,
                    location.longitude,
                    location.altitude,
                    PRIORITY_HIGH,
                    "voice_report",
                    description,
                    imgData, imgSize,
                    (uint8_t*)pcmData, pcmSize
                );

                if (result.success) {
                    sent = true;
                    ESP_LOGI(TAG, "Sent via WiFi - ID: %s", result.incidentId);
                    display.setIncidentId(result.incidentId);
                } else {
                    ESP_LOGE(TAG, "WiFi send failed: %s (HTTP %d)",
                             result.message, result.httpCode);
                    display.setErrorMessage(result.message);
                }
            } else {
                ESP_LOGW(TAG, "WiFi not available, skipping direct upload");
            }

            // Step 2: Fall back to mesh relay if WiFi didn't work
            if (!sent && meshBle.isReady()) {
                ESP_LOGI(TAG, "Trying mesh relay...");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                char shortDesc[50];
                strncpy(shortDesc, description, sizeof(shortDesc) - 1);
                shortDesc[sizeof(shortDesc) - 1] = '\0';

                char bodyJson[140];
                snprintf(bodyJson, sizeof(bodyJson),
                    "{\"title\":\"Voice Report\",\"description\":\"%.40s\","
                    "\"latitude\":%.5f,\"longitude\":%.5f}",
                    shortDesc, location.latitude, location.longitude);

                char escapedBody[160];
                escapeJsonQuotes(bodyJson, escapedBody, sizeof(escapedBody));

                unsigned long relayTs = millis_now();

                char relayMsg[237];
                snprintf(relayMsg, sizeof(relayMsg),
                    "RELAY:{\"id\":\"%08lx\",\"method\":\"POST\","
                    "\"path\":\"/api/incident/create\",\"body\":\"%s\"}",
                    relayTs, escapedBody);
#pragma GCC diagnostic pop

                meshSent = meshBle.sendTextMessage(relayMsg);
                if (meshSent) {
                    sent = true;
                    ESP_LOGI(TAG, "RELAY mesh message sent (%d bytes)", (int)strlen(relayMsg));
                    display.setIncidentId("mesh");
                } else {
                    ESP_LOGW(TAG, "RELAY mesh message send failed");
                }
            }

            // Step 3: If nothing worked, save for offline retry
            if (!sent) {
                size_t payloadLen = 0;
                uint8_t* payload = httpClient.buildIncidentBinary(
                    location.latitude,
                    location.longitude,
                    location.altitude,
                    PRIORITY_HIGH,
                    description,
                    imgData, imgSize,
                    (uint8_t*)pcmData, pcmSize,
                    &payloadLen
                );
                if (payload) {
                    savePendingPayload(payload, payloadLen);
                    free(payload);
                }
            }

            // Cleanup captured image
#ifdef HAS_CAMERA
            if (wantPhoto) {
                camera.release();
            }
#endif

            if (sent) {
                handle_state_transition(STATE_SUCCESS);
            } else {
                handle_error("Send failed - buffered for retry");
            }
            break;
        }

        case STATE_SUCCESS:
            // Brief success indication, then return to idle
            if (millis_now() - stateEntryTime > 2000) {
                handle_state_transition(STATE_IDLE);
            }
            break;

        case STATE_ERROR:
            // Brief error indication, then return to idle
            if (millis_now() - stateEntryTime > 3000) {
                handle_state_transition(STATE_IDLE);
            }
            break;

        default:
            handle_state_transition(STATE_IDLE);
            break;
        }

        // Poll buttons
        ButtonHandler::Event btnEvent = buttons.poll();
        if (btnEvent != ButtonHandler::EVENT_NONE) {
            switch (btnEvent) {
                case ButtonHandler::EVENT_ACTION_PRESS:
                    // Action button = same as wake word in idle state
                    if (current_state == STATE_IDLE) {
                        ESP_LOGI(TAG, "Action button pressed - starting command listening");
                        handle_state_transition(STATE_WAKE_DETECTED);
                    }
                    break;

                case ButtonHandler::EVENT_ACTION_LONG_PRESS:
                    // Toggle flashlight
                    flashlightOn = !flashlightOn;
                    gpio_set_level((gpio_num_t)FLASHLIGHT_PIN, flashlightOn ? 1 : 0);
                    ESP_LOGI(TAG, "Flashlight %s", flashlightOn ? "ON" : "OFF");
                    break;

                case ButtonHandler::EVENT_CANCEL_PRESS:
                    // Cancel current operation
                    if (current_state != STATE_IDLE && current_state != STATE_SUCCESS) {
                        ESP_LOGI(TAG, "Cancel button - aborting");
                        cmdParser.disable();
                        voiceListenForCommand = false;
                        handle_state_transition(STATE_IDLE);
                    }
                    break;

                case ButtonHandler::EVENT_MODE_PRESS:
                    // Cycle display mode (only when idle)
                    if (current_state == STATE_IDLE) {
                        display.cycleMode();
                    }
                    break;

                default:
                    break;
            }
        }

        // Periodic status print (every 30 seconds, debug)
        if (millis_now() - lastStatusPrint > 30000) {
            print_status();
            lastStatusPrint = millis_now();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------------------------
// Voice task - continuous audio capture and recognition (Core 1)
// ---------------------------------------------------------------------------

static void voice_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Voice task started on core %d", xPortGetCoreID());

    // Determine chunk size - use the larger of wake word and command parser
    int wakeChunkSize = wakeWord.getChunkSize();
    int cmdChunkSize = cmdParser.getChunkSize();
    int chunkSize = (cmdChunkSize > wakeChunkSize) ? cmdChunkSize : wakeChunkSize;
    if (chunkSize <= 0) chunkSize = 512;

    // Allocate audio buffer for chunk processing
    int16_t* audioChunk = (int16_t*)malloc(chunkSize * sizeof(int16_t));
    if (!audioChunk) {
        ESP_LOGE(TAG, "Failed to allocate voice audio buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Voice processing chunk size: %d samples (wake=%d, cmd=%d)",
             chunkSize, wakeChunkSize, cmdChunkSize);

    int debugCounter = 0;

    while (1) {
        // Pause voice task when main task is recording audio
        if (voiceTaskPaused) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Read audio chunk from microphone
        size_t samplesRead = audio.read(audioChunk, chunkSize);
        if (samplesRead == 0) {
            if (debugCounter++ % 100 == 0) {
                ESP_LOGW(TAG, "Voice: audio.read returned 0 (iter %d)", debugCounter);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Debug: log audio stats periodically
        if (++debugCounter % 500 == 0) {
            int64_t sum = 0;
            for (int i = 0; i < (int)samplesRead; i++) {
                sum += (int64_t)audioChunk[i] * audioChunk[i];
            }
            int rms = (int)sqrt((double)sum / samplesRead);
            ESP_LOGI(TAG, "Voice: read=%d samples, rms=%d, wake=%d, cmd=%d",
                     (int)samplesRead, rms,
                     voiceListenForWakeWord ? 1 : 0,
                     voiceListenForCommand ? 1 : 0);
        }

        // Wake word detection
        if (voiceListenForWakeWord && wakeWord.isEnabled()) {
            wakeWord.processAudio(audioChunk, samplesRead);
            if (wakeWord.isAwake()) {
                voice_event_t event = {};
                event.type = EVENT_WAKE_DETECTED;
                event.command_id = WORD_NONE;
                xQueueSend(voice_event_queue, &event, 0);
                wakeWord.reset();
            }
        }

        // Command word recognition (no recording here - dedicated phase after)
        if (voiceListenForCommand && cmdParser.isEnabled()) {
            int wordId = cmdParser.parseCommand(audioChunk, samplesRead);
            if (wordId != WORD_NONE) {
                voice_event_t event = {};
                event.type = EVENT_COMMAND_RECOGNIZED;
                event.command_id = wordId;
                xQueueSend(voice_event_queue, &event, 0);
            }
        }

        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    free(audioChunk);
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// State transition
// ---------------------------------------------------------------------------

static void handle_state_transition(system_state_t newState)
{
    if (current_state == newState) return;

    ESP_LOGI(TAG, "State: %s -> %s",
             stateToString(current_state), stateToString(newState));

    current_state = newState;
    stateEntryTime = millis_now();

    // Update status flags for display
    display.setStatusFlags(
        wifi.isConnected(),
        gps.getLocation().valid,
        meshBle.isReady(),
        power.getBatteryPercent()
    );

    // Update LED, display, and print user-facing feedback
    switch (newState) {
        case STATE_IDLE:
            led.setState(LEDFeedback::STATE_IDLE);
            voiceTaskPaused = false;
            wakeWord.reset();
            wakeWord.enable();
            display.showScreen(DisplayManager::SCREEN_IDLE);
            ESP_LOGI(TAG, "Listening for wake word - say \"%s\"", WAKE_WORD);
            break;
        case STATE_WAKE_DETECTED:
            led.setState(LEDFeedback::STATE_SUCCESS);
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "WAKE WORD DETECTED - say command words");
            ESP_LOGI(TAG, "========================================");
            break;
        case STATE_LISTENING_COMMAND:
            led.setState(LEDFeedback::STATE_RECORDING);
            display.showScreen(DisplayManager::SCREEN_LISTENING);
            ESP_LOGI(TAG, ">>> LISTENING FOR COMMANDS <<<");
            break;
        case STATE_RECORDING_VOICE:
            led.setState(LEDFeedback::STATE_RECORDING);
            display.showScreen(DisplayManager::SCREEN_RECORDING);
            ESP_LOGI(TAG, ">>> RECORDING VOICE <<<");
            break;
        case STATE_CAPTURING_IMAGE:
            led.setState(LEDFeedback::STATE_PROCESSING);
            display.showScreen(DisplayManager::SCREEN_CAPTURING);
            ESP_LOGI(TAG, ">>> CAPTURING IMAGE <<<");
            break;
        case STATE_PROCESSING:
            led.setState(LEDFeedback::STATE_PROCESSING);
            display.setTranscription(commandDescription);
            display.showScreen(DisplayManager::SCREEN_PREVIEW);
            break;
        case STATE_SENDING:
            led.setState(LEDFeedback::STATE_UPLOADING);
            display.showScreen(DisplayManager::SCREEN_SENDING);
            break;
        case STATE_SUCCESS:
            led.setState(LEDFeedback::STATE_SUCCESS);
            display.showScreen(DisplayManager::SCREEN_SUCCESS);
            ESP_LOGI(TAG, ">>> INCIDENT SENT SUCCESSFULLY <<<");
            break;
        case STATE_ERROR:
            led.setState(LEDFeedback::STATE_ERROR);
            display.showScreen(DisplayManager::SCREEN_ERROR);
            break;
        default:
            break;
    }
}

static void handle_error(const char* message)
{
    ESP_LOGE(TAG, "ERROR: %s", message);
    handle_state_transition(STATE_ERROR);
}

// ---------------------------------------------------------------------------
// Status printing
// ---------------------------------------------------------------------------

static void print_status(void)
{
    ESP_LOGI(TAG, "========== STATUS ==========");
    ESP_LOGI(TAG, "State: %s", stateToString(current_state));

    ESP_LOGI(TAG, "Mesh BLE: %s", meshBle.getStateString());
    if (meshBle.isReady()) {
        ESP_LOGI(TAG, "  Connected to: %s", meshBle.getConnectedDeviceName());
    }

    if (wifi.isConnected()) {
        ESP_LOGI(TAG, "WiFi: Connected (%s, RSSI: %d dBm)",
                 wifi.getSSID(), wifi.getRSSI());
    } else {
        ESP_LOGI(TAG, "WiFi: Disconnected");
    }

#ifdef GPS_ENABLED
    GPSLocation loc = gps.getLocation();
    if (loc.valid) {
        ESP_LOGI(TAG, "GPS: FIX (%.6f, %.6f, %d sats)",
                 loc.latitude, loc.longitude, gps.getSatellites());
    } else {
        ESP_LOGI(TAG, "GPS: NO FIX");
    }
#endif

#ifdef BATTERY_ENABLED
    ESP_LOGI(TAG, "Battery: %d%% (%d mV)",
             power.getBatteryPercent(), power.getBatteryVoltage());
#endif

    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Uptime: %lu seconds", millis_now() / 1000);
    ESP_LOGI(TAG, "============================");
}

// ---------------------------------------------------------------------------
// System info
// ---------------------------------------------------------------------------

static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "Chip: %s with %d CPU cores, WiFi%s%s",
             CONFIG_IDF_TARGET,
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    ESP_LOGI(TAG, "Silicon revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "Flash: %lu MB %s", flash_size / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free internal heap: %lu bytes", esp_get_free_internal_heap_size());

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    ESP_LOGI(TAG, "Device ID: xiao-esp32s3-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ---------------------------------------------------------------------------
// NVS initialization
// ---------------------------------------------------------------------------

static void initialize_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
}
