/**
 * Command Parser Implementation
 *
 * TODO: Integrate ESP32-SR library
 * - Install: cd lib && git clone https://github.com/espressif/esp-sr.git
 * - Include: esp_mn_iface.h, esp_mn_models.h
 * - Models: MultiNet6 (recommended for ESP32-S3)
 */

#include "voice/command_parser.h"

#ifdef ESP32_SR_ENABLED
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#endif

CommandParser::CommandParser()
    : _state(STATE_UNINITIALIZED),
      _enabled(false),
      _lastCommand(CMD_NONE),
      _confidence(0),
      _modelData(nullptr),
      _commandStartTime(0) {
}

CommandParser::~CommandParser() {
    end();
}

bool CommandParser::begin() {
    Serial.println("[CommandParser] Initializing command parser...");

#ifdef ESP32_SR_ENABLED
    // Initialize ESP32-SR MultiNet for command recognition
    Serial.println("[CommandParser] Initializing ESP32-SR MultiNet...");

    // Get MultiNet interface handle
    esp_mn_iface_t *multinet = esp_mn_handle_from_name((char*)"mn6_en");
    if (multinet == nullptr) {
        Serial.println("[CommandParser] ERROR: Failed to get MultiNet handle");
        _state = STATE_ERROR;
        return false;
    }

    // Create MultiNet model instance
    model_iface_data_t *model_data = multinet->create((char*)"mn6_en", 5760);
    if (model_data == nullptr) {
        Serial.println("[CommandParser] ERROR: Failed to create MultiNet instance");
        _state = STATE_ERROR;
        return false;
    }

    // Build command list (ESP32-SR requires linked list of commands)
    // Create phrases (must persist for model lifetime)
    static esp_mn_phrase_t phrases[5];
    static esp_mn_node_t nodes[5];

    // Define commands (order determines command_id mapping)
    phrases[0].string = (char*)"take photo";
    phrases[0].phonemes = nullptr;
    phrases[0].command_id = 0;
    phrases[0].threshold = 0.0;
    phrases[0].wave = nullptr;

    phrases[1].string = (char*)"record voice";
    phrases[1].phonemes = nullptr;
    phrases[1].command_id = 1;
    phrases[1].threshold = 0.0;
    phrases[1].wave = nullptr;

    phrases[2].string = (char*)"send incident";
    phrases[2].phonemes = nullptr;
    phrases[2].command_id = 2;
    phrases[2].threshold = 0.0;
    phrases[2].wave = nullptr;

    phrases[3].string = (char*)"cancel";
    phrases[3].phonemes = nullptr;
    phrases[3].command_id = 3;
    phrases[3].threshold = 0.0;
    phrases[3].wave = nullptr;

    phrases[4].string = (char*)"status check";
    phrases[4].phonemes = nullptr;
    phrases[4].command_id = 4;
    phrases[4].threshold = 0.0;
    phrases[4].wave = nullptr;

    // Build linked list
    for (int i = 0; i < 5; i++) {
        nodes[i].phrase = &phrases[i];
        nodes[i].next = (i < 4) ? &nodes[i+1] : nullptr;
    }

    // Set commands
    esp_mn_error_t *error = multinet->set_speech_commands(model_data, &nodes[0]);
    if (error && error->num > 0) {
        Serial.printf("[CommandParser] WARNING: %d commands failed to add\n", error->num);
    }

    _modelData = model_data;
    _state = STATE_READY;
    _enabled = true;
    Serial.println("[CommandParser] ESP32-SR MultiNet initialized successfully");
    Serial.println("[CommandParser] Commands: take photo, record voice, send incident, cancel, status check");
    Serial.printf("[CommandParser] Sample rate: %d Hz\n", multinet->get_samp_rate(model_data));
    Serial.printf("[CommandParser] Chunk size: %d samples\n", multinet->get_samp_chunksize(model_data));
    return true;
#else
    Serial.println("[CommandParser] ESP32-SR not enabled - using stub mode");
    _state = STATE_READY;
    _enabled = true;
    return true;
#endif
}

void CommandParser::end() {
    if (_modelData != nullptr) {
#ifdef ESP32_SR_ENABLED
        // Cleanup ESP32-SR resources
        esp_mn_iface_t *multinet = esp_mn_handle_from_name((char*)"mn6_en");
        if (multinet != nullptr) {
            multinet->destroy(_modelData);
        }
#endif
        _modelData = nullptr;
    }
    _state = STATE_UNINITIALIZED;
    _enabled = false;
}

CommandParser::VoiceCommand CommandParser::parseCommand(int16_t* audioBuffer, size_t samples) {
    if (!_enabled || _state != STATE_LISTENING) {
        return CMD_NONE;
    }

    // Check timeout
    if (millis() - _commandStartTime > COMMAND_TIMEOUT) {
        Serial.println("[CommandParser] Command timeout");
        reset();
        return CMD_NONE;
    }

#ifdef ESP32_SR_ENABLED
    // Process audio through ESP32-SR MultiNet
    esp_mn_iface_t *multinet = esp_mn_handle_from_name((char*)"mn6_en");
    if (multinet == nullptr || _modelData == nullptr) {
        return CMD_NONE;
    }

    // Detect command in audio buffer
    esp_mn_state_t mn_state = multinet->detect(_modelData, audioBuffer);

    if (mn_state == ESP_MN_STATE_DETECTED) {
        // Command detected - get command ID and map to our enum
        esp_mn_results_t *mn_result = multinet->get_results(_modelData);
        int command_id = mn_result->command_id[0];  // Get first result

        _lastCommand = mapCommandId(command_id);
        _confidence = 90;  // MultiNet doesn't provide confidence, use high value
        _state = STATE_READY;

        Serial.printf("[CommandParser] Command detected: %s (ID: %d)\n",
                     commandToString(_lastCommand), command_id);

        return _lastCommand;
    }
#else
    // Stub mode - no actual parsing
    // For testing, return CMD_NONE
#endif

    return CMD_NONE;
}

const char* CommandParser::commandToString(VoiceCommand cmd) {
    switch (cmd) {
        case CMD_NONE: return "none";
        case CMD_TAKE_PHOTO: return "take photo";
        case CMD_RECORD_VOICE: return "record voice";
        case CMD_SEND_INCIDENT: return "send incident";
        case CMD_CANCEL: return "cancel";
        case CMD_STATUS_CHECK: return "status check";
        case CMD_UNKNOWN: return "unknown";
        default: return "invalid";
    }
}

void CommandParser::reset() {
    _lastCommand = CMD_NONE;
    _confidence = 0;
    _state = STATE_READY;
    _commandStartTime = 0;
}

void CommandParser::enable() {
    _enabled = true;
    _state = STATE_LISTENING;
    _commandStartTime = millis();
    Serial.println("[CommandParser] Listening for command...");
}

void CommandParser::disable() {
    _enabled = false;
    _state = STATE_READY;
}

bool CommandParser::initModel() {
    // TODO: Load MultiNet6 model
    // Model files should be in lib/esp-sr/model/
    return true;
}

void CommandParser::cleanupModel() {
    // TODO: Cleanup model resources
}

CommandParser::VoiceCommand CommandParser::mapCommandId(int commandId) {
    // Map ESP32-SR command IDs to our enum
    // Command IDs correspond to the order they were added
    switch (commandId) {
        case 0: return CMD_TAKE_PHOTO;
        case 1: return CMD_RECORD_VOICE;
        case 2: return CMD_SEND_INCIDENT;
        case 3: return CMD_CANCEL;
        case 4: return CMD_STATUS_CHECK;
        default: return CMD_UNKNOWN;
    }
}
