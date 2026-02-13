/**
 * SIMS-SMART Device - Main Firmware
 *
 * Voice-controlled hands-free incident reporting
 * Hardware: Seeed Studio XIAO ESP32S3 Sense
 */

#include <Arduino.h>
#include "config.h"
#include "sensors/gps_service.h"
#include "network/wifi_service.h"
#include "network/http_client.h"
#include "voice/wake_word_service.h"
#include "voice/command_parser.h"
#include "led_feedback.h"

// TODO: Add camera and audio services when implemented
// #include "sensors/camera_service.h"
// #include "sensors/audio_service.h"

// State machine
enum SystemState {
    STATE_INIT,
    STATE_IDLE,                  // Listening for wake word
    STATE_WAKE_DETECTED,         // Wake word detected
    STATE_LISTENING_COMMAND,     // Listening for voice command
    STATE_CAPTURING_IMAGE,       // Taking photo
    STATE_RECORDING_VOICE,       // Recording audio
    STATE_PROCESSING,            // Getting GPS, building payload
    STATE_UPLOADING,             // Sending to backend
    STATE_SUCCESS,               // Upload successful
    STATE_ERROR                  // Error occurred
};

// Global services
GPSService gps;
WiFiService wifi;
HTTPClientService httpClient;
WakeWordService wakeWord;
CommandParser commandParser;
LEDFeedback led;

// State variables
SystemState currentState = STATE_INIT;
CommandParser::VoiceCommand lastCommand = CommandParser::CMD_NONE;
String incidentDescription = "";

// Captured data buffers
uint8_t* imageBuffer = nullptr;
size_t imageSize = 0;
uint8_t* audioBuffer = nullptr;
size_t audioSize = 0;

// Function declarations
void handleStateTransition(SystemState newState);
void handleWakeDetection();
void handleVoiceCommand();
void captureImage();
void recordAudio();
void processIncident();
void uploadIncident();
void handleError(const char* message);
void printStatus();

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("SIMS-SMART Device");
    Serial.println("Version: " FIRMWARE_VERSION);
    Serial.printf("Device ID: xiao-esp32s3-%08X\n", (uint32_t)ESP.getEfuseMac());
    Serial.println("========================================\n");

    // Initialize LED first for visual feedback
    if (!led.begin()) {
        Serial.println("[MAIN] ERROR: LED initialization failed");
    }

    led.setState(LEDFeedback::STATE_PROCESSING);

    // Initialize WiFi
    Serial.println("[MAIN] Initializing WiFi...");
    if (!wifi.begin()) {
        handleError("WiFi initialization failed");
        return;
    }

    // Initialize HTTP client
    httpClient.begin(BACKEND_URL);

    // Initialize GPS
#ifdef GPS_ENABLED
    Serial.println("[MAIN] Initializing GPS...");
    if (!gps.begin(GPS_RX_PIN, GPS_TX_PIN)) {
        handleError("GPS initialization failed");
        return;
    }
#endif

    // Initialize voice services
    Serial.println("[MAIN] Initializing voice recognition...");
    if (!wakeWord.begin(WAKE_WORD)) {
        Serial.println("[MAIN] WARNING: Wake word initialization failed (stub mode)");
    }

    if (!commandParser.begin()) {
        Serial.println("[MAIN] WARNING: Command parser initialization failed (stub mode)");
    }

    // TODO: Initialize camera and audio services
    /*
    #ifdef HAS_CAMERA
    if (!camera.begin()) {
        handleError("Camera initialization failed");
        return;
    }
    #endif

    #ifdef HAS_MICROPHONE
    if (!audio.begin()) {
        handleError("Audio initialization failed");
        return;
    }
    #endif
    */

    Serial.println("[MAIN] System initialized successfully");
    Serial.println("[MAIN] Ready for voice commands");
    Serial.printf("[MAIN] Wake word: \"%s\"\n", WAKE_WORD);

    // Transition to idle state
    handleStateTransition(STATE_IDLE);
}

