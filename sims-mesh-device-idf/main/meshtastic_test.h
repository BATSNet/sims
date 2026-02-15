/**
 * Meshtastic Test Mode - Simple Packet Encoder
 * Creates basic Meshtastic-compatible packets for testing
 * ESP-IDF version - replaced random() with esp_random()
 *
 * MeshPacket proto field types (from mesh.proto):
 *   from = 1 (fixed32, wire type 5)
 *   to = 2 (fixed32, wire type 5)
 *   channel = 3 (varint)
 *   decoded = 5 (length-delimited, wire type 2)
 *   id = 6 (fixed32, wire type 5)
 *   hop_limit = 11 (varint)
 *   want_ack = 12 (varint)
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
    buffer[offset++] = (fieldNum << 3) | 2;  // wire type 2 = length-delimited
    pbEncodeVarint(buffer, offset, len);
    memcpy(&buffer[offset], str, len);
    offset += len;
}

// Encode a varint field (wire type 0)
inline void pbEncodeUint32(uint8_t* buffer, size_t& offset, uint8_t fieldNum, uint32_t value) {
    buffer[offset++] = (fieldNum << 3) | 0;  // wire type 0 = varint
    pbEncodeVarint(buffer, offset, value);
}

// Encode a fixed32 field (wire type 5) - little-endian 4 bytes
inline void pbEncodeFixed32(uint8_t* buffer, size_t& offset, uint8_t fieldNum, uint32_t value) {
    buffer[offset++] = (fieldNum << 3) | 5;  // wire type 5 = 32-bit
    buffer[offset++] = (value >>  0) & 0xFF;
    buffer[offset++] = (value >>  8) & 0xFF;
    buffer[offset++] = (value >> 16) & 0xFF;
    buffer[offset++] = (value >> 24) & 0xFF;
}

// Parse a raw MeshPacket protobuf to extract key fields for ACK handling
// Returns true if parsing succeeded and all fields were found
inline bool extractMeshPacketFields(const uint8_t* buf, size_t len,
                                     uint32_t* from, uint32_t* to,
                                     uint32_t* id, bool* wantAck) {
    *from = 0;
    *to = 0;
    *id = 0;
    *wantAck = false;

    size_t pos = 0;
    bool foundFrom = false;

    while (pos < len) {
        if (pos >= len) break;
        uint8_t tag = buf[pos++];
        uint8_t fieldNum = tag >> 3;
        uint8_t wireType = tag & 0x07;

        switch (wireType) {
            case 0: {  // varint
                uint32_t val = 0;
                int shift = 0;
                while (pos < len) {
                    uint8_t b = buf[pos++];
                    val |= (uint32_t)(b & 0x7F) << shift;
                    shift += 7;
                    if ((b & 0x80) == 0) break;
                }
                if (fieldNum == 12) {  // want_ack
                    *wantAck = (val != 0);
                }
                break;
            }
            case 2: {  // length-delimited - skip over
                uint32_t subLen = 0;
                int shift = 0;
                while (pos < len) {
                    uint8_t b = buf[pos++];
                    subLen |= (uint32_t)(b & 0x7F) << shift;
                    shift += 7;
                    if ((b & 0x80) == 0) break;
                }
                pos += subLen;  // skip submessage/bytes
                break;
            }
            case 5: {  // 32-bit (fixed32)
                if (pos + 4 > len) return false;
                uint32_t val = buf[pos] | (buf[pos+1] << 8) |
                               (buf[pos+2] << 16) | (buf[pos+3] << 24);
                pos += 4;
                if (fieldNum == 1) { *from = val; foundFrom = true; }
                else if (fieldNum == 2) { *to = val; }
                else if (fieldNum == 6) { *id = val; }
                break;
            }
            case 1: {  // 64-bit - skip
                pos += 8;
                break;
            }
            default:
                return false;  // unknown wire type
        }
    }

    return foundFrom;
}

// Create a Meshtastic routing ACK packet
// This tells the sender that we received their message
inline size_t createMeshtasticRoutingAck(uint8_t* packet, uint32_t ourNodeId,
                                          uint32_t destNodeId, uint32_t requestId) {
    size_t offset = 0;

    // MeshPacket header
    pbEncodeFixed32(packet, offset, 1, ourNodeId);     // from (fixed32)
    pbEncodeFixed32(packet, offset, 2, destNodeId);    // to (fixed32)
    pbEncodeFixed32(packet, offset, 6, esp_random());  // id (fixed32)
    pbEncodeUint32(packet, offset, 11, 3);             // hop_limit (varint)

    // decoded (field 5, length-delimited)
    // Contains: portnum=1 (ROUTING_APP), empty payload (Routing{error_reason=NONE}),
    //           request_id=original packet id
    packet[offset++] = (5 << 3) | 2;  // field 5, wire type 2

    size_t payloadStart = offset + 1;  // reserve 1 byte for length
    size_t payloadOffset = payloadStart;

    // Data.portnum = 1 (ROUTING_APP)
    pbEncodeUint32(packet, payloadOffset, 1, 1);

    // Data.payload = Routing{error_reason=NONE} = empty bytes in proto3
    // Encode as zero-length bytes field
    packet[payloadOffset++] = (2 << 3) | 2;  // field 2, wire type 2
    packet[payloadOffset++] = 0;              // length = 0

    // Data.request_id (field 6, fixed32)
    pbEncodeFixed32(packet, payloadOffset, 6, requestId);

    size_t payloadLen = payloadOffset - payloadStart;
    packet[offset] = payloadLen;
    offset = payloadOffset;

    return offset;
}

inline size_t createMeshtasticTextPacket(uint8_t* packet, uint32_t fromNodeId, const char* message) {
    size_t offset = 0;

    // MeshPacket header - from/to/id are fixed32
    pbEncodeFixed32(packet, offset, 1, fromNodeId);    // from (fixed32)
    pbEncodeFixed32(packet, offset, 2, 0xFFFFFFFF);    // to (fixed32, broadcast)
    pbEncodeFixed32(packet, offset, 6, esp_random());  // id (fixed32)
    pbEncodeUint32(packet, offset, 11, 3);             // hop_limit (varint)
    pbEncodeUint32(packet, offset, 12, 0);             // want_ack (varint)

    // decoded (field 5, length-delimited)
    packet[offset++] = (5 << 3) | 2;

    size_t payloadStart = offset + 1;  // reserve 1 byte for length
    size_t payloadOffset = payloadStart;

    // Data.portnum (field 1, varint)
    pbEncodeUint32(packet, payloadOffset, 1, MESHTASTIC_PORTNUM_TEXT_MESSAGE_APP);

    // Data.payload (field 2, length-delimited)
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

    // MeshPacket header - from/to/id are fixed32
    pbEncodeFixed32(packet, offset, 1, fromNodeId);    // from (fixed32)
    pbEncodeFixed32(packet, offset, 2, 0xFFFFFFFF);    // to (fixed32, broadcast)
    pbEncodeFixed32(packet, offset, 6, esp_random());  // id (fixed32)
    pbEncodeUint32(packet, offset, 11, 3);             // hop_limit (varint)
    pbEncodeUint32(packet, offset, 12, 0);             // want_ack (varint)

    // decoded (field 5, length-delimited)
    packet[offset++] = (5 << 3) | 2;

    size_t payloadStart = offset + 1;  // reserve 1 byte for length
    size_t payloadOffset = payloadStart;

    // Data.portnum (field 1, varint)
    pbEncodeUint32(packet, payloadOffset, 1, MESHTASTIC_PORTNUM_NODEINFO_APP);

    // Data.payload (field 2, length-delimited) - contains User message
    packet[payloadOffset++] = (2 << 3) | 2;

    size_t userStart = payloadOffset + 1;  // reserve 1 byte for length
    size_t userOffset = userStart;

    // User fields (all strings/varint, no fixed32)
    char idStr[16];
    snprintf(idStr, sizeof(idStr), "!%08x", (unsigned int)fromNodeId);
    pbEncodeString(packet, userOffset, 1, idStr);       // id (string)
    pbEncodeString(packet, userOffset, 2, longName);    // long_name (string)
    pbEncodeString(packet, userOffset, 3, shortName);   // short_name (string)
    pbEncodeUint32(packet, userOffset, 4, 255);         // hw_model (varint)

    size_t userLen = userOffset - userStart;
    packet[payloadOffset] = userLen;
    payloadOffset = userOffset;

    size_t payloadLen = payloadOffset - payloadStart;
    packet[offset] = payloadLen;
    offset = payloadOffset;

    return offset;
}

#endif // MESHTASTIC_TEST_H
