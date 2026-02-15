/**
 * SIMS Mesh Device - Main Firmware
 *
 * ESP32-S3 based mesh network node for incident reporting
 * Supports: LoRa mesh, GPS, camera, voice recording, on-device AI
 *
 * Hardware Targets:
 * - Heltec LoRa 32 V3 (ESP32-S3 + SX1262)
 * - Seeeduino XIAO ESP32S3 Sense (with camera)
 */

#include <Arduino.h>
#include <LittleFS.h>
#include <Wire.h>
#include "lora_transport.h"
#include "mesh/mesh_protocol.h"
#include "sensors/gps_service.h"
#include "sensors/camera_service.h"
#include "sensors/audio_service.h"
#include "storage/message_storage.h"
#include "display_manager.h"
#include "config.h"

#ifdef MESHTASTIC_TEST_MODE
#include "meshtastic_test.h"
#include "meshtastic_ble.h"
#endif

#define LITTLEFS LittleFS

// Pin definitions moved to config.h (see include/config.h)
// GPS pins updated: GPIO39/40 instead of GPIO43/44 to avoid USB serial conflict

/**
 * INTEGRATION GUIDE - WiFi, BLE, MQTT, Meshtastic
 *
 * This firmware now supports:
 * - Phase 0: GPS pin fix (GPIO39/40)
 * - Phase 1: WiFi connectivity with BLE configuration
 * - Phase 2: HTTP direct upload to backend
 * - Phase 3: MQTT gateway mode
 * - Phase 4: BLE mesh bridge (phones → LoRa)
 * - Phase 5: Meshtastic protocol integration
 *
 * TO ENABLE FULL FUNCTIONALITY:
 *
 * 1. Add service includes at top of file:
 *    #include "network/wifi_service.h"
 *    #include "network/wifi_config_ble.h"
 *    #include "network/http_client.h"
 *    #include "network/mqtt_client.h"
 *    #include "network/transport_manager.h"
 *    #include "ble/ble_service.h"
 *    #include "meshtastic_adapter.h"
 *    #include "protocol_manager.h"
 *
 * 2. Uncomment and add new service instances:
 *    WiFiService wifiService;
 *    WiFiConfigBLE wifiConfigBLE(&wifiService);
 *    HTTPClientService httpClient;
 *    MQTTClientService mqttClient;
 *    SIMSBLEService bleService;
 *    MeshtasticAdapter meshtasticAdapter;
 *    ProtocolManager protocolManager(&loraTransport);
 *    TransportManager transportManager(&wifiService, &httpClient, &messageStorage,
 *                                     &loraTransport, &meshProtocol);
 *
 * 3. In setupServices():
 *    // WiFi setup
 *    wifiService.begin();
 *    if (!wifiService.isConnected()) {
 *        wifiConfigBLE.begin();  // Start BLE config if no WiFi
 *    }
 *
 *    // HTTP client
 *    httpClient.begin(BACKEND_HOST, BACKEND_PORT);
 *
 *    // MQTT gateway (if enabled)
 *    if (GATEWAY_MODE_ENABLED && wifiService.isConnected()) {
 *        String clientId = MQTT_CLIENT_ID_PREFIX + String((uint32_t)ESP.getEfuseMac(), HEX);
 *        mqttClient.begin(MQTT_BROKER, MQTT_PORT, clientId.c_str());
 *    }
 *
 *    // BLE service
 *    bleService.begin(&loraTransport, &meshProtocol);
 *    bleService.setGPSLocation(&gpsService.getLocation());
 *
 *    // Meshtastic integration
 *    protocolManager.begin(PROTOCOL_MODE);
 *    protocolManager.setMeshtasticAdapter(&meshtasticAdapter);
 *
 * 4. In loop():
 *    // Update WiFi
 *    wifiService.update();
 *    wifiConfigBLE.update();
 *
 *    // Update MQTT
 *    if (mqttClient.isConnected()) {
 *        mqttClient.update();
 *
 *        // Publish status periodically
 *        static unsigned long lastStatusPublish = 0;
 *        if (millis() - lastStatusPublish > 60000) {
 *            MQTTClientService::NetworkStatus status;
 *            status.nodeCount = meshProtocol.getNodeCount();
 *            status.rssi = loraTransport.getLastRSSI();
 *            status.hopCount = 0;
 *            status.pendingMessages = messageStorage.getPendingCount();
 *            mqttClient.publishStatus(status);
 *            lastStatusPublish = millis();
 *        }
 *    }
 *
 *    // Update BLE
 *    bleService.update();
 *
 *    // Process offline queue
 *    transportManager.processQueue();
 *
 *    // Handle mesh messages and forward to BLE/MQTT
 *    if (meshProtocol.hasMessage()) {
 *        MeshMessage msg;
 *        if (meshProtocol.receiveMessage(msg)) {
 *            // Forward to BLE clients
 *            bleService.notifyMeshMessage(msg.payload, msg.payloadSize);
 *
 *            // Forward to MQTT if gateway mode
 *            if (GATEWAY_MODE_ENABLED && mqttClient.isConnected()) {
 *                mqttClient.publishIncident(msg.payload, msg.payloadSize, msg.priority);
 *            }
 *        }
 *    }
 *
 *    // Update display with WiFi/BLE status
 *    displayManager.updateStatus(
 *        gpsService.isValid(),
 *        gpsService.getSatellites(),
 *        meshProtocol.getNodeCount(),
 *        messageStorage.getPendingCount(),
 *        100,  // Battery percent
 *        wifiService.isConnected(),
 *        wifiService.getRSSI(),
 *        bleService.getConnectedClientCount() > 0
 *    );
 *
 * 5. In sendIncidentReport():
 *    // Use transport manager instead of direct mesh send
 *    auto status = transportManager.sendIncident(
 *        gpsService.getLatitude(),
 *        gpsService.getLongitude(),
 *        gpsService.getAltitude(),
 *        PRIORITY_HIGH,
 *        0,  // Category
 *        "Incident description",
 *        imageData, imageSize,
 *        audioData, audioSize
 *    );
 *
 *    if (status.result == TransportManager::SEND_SUCCESS_WIFI) {
 *        Serial.println("Incident sent via WiFi");
 *    } else if (status.result == TransportManager::SEND_SUCCESS_LORA) {
 *        Serial.println("Incident sent via LoRa");
 *    } else if (status.result == TransportManager::SEND_QUEUED) {
 *        Serial.println("Incident queued for later");
 *    }
 *
 * CONFIGURATION:
 * - Edit config.h to set BACKEND_HOST, BACKEND_PORT, MQTT_BROKER
 * - Set GATEWAY_MODE_ENABLED true/false
 * - Set PROTOCOL_MODE (SIMS_ONLY/MESHTASTIC_ONLY/DUAL_HYBRID/BRIDGE)
 * - WiFi credentials stored via BLE or in NVS
 */

