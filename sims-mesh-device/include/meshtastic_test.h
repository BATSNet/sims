/**
 * Meshtastic Test Mode - Simple Packet Encoder
 * Creates basic Meshtastic-compatible packets for testing
 */

#ifndef MESHTASTIC_TEST_H
#define MESHTASTIC_TEST_H

#include <Arduino.h>

// Meshtastic packet types
#define MESHTASTIC_PORTNUM_TEXT_MESSAGE_APP 1
#define MESHTASTIC_PORTNUM_NODEINFO_APP 4
#define MESHTASTIC_PORTNUM_POSITION_APP 3

// Simple protobuf field encoding helpers
inline void pbEncodeVarint(uint8_t* buffer, size_t& offset, uint32_t value) {
    while (value > 0x7F) {
        buffer[offset++] = (value & 0x7F) | 0x80;
        value >>= 7;
    }
    buffer[offset++] = value & 0x7F;
}

inline void pbEncodeString(uint8_t* buffer, size_t& offset, uint8_t fieldNum, const char* str) {
    size_t len = strlen(str);
    buffer[offset++] = (fieldNum << 3) | 2;  // Wire type 2 (length-delimited)
    pbEncodeVarint(buffer, offset, len);
    memcpy(&buffer[offset], str, len);
    offset += len;
}

inline void pbEncodeUint32(uint8_t* buffer, size_t& offset, uint8_t fieldNum, uint32_t value) {
    buffer[offset++] = (fieldNum << 3) | 0;  // Wire type 0 (varint)
    pbEncodeVarint(buffer, offset, value);
}

/**
 * Create a basic Meshtastic text message packet
 */
inline size_t createMeshtasticTextPacket(uint8_t* packet, uint32_t fromNodeId, const char* message) {
    size_t offset = 0;

    // Field 1: from (node ID)
    pbEncodeUint32(packet, offset, 1, fromNodeId);

    // Field 2: to (0xFFFFFFFF = broadcast)
    pbEncodeUint32(packet, offset, 2, 0xFFFFFFFF);

    // Field 3: id (random packet ID)
    pbEncodeUint32(packet, offset, 3, random(1000000));

    // Field 11: hop_limit (CRITICAL - defaults to 3)
    pbEncodeUint32(packet, offset, 11, 3);

    // Field 12: want_ack (false for broadcast)
    pbEncodeUint32(packet, offset, 12, 0);

    // Field 5: decoded (embedded Data message)
    packet[offset++] = (5 << 3) | 2;  // Field 5, wire type 2

    // Calculate payload size first
    size_t payloadStart = offset + 1;  // +1 for length byte (will fill later)
    size_t payloadOffset = payloadStart;

    // Data.portnum (field 1) = TEXT_MESSAGE_APP
    pbEncodeUint32(packet, payloadOffset, 1, MESHTASTIC_PORTNUM_TEXT_MESSAGE_APP);

    // Data.payload (field 2) = message bytes
    packet[payloadOffset++] = (2 << 3) | 2;  // Field 2, wire type 2
    size_t msgLen = strlen(message);
    pbEncodeVarint(packet, payloadOffset, msgLen);
    memcpy(&packet[payloadOffset], message, msgLen);
    payloadOffset += msgLen;

    // Fill in payload length
    size_t payloadLen = payloadOffset - payloadStart;
    packet[offset] = payloadLen;
    offset = payloadOffset;

    return offset;
}

/**
 * Create a Meshtastic node info packet (so device shows up in node list)
 */
inline size_t createMeshtasticNodeInfoPacket(uint8_t* packet, uint32_t fromNodeId,
                                               const char* longName, const char* shortName) {
    size_t offset = 0;

    // Field 1: from
    pbEncodeUint32(packet, offset, 1, fromNodeId);

    // Field 2: to (broadcast)
    pbEncodeUint32(packet, offset, 2, 0xFFFFFFFF);

    // Field 3: id
    pbEncodeUint32(packet, offset, 3, random(1000000));

    // Field 11: hop_limit
    pbEncodeUint32(packet, offset, 11, 3);

    // Field 12: want_ack (false)
    pbEncodeUint32(packet, offset, 12, 0);

    // Field 5: decoded (Data message)
    packet[offset++] = (5 << 3) | 2;

    size_t payloadStart = offset + 1;
    size_t payloadOffset = payloadStart;

    // Data.portnum = NODEINFO_APP
    pbEncodeUint32(packet, payloadOffset, 1, MESHTASTIC_PORTNUM_NODEINFO_APP);

    // Data.payload = User message
    packet[payloadOffset++] = (2 << 3) | 2;  // Field 2

    // Build User submessage
    size_t userStart = payloadOffset + 1;
    size_t userOffset = userStart;

    // User.id (field 1)
    char idStr[16];
    snprintf(idStr, sizeof(idStr), "!%08x", fromNodeId);
    pbEncodeString(packet, userOffset, 1, idStr);

    // User.longName (field 2)
    pbEncodeString(packet, userOffset, 2, longName);

    // User.shortName (field 3)
    pbEncodeString(packet, userOffset, 3, shortName);

    // User.hwModel (field 4) - set to 255 (unset/unknown)
    pbEncodeUint32(packet, userOffset, 4, 255);

    // Fill User length
    size_t userLen = userOffset - userStart;
    packet[payloadOffset] = userLen;
    payloadOffset = userOffset;

    // Fill payload length
    size_t payloadLen = payloadOffset - payloadStart;
    packet[offset] = payloadLen;
    offset = payloadOffset;

    return offset;
}

#endif // MESHTASTIC_TEST_H