void loop() {
    // Update services
    wifi.update();
    led.update();

#ifdef GPS_ENABLED
    gps.update();
#endif

    // State machine
    switch (currentState) {
        case STATE_IDLE:
            // Listening for wake word
            wakeWord.enable();
            if (wakeWord.isAwake()) {
                handleWakeDetection();
            }
            break;

        case STATE_WAKE_DETECTED:
            // Wake word detected, transition to listening for command
            handleStateTransition(STATE_LISTENING_COMMAND);
            break;

        case STATE_LISTENING_COMMAND:
            // Listening for voice command
            commandParser.enable();
            // TODO: Feed audio to command parser
            // For now, simulate command detection for testing
            handleVoiceCommand();
            break;

        case STATE_CAPTURING_IMAGE:
            captureImage();
            break;

        case STATE_RECORDING_VOICE:
            recordAudio();
            break;

        case STATE_PROCESSING:
            processIncident();
            break;

        case STATE_UPLOADING:
            uploadIncident();
            break;

        case STATE_SUCCESS:
            // Brief success indication, then return to idle
            delay(2000);
            handleStateTransition(STATE_IDLE);
            break;

        case STATE_ERROR:
            // Brief error indication, then return to idle
            delay(3000);
            handleStateTransition(STATE_IDLE);
            break;

        default:
            handleStateTransition(STATE_IDLE);
            break;
    }

    // Print status every 30 seconds (in debug mode)
#ifdef DEBUG_SERIAL
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 30000) {
        printStatus();
        lastStatusPrint = millis();
    }
#endif

    delay(10);  // Small delay to prevent tight loop
}