// Global service instances
DisplayManager displayManager;
LoRaTransport loraTransport;
MeshProtocol meshProtocol;
GPSService gpsService;
// CameraService cameraService;  // Not needed for mesh testing
// AudioService audioService;     // Not needed for mesh testing
MessageStorage messageStorage;

#ifdef MESHTASTIC_TEST_MODE
MeshtasticBLE meshtasticBLE;
#endif

// New service instances (uncomment to enable)
// WiFiService wifiService;
// WiFiConfigBLE wifiConfigBLE(&wifiService);
// HTTPClientService httpClient;
// MQTTClientService mqttClient;
// SIMSBLEService bleService;
// MeshtasticAdapter meshtasticAdapter;
// ProtocolManager protocolManager(&loraTransport);
// TransportManager transportManager(&wifiService, &httpClient, &messageStorage);

// Device state
enum DeviceState {
    STATE_IDLE,
    STATE_RECORDING_VOICE,
    STATE_CAPTURING_IMAGE,
    STATE_PROCESSING,
    STATE_TRANSMITTING
};

DeviceState currentState = STATE_IDLE;
bool pttPressed = false;
unsigned long pttPressTime = 0;

// Function prototypes
void setupHardware();
void setupServices();
void handlePushToTalk();
void handleMeshMessages();
void sendIncidentReport();
void updateDisplay();
void enterDeepSleep();

