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
static bool batteryUseHighForDivider = false;  // Cached board revision (V3.1=LOW, V3.2=HIGH)
static float batterySmoothedVoltage = 0.0f;    // EMA smoothed voltage
static bool batteryFirstReading = true;

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
    // GPIO 37 controls battery voltage divider on Heltec V3
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

    // Detect board revision ONCE: V3.1 uses LOW, V3.2 uses HIGH to enable divider
    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, 0);  // Try LOW (V3.1)
    vTaskDelay(pdMS_TO_TICKS(10));
    int raw0 = 0;
    adc_oneshot_read(adcHandle, ADC_CHANNEL_0, &raw0);

    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, 1);  // Try HIGH (V3.2)
    vTaskDelay(pdMS_TO_TICKS(10));
    int raw1 = 0;
    adc_oneshot_read(adcHandle, ADC_CHANNEL_0, &raw1);

    batteryUseHighForDivider = (raw1 > raw0);
    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, batteryUseHighForDivider ? 1 : 0);

    ESP_LOGI(TAG, "Battery ADC initialized (GPIO %d, ADC1_CH0) - board %s (LOW=%d, HIGH=%d)",
             BATTERY_ADC_PIN, batteryUseHighForDivider ? "V3.2" : "V3.1", raw0, raw1);
}

// LiPo discharge curve lookup table (voltage -> percentage)
static const float lipoVoltages[] = { 4.20f, 4.10f, 4.00f, 3.90f, 3.80f, 3.70f, 3.60f, 3.50f, 3.30f, 3.00f };
static const int   lipoPercents[] = {   100,    90,    80,    60,    40,    30,    20,    10,     5,     0 };
static const int   lipoTableSize  = sizeof(lipoVoltages) / sizeof(lipoVoltages[0]);

static int voltageToPercent(float voltage) {
    if (voltage >= lipoVoltages[0]) return 100;
    if (voltage <= lipoVoltages[lipoTableSize - 1]) return 0;

    for (int i = 0; i < lipoTableSize - 1; i++) {
        if (voltage >= lipoVoltages[i + 1]) {
            // Linear interpolation between table points
            float vRange = lipoVoltages[i] - lipoVoltages[i + 1];
            float pRange = (float)(lipoPercents[i] - lipoPercents[i + 1]);
            float ratio = (voltage - lipoVoltages[i + 1]) / vRange;
            return lipoPercents[i + 1] + (int)(ratio * pRange);
        }
    }
    return 0;
}

