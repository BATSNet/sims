#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <Arduino.h>

// Forward declarations for ESP32-SR types
typedef struct esp_mn_iface esp_mn_iface_t;
typedef struct model_iface_data_t model_iface_data_t;

class CommandParser {
public:
    // Supported voice commands
    enum VoiceCommand {
        CMD_NONE = 0,
        CMD_TAKE_PHOTO,
        CMD_RECORD_VOICE,
        CMD_SEND_INCIDENT,
        CMD_CANCEL,
        CMD_STATUS_CHECK,
        CMD_UNKNOWN
    };

    CommandParser();
    ~CommandParser();

    // Initialize command recognition
    bool begin();

    // Stop command recognition
    void end();

    // Parse audio buffer and return recognized command
    VoiceCommand parseCommand(int16_t* audioBuffer, size_t samples);

    // Get command as string
    const char* commandToString(VoiceCommand cmd);

    // Get last command confidence (0-100)
    uint8_t getConfidence() const { return _confidence; }

    // Get current state
    enum State {
        STATE_UNINITIALIZED,
        STATE_READY,
        STATE_LISTENING,
        STATE_PROCESSING,
        STATE_ERROR
    };
    State getState() const { return _state; }

    // Reset to ready state
    void reset();

    // Enable/disable command recognition
    void enable();
    void disable();
    bool isEnabled() const { return _enabled; }

private:
    State _state;
    bool _enabled;
    VoiceCommand _lastCommand;
    uint8_t _confidence;

    // ESP32-SR model handles
    model_iface_data_t* _modelData;

    // Internal methods
    bool initModel();
    void cleanupModel();
    VoiceCommand mapCommandId(int commandId);

    // Command recognition timeout
    unsigned long _commandStartTime;
    static const unsigned long COMMAND_TIMEOUT = 5000; // 5 seconds
};

#endif // COMMAND_PARSER_H
