/**
 * SIMS Mesh Device - Main Firmware (ESP-IDF)
 *
 * ESP32-S3 based mesh network node for incident reporting
 * Hardware: Heltec LoRa 32 V3 (ESP32-S3 + SX1262)
 */

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_sleep.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "config.h"
#include "display_manager.h"
#include "lora_transport.h"
#include "mesh/mesh_protocol.h"
#include "sensors/gps_service.h"
#include "storage/message_storage.h"

#ifdef MESHTASTIC_TEST_MODE
#include "meshtastic_test.h"
#include "meshtastic_ble.h"
#endif

static const char* TAG = "MAIN";

// millis() is provided by TinyGPS++.cpp (uses esp_timer_get_time)

// Global service instances
DisplayManager displayManager;
LoRaTransport loraTransport;
MeshProtocol meshProtocol;
GPSService gpsService;
MessageStorage messageStorage;

#ifdef MESHTASTIC_TEST_MODE
MeshtasticBLE meshtasticBLE;
#endif

// Device state
enum DeviceState {
    STATE_IDLE,
    STATE_RECORDING_VOICE,
    STATE_CAPTURING_IMAGE,
    STATE_PROCESSING,
    STATE_TRANSMITTING
};

DeviceState currentState = STATE_IDLE;

// Battery ADC
static adc_oneshot_unit_handle_t adcHandle = nullptr;
static adc_cali_handle_t adcCaliHandle = nullptr;
static int batteryPercent = 100;

// Button state (GPIO 0 - active low)
static bool buttonPressed = false;
static unsigned long buttonPressStart = 0;
static bool longPressHandled = false;

// Function prototypes
void setupHardware();
void setupServices();
void initBatteryADC();
int readBatteryPercent();
void enterDeepSleep();
void handleButton();

// --- Battery ADC ---

void initBatteryADC() {
    // GPIO 37 must be LOW to enable the battery voltage divider on Heltec V3
    gpio_config_t adc_ctrl_conf = {};
    adc_ctrl_conf.pin_bit_mask = (1ULL << BATTERY_ADC_CTRL);
    adc_ctrl_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&adc_ctrl_conf);
    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, 0);

    adc_oneshot_unit_init_cfg_t initCfg = {};
    initCfg.unit_id = ADC_UNIT_1;
    initCfg.ulp_mode = ADC_ULP_MODE_DISABLE;

    esp_err_t err = adc_oneshot_new_unit(&initCfg, &adcHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed: %s", esp_err_to_name(err));
        return;
    }

    adc_oneshot_chan_cfg_t chanCfg = {};
    chanCfg.atten = ADC_ATTEN_DB_12;
    chanCfg.bitwidth = ADC_BITWIDTH_12;

    err = adc_oneshot_config_channel(adcHandle, ADC_CHANNEL_0, &chanCfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC channel config failed: %s", esp_err_to_name(err));
        return;
    }

    // Try to create calibration handle
    adc_cali_curve_fitting_config_t caliCfg = {};
    caliCfg.unit_id = ADC_UNIT_1;
    caliCfg.chan = ADC_CHANNEL_0;
    caliCfg.atten = ADC_ATTEN_DB_12;
    caliCfg.bitwidth = ADC_BITWIDTH_12;

    err = adc_cali_create_scheme_curve_fitting(&caliCfg, &adcCaliHandle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
        adcCaliHandle = nullptr;
    }

    ESP_LOGI(TAG, "Battery ADC initialized (GPIO %d, ADC1_CH0)", BATTERY_ADC_PIN);
}

