/**
 * Meshtastic Test Mode - Simple Packet Encoder
 * Creates basic Meshtastic-compatible packets for testing
 * ESP-IDF version - replaced random() with esp_random()
 */

#ifndef MESHTASTIC_TEST_H
#define MESHTASTIC_TEST_H

#include <stdint.h>
#include <string.h>
#include "esp_random.h"

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
    buffer[offset++] = (fieldNum << 3) | 2;
    pbEncodeVarint(buffer, offset, len);
    memcpy(&buffer[offset], str, len);
    offset += len;
}

inline void pbEncodeUint32(uint8_t* buffer, size_t& offset, uint8_t fieldNum, uint32_t value) {
    buffer[offset++] = (fieldNum << 3) | 0;
    pbEncodeVarint(buffer, offset, value);
}

inline size_t createMeshtasticTextPacket(uint8_t* packet, uint32_t fromNodeId, const char* message) {
    size_t offset = 0;

    pbEncodeUint32(packet, offset, 1, fromNodeId);
    pbEncodeUint32(packet, offset, 2, 0xFFFFFFFF);
    pbEncodeUint32(packet, offset, 3, esp_random() % 1000000);
    pbEncodeUint32(packet, offset, 11, 3);
    pbEncodeUint32(packet, offset, 12, 0);

    packet[offset++] = (5 << 3) | 2;

    size_t payloadStart = offset + 1;
    size_t payloadOffset = payloadStart;

    pbEncodeUint32(packet, payloadOffset, 1, MESHTASTIC_PORTNUM_TEXT_MESSAGE_APP);

    packet[payloadOffset++] = (2 << 3) | 2;
    size_t msgLen = strlen(message);
    pbEncodeVarint(packet, payloadOffset, msgLen);
    memcpy(&packet[payloadOffset], message, msgLen);
    payloadOffset += msgLen;

    size_t payloadLen = payloadOffset - payloadStart;
    packet[offset] = payloadLen;
    offset = payloadOffset;

    return offset;
}

inline size_t createMeshtasticNodeInfoPacket(uint8_t* packet, uint32_t fromNodeId,
                                               const char* longName, const char* shortName) {
    size_t offset = 0;

    pbEncodeUint32(packet, offset, 1, fromNodeId);
    pbEncodeUint32(packet, offset, 2, 0xFFFFFFFF);
    pbEncodeUint32(packet, offset, 3, esp_random() % 1000000);
    pbEncodeUint32(packet, offset, 11, 3);
    pbEncodeUint32(packet, offset, 12, 0);

    packet[offset++] = (5 << 3) | 2;

    size_t payloadStart = offset + 1;
    size_t payloadOffset = payloadStart;

    pbEncodeUint32(packet, payloadOffset, 1, MESHTASTIC_PORTNUM_NODEINFO_APP);

    packet[payloadOffset++] = (2 << 3) | 2;

    size_t userStart = payloadOffset + 1;
    size_t userOffset = userStart;

    char idStr[16];
    snprintf(idStr, sizeof(idStr), "!%08x", (unsigned int)fromNodeId);
    pbEncodeString(packet, userOffset, 1, idStr);
    pbEncodeString(packet, userOffset, 2, longName);
    pbEncodeString(packet, userOffset, 3, shortName);
    pbEncodeUint32(packet, userOffset, 4, 255);

    size_t userLen = userOffset - userStart;
    packet[payloadOffset] = userLen;
    payloadOffset = userOffset;

    size_t payloadLen = payloadOffset - payloadStart;
    packet[offset] = payloadLen;
    offset = payloadOffset;

    return offset;
}

#endif // MESHTASTIC_TEST_H
