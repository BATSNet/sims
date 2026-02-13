/**
 * Protocol Manager
 *
 * Manages dual-protocol operation (SIMS + Meshtastic)
 * Features:
 * - Protocol detection based on sync word
 * - Automatic protocol switching for TX
 * - Message routing based on type and priority
 * - Operating modes: SIMS_ONLY, MESHTASTIC_ONLY, DUAL_HYBRID, BRIDGE
 */

#ifndef PROTOCOL_MANAGER_H
#define PROTOCOL_MANAGER_H

#include "lora_transport.h"
#include "meshtastic_adapter.h"
#include "config.h"

class ProtocolManager {
public:
    ProtocolManager(LoRaTransport* loraTransport);
    ~ProtocolManager();

    // Initialize with protocol mode
    bool begin(int protocolMode = PROTOCOL_MODE_SIMS_ONLY);

    // Set Meshtastic adapter
    void setMeshtasticAdapter(MeshtasticAdapter* adapter);

    // Send methods (automatically route based on mode and message type)
    bool sendIncident(
        float latitude,
        float longitude,
        const char* description,
        uint8_t priority,
        bool hasMedia = false
    );

    bool sendTextMessage(const char* text);
    bool sendPosition(float latitude, float longitude, float altitude);

    // Receive and process packet (auto-detect protocol)
    struct ReceivedMessage {
        enum Protocol {
            PROTOCOL_SIMS,
            PROTOCOL_MESHTASTIC
        } protocol;

        float latitude;
        float longitude;
        char description[256];
        uint8_t priority;
        bool valid;
    };

    bool receiveMessage(ReceivedMessage& message);

    // Protocol mode management
    void setProtocolMode(int mode);
    int getProtocolMode();

    // Statistics
    int getSentCountSIMS();
    int getSentCountMeshtastic();
    int getReceivedCountSIMS();
    int getReceivedCountMeshtastic();

private:
    LoRaTransport* loraTransport;
    MeshtasticAdapter* meshtasticAdapter;

    int currentProtocolMode;
    uint8_t currentSyncWord;

    // Statistics
    int sentCountSIMS;
    int sentCountMeshtastic;
    int receivedCountSIMS;
    int receivedCountMeshtastic;

    // Protocol selection logic
    bool shouldUseSIMS(uint8_t priority, bool hasMedia);
    bool shouldUseMeshtastic(const char* text);

    // Protocol switching
    bool switchToSIMS();
    bool switchToMeshtastic();
    bool setSyncWord(uint8_t syncWord);

    // Send via specific protocol
    bool sendViaSIMS(
        float latitude,
        float longitude,
        const char* description,
        uint8_t priority
    );

    bool sendViaMeshtastic(const MeshtasticPacket& packet);

    // Receive from specific protocol
    bool receiveFromSIMS(ReceivedMessage& message);
    bool receiveFromMeshtastic(ReceivedMessage& message);
};

#endif // PROTOCOL_MANAGER_H
