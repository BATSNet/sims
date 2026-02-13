/**
 * Meshtastic Protocol Adapter
 *
 * Converts between SIMS incidents and Meshtastic packets
 * Supports:
 * - Text messages (Meshtastic TEXT_MESSAGE_APP)
 * - Position updates (Meshtastic POSITION_APP)
 * - Telemetry (Meshtastic TELEMETRY_APP)
 *
 * NOTE: This is a simplified adapter. Full implementation requires:
 * 1. Meshtastic protobuf files (mesh.proto, portnums.proto, etc.)
 * 2. Nanopb code generation
 * 3. Integration with Meshtastic packet structure
 *
 * For now, this provides the interface for future integration.
 */

#ifndef MESHTASTIC_ADAPTER_H
#define MESHTASTIC_ADAPTER_H

#include <Arduino.h>
#include "config.h"

// Meshtastic packet types (simplified)
enum MeshtasticPortNum {
    MESHTASTIC_TEXT_MESSAGE = 1,
    MESHTASTIC_POSITION = 3,
    MESHTASTIC_NODEINFO = 4,
    MESHTASTIC_TELEMETRY = 67
};

// Simplified Meshtastic packet structure
// TODO: Replace with actual Meshtastic protobuf when integrated
struct MeshtasticPacket {
    uint32_t from;
    uint32_t to;
    uint8_t channel;
    uint8_t portNum;
    uint8_t payload[237];  // Max Meshtastic payload
    size_t payloadSize;
    uint8_t hopLimit;
    uint8_t wantAck;
};

// Simplified position data
struct MeshtasticPosition {
    int32_t latitudeI;   // Latitude * 1e-7
    int32_t longitudeI;  // Longitude * 1e-7
    int32_t altitude;    // Meters
    uint32_t time;       // Unix timestamp
};

class MeshtasticAdapter {
public:
    MeshtasticAdapter();

    // Convert SIMS incident to Meshtastic packet
    bool simsToMeshtastic(
        float latitude,
        float longitude,
        const char* description,
        MeshtasticPacket& outPacket
    );

    // Convert Meshtastic packet to SIMS format
    bool meshtasticToSims(
        const MeshtasticPacket& packet,
        float& outLatitude,
        float& outLongitude,
        char* outDescription,
        size_t descMaxLen
    );

    // Encode Meshtastic packet to bytes (for transmission)
    size_t encodePacket(const MeshtasticPacket& packet, uint8_t* buffer, size_t bufferSize);

    // Decode bytes to Meshtastic packet
    bool decodePacket(const uint8_t* data, size_t dataSize, MeshtasticPacket& outPacket);

    // Create position packet
    bool createPositionPacket(
        float latitude,
        float longitude,
        float altitude,
        MeshtasticPacket& outPacket
    );

    // Create text message packet
    bool createTextMessagePacket(
        const char* text,
        MeshtasticPacket& outPacket
    );

private:
    uint32_t deviceId;

    // Helper: Convert GPS coordinates to Meshtastic format
    int32_t latitudeToInt(float lat);
    int32_t longitudeToInt(float lon);
    float intToLatitude(int32_t latI);
    float intToLongitude(int32_t lonI);
};

#endif // MESHTASTIC_ADAPTER_H