void setup() {
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH);  // LED on during setup

    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize

    Serial.println("\n\n========================================");
    Serial.println("SIMS Mesh Device - Production Ready");
    Serial.println("Version: 1.0.0");
    Serial.println("Board: Heltec WiFi LoRa 32 V3");
    #ifdef MESHTASTIC_TEST_MODE
    Serial.println("Mode: MESHTASTIC TEST (Sync: 0x2B)");
    Serial.println("Sending test messages every 30s");
    #else
    Serial.println("Mode: SIMS Protocol (Sync: 0x12)");
    #endif
    Serial.println("========================================\n");

    // Initialize hardware
    setupHardware();

    // Initialize services
    setupServices();

    Serial.println("\n[SYSTEM] Initialization complete");
    Serial.println("[SYSTEM] Device ready for operation");
    Serial.println("[SYSTEM] Loop starting...\n");

    digitalWrite(STATUS_LED, LOW);  // LED off after setup complete
}

void loop() {
    static bool firstLoop = true;
    if (firstLoop) {
        Serial.println(">>>>>> LOOP FIRST ITERATION - NEW FIRMWARE RUNNING <<<<<<");
        firstLoop = false;
    }

    // Update GPS service to process incoming NMEA data
    gpsService.update();

    // Update mesh protocol to handle incoming/outgoing messages
    meshProtocol.update();

    #ifdef MESHTASTIC_TEST_MODE
    // Update Meshtastic BLE (process messages between app and mesh)
    meshtasticBLE.update();
    #endif

    // Handle received mesh messages
    if (meshProtocol.hasMessage()) {
        MeshMessage msg = meshProtocol.receiveMessage();
        Serial.printf("[MESH] Received message: type=%d, from=0x%08X, RSSI=%d\n",
                     msg.messageType, msg.sourceId, loraTransport.getRSSI());

        // Process message based on type
        switch (msg.messageType) {
            case MSG_TYPE_INCIDENT:
                Serial.println("[MESH] Incident report received (relay)");
                break;
            case MSG_TYPE_HEARTBEAT:
                Serial.println("[MESH] Heartbeat received");
                break;
            case MSG_TYPE_ACK:
                Serial.println("[MESH] Acknowledgment received");
                messageStorage.markAsSent(msg.sequenceNumber);
                break;
            default:
                Serial.printf("[MESH] Unknown message type: %d\n", msg.messageType);
                break;
        }
    }

    // Heartbeat LED blink
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 1000) {
        digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
        lastBlink = millis();
    }

    // Meshtastic Test Mode: Send node info + test messages
    #ifdef MESHTASTIC_TEST_MODE
    static unsigned long lastTestMessage = 0;
    static bool nodeInfoSent = false;

    // Send node info once on startup (so device shows up in Meshtastic)
    if (!nodeInfoSent && millis() > 10000) {
        uint8_t packet[256];
        uint32_t nodeId = meshProtocol.getDeviceId();

        size_t len = createMeshtasticNodeInfoPacket(packet, nodeId, "SIMS-MESH", "SIMS");

        if (loraTransport.send(packet, len)) {
            Serial.println("[MESHTASTIC] Node info sent - device should appear in Meshtastic!");
            Serial.printf("[MESHTASTIC] Node ID: !%08x\n", nodeId);
            nodeInfoSent = true;
        } else {
            Serial.println("[MESHTASTIC] Node info FAILED");
        }
    }

    // Send text message every 30 seconds
    if (nodeInfoSent && millis() - lastTestMessage > 30000) {
        uint8_t packet[256];
        uint32_t nodeId = meshProtocol.getDeviceId();

        static int msgCount = 0;
        char message[64];
        snprintf(message, sizeof(message), "SIMS Test #%d - Hello Meshtastic!", ++msgCount);

        size_t len = createMeshtasticTextPacket(packet, nodeId, message);

        if (loraTransport.send(packet, len)) {
            Serial.printf("[MESHTASTIC] Text message sent: %s\n", message);
            Serial.printf("[MESHTASTIC] Packet size: %d bytes\n", len);
            displayManager.showMessage("MT SENT", 1000);
        } else {
            Serial.println("[MESHTASTIC] Text message FAILED");
        }

        lastTestMessage = millis();
    }
    #endif

    // Update status display every 5 seconds
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 5000) {
        // Get real service data
        bool gpsValid = gpsService.hasFix();
        int satellites = gpsService.getSatellites();
        int meshNodes = meshProtocol.getConnectedNodes();
        int pendingMessages = messageStorage.getPendingCount();
        int batteryPercent = 100;     // TODO: Read actual battery level (ADC)
        bool wifiConnected = false;   // Not enabled yet (Step 2)
        int wifiRSSI = 0;            // Not enabled yet (Step 2)
        bool bleConnected = false;    // Not enabled yet (Step 3)
        int loraRSSI = loraTransport.getRSSI();
        float loraSNR = loraTransport.getSNR();

        // Get packets received from mesh stats
        uint32_t sent, received, relayed;
        meshProtocol.getStats(sent, received, relayed);
        int packetsReceived = (int)received;

        displayManager.updateStatus(gpsValid, satellites, meshNodes,
                                   pendingMessages, batteryPercent,
                                   wifiConnected, wifiRSSI, bleConnected,
                                   loraRSSI, loraSNR, packetsReceived);
        lastDisplayUpdate = millis();

        // Debug output
        Serial.printf("[STATUS] GPS:%s Sats:%d Mesh:%d/%dpkts RSSI:%d SNR:%.1f Queue:%d\n",
                     gpsValid ? "OK" : "NO", satellites, meshNodes, packetsReceived,
                     loraRSSI, loraSNR, pendingMessages);
    }

    delay(10);
}

