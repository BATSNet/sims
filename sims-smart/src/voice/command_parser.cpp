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
// TODO: Uncomment when ESP32-SR is installed
// #include "esp_mn_iface.h"
// #include "esp_mn_models.h"
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
    // TODO: Initialize ESP32-SR MultiNet when library is installed
    // Example initialization code:
    /*
    // Get MultiNet handle
    esp_mn_iface_t *multinet = (esp_mn_iface_t *)&MULTINET_MODEL;

    // Create model instance
    model_iface_data_t *model_data = multinet->create("mn6_en", 5760);  // MultiNet6 English
    if (model_data == nullptr) {
        Serial.println("[CommandParser] ERROR: Failed to create MultiNet instance");
        _state = STATE_ERROR;
        return false;
    }

    // Add commands (command IDs will be used for mapping)
    multinet->add_command(model_data, "take photo");
    multinet->add_command(model_data, "record voice");
    multinet->add_command(model_data, "send incident");
    multinet->add_command(model_data, "cancel");
    multinet->add_command(model_data, "status check");

    _modelData = model_data;
    */

    _state = STATE_READY;
    _enabled = true;
    Serial.println("[CommandParser] Initialized (ESP32-SR stub)");
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
        // TODO: Cleanup ESP32-SR resources
        // multinet->destroy(_modelData);
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
    // TODO: Process audio through ESP32-SR MultiNet when library is installed
    /*
    // Feed audio to MultiNet
    esp_mn_iface_t *multinet = (esp_mn_iface_t *)&MULTINET_MODEL;

    // Detect command
    esp_mn_state_t mn_state = multinet->detect(_modelData, audioBuffer);

    if (mn_state == ESP_MN_STATE_DETECTED) {
        // Command detected - get command ID
        esp_mn_results_t *mn_result = multinet->get_results(_modelData);
        int command_id = mn_result->command_id[0];  // Get first result

        _lastCommand = mapCommandId(command_id);
        _confidence = 90;  // MultiNet doesn't provide confidence, use high value
        _state = STATE_READY;

        Serial.printf("[CommandParser] Command detected: %s\n",
                     commandToString(_lastCommand));

        return _lastCommand;
    }
    */
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
