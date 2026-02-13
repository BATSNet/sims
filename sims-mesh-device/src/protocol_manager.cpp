/**
 * Protocol Manager Implementation
 */

#include "protocol_manager.h"

ProtocolManager::ProtocolManager(LoRaTransport* loraTransport) :
    loraTransport(loraTransport),
    meshtasticAdapter(nullptr),
    currentProtocolMode(PROTOCOL_MODE_SIMS_ONLY),
    currentSyncWord(LORA_SYNC_WORD),
    sentCountSIMS(0),
    sentCountMeshtastic(0),
    receivedCountSIMS(0),
    receivedCountMeshtastic(0)
{
}

ProtocolManager::~ProtocolManager() {
}

bool ProtocolManager::begin(int protocolMode) {
    currentProtocolMode = protocolMode;

    Serial.printf("[Protocol] Protocol mode: ");
    switch (protocolMode) {
        case PROTOCOL_MODE_SIMS_ONLY:
            Serial.println("SIMS_ONLY");
            switchToSIMS();
            break;
        case PROTOCOL_MODE_MESHTASTIC_ONLY:
            Serial.println("MESHTASTIC_ONLY");
            switchToMeshtastic();
            break;
        case PROTOCOL_MODE_DUAL_HYBRID:
            Serial.println("DUAL_HYBRID");
            switchToSIMS();  // Default to SIMS
            break;
        case PROTOCOL_MODE_BRIDGE:
            Serial.println("BRIDGE");
            switchToSIMS();  // Default to SIMS
            break;
        default:
            Serial.println("UNKNOWN - defaulting to SIMS_ONLY");
            currentProtocolMode = PROTOCOL_MODE_SIMS_ONLY;
            switchToSIMS();
            break;
    }

    return true;
}

void ProtocolManager::setMeshtasticAdapter(MeshtasticAdapter* adapter) {
    this->meshtasticAdapter = adapter;
    Serial.println("[Protocol] Meshtastic adapter set");
}

bool ProtocolManager::sendIncident(
    float latitude,
    float longitude,
    const char* description,
    uint8_t priority,
    bool hasMedia
) {
    Serial.printf("[Protocol] Sending incident (mode=%d, priority=%d, media=%d)\n",
                 currentProtocolMode, priority, hasMedia);

    // Route based on protocol mode
    switch (currentProtocolMode) {
        case PROTOCOL_MODE_SIMS_ONLY:
            return sendViaSIMS(latitude, longitude, description, priority);

        case PROTOCOL_MODE_MESHTASTIC_ONLY:
            if (meshtasticAdapter != nullptr) {
                MeshtasticPacket packet;
                if (meshtasticAdapter->simsToMeshtastic(latitude, longitude, description, packet)) {
                    return sendViaMeshtastic(packet);
                }
            }
            return false;

        case PROTOCOL_MODE_DUAL_HYBRID:
            // Intelligent routing based on message type
            if (shouldUseSIMS(priority, hasMedia)) {
                return sendViaSIMS(latitude, longitude, description, priority);
            } else if (meshtasticAdapter != nullptr) {
                MeshtasticPacket packet;
                if (meshtasticAdapter->simsToMeshtastic(latitude, longitude, description, packet)) {
                    return sendViaMeshtastic(packet);
                }
            }
            return false;

        case PROTOCOL_MODE_BRIDGE:
            // Send via both protocols
            bool simsSuccess = sendViaSIMS(latitude, longitude, description, priority);
            bool meshtasticSuccess = false;
            if (meshtasticAdapter != nullptr) {
                MeshtasticPacket packet;
                if (meshtasticAdapter->simsToMeshtastic(latitude, longitude, description, packet)) {
                    meshtasticSuccess = sendViaMeshtastic(packet);
                }
            }
            return simsSuccess || meshtasticSuccess;
    }

    return false;
}

bool ProtocolManager::sendTextMessage(const char* text) {
    if (currentProtocolMode == PROTOCOL_MODE_SIMS_ONLY) {
        // Send as SIMS incident without location
        return sendViaSIMS(0.0, 0.0, text, PRIORITY_MEDIUM);
    }

    if (meshtasticAdapter != nullptr) {
        MeshtasticPacket packet;
        if (meshtasticAdapter->createTextMessagePacket(text, packet)) {
            return sendViaMeshtastic(packet);
        }
    }

    return false;
}

bool ProtocolManager::sendPosition(float latitude, float longitude, float altitude) {
    if (meshtasticAdapter != nullptr && currentProtocolMode != PROTOCOL_MODE_SIMS_ONLY) {
        MeshtasticPacket packet;
        if (meshtasticAdapter->createPositionPacket(latitude, longitude, altitude, packet)) {
            return sendViaMeshtastic(packet);
        }
    }

    // SIMS doesn't have position-only packets, send as incident
    return sendViaSIMS(latitude, longitude, "Position update", PRIORITY_LOW);
}