void setupHardware() {
    // Configure pins
    pinMode(PTT_BUTTON, INPUT_PULLUP);
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);

    // CRITICAL: Enable Vext power for external peripherals
    // GPIO36 controls P-channel FET: LOW = power ON, HIGH = power OFF
    // Required for: OLED, GPS module, camera module, any I2C/SPI peripherals
    pinMode(VEXT_CTRL, OUTPUT);
    digitalWrite(VEXT_CTRL, LOW);
    delay(100);  // Give power time to stabilize

    // 1 blink: Starting I2C init
    digitalWrite(STATUS_LED, HIGH);
    delay(200);
    digitalWrite(STATUS_LED, LOW);
    delay(200);

    // Initialize I2C for OLED
    Wire.begin(OLED_SDA, OLED_SCL);

    // 1.5 blinks: Wire.begin() completed
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);
    delay(300);

    // 2 blinks: I2C done, starting display init
    for (int i = 0; i < 2; i++) {
        digitalWrite(STATUS_LED, HIGH);
        delay(200);
        digitalWrite(STATUS_LED, LOW);
        delay(200);
    }

    // Initialize display FIRST (so we can show boot progress)
    if (displayManager.begin()) {
        // Display init SUCCESS - 3 blinks
        for (int i = 0; i < 3; i++) {
            digitalWrite(STATUS_LED, HIGH);
            delay(200);
            digitalWrite(STATUS_LED, LOW);
            delay(200);
        }
        displayManager.showBootScreen();
    } else {
        // Display init FAILED - rapid 10 blinks
        for (int i = 0; i < 10; i++) {
            digitalWrite(STATUS_LED, HIGH);
            delay(50);
            digitalWrite(STATUS_LED, LOW);
            delay(50);
        }
    }

    // Initialize PSRAM (required for AI models)
    if (psramFound()) {
        Serial.printf("PSRAM found: %d bytes\n", ESP.getPsramSize());
    } else {
        Serial.println("WARNING: PSRAM not found! AI features may be limited.");
    }

    // Initialize file system for message storage
    displayManager.showInitProgress("Storage", 10);
    if (!LITTLEFS.begin(true)) {
        Serial.println("ERROR: Failed to mount LITTLEFS");
    } else {
        Serial.println("LITTLEFS mounted successfully");
    }
}