int readBatteryPercent() {
    if (!adcHandle) return 100;

    // Enable voltage divider using cached board revision
    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, batteryUseHighForDivider ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    int rawSum = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        int raw = 0;
        adc_oneshot_read(adcHandle, ADC_CHANNEL_0, &raw);
        rawSum += raw;
    }
    int rawAvg = rawSum / BATTERY_SAMPLES;

    // Disable voltage divider to save power
    gpio_set_level((gpio_num_t)BATTERY_ADC_CTRL, batteryUseHighForDivider ? 0 : 1);

    float voltage;
    if (adcCaliHandle) {
        int mv = 0;
        adc_cali_raw_to_voltage(adcCaliHandle, rawAvg, &mv);
        voltage = (mv / 1000.0f) * BATTERY_DIVIDER;
    } else {
        // Fallback: 12-bit ADC with 12dB atten gives ~0-3.1V range
        voltage = (rawAvg / 4095.0f) * 3.1f * BATTERY_DIVIDER;
    }

    // Exponential moving average to smooth readings
    if (batteryFirstReading) {
        batterySmoothedVoltage = voltage;
        batteryFirstReading = false;
    } else {
        batterySmoothedVoltage = 0.7f * batterySmoothedVoltage + 0.3f * voltage;
    }

    // Map smoothed voltage to percentage using LiPo discharge curve
    int percent = voltageToPercent(batterySmoothedVoltage);

    ESP_LOGI(TAG, "Battery: raw=%d, voltage=%.2fV, smoothed=%.2fV, percent=%d%%",
             rawAvg, voltage, batterySmoothedVoltage, percent);
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
            if (!displayManager.isDisplayOn()) {
                // Screen is off - turn it on
                displayManager.setScreenPower(true);
                ESP_LOGI(TAG, "Display turned ON");
            } else if (displayManager.isIdle(IDLE_SCREEN_TIMEOUT_MS)) {
                // Screen is on but idle - wake to status view
                displayManager.registerActivity();
                ESP_LOGI(TAG, "Display woken from idle");
            } else {
                // Screen is on and active - turn it off
                displayManager.setScreenPower(false);
                ESP_LOGI(TAG, "Display turned OFF");
            }
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
    vTaskDelay(pdMS_TO_TICKS(300));  // Let peripherals stabilize after power-on

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

    // Initialize LoRa Transport (retry up to 3 times on failure)
    displayManager.showInitProgress("LoRa Radio", 60);
    ESP_LOGI(TAG, "Initializing LoRa SX1262...");
    {
        bool loraOk = false;
        for (int attempt = 1; attempt <= 3; attempt++) {
            if (loraTransport.begin(LORA_CS, LORA_RST, LORA_DIO1, LORA_BUSY)) {
                ESP_LOGI(TAG, "LoRa radio ready (attempt %d)", attempt);
                loraOk = true;
                break;
            }
            ESP_LOGW(TAG, "LoRa init attempt %d/3 failed", attempt);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (!loraOk) {
            ESP_LOGE(TAG, "LoRa initialization failed after 3 attempts!");
            displayManager.showMessage("LoRa FAIL!", 3000);
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
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

    // Set device name on display for identification
    {
        char devName[16];
        uint32_t devId = meshProtocol.getDeviceId();
        snprintf(devName, sizeof(devName), "%04X", (unsigned)(devId & 0xFFFF));
        displayManager.setDeviceName(devName);
    }

    #ifdef MESHTASTIC_TEST_MODE
    ESP_LOGI(TAG, "MESHTASTIC_TEST_MODE active - initializing BLE service");
    // Build unique BLE name from device ID so nodes are distinguishable
    char bleName[16];
    uint32_t devId = meshProtocol.getDeviceId();
    snprintf(bleName, sizeof(bleName), "SIMS-%04X", (unsigned)(devId & 0xFFFF));
    ESP_LOGI(TAG, "BLE device name: %s (deviceId: 0x%08X)", bleName, devId);
    if (meshtasticBLE.begin(bleName, &loraTransport, &meshProtocol)) {
        ESP_LOGI(TAG, "Meshtastic BLE service ready - device should appear in app!");
    } else {
        ESP_LOGE(TAG, "Meshtastic BLE service failed to start");
    }
    #endif

    // Reset idle timer so the 2-min timeout starts from end of init
    displayManager.registerActivity();
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

        #ifdef MESHTASTIC_TEST_MODE
        // In Meshtastic mode: handle raw LoRa packets directly (bypass mesh protocol)
        meshtasticBLE.update();

        if (loraTransport.available()) {
            uint8_t rawBuf[256];
            size_t rawLen = 0;
            if (loraTransport.receive(rawBuf, &rawLen) && rawLen > 0) {
                ESP_LOGI(TAG, "LoRa raw packet: %d bytes, RSSI=%d, SNR=%.1f",
                         (int)rawLen, loraTransport.getRSSI(), loraTransport.getSNR());
                displayManager.registerActivity();

                // Parse packet fields for ACK handling
                uint32_t pktFrom = 0, pktTo = 0, pktId = 0;
                bool wantAck = false;
                bool parsed = extractMeshPacketFields(rawBuf, rawLen, &pktFrom, &pktTo, &pktId, &wantAck);

                // Ignore our own packets
                if (parsed && pktFrom == meshProtocol.getDeviceId()) {
                    ESP_LOGD(TAG, "Ignoring own packet (from=0x%08X)", (unsigned)pktFrom);
                } else {
                    // Forward raw MeshPacket to BLE client
                    if (meshtasticBLE.isConnected()) {
                        meshtasticBLE.queueRawMeshPacket(rawBuf, rawLen);
                        ESP_LOGI(TAG, "Raw MeshPacket queued for BLE client");
                    }

                    // Send routing ACK if requested
                    if (parsed && wantAck) {
                        uint8_t ackBuf[64];
                        size_t ackLen = createMeshtasticRoutingAck(
                            ackBuf, meshProtocol.getDeviceId(), pktFrom, pktId);
                        if (ackLen > 0) {
                            vTaskDelay(pdMS_TO_TICKS(50));  // Brief delay before ACK
                            if (loraTransport.send(ackBuf, ackLen)) {
                                ESP_LOGI(TAG, "Routing ACK sent to 0x%08X for pkt 0x%08X",
                                         (unsigned)pktFrom, (unsigned)pktId);
                            } else {
                                ESP_LOGW(TAG, "Routing ACK send failed");
                            }
                        }
                    }
                }
            }
        }
        #else
        // Normal SIMS mode: use mesh protocol
        meshProtocol.update();

        if (meshProtocol.hasMessage()) {
            MeshMessage msg = meshProtocol.receiveMessage();
            ESP_LOGI(TAG, "Received message: type=%d, from=0x%08X, RSSI=%d",
                     msg.messageType, (unsigned int)msg.sourceId, loraTransport.getRSSI());

            displayManager.registerActivity();

            switch (msg.messageType) {
                case MSG_TYPE_INCIDENT:
                    ESP_LOGI(TAG, "Incident report received (%d bytes from 0x%08X)",
                             msg.payloadSize, (unsigned int)msg.sourceId);
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
        #endif

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

            size_t len = createMeshtasticNodeInfoPacket(packet, nodeId,
                meshtasticBLE.storedDeviceName, meshtasticBLE.storedShortName);

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
                displayManager.updateIdleAnimation(batteryPercent);

                // Auto-sleep after extended idle with no BLE clients
                if (!bleConnected && displayManager.isIdle(AUTO_SLEEP_TIMEOUT_MS)) {
                    ESP_LOGI(TAG, "Auto-sleep: idle for %ds with no BLE clients",
                             AUTO_SLEEP_TIMEOUT_MS / 1000);
                    enterDeepSleep();
                }
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
    // Log wake cause
    esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Woke from deep sleep (button press)");
    } else if (wakeCause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "Woke from deep sleep (cause=%d)", (int)wakeCause);
    }

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

    // Rapid 10-blink identification pattern at very start
    {
        gpio_config_t led_io = {};
        led_io.pin_bit_mask = (1ULL << STATUS_LED);
        led_io.mode = GPIO_MODE_OUTPUT;
        gpio_config(&led_io);
        for (int i = 0; i < 10; i++) {
            gpio_set_level((gpio_num_t)STATUS_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level((gpio_num_t)STATUS_LED, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Wait for button release after wake (prevents immediate re-sleep if held)
    if (wakeCause == ESP_SLEEP_WAKEUP_EXT0) {
        gpio_config_t btn_conf = {};
        btn_conf.pin_bit_mask = (1ULL << PTT_BUTTON);
        btn_conf.mode = GPIO_MODE_INPUT;
        btn_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&btn_conf);
        while (gpio_get_level(GPIO_NUM_0) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // Debounce
    }

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
