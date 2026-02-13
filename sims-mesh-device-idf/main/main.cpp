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

// Function prototypes
void setupHardware();
void setupServices();

void setupHardware() {
    // Configure status LED
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << STATUS_LED);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)STATUS_LED, 0);

    // Configure PTT button
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

void main_task(void* param) {
    bool firstLoop = true;

    while (1) {
        if (firstLoop) {
            ESP_LOGI(TAG, ">>>>>> LOOP FIRST ITERATION - NEW FIRMWARE RUNNING <<<<<<");
            firstLoop = false;
        }

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
                displayManager.showMessage("MT SENT", 1000);
            } else {
                ESP_LOGE(TAG, "Meshtastic text message FAILED");
            }

            lastTestMessage = millis();
        }
        #endif

        // Update status display every 5 seconds
        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 5000) {
            bool gpsValid = gpsService.hasFix();
            int satellites = gpsService.getSatellites();
            int meshNodes = meshProtocol.getConnectedNodes();
            int pendingMessages = messageStorage.getPendingCount();
            int batteryPercent = 100;
            bool wifiConnected = false;
            int wifiRSSI = 0;
            bool bleConnected = false;
            int loraRSSI = loraTransport.getRSSI();
            float loraSNR = loraTransport.getSNR();

            uint32_t sent, received, relayed;
            meshProtocol.getStats(sent, received, relayed);
            int packetsReceived = (int)received;

            displayManager.updateStatus(gpsValid, satellites, meshNodes,
                                       pendingMessages, batteryPercent,
                                       wifiConnected, wifiRSSI, bleConnected,
                                       loraRSSI, loraSNR, packetsReceived);
            lastDisplayUpdate = millis();

            ESP_LOGI(TAG, "GPS:%s Sats:%d Mesh:%d/%dpkts RSSI:%d SNR:%.1f Queue:%d",
                     gpsValid ? "OK" : "NO", satellites, meshNodes, packetsReceived,
                     loraRSSI, loraSNR, pendingMessages);
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