void setupServices() {
    // Initialize Message Storage (LittleFS already mounted in setupHardware)
    displayManager.showInitProgress("Storage", 20);
    if (!messageStorage.begin()) {
        Serial.println("[ERROR] Message storage initialization failed!");
    } else {
        Serial.println("[INIT] Message storage ready");
    }
    delay(200);

    // Initialize GPS Service
    displayManager.showInitProgress("GPS", 40);
    if (!gpsService.begin(GPS_RX, GPS_TX)) {
        Serial.println("[WARN] GPS initialization failed - continuing without GPS");
    } else {
        Serial.println("[INIT] GPS service ready");
    }
    delay(200);

    // Initialize LoRa Transport
    displayManager.showInitProgress("LoRa Radio", 60);
    Serial.println("[INIT] Initializing LoRa SX1262...");
    if (!loraTransport.begin(LORA_CS, LORA_RST, LORA_DIO1, LORA_BUSY)) {
        Serial.println("[ERROR] LoRa initialization failed!");
        displayManager.showMessage("LoRa FAIL!", 3000);
        // Don't halt - allow restart attempt
        delay(3000);
    } else {
        Serial.println("[INIT] LoRa radio ready");
    }
    delay(200);

    // Initialize Mesh Protocol
    displayManager.showInitProgress("Mesh Proto", 80);
    uint32_t deviceId = (uint32_t)ESP.getEfuseMac();
    if (!meshProtocol.begin(&loraTransport)) {
        Serial.println("[ERROR] Mesh protocol initialization failed!");
    } else {
        meshProtocol.setDeviceId(deviceId);
        Serial.printf("[INIT] Mesh protocol ready (ID: 0x%08X)\n", deviceId);
    }
    delay(200);

    displayManager.showInitProgress("Complete", 100);
    Serial.println("[INIT] All core services ready");
    Serial.println("IMMEDIATE TEST - LINE RIGHT AFTER");

    // TEST: Toggle LED to prove this code executes
    digitalWrite(STATUS_LED, LOW);
    delay(100);
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);

    Serial.println("[INIT] LED toggle complete - starting BLE...");

    #ifdef MESHTASTIC_TEST_MODE
    Serial.println("[INIT] MESHTASTIC_TEST_MODE active - initializing BLE service");
    // Build unique BLE name from device ID so nodes are distinguishable
    char bleName[16];
    uint32_t devId = meshProtocol.getDeviceId();
    snprintf(bleName, sizeof(bleName), "SIMS-%04X", (unsigned)(devId & 0xFFFF));
    Serial.printf("[INIT] BLE device name: %s (deviceId: 0x%08X)\n", bleName, devId);
    if (meshtasticBLE.begin(bleName, &loraTransport, &meshProtocol)) {
        Serial.println("[INIT] Meshtastic BLE service ready - device should appear in app!");
    } else {
        Serial.println("[ERROR] Meshtastic BLE service failed to start");
    }
    #endif
}