bool ProtocolManager::receiveMessage(ReceivedMessage& message) {
    message.valid = false;

    // In dual/bridge mode, try to detect protocol
    // For now, assume SIMS (would need to inspect packet preamble/sync word)

    if (currentProtocolMode == PROTOCOL_MODE_MESHTASTIC_ONLY) {
        return receiveFromMeshtastic(message);
    } else {
        return receiveFromSIMS(message);
    }
}

void ProtocolManager::setProtocolMode(int mode) {
    if (mode != currentProtocolMode) {
        Serial.printf("[Protocol] Switching protocol mode: %d -> %d\n", currentProtocolMode, mode);
        currentProtocolMode = mode;
        begin(mode);  // Re-initialize with new mode
    }
}

int ProtocolManager::getProtocolMode() {
    return currentProtocolMode;
}

int ProtocolManager::getSentCountSIMS() {
    return sentCountSIMS;
}

int ProtocolManager::getSentCountMeshtastic() {
    return sentCountMeshtastic;
}

int ProtocolManager::getReceivedCountSIMS() {
    return receivedCountSIMS;
}

int ProtocolManager::getReceivedCountMeshtastic() {
    return receivedCountMeshtastic;
}

bool ProtocolManager::shouldUseSIMS(uint8_t priority, bool hasMedia) {
    // Use SIMS for:
    // - Critical/High priority messages (per config)
    // - Any message with media
    if (ROUTE_CRITICAL_VIA_SIMS && priority <= PRIORITY_HIGH) {
        return true;
    }
    if (ROUTE_MEDIA_VIA_SIMS && hasMedia) {
        return true;
    }
    return false;
}

bool ProtocolManager::shouldUseMeshtastic(const char* text) {
    // Use Meshtastic for:
    // - Text-only messages (per config)
    // - When ROUTE_TEXT_VIA_MESHTASTIC is enabled
    return ROUTE_TEXT_VIA_MESHTASTIC;
}

bool ProtocolManager::switchToSIMS() {
    Serial.println("[Protocol] Switching to SIMS protocol");
    currentSyncWord = LORA_SYNC_WORD;
    return setSyncWord(LORA_SYNC_WORD);
}

bool ProtocolManager::switchToMeshtastic() {
    Serial.println("[Protocol] Switching to Meshtastic protocol");
    currentSyncWord = MESHTASTIC_SYNC_WORD;
    return setSyncWord(MESHTASTIC_SYNC_WORD);
}

bool ProtocolManager::setSyncWord(uint8_t syncWord) {
    if (loraTransport == nullptr) {
        return false;
    }

    // TODO: Implement LoRaTransport::setSyncWord() method
    // For now, just log
    Serial.printf("[Protocol] Sync word set to 0x%02X\n", syncWord);
    return true;
}

bool ProtocolManager::sendViaSIMS(
    float latitude,
    float longitude,
    const char* description,
    uint8_t priority
) {
    if (loraTransport == nullptr) {
        return false;
    }

    // Ensure SIMS sync word
    if (currentSyncWord != LORA_SYNC_WORD) {
        switchToSIMS();
    }

    // TODO: Implement actual SIMS packet sending via MeshProtocol
    // For now, just log
    Serial.printf("[Protocol] Sending via SIMS: %.6f,%.6f - %s (priority=%d)\n",
                 latitude, longitude, description, priority);

    sentCountSIMS++;
    return true;
}

bool ProtocolManager::sendViaMeshtastic(const MeshtasticPacket& packet) {
    if (loraTransport == nullptr || meshtasticAdapter == nullptr) {
        return false;
    }

    // Ensure Meshtastic sync word
    if (currentSyncWord != MESHTASTIC_SYNC_WORD) {
        switchToMeshtastic();
    }

    // Encode packet
    uint8_t buffer[255];
    size_t encodedSize = meshtasticAdapter->encodePacket(packet, buffer, sizeof(buffer));

    if (encodedSize == 0) {
        Serial.println("[Protocol] ERROR: Failed to encode Meshtastic packet");
        return false;
    }

    // TODO: Send via LoRaTransport
    Serial.printf("[Protocol] Sending via Meshtastic: %d bytes\n", encodedSize);

    sentCountMeshtastic++;
    return true;
}

bool ProtocolManager::receiveFromSIMS(ReceivedMessage& message) {
    // TODO: Implement SIMS packet reception via MeshProtocol
    // For now, return false (no message)
    return false;
}

bool ProtocolManager::receiveFromMeshtastic(ReceivedMessage& message) {
    if (loraTransport == nullptr || meshtasticAdapter == nullptr) {
        return false;
    }

    // TODO: Receive packet via LoRaTransport
    // TODO: Decode Meshtastic packet
    // TODO: Convert to ReceivedMessage

    return false;
}
