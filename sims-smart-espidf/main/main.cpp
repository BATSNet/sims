/**
 * SIMS-SMART Device - ESP-IDF Port
 *
 * Voice-controlled hands-free incident reporting
 * Hardware: Seeed Studio XIAO ESP32S3 Sense
 *
 * This is the complete ESP-IDF port of the Arduino firmware to enable
 * full ESP32-SR voice recognition support (WakeNet9 + MultiNet6).
 *
 * Architecture:
 * - main_task: State machine, service coordination, HTTP uploads (Core 0)
 * - voice_task: Continuous audio capture and voice recognition (Core 1)
 * - GPS updated periodically from main_task
 * - LED updated from main_task
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_mac.h"

#include "model_path.h"
#include "config.h"
#include "network/wifi_service.h"
#include "network/http_client.h"
#include "sensors/gps_service.h"
#include "sensors/camera_service.h"
#include "sensors/audio_service.h"
#include "voice/wake_word_service.h"
#include "voice/command_parser.h"
#include "led/led_feedback.h"
#include "power/power_manager.h"

static const char *TAG = "MAIN";

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

typedef enum {
    STATE_INIT,
    STATE_IDLE,                  // Listening for wake word
    STATE_WAKE_DETECTED,         // Wake word detected, brief feedback
    STATE_LISTENING_COMMAND,     // Listening for voice command
    STATE_CAPTURING_IMAGE,       // Taking photo
    STATE_RECORDING_VOICE,       // Recording audio message
    STATE_PROCESSING,            // Getting GPS, building payload
    STATE_UPLOADING,             // Sending to backend
    STATE_SUCCESS,               // Upload successful
    STATE_ERROR                  // Error occurred
} system_state_t;

static const char* stateToString(system_state_t s) {
    switch (s) {
        case STATE_INIT:             return "INIT";
        case STATE_IDLE:             return "IDLE";
        case STATE_WAKE_DETECTED:    return "WAKE_DETECTED";
        case STATE_LISTENING_COMMAND:return "LISTENING_COMMAND";
        case STATE_CAPTURING_IMAGE:  return "CAPTURING_IMAGE";
        case STATE_RECORDING_VOICE:  return "RECORDING_VOICE";
        case STATE_PROCESSING:       return "PROCESSING";
        case STATE_UPLOADING:        return "UPLOADING";
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
    EVENT_COMMAND_RECOGNIZED,
    EVENT_TIMEOUT
} voice_event_type_t;

typedef struct {
    voice_event_type_t type;
    int command_id;  // CommandParser::VoiceCommand value
} voice_event_t;

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

static system_state_t current_state = STATE_INIT;
static CommandParser::VoiceCommand lastCommand = CommandParser::CMD_NONE;
static char incidentDescription[256] = "";

// Captured data
static uint8_t* capturedAudioBuffer = nullptr;
static size_t capturedAudioSize = 0;

// Timing
static unsigned long stateEntryTime = 0;

// ---------------------------------------------------------------------------
// Inter-task communication
// ---------------------------------------------------------------------------

static QueueHandle_t voice_event_queue = NULL;

// Shared flag: main task tells voice task whether to run wake word or command
static volatile bool voiceListenForWakeWord = true;
static volatile bool voiceListenForCommand = false;

// ---------------------------------------------------------------------------
// Task handles
// ---------------------------------------------------------------------------

static TaskHandle_t main_task_handle = NULL;
static TaskHandle_t voice_task_handle = NULL;

// ---------------------------------------------------------------------------
// Global services
// ---------------------------------------------------------------------------

static WiFiService    wifi;
static HTTPClientService httpClient;
static GPSService     gps;
static CameraService  camera;
static AudioService   audio;
static WakeWordService wakeWord;
static CommandParser  commandParser;
static LEDFeedback    led;
static PowerManager   power;

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

// ---------------------------------------------------------------------------
// Helper: milliseconds since boot
// ---------------------------------------------------------------------------

static unsigned long millis_now(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

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

    // Main task on Core 0 (networking, state machine)
    BaseType_t ret = xTaskCreatePinnedToCore(
        main_task, "main_task", 8192, NULL,
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

    // WiFi
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
    // camera.begin() will be called in STATE_CAPTURING_IMAGE
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
    ESP_LOGI(TAG, "Initializing voice recognition...");
    if (!wakeWord.begin(WAKE_WORD)) {
        ESP_LOGW(TAG, "Wake word init failed");
    }
    if (!commandParser.begin()) {
        ESP_LOGW(TAG, "Command parser init failed");
    }

    // Power manager
    power.begin();

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
        wifi.update();
        led.update();

#ifdef GPS_ENABLED
        gps.update();
#endif

        // State machine
        switch (current_state) {

        case STATE_IDLE: {
            // Tell voice task to listen for wake word
            voiceListenForWakeWord = true;
            voiceListenForCommand = false;

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

        case STATE_WAKE_DETECTED:
            // Brief pause for user feedback
            vTaskDelay(pdMS_TO_TICKS(300));
            handle_state_transition(STATE_LISTENING_COMMAND);
            break;

        case STATE_LISTENING_COMMAND: {
            // Tell voice task to listen for commands
            voiceListenForWakeWord = false;
            voiceListenForCommand = true;

            // Wait for command event or timeout
            voice_event_t event;
            if (xQueueReceive(voice_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (event.type == EVENT_COMMAND_RECOGNIZED) {
                    lastCommand = (CommandParser::VoiceCommand)event.command_id;
                    ESP_LOGI(TAG, "Command: %s",
                             CommandParser::commandToString(lastCommand));

                    switch (lastCommand) {
                        case CommandParser::CMD_TAKE_PHOTO:
                            handle_state_transition(STATE_CAPTURING_IMAGE);
                            break;
                        case CommandParser::CMD_RECORD_VOICE:
                            handle_state_transition(STATE_RECORDING_VOICE);
                            break;
                        case CommandParser::CMD_SEND_INCIDENT:
                            handle_state_transition(STATE_PROCESSING);
                            break;
                        case CommandParser::CMD_CANCEL:
                            handle_state_transition(STATE_IDLE);
                            break;
                        case CommandParser::CMD_STATUS_CHECK:
                            print_status();
                            handle_state_transition(STATE_IDLE);
                            break;
                        default:
                            handle_error("Unknown command");
                            break;
                    }
                }
            }

            // Timeout after COMMAND_TIMEOUT_MS
            if (millis_now() - stateEntryTime > COMMAND_TIMEOUT_MS) {
                ESP_LOGW(TAG, "Command timeout - returning to idle");
                handle_state_transition(STATE_IDLE);
            }
            break;
        }

        case STATE_CAPTURING_IMAGE: {
            ESP_LOGI(TAG, "Capturing image...");
#ifdef HAS_CAMERA
            // Lazy init camera on first capture
            if (!camera.isInitialized()) {
                ESP_LOGI(TAG, "Initializing camera...");
                if (!camera.begin()) {
                    handle_error("Camera initialization failed");
                    break;
                }
            }
            if (camera.capture()) {
                ESP_LOGI(TAG, "Image captured: %d bytes", camera.getImageSize());
                handle_state_transition(STATE_PROCESSING);
            } else {
                handle_error("Camera capture failed");
            }
#else
            ESP_LOGW(TAG, "Camera not available");
            handle_state_transition(STATE_PROCESSING);
#endif
            break;
        }

        case STATE_RECORDING_VOICE: {
            ESP_LOGI(TAG, "Recording voice message...");
#ifdef HAS_MICROPHONE
            // Stop voice recognition while recording
            voiceListenForWakeWord = false;
            voiceListenForCommand = false;

            // Record audio
            capturedAudioBuffer = audio.record(MAX_VOICE_DURATION_MS, &capturedAudioSize);
            if (capturedAudioBuffer && capturedAudioSize > 0) {
                ESP_LOGI(TAG, "Audio recorded: %d bytes", capturedAudioSize);
                handle_state_transition(STATE_PROCESSING);
            } else {
                handle_error("Audio recording failed");
            }
#else
            ESP_LOGW(TAG, "Microphone not available");
            handle_state_transition(STATE_PROCESSING);
#endif
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

            // Build description from voice command
            snprintf(incidentDescription, sizeof(incidentDescription),
                     "Voice incident report - Command: %s",
                     CommandParser::commandToString(lastCommand));

            handle_state_transition(STATE_UPLOADING);
            break;
        }

        case STATE_UPLOADING: {
            ESP_LOGI(TAG, "Uploading incident...");

            if (!wifi.isConnected()) {
                handle_error("WiFi not connected");
                break;
            }

            GPSLocation location = gps.getLocation();

            // Upload incident
            HTTPClientService::IncidentUploadResult result = httpClient.uploadIncident(
                location.latitude,
                location.longitude,
                location.altitude,
                PRIORITY_HIGH,
                CommandParser::commandToString(lastCommand),
                incidentDescription,
                camera.getImageBuffer(),
                camera.getImageSize(),
                capturedAudioBuffer,
                capturedAudioSize
            );

            // Cleanup captured data
#ifdef HAS_CAMERA
            camera.release();
#endif
            if (capturedAudioBuffer) {
                free(capturedAudioBuffer);
                capturedAudioBuffer = nullptr;
                capturedAudioSize = 0;
            }

            if (result.success) {
                ESP_LOGI(TAG, "SUCCESS! Incident ID: %s", result.incidentId);
                handle_state_transition(STATE_SUCCESS);
            } else {
                ESP_LOGE(TAG, "FAILED: %s (HTTP %d)", result.message, result.httpCode);
                handle_error("Upload failed");
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

    // Determine chunk size from models
    int chunkSize = wakeWord.getChunkSize();
    if (chunkSize <= 0) chunkSize = 512;

    // Allocate audio buffer for chunk processing
    int16_t* audioChunk = (int16_t*)malloc(chunkSize * sizeof(int16_t));
    if (!audioChunk) {
        ESP_LOGE(TAG, "Failed to allocate voice audio buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Voice processing chunk size: %d samples", chunkSize);

    while (1) {
        // Read audio chunk from microphone
        size_t samplesRead = audio.read(audioChunk, chunkSize);
        if (samplesRead == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Wake word detection
        if (voiceListenForWakeWord && wakeWord.isEnabled()) {
            wakeWord.processAudio(audioChunk, samplesRead);
            if (wakeWord.isAwake()) {
                voice_event_t event = {};
                event.type = EVENT_WAKE_DETECTED;
                xQueueSend(voice_event_queue, &event, 0);
                wakeWord.reset();
            }
        }

        // Command recognition
        if (voiceListenForCommand && commandParser.isEnabled()) {
            CommandParser::VoiceCommand cmd =
                commandParser.parseCommand(audioChunk, samplesRead);
            if (cmd != CommandParser::CMD_NONE) {
                voice_event_t event = {};
                event.type = EVENT_COMMAND_RECOGNIZED;
                event.command_id = (int)cmd;
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

    // Update LED
    switch (newState) {
        case STATE_IDLE:
            led.setState(LEDFeedback::STATE_IDLE);
            wakeWord.reset();
            wakeWord.enable();
            commandParser.reset();
            break;
        case STATE_WAKE_DETECTED:
            led.setState(LEDFeedback::STATE_SUCCESS);
            break;
        case STATE_LISTENING_COMMAND:
            led.setState(LEDFeedback::STATE_LISTENING);
            commandParser.enable();
            break;
        case STATE_CAPTURING_IMAGE:
            led.setState(LEDFeedback::STATE_PROCESSING);
            break;
        case STATE_RECORDING_VOICE:
            led.setState(LEDFeedback::STATE_RECORDING);
            break;
        case STATE_PROCESSING:
            led.setState(LEDFeedback::STATE_PROCESSING);
            break;
        case STATE_UPLOADING:
            led.setState(LEDFeedback::STATE_UPLOADING);
            break;
        case STATE_SUCCESS:
            led.setState(LEDFeedback::STATE_SUCCESS);
            break;
        case STATE_ERROR:
            led.setState(LEDFeedback::STATE_ERROR);
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