/*
void handlePushToTalk() {
    bool buttonPressed = digitalRead(PTT_BUTTON) == LOW;

    // Button press detected
    if (buttonPressed && !pttPressed) {
        pttPressed = true;
        pttPressTime = millis();

        // Short press: capture image
        // Long press: record voice
        Serial.println("PTT button pressed");
        digitalWrite(STATUS_LED, HIGH);
    }

    // Button released
    if (!buttonPressed && pttPressed) {
        pttPressed = false;
        unsigned long pressDuration = millis() - pttPressTime;
        digitalWrite(STATUS_LED, LOW);

        Serial.printf("PTT released after %lu ms\n", pressDuration);

        if (pressDuration < 500) {
            // Short press - capture image
            Serial.println("Short press detected - capturing image");
            currentState = STATE_CAPTURING_IMAGE;

            #ifdef HAS_CAMERA
            if (cameraService.captureImage()) {
                Serial.println("Image captured successfully");
                sendIncidentReport();
            } else {
                Serial.println("Image capture failed");
                currentState = STATE_IDLE;
            }
            #else
            Serial.println("Camera not available");
            currentState = STATE_IDLE;
            #endif

        } else {
            // Long press - stop voice recording
            Serial.println("Long press detected - stopping voice recording");
            currentState = STATE_PROCESSING;

            if (audioService.stopRecording()) {
                Serial.println("Voice recording stopped");
                sendIncidentReport();
            } else {
                Serial.println("Voice recording failed");
                currentState = STATE_IDLE;
            }
        }
    }

    // Long press in progress - start voice recording
    if (pttPressed && !audioService.isRecording() &&
        millis() - pttPressTime > 500) {
        Serial.println("Starting voice recording...");
        currentState = STATE_RECORDING_VOICE;
        audioService.startRecording();
    }
}

void handleMeshMessages() {
    // Check for incoming mesh messages
    if (meshProtocol.hasMessage()) {
        MeshMessage msg = meshProtocol.receiveMessage();

        Serial.printf("Received mesh message: type=%d, from=%08X\n",
                     msg.messageType, msg.sourceId);

        // Process message based on type
        switch (msg.messageType) {
            case MSG_TYPE_INCIDENT:
                Serial.println("Received incident report (relay)");
                break;

            case MSG_TYPE_HEARTBEAT:
                Serial.println("Received heartbeat");
                break;

            case MSG_TYPE_ACK:
                Serial.println("Received acknowledgment");
                messageStorage.markAsSent(msg.sequenceNumber);
                break;

            case MSG_TYPE_ROUTE_REQUEST:
                Serial.println("Received route request");
                break;

            default:
                Serial.printf("Unknown message type: %d\n", msg.messageType);
                break;
        }
    }
}

void sendIncidentReport() {
    currentState = STATE_TRANSMITTING;

    Serial.println("Preparing incident report...");

    // Get GPS location
    GPSLocation location = gpsService.getLocation();

    if (!location.valid) {
        Serial.println("WARNING: No GPS fix available");
    } else {
        Serial.printf("GPS: lat=%.6f, lon=%.6f, alt=%.1f\n",
                     location.latitude, location.longitude, location.altitude);
    }

    // Create incident message
    IncidentReport incident;
    incident.latitude = location.latitude;
    incident.longitude = location.longitude;
    incident.altitude = location.altitude;
    incident.timestamp = millis();
    incident.deviceId = meshProtocol.getDeviceId();

    // Add captured media
    if (cameraService.hasImage()) {
        incident.hasImage = true;
        incident.imageData = cameraService.getImageData();
        incident.imageSize = cameraService.getImageSize();
        Serial.printf("Attaching image: %d bytes\n", incident.imageSize);
    }

    if (audioService.hasAudio()) {
        incident.hasAudio = true;
        incident.audioData = audioService.getAudioData();
        incident.audioSize = audioService.getAudioSize();
        Serial.printf("Attaching audio: %d bytes\n", incident.audioSize);
    }

    // TODO: Run on-device AI processing
    // - Voice-to-text transcription
    // - Image object detection
    // - Classification and compression

    // Send via mesh network
    Serial.println("Transmitting incident via mesh network...");
    if (meshProtocol.sendIncident(incident)) {
        Serial.println("Incident transmitted successfully");

        // Store in local storage (for retry if ACK not received)
        messageStorage.storeMessage(incident);
    } else {
        Serial.println("ERROR: Failed to transmit incident");
        // Store for retry
        messageStorage.storeMessage(incident);
    }

    // Cleanup
    cameraService.clearImage();
    audioService.clearAudio();

    currentState = STATE_IDLE;
    Serial.println("Ready for next incident");
}

void updateDisplay() {
    // Get GPS status
    GPSLocation location = gpsService.getLocation();
    bool gpsValid = location.valid;
    int satellites = gpsService.getSatellites();

    // Get mesh network status
    int meshNodes = meshProtocol.getConnectedNodes();

    // Get pending messages count
    int pendingMessages = messageStorage.getPendingCount();

    // Get battery level (placeholder - would need battery reading code)
    int batteryPercent = 100;  // TODO: Implement battery reading

    // Update display with current status
    displayManager.updateStatus(gpsValid, satellites, meshNodes,
                                pendingMessages, batteryPercent);
}

void enterDeepSleep() {
    Serial.println("Entering deep sleep mode...");

    // Configure wake-up source (PTT button)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PTT_BUTTON, LOW);

    // Power down peripherals
    // loraTransport.sleep();

    // Enter deep sleep
    esp_deep_sleep_start();
}
*/
