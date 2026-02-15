/**
 * Command Parser Implementation - ESP-IDF Port
 *
 * MultiNet7 voice command recognition with short 2-word phrases.
 * Each phrase maps to a complete incident description string.
 */

#include "voice/command_parser.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP32-SR includes
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"

static const char *TAG = "CmdParser";

// Spoken phrases - what the user says (all 2 words or less)
static const char* CMD_PHRASES[CMD_COUNT] = {
    "send it",              // 0  - submit
    "cancel",               // 1  - abort
    "take photo",           // 2  - photo capture
    "capture",              // 3  - photo capture
    "picture",              // 4  - photo capture
    "drone north",          // 5
    "drone south",          // 6
    "drone east",           // 7
    "drone west",           // 8
    "vehicle north",        // 9
    "vehicle south",        // 10
    "vehicle east",         // 11
    "vehicle west",         // 12
    "drone spotted",        // 13
    "vehicle spotted",      // 14
    "person spotted",       // 15
    "fire detected",        // 16
    "smoke detected",       // 17
    "armed drone",          // 18
};

// Description strings - what gets sent as the incident description
static const char* CMD_DESCRIPTIONS[CMD_COUNT] = {
    "",                                     // 0  - send
    "",                                     // 1  - cancel
    "",                                     // 2  - take photo
    "",                                     // 3  - capture
    "",                                     // 4  - picture
    "Drone spotted, heading north",         // 5
    "Drone spotted, heading south",         // 6
    "Drone spotted, heading east",          // 7
    "Drone spotted, heading west",          // 8
    "Vehicle spotted, heading north",       // 9
    "Vehicle spotted, heading south",       // 10
    "Vehicle spotted, heading east",        // 11
    "Vehicle spotted, heading west",        // 12
    "Drone spotted",                        // 13
    "Vehicle spotted",                      // 14
    "Person spotted",                       // 15
    "Fire detected",                        // 16
    "Smoke detected",                       // 17
    "Armed drone spotted",                  // 18
};

CommandParser::CommandParser()
    : _state(STATE_UNINITIALIZED),
      _enabled(false),
      _lastWordId(CMD_NONE),
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
    ESP_LOGI(TAG, "Initializing command parser with %d phrases (MultiNet7)...", CMD_COUNT);

    esp_mn_iface_t *multinet = esp_mn_handle_from_name("mn7_en");
    if (multinet == nullptr) {
        ESP_LOGE(TAG, "Failed to get MultiNet handle for 'mn7_en'");
        _state = STATE_ERROR;
        return false;
    }

    model_iface_data_t *model_data = multinet->create("mn7_en", 5760);
    if (model_data == nullptr) {
        ESP_LOGE(TAG, "Failed to create MultiNet7 instance");
        _state = STATE_ERROR;
        return false;
    }

    _modelData = model_data;
    _multinetHandle = (void*)multinet;

    esp_err_t err = esp_mn_commands_alloc(multinet, model_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to alloc commands: %s", esp_err_to_name(err));
        multinet->destroy(model_data);
        _modelData = nullptr;
        _multinetHandle = nullptr;
        _state = STATE_ERROR;
        return false;
    }

    for (int i = 0; i < CMD_COUNT; i++) {
        err = esp_mn_commands_add(i, CMD_PHRASES[i]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to add phrase '%s': %s", CMD_PHRASES[i], esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Added phrase [%d]: '%s'", i, CMD_PHRASES[i]);
        }
    }

    esp_mn_error_t *mn_err = esp_mn_commands_update();
    if (mn_err != nullptr) {
        ESP_LOGW(TAG, "%d phrases failed phoneme parsing", mn_err->num);
        for (int i = 0; i < mn_err->num; i++) {
            if (mn_err->phrases[i]) {
                ESP_LOGW(TAG, "  Failed: '%s'", mn_err->phrases[i]->string);
            }
        }
    }

    _sampleRate = multinet->get_samp_rate(model_data);
    _chunkSize = multinet->get_samp_chunksize(model_data);

    _state = STATE_READY;
    _enabled = false;

    ESP_LOGI(TAG, "MultiNet7 initialized with %d command phrases", CMD_COUNT);
    ESP_LOGI(TAG, "Sample rate: %d Hz, Chunk size: %d samples", _sampleRate, _chunkSize);

    return true;
}

void CommandParser::end() {
    if (_modelData != nullptr && _multinetHandle != nullptr) {
        esp_mn_commands_free();
        esp_mn_iface_t *multinet = (esp_mn_iface_t*)_multinetHandle;
        multinet->destroy(_modelData);
        _modelData = nullptr;
        _multinetHandle = nullptr;
    }
    _state = STATE_UNINITIALIZED;
    _enabled = false;
}

int CommandParser::parseCommand(int16_t* audioBuffer, size_t samples) {
    if (!_enabled || _state != STATE_LISTENING) {
        return CMD_NONE;
    }

    if (_multinetHandle == nullptr || _modelData == nullptr) {
        return CMD_NONE;
    }

    esp_mn_iface_t *multinet = (esp_mn_iface_t*)_multinetHandle;
    esp_mn_state_t mn_state = multinet->detect(_modelData, audioBuffer);

    if (mn_state == ESP_MN_STATE_DETECTED) {
        esp_mn_results_t *mn_result = multinet->get_results(_modelData);
        int cmdId = mn_result->command_id[0];

        if (cmdId >= 0 && cmdId < CMD_COUNT) {
            _lastWordId = cmdId;
            _confidence = 90;

            ESP_LOGI(TAG, "Phrase detected: \"%s\" (ID: %d)", getWordString(cmdId), cmdId);
            return cmdId;
        }
    }

    return CMD_NONE;
}

const char* CommandParser::getWordString(int cmdId) {
    if (cmdId >= 0 && cmdId < CMD_COUNT) {
        return CMD_PHRASES[cmdId];
    }
    return "";
}

const char* CommandParser::getDescription(int cmdId) {
    if (cmdId >= 0 && cmdId < CMD_COUNT) {
        return CMD_DESCRIPTIONS[cmdId];
    }
    return "";
}

bool CommandParser::isActionWord(int cmdId) {
    return cmdId == CMD_SEND || cmdId == CMD_CANCEL;
}

bool CommandParser::isPhotoWord(int cmdId) {
    return cmdId == CMD_TAKE_PHOTO || cmdId == CMD_CAPTURE || cmdId == CMD_PICTURE;
}

bool CommandParser::isDescriptiveWord(int cmdId) {
    return cmdId >= CMD_DRONE_NORTH && cmdId < CMD_COUNT;
}

void CommandParser::reset() {
    _lastWordId = CMD_NONE;
    _confidence = 0;
    _state = STATE_READY;
    _commandStartTime = 0;
}

void CommandParser::enable() {
    if (_state != STATE_UNINITIALIZED && _state != STATE_ERROR) {
        _enabled = true;
        _state = STATE_LISTENING;
        _commandStartTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ESP_LOGI(TAG, "Listening for command phrases...");
    }
}

void CommandParser::disable() {
    _enabled = false;
    if (_state == STATE_LISTENING) {
        _state = STATE_READY;
    }
}
