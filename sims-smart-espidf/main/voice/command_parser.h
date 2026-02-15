/**
 * Command Parser - ESP-IDF Port
 *
 * ESP32-SR MultiNet7 integration for voice command recognition.
 * Short 2-word command phrases for reliable recognition.
 */

#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration for ESP32-SR model data
struct model_iface_data_t;

// Command IDs - action commands
#define CMD_SEND              0
#define CMD_CANCEL            1
#define CMD_TAKE_PHOTO        2
#define CMD_CAPTURE           3
#define CMD_PICTURE           4

// Command IDs - incident description phrases (2 words each)
#define CMD_DRONE_NORTH       5
#define CMD_DRONE_SOUTH       6
#define CMD_DRONE_EAST        7
#define CMD_DRONE_WEST        8
#define CMD_VEHICLE_NORTH     9
#define CMD_VEHICLE_SOUTH     10
#define CMD_VEHICLE_EAST      11
#define CMD_VEHICLE_WEST      12
#define CMD_DRONE_SPOTTED     13
#define CMD_VEHICLE_SPOTTED   14
#define CMD_PERSON_SPOTTED    15
#define CMD_FIRE_DETECTED     16
#define CMD_SMOKE_DETECTED    17
#define CMD_ARMED_DRONE       18

#define CMD_COUNT             19
#define CMD_NONE             -1

// Legacy aliases for main.cpp compatibility
#define WORD_SEND    CMD_SEND
#define WORD_CANCEL  CMD_CANCEL
#define WORD_NONE    CMD_NONE

class CommandParser {
public:
    CommandParser();
    ~CommandParser();

    bool begin();
    void end();

    int parseCommand(int16_t* audioBuffer, size_t samples);

    static const char* getWordString(int cmdId);
    static const char* getDescription(int cmdId);
    static bool isActionWord(int cmdId);
    static bool isPhotoWord(int cmdId);
    static bool isDescriptiveWord(int cmdId);

    uint8_t getConfidence() const { return _confidence; }

    enum State {
        STATE_UNINITIALIZED,
        STATE_READY,
        STATE_LISTENING,
        STATE_PROCESSING,
        STATE_ERROR
    };
    State getState() const { return _state; }

    void reset();
    void enable();
    void disable();
    bool isEnabled() const { return _enabled; }

    int getChunkSize() const { return _chunkSize; }
    int getSampleRate() const { return _sampleRate; }

private:
    State _state;
    bool _enabled;
    int _lastWordId;
    uint8_t _confidence;

    void* _multinetHandle;
    model_iface_data_t* _modelData;

    int _chunkSize;
    int _sampleRate;

    unsigned long _commandStartTime;
    static const unsigned long COMMAND_TIMEOUT = 5000;
};

#endif // COMMAND_PARSER_H
