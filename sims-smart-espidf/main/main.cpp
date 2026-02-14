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
 * - voice_task: Continuous audio capture and WakeNet9 wake word detection (Core 1)
 * - GPS updated periodically from main_task
 * - LED updated from main_task
 *
 * Flow: "Hi ESP" -> auto-record (stops on silence) -> auto-photo -> auto-send
 */

#include <stdio.h>
#include <string.h>
#include <cmath>
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
    EVENT_WAKE_DETECTED
} voice_event_type_t;

typedef struct {
    voice_event_type_t type;
} voice_event_t;

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

static system_state_t current_state = STATE_INIT;

// Captured data
static uint8_t* capturedAudioBuffer = nullptr;
static size_t capturedAudioSize = 0;

// Timing
static unsigned long stateEntryTime = 0;

// ---------------------------------------------------------------------------
// Inter-task communication
// ---------------------------------------------------------------------------

static QueueHandle_t voice_event_queue = NULL;

// Shared flag: main task tells voice task whether to listen for wake word
static volatile bool voiceListenForWakeWord = true;

// Pause flag: main task sets true during audio.record() to prevent voice_task
// from calling audio.read() concurrently (both use the same I2S channel)
static volatile bool voicePauseAudio = false;

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

    // Voice recognition (wake word only - no command parser needed)
    ESP_LOGI(TAG, "Initializing wake word detection...");
    if (!wakeWord.begin(WAKE_WORD)) {
        ESP_LOGW(TAG, "Wake word init failed");
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
            // Listen for wake word
            voiceListenForWakeWord = true;

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
            // Brief pause for user feedback, then start recording
            vTaskDelay(pdMS_TO_TICKS(300));
            handle_state_transition(STATE_RECORDING_VOICE);
            break;

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
            // Stop wake word detection while recording
            voiceListenForWakeWord = false;

            // Pause voice_task audio reads to prevent I2S channel conflict
            voicePauseAudio = true;
            vTaskDelay(pdMS_TO_TICKS(50));  // Let voice_task finish current read

            ESP_LOGI(TAG, ">>> RECORDING STARTED - speak now (%d ms max, silence stops) <<<",
                     MAX_VOICE_DURATION_MS);

            // Record audio with silence detection
            capturedAudioBuffer = audio.record(MAX_VOICE_DURATION_MS, &capturedAudioSize);

            ESP_LOGI(TAG, ">>> RECORDING STOPPED <<<");

            // Resume voice_task audio reads
            voicePauseAudio = false;

            if (capturedAudioBuffer && capturedAudioSize > 0) {
                ESP_LOGI(TAG, "Audio recorded: %d bytes", (int)capturedAudioSize);
                handle_state_transition(STATE_CAPTURING_IMAGE);
            } else {
                handle_error("Audio recording failed");
            }
#else
            ESP_LOGW(TAG, "Microphone not available");
            handle_state_transition(STATE_CAPTURING_IMAGE);
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

            // Upload incident (no audio - SR handles voice on-device)
            HTTPClientService::IncidentUploadResult result = httpClient.uploadIncident(
                location.latitude,
                location.longitude,
                location.altitude,
                PRIORITY_HIGH,
                "voice_report",
                "Voice-activated incident report",
                camera.getImageBuffer(),
                camera.getImageSize(),
                nullptr,
                0
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

    int debugCounter = 0;

    while (1) {
        // When main_task is recording, pause audio reads to avoid I2S conflict
        if (voicePauseAudio) {
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
            // Calculate RMS level to verify mic is capturing sound
            int64_t sum = 0;
            for (int i = 0; i < (int)samplesRead; i++) {
                sum += (int64_t)audioChunk[i] * audioChunk[i];
            }
            int rms = (int)sqrt((double)sum / samplesRead);
            ESP_LOGI(TAG, "Voice: read=%d samples, rms=%d, wakeEnabled=%d, listenWake=%d",
                     (int)samplesRead, rms, wakeWord.isEnabled() ? 1 : 0,
                     voiceListenForWakeWord ? 1 : 0);
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

    // Update LED and print user-facing feedback
    switch (newState) {
        case STATE_IDLE:
            led.setState(LEDFeedback::STATE_IDLE);
            wakeWord.reset();
            wakeWord.enable();
            ESP_LOGI(TAG, "Listening for wake word - say \"%s\"", WAKE_WORD);
            break;
        case STATE_WAKE_DETECTED:
            led.setState(LEDFeedback::STATE_SUCCESS);
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "WAKE WORD DETECTED - recording starts...");
            ESP_LOGI(TAG, "========================================");
            break;
        case STATE_CAPTURING_IMAGE:
            led.setState(LEDFeedback::STATE_PROCESSING);
            ESP_LOGI(TAG, ">>> CAPTURING IMAGE <<<");
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
            ESP_LOGI(TAG, ">>> INCIDENT SENT SUCCESSFULLY <<<");
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
