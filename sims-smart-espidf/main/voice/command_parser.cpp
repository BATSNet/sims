/**
 * Command Parser Implementation - ESP-IDF Port
 *
 * Now with direct ESP32-SR MultiNet6 support.
 * Command recognition for: take photo, record voice, send incident, cancel, status check
 */

#include "voice/command_parser.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP32-SR includes (only available in ESP-IDF)
#include "esp_mn_iface.h"
#include "esp_mn_models.h"

static const char *TAG = "CmdParser";

CommandParser::CommandParser()
    : _state(STATE_UNINITIALIZED),
      _enabled(false),
      _lastCommand(CMD_NONE),
      _confidence(0),
      _multinetHandle(nullptr),
      _modelData(nullptr),
      _chunkSize(0),
      _sampleRate(16000),
      _commandStartTime(0) {
}

CommandParser::~CommandParser() {
    end();
}

bool CommandParser::begin() {
    ESP_LOGI(TAG, "Initializing command parser...");

    // Get MultiNet interface handle
    esp_mn_iface_t *multinet = esp_mn_handle_from_name("mn6_en");
    if (multinet == nullptr) {
        ESP_LOGE(TAG, "Failed to get MultiNet handle for 'mn6_en'");
        _state = STATE_ERROR;
        return false;
    }

    // Create MultiNet model instance
    model_iface_data_t *model_data = multinet->create("mn6_en", 5760);
    if (model_data == nullptr) {
        ESP_LOGE(TAG, "Failed to create MultiNet instance");
        _state = STATE_ERROR;
        return false;
    }

    // Build command list using ESP32-SR linked list structure
    static esp_mn_phrase_t phrases[5];
    static esp_mn_node_t nodes[5];

    // Define commands (order determines command_id mapping)
    phrases[0] = { .string = (char*)"take photo", .phonemes = nullptr,
                   .command_id = 0, .threshold = 0.0, .wave = nullptr };
    phrases[1] = { .string = (char*)"record voice", .phonemes = nullptr,
                   .command_id = 1, .threshold = 0.0, .wave = nullptr };
    phrases[2] = { .string = (char*)"send incident", .phonemes = nullptr,
                   .command_id = 2, .threshold = 0.0, .wave = nullptr };
    phrases[3] = { .string = (char*)"cancel", .phonemes = nullptr,
                   .command_id = 3, .threshold = 0.0, .wave = nullptr };
    phrases[4] = { .string = (char*)"status check", .phonemes = nullptr,
                   .command_id = 4, .threshold = 0.0, .wave = nullptr };

    // Build linked list
    for (int i = 0; i < 5; i++) {
        nodes[i].phrase = &phrases[i];
        nodes[i].next = (i < 4) ? &nodes[i + 1] : nullptr;
    }

    // Set commands in model
    esp_mn_error_t *err = multinet->set_speech_commands(model_data, &nodes[0]);
    if (err && err->num > 0) {
        ESP_LOGW(TAG, "%d commands failed to add", err->num);
    }

    _modelData = model_data;
    _multinetHandle = (void*)multinet;

    // Get audio parameters from model
    _sampleRate = multinet->get_samp_rate(model_data);
    _chunkSize = multinet->get_samp_chunksize(model_data);

    _state = STATE_READY;
    _enabled = true;

    ESP_LOGI(TAG, "MultiNet6 initialized successfully");
    ESP_LOGI(TAG, "Commands: take photo, record voice, send incident, cancel, status check");
    ESP_LOGI(TAG, "Sample rate: %d Hz, Chunk size: %d samples", _sampleRate, _chunkSize);

    return true;
}

void CommandParser::end() {
    if (_modelData != nullptr && _multinetHandle != nullptr) {
        esp_mn_iface_t *multinet = (esp_mn_iface_t*)_multinetHandle;
        multinet->destroy(_modelData);
        _modelData = nullptr;
        _multinetHandle = nullptr;
    }
    _state = STATE_UNINITIALIZED;
    _enabled = false;
}

CommandParser::VoiceCommand CommandParser::parseCommand(int16_t* audioBuffer, size_t samples) {
    if (!_enabled || _state != STATE_LISTENING) {
        return CMD_NONE;
    }

    // Check timeout
    unsigned long now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (_commandStartTime > 0 && (now - _commandStartTime > COMMAND_TIMEOUT)) {
        ESP_LOGW(TAG, "Command timeout");
        reset();
        return CMD_NONE;
    }

    if (_multinetHandle == nullptr || _modelData == nullptr) {
        return CMD_NONE;
    }

    esp_mn_iface_t *multinet = (esp_mn_iface_t*)_multinetHandle;

    // Detect command in audio buffer
    esp_mn_state_t mn_state = multinet->detect(_modelData, audioBuffer);

    if (mn_state == ESP_MN_STATE_DETECTED) {
        // Command detected - get command ID
        esp_mn_results_t *mn_result = multinet->get_results(_modelData);
        int command_id = mn_result->command_id[0];

        _lastCommand = mapCommandId(command_id);
        _confidence = 90;
        _state = STATE_READY;

        ESP_LOGI(TAG, "Command detected: %s (ID: %d, confidence: %d%%)",
                 commandToString(_lastCommand), command_id, _confidence);

        return _lastCommand;
    }

    return CMD_NONE;
}

const char* CommandParser::commandToString(VoiceCommand cmd) {
    switch (cmd) {
        case CMD_NONE:          return "none";
        case CMD_TAKE_PHOTO:    return "take photo";
        case CMD_RECORD_VOICE:  return "record voice";
        case CMD_SEND_INCIDENT: return "send incident";
        case CMD_CANCEL:        return "cancel";
        case CMD_STATUS_CHECK:  return "status check";
        case CMD_UNKNOWN:       return "unknown";
        default:                return "invalid";
    }
}

void CommandParser::reset() {
    _lastCommand = CMD_NONE;
    _confidence = 0;
    _state = STATE_READY;
    _commandStartTime = 0;
}

void CommandParser::enable() {
    if (_state != STATE_UNINITIALIZED && _state != STATE_ERROR) {
        _enabled = true;
        _state = STATE_LISTENING;
        _commandStartTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ESP_LOGI(TAG, "Listening for command...");
    }
}

void CommandParser::disable() {
    _enabled = false;
    if (_state == STATE_LISTENING) {
        _state = STATE_READY;
    }
}

CommandParser::VoiceCommand CommandParser::mapCommandId(int commandId) {
    switch (commandId) {
        case 0: return CMD_TAKE_PHOTO;
        case 1: return CMD_RECORD_VOICE;
        case 2: return CMD_SEND_INCIDENT;
        case 3: return CMD_CANCEL;
        case 4: return CMD_STATUS_CHECK;
        default: return CMD_UNKNOWN;
    }
}