void handleStateTransition(SystemState newState) {
    if (currentState == newState) return;

    Serial.printf("[STATE] %d -> %d\n", currentState, newState);
    currentState = newState;

    // Update LED based on state
    switch (newState) {
        case STATE_IDLE:
            led.setState(LEDFeedback::STATE_IDLE);
            wakeWord.reset();
            commandParser.reset();
            break;

        case STATE_WAKE_DETECTED:
            led.setState(LEDFeedback::STATE_SUCCESS);
            break;

        case STATE_LISTENING_COMMAND:
            led.setState(LEDFeedback::STATE_LISTENING);
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

void handleWakeDetection() {
    Serial.println("[MAIN] Wake word detected!");
    handleStateTransition(STATE_WAKE_DETECTED);

    // Small delay for user feedback
    delay(500);
}

void handleVoiceCommand() {
    // TODO: Process audio through command parser
    // For now, this is a stub

    // Simulate command detection after timeout for testing
    static unsigned long commandWaitStart = 0;
    if (commandWaitStart == 0) {
        commandWaitStart = millis();
    }

    // In production, this would be:
    // CommandParser::VoiceCommand cmd = commandParser.parseCommand(audioBuffer, samples);

    // For testing, simulate timeout and return to idle
    if (millis() - commandWaitStart > COMMAND_TIMEOUT_MS) {
        Serial.println("[MAIN] Command timeout - returning to idle");
        commandWaitStart = 0;
        handleStateTransition(STATE_IDLE);
    }

    // Example of handling detected commands:
    /*
    if (cmd != CommandParser::CMD_NONE) {
        lastCommand = cmd;
        Serial.printf("[MAIN] Command: %s\n", commandParser.commandToString(cmd));

        switch (cmd) {
            case CommandParser::CMD_TAKE_PHOTO:
                handleStateTransition(STATE_CAPTURING_IMAGE);
                break;

            case CommandParser::CMD_RECORD_VOICE:
                handleStateTransition(STATE_RECORDING_VOICE);
                break;

            case CommandParser::CMD_SEND_INCIDENT:
                handleStateTransition(STATE_PROCESSING);
                break;

            case CommandParser::CMD_CANCEL:
                handleStateTransition(STATE_IDLE);
                break;

            case CommandParser::CMD_STATUS_CHECK:
                printStatus();
                handleStateTransition(STATE_IDLE);
                break;

            default:
                handleError("Unknown command");
                break;
        }
    }
    */
}

void captureImage() {
    Serial.println("[MAIN] Capturing image...");

    // TODO: Implement camera capture
    /*
    #ifdef HAS_CAMERA
    imageBuffer = camera.capture(&imageSize);
    if (imageBuffer == nullptr) {
        handleError("Camera capture failed");
        return;
    }
    Serial.printf("[MAIN] Image captured: %d bytes\n", imageSize);
    #endif
    */

    // After capture, decide next action based on command
    handleStateTransition(STATE_PROCESSING);
}

void recordAudio() {
    Serial.println("[MAIN] Recording audio...");

    // TODO: Implement audio recording
    /*
    #ifdef HAS_MICROPHONE
    audioBuffer = audio.record(MAX_VOICE_DURATION_MS, &audioSize);
    if (audioBuffer == nullptr) {
        handleError("Audio recording failed");
        return;
    }
    Serial.printf("[MAIN] Audio recorded: %d bytes\n", audioSize);
    #endif
    */

    // After recording, process incident
    handleStateTransition(STATE_PROCESSING);
}

void processIncident() {
    Serial.println("[MAIN] Processing incident...");

    // Get GPS location
    GPSLocation location = gps.getLocation();
    if (!location.valid) {
        Serial.println("[MAIN] WARNING: No GPS fix");
#ifdef GPS_USE_CACHED
        Serial.println("[MAIN] Using cached GPS location");
#else
        handleError("No GPS fix available");
        return;
#endif
    }

    // Build description from voice command
    incidentDescription = "Voice incident report";
    if (lastCommand != CommandParser::CMD_NONE) {
        incidentDescription = "Command: ";
        incidentDescription += commandParser.commandToString(lastCommand);
    }

    // Ready to upload
    handleStateTransition(STATE_UPLOADING);
}

void uploadIncident() {
    Serial.println("[MAIN] Uploading incident...");

    if (!wifi.isConnected()) {
        handleError("WiFi not connected");
        return;
    }

    GPSLocation location = gps.getLocation();

    // Upload incident
    HTTPClientService::IncidentUploadResult result = httpClient.uploadIncident(
        location.latitude,
        location.longitude,
        location.altitude,
        PRIORITY_HIGH,
        commandParser.commandToString(lastCommand),
        incidentDescription.c_str(),
        imageBuffer,
        imageSize,
        audioBuffer,
        audioSize
    );

    // Cleanup buffers
    if (imageBuffer != nullptr) {
        free(imageBuffer);
        imageBuffer = nullptr;
        imageSize = 0;
    }
    if (audioBuffer != nullptr) {
        free(audioBuffer);
        audioBuffer = nullptr;
        audioSize = 0;
    }

    // Handle result
    if (result.success) {
        Serial.printf("[MAIN] SUCCESS! Incident ID: %s\n", result.incidentId.c_str());
        handleStateTransition(STATE_SUCCESS);
    } else {
        Serial.printf("[MAIN] FAILED: %s (HTTP %d)\n",
                     result.message.c_str(), result.httpCode);
        handleError("Upload failed");
    }
}

void handleError(const char* message) {
    Serial.printf("[MAIN] ERROR: %s\n", message);
    handleStateTransition(STATE_ERROR);
}

void printStatus() {
    Serial.println("\n========== STATUS ==========");
    Serial.printf("State: %d\n", currentState);
    Serial.printf("WiFi: %s", wifi.isConnected() ? "Connected" : "Disconnected");
    if (wifi.isConnected()) {
        Serial.printf(" (%s, RSSI: %d dBm)", wifi.getSSID(), wifi.getRSSI());
    }
    Serial.println();

#ifdef GPS_ENABLED
    GPSLocation loc = gps.getLocation();
    Serial.printf("GPS: %s", loc.valid ? "FIX" : "NO FIX");
    if (loc.valid) {
        Serial.printf(" (%.6f, %.6f, %d sats)",
                     loc.latitude, loc.longitude, gps.getSatellites());
    }
    Serial.println();
#endif

    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
    Serial.println("============================\n");
}