int readBatteryPercent() {
    if (!adcHandle) return 100;

    // Enable voltage divider - try both LOW and HIGH to auto-detect board revision
    // V3.1: GPIO 37 LOW enables divider, V3.2: GPIO 37 HIGH enables divider
    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, 0);  // Try LOW first (V3.1)
    vTaskDelay(pdMS_TO_TICKS(10));

    int raw0 = 0;
    adc_oneshot_read(adcHandle, ADC_CHANNEL_0, &raw0);

    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, 1);  // Try HIGH (V3.2)
    vTaskDelay(pdMS_TO_TICKS(10));

    int raw1 = 0;
    adc_oneshot_read(adcHandle, ADC_CHANNEL_0, &raw1);

    // Use whichever gave a higher reading
    bool useHigh = (raw1 > raw0);
    ESP_LOGI(TAG, "Battery ADC probe: GPIO37 LOW->raw=%d, HIGH->raw=%d, using %s",
             raw0, raw1, useHigh ? "HIGH (V3.2)" : "LOW (V3.1)");
    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, useHigh ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    int rawSum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        int raw = 0;
        adc_oneshot_read(adcHandle, ADC_CHANNEL_0, &raw);
        rawSum += raw;
    }
    int rawAvg = rawSum / BATTERY_SAMPLES;

    // Disable voltage divider to save power
    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, useHigh ? 0 : 1);

    float voltage;
    if (adcCaliHandle) {
        int mv = 0;
        adc_cali_raw_to_voltage(adcCaliHandle, rawAvg, &mv);
        voltage = (mv / 1000.0f) * BATTERY_DIVIDER;
    } else {
        // Fallback: 12-bit ADC with 12dB atten gives ~0-3.1V range
        voltage = (rawAvg / 4095.0f) * 3.1f * BATTERY_DIVIDER;
    }

    // Map voltage to percentage
    int percent = (int)((voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    ESP_LOGI(TAG, "Battery: raw=%d, voltage=%.2fV, percent=%d%%", rawAvg, voltage, percent);
    return percent;
}

// --- Deep Sleep ---

void enterDeepSleep() {
    ESP_LOGI(TAG, "Entering deep sleep...");

    // Show sleep screen
    displayManager.showSleepScreen();

    // Turn off display
    displayManager.setScreenPower(false);

    // Turn off Vext (GPIO 36 HIGH = power OFF)
    gpio_set_level((gpio_num_t)VEXT_CTRL, 1);

    // Turn off LED
    gpio_set_level((gpio_num_t)STATUS_LED, 0);

    // Configure wake on GPIO 0 (button press = LOW)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);

    // Enter deep sleep (device resets on wake)
    esp_deep_sleep_start();
}

// --- Button Handling ---

void handleButton() {
    bool pressed = (gpio_get_level(GPIO_NUM_0) == 0);  // Active low

    if (pressed && !buttonPressed) {
        // Button just pressed
        buttonPressed = true;
        buttonPressStart = millis();
        longPressHandled = false;
    } else if (pressed && buttonPressed && !longPressHandled) {
        // Button held - check for long press
        if (millis() - buttonPressStart >= BUTTON_LONG_PRESS_MS) {
            longPressHandled = true;
            displayManager.registerActivity();
            enterDeepSleep();
            // Never returns - device resets on wake
        }
    } else if (!pressed && buttonPressed) {
        // Button released
        unsigned long pressDuration = millis() - buttonPressStart;
        buttonPressed = false;

        if (!longPressHandled && pressDuration >= BUTTON_DEBOUNCE_MS
            && pressDuration < BUTTON_SHORT_PRESS_MAX_MS) {
            // Short press: toggle display on/off
            bool isOn = displayManager.isDisplayOn();
            displayManager.setScreenPower(!isOn);
            displayManager.registerActivity();
            ESP_LOGI(TAG, "Display toggled %s", isOn ? "OFF" : "ON");
        }
    }
}

// --- Hardware & Services Setup ---

void setupHardware() {
    // Configure status LED
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << STATUS_LED);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)STATUS_LED, 0);

    // Configure PTT button (GPIO 0)
    io_conf.pin_bit_mask = (1ULL << PTT_BUTTON);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // CRITICAL: Enable Vext power for external peripherals
    // GPIO36 controls P-channel FET: LOW = power ON, HIGH = power OFF
    io_conf.pin_bit_mask = (1ULL << VEXT_CTRL);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)VEXT_CTRL, 0);  // Power ON
    vTaskDelay(pdMS_TO_TICKS(100));

    // LED blink: starting I2C init
    gpio_set_level((gpio_num_t)STATUS_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level((gpio_num_t)STATUS_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(200));

    // Initialize display FIRST (so we can show boot progress)
    // SSD1306 driver handles I2C internally
    if (displayManager.begin()) {
        // Display init SUCCESS - 3 blinks
        for (int i = 0; i < 3; i++) {
            gpio_set_level((gpio_num_t)STATUS_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level((gpio_num_t)STATUS_LED, 0);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        displayManager.showBootScreen();
    } else {
        // Display init FAILED - rapid 10 blinks
        for (int i = 0; i < 10; i++) {
            gpio_set_level((gpio_num_t)STATUS_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level((gpio_num_t)STATUS_LED, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    // Initialize battery ADC
    initBatteryADC();
    batteryPercent = readBatteryPercent();

    // Initialize SPIFFS for message storage
    displayManager.showInitProgress("Storage", 10);
    esp_vfs_spiffs_conf_t spiffs_conf = {};
    spiffs_conf.base_path = SPIFFS_MOUNT_POINT;
    spiffs_conf.partition_label = "storage";
    spiffs_conf.max_files = 10;
    spiffs_conf.format_if_mount_failed = true;

    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(ret));
    } else {
        size_t total = 0, used = 0;
        esp_spiffs_info(spiffs_conf.partition_label, &total, &used);
        ESP_LOGI(TAG, "SPIFFS mounted: %d/%d bytes used", used, total);
    }
}

void setupServices() {
    // Initialize Message Storage
    displayManager.showInitProgress("Storage", 20);
    if (!messageStorage.begin()) {
        ESP_LOGE(TAG, "Message storage initialization failed!");
    } else {
        ESP_LOGI(TAG, "Message storage ready");
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    // Initialize GPS Service
    displayManager.showInitProgress("GPS", 40);
    if (!gpsService.begin(GPS_RX, GPS_TX)) {
        ESP_LOGW(TAG, "GPS initialization failed - continuing without GPS");
    } else {
        ESP_LOGI(TAG, "GPS service ready");
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    // Initialize LoRa Transport
    displayManager.showInitProgress("LoRa Radio", 60);
    ESP_LOGI(TAG, "Initializing LoRa SX1262...");
    if (!loraTransport.begin(LORA_CS, LORA_RST, LORA_DIO1, LORA_BUSY)) {
        ESP_LOGE(TAG, "LoRa initialization failed!");
        displayManager.showMessage("LoRa FAIL!", 3000);
        vTaskDelay(pdMS_TO_TICKS(3000));
    } else {
        ESP_LOGI(TAG, "LoRa radio ready");
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    // Initialize Mesh Protocol
    displayManager.showInitProgress("Mesh Proto", 80);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    uint32_t deviceId = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];

    if (!meshProtocol.begin(&loraTransport)) {
        ESP_LOGE(TAG, "Mesh protocol initialization failed!");
    } else {
        meshProtocol.setDeviceId(deviceId);
        ESP_LOGI(TAG, "Mesh protocol ready (ID: 0x%08X)", (unsigned int)deviceId);
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    displayManager.showInitProgress("Complete", 100);
    ESP_LOGI(TAG, "All core services ready");

    // Toggle LED to prove code executed
    gpio_set_level((gpio_num_t)STATUS_LED, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)STATUS_LED, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)STATUS_LED, 0);

    ESP_LOGI(TAG, "LED toggle complete - starting BLE...");

    #ifdef MESHTASTIC_TEST_MODE
    ESP_LOGI(TAG, "MESHTASTIC_TEST_MODE active - initializing BLE service");
    if (meshtasticBLE.begin("SIMS-MESH", &loraTransport, &meshProtocol)) {
        ESP_LOGI(TAG, "Meshtastic BLE service ready - device should appear in app!");
    } else {
        ESP_LOGE(TAG, "Meshtastic BLE service failed to start");
    }
    #endif
}

// --- Main Task ---

void main_task(void* param) {
    bool firstLoop = true;

    while (1) {
        if (firstLoop) {
            ESP_LOGI(TAG, ">>>>>> LOOP FIRST ITERATION - NEW FIRMWARE RUNNING <<<<<<");
            firstLoop = false;
        }

        // Button polling (before anything else)
        handleButton();

        // Update GPS service
        gpsService.update();

        // Update mesh protocol
        meshProtocol.update();

        #ifdef MESHTASTIC_TEST_MODE
        meshtasticBLE.update();
        #endif

        // Handle received mesh messages
        if (meshProtocol.hasMessage()) {
            MeshMessage msg = meshProtocol.receiveMessage();
            ESP_LOGI(TAG, "Received message: type=%d, from=0x%08X, RSSI=%d",
                     msg.messageType, (unsigned int)msg.sourceId, loraTransport.getRSSI());

            displayManager.registerActivity();

            switch (msg.messageType) {
                case MSG_TYPE_INCIDENT:
                    ESP_LOGI(TAG, "Incident report received (relay)");
                    break;
                case MSG_TYPE_HEARTBEAT:
                    ESP_LOGI(TAG, "Heartbeat received");
                    break;
                case MSG_TYPE_ACK:
                    ESP_LOGI(TAG, "Acknowledgment received");
                    messageStorage.markAsSent(msg.sequenceNumber);
                    break;
                default:
                    ESP_LOGW(TAG, "Unknown message type: %d", msg.messageType);
                    break;
            }
        }

        // Heartbeat LED blink
        static unsigned long lastBlink = 0;
        if (millis() - lastBlink > 1000) {
            static bool ledState = false;
            ledState = !ledState;
            gpio_set_level((gpio_num_t)STATUS_LED, ledState ? 1 : 0);
            lastBlink = millis();
        }

        // Meshtastic Test Mode: Send node info + test messages
        #ifdef MESHTASTIC_TEST_MODE
        static unsigned long lastTestMessage = 0;
        static bool nodeInfoSent = false;

        if (!nodeInfoSent && millis() > 10000) {
            uint8_t packet[256];
            uint32_t nodeId = meshProtocol.getDeviceId();

            size_t len = createMeshtasticNodeInfoPacket(packet, nodeId, "SIMS-MESH", "SIMS");

            if (loraTransport.send(packet, len)) {
                ESP_LOGI(TAG, "Meshtastic node info sent - device should appear in Meshtastic!");
                ESP_LOGI(TAG, "Node ID: !%08x", (unsigned int)nodeId);
                nodeInfoSent = true;
            } else {
                ESP_LOGE(TAG, "Meshtastic node info FAILED");
            }
        }

        if (nodeInfoSent && millis() - lastTestMessage > 30000) {
            uint8_t packet[256];
            uint32_t nodeId = meshProtocol.getDeviceId();

            static int msgCount = 0;
            char message[64];
            snprintf(message, sizeof(message), "SIMS Test #%d - Hello Meshtastic!", ++msgCount);

            size_t len = createMeshtasticTextPacket(packet, nodeId, message);

            if (loraTransport.send(packet, len)) {
                ESP_LOGI(TAG, "Meshtastic text message sent: %s", message);
                ESP_LOGI(TAG, "Packet size: %d bytes", len);
                displayManager.notifyTx(1500);
                displayManager.registerActivity();
            } else {
                ESP_LOGE(TAG, "Meshtastic text message FAILED");
            }

            lastTestMessage = millis();
        }
        #endif

        // Battery reading every 60s
        static unsigned long lastBatteryRead = 0;
        if (millis() - lastBatteryRead > BATTERY_CHECK_INTERVAL) {
            batteryPercent = readBatteryPercent();
            lastBatteryRead = millis();
        }

        // Update status display every 5 seconds
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 5000) {
            bool gpsValid = gpsService.hasFix();
            int satellites = gpsService.getSatellites();
            int meshNodes = meshProtocol.getConnectedNodes();
            int pendingMessages = messageStorage.getPendingCount();
            bool bleConnected = false;
            int bleClients = 0;
            int loraRSSI = loraTransport.getRSSI();
            float loraSNR = loraTransport.getSNR();

            #ifdef MESHTASTIC_TEST_MODE
            bleConnected = meshtasticBLE.isConnected();
            bleClients = meshtasticBLE.getConnectedCount();
            #endif

            uint32_t sent, received, relayed;
            meshProtocol.getStats(sent, received, relayed);
            int packetsReceived = (int)received;

            // Check idle timeout - show idle screen if no activity
            if (displayManager.isIdle(IDLE_SCREEN_TIMEOUT_MS)) {
                displayManager.showIdleScreen(batteryPercent);
            } else {
                displayManager.updateStatus(gpsValid, satellites, meshNodes,
                                           pendingMessages, batteryPercent,
                                           bleConnected, bleClients,
                                           loraRSSI, loraSNR, packetsReceived);
            }

            lastDisplayUpdate = millis();

            ESP_LOGI(TAG, "GPS:%s Sats:%d Mesh:%d/%dpkts RSSI:%d SNR:%.1f Queue:%d Bat:%d%%",
                     gpsValid ? "OK" : "NO", satellites, meshNodes, packetsReceived,
                     loraRSSI, loraSNR, pendingMessages, batteryPercent);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SIMS Mesh Device - ESP-IDF");
    ESP_LOGI(TAG, "Version: %s", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "Board: Heltec WiFi LoRa 32 V3");
    #ifdef MESHTASTIC_TEST_MODE
    ESP_LOGI(TAG, "Mode: MESHTASTIC TEST (Sync: 0x2B)");
    ESP_LOGI(TAG, "Sending test messages every 30s");
    #else
    ESP_LOGI(TAG, "Mode: SIMS Protocol (Sync: 0x12)");
    #endif
    ESP_LOGI(TAG, "========================================");

    gpio_set_level((gpio_num_t)STATUS_LED, 1);  // LED on during setup

    setupHardware();
    setupServices();

    ESP_LOGI(TAG, "Initialization complete");
    ESP_LOGI(TAG, "Device ready for operation");

    gpio_set_level((gpio_num_t)STATUS_LED, 0);  // LED off after setup

    // Create main task
    xTaskCreatePinnedToCore(main_task, "main_task", MAIN_TASK_STACK_SIZE, NULL,
                            MAIN_TASK_PRIORITY, NULL, MAIN_TASK_CORE);
}
