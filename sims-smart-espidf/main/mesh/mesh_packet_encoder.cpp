/**
 * Mesh Packet Encoder Implementation
 *
 * Hand-encoded protobuf for Meshtastic ToRadio/FromRadio messages.
 * Ported from Flutter's mesh_packet_builder.dart.
 */

#include "mesh/mesh_packet_encoder.h"
#include <string.h>

// ---------------------------------------------------------------------------
// ProtoWriter
// ---------------------------------------------------------------------------

ProtoWriter::ProtoWriter(uint8_t* buf, size_t maxLen)
    : buf_(buf), maxLen_(maxLen), pos_(0), overflow_(false) {}

void ProtoWriter::writeByte(uint8_t b) {
    if (pos_ < maxLen_) {
        buf_[pos_++] = b;
    } else {
        overflow_ = true;
    }
}

void ProtoWriter::writeRawVarint(uint32_t value) {
    while (value > 0x7F) {
        writeByte((value & 0x7F) | 0x80);
        value >>= 7;
    }
    writeByte(value & 0x7F);
}

void ProtoWriter::writeTag(int fieldNumber, int wireType) {
    writeRawVarint((fieldNumber << 3) | wireType);
}

void ProtoWriter::writeVarint(int fieldNumber, uint32_t value) {
    writeTag(fieldNumber, 0);  // wire type 0 = varint
    writeRawVarint(value);
}

void ProtoWriter::writeFixed32(int fieldNumber, uint32_t value) {
    writeTag(fieldNumber, 5);  // wire type 5 = fixed32
    writeByte(value & 0xFF);
    writeByte((value >> 8) & 0xFF);
    writeByte((value >> 16) & 0xFF);
    writeByte((value >> 24) & 0xFF);
}

void ProtoWriter::writeBytes(int fieldNumber, const uint8_t* data, size_t len) {
    writeTag(fieldNumber, 2);  // wire type 2 = length-delimited
    writeRawVarint(len);
    if (pos_ + len <= maxLen_) {
        memcpy(buf_ + pos_, data, len);
        pos_ += len;
    } else {
        overflow_ = true;
    }
}

// ---------------------------------------------------------------------------
// ProtoReader
// ---------------------------------------------------------------------------

ProtoReader::VarintResult ProtoReader::readRawVarint(const uint8_t* data, size_t len, size_t pos) {
    VarintResult result = {0, pos, false};
    int shift = 0;
    while (pos < len) {
        uint8_t b = data[pos++];
        result.value |= (uint32_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) {
            result.nextPos = pos;
            result.valid = true;
            return result;
        }
        shift += 7;
        if (shift >= 35) break;  // protect against malformed data
    }
    return result;
}

int32_t ProtoReader::extractVarint(const uint8_t* data, size_t len, int targetField) {
    size_t pos = 0;
    while (pos < len) {
        VarintResult tag = readRawVarint(data, len, pos);
        if (!tag.valid) break;
        pos = tag.nextPos;

        int fieldNumber = tag.value >> 3;
        int wireType = tag.value & 0x07;

        if (wireType == 0) {
            // Varint
            VarintResult val = readRawVarint(data, len, pos);
            if (!val.valid) break;
            pos = val.nextPos;
            if (fieldNumber == targetField) return (int32_t)val.value;
        } else if (wireType == 2) {
            // Length-delimited - skip
            VarintResult vLen = readRawVarint(data, len, pos);
            if (!vLen.valid) break;
            pos = vLen.nextPos + vLen.value;
        } else if (wireType == 5) {
            // Fixed32 - skip
            pos += 4;
        } else if (wireType == 1) {
            // Fixed64 - skip
            pos += 8;
        } else {
            break;
        }
    }
    return -1;
}

const uint8_t* ProtoReader::extractLengthDelimited(const uint8_t* data, size_t len,
                                                     int targetField, size_t* outLen) {
    size_t pos = 0;
    while (pos < len) {
        VarintResult tag = readRawVarint(data, len, pos);
        if (!tag.valid) break;
        pos = tag.nextPos;

        int fieldNumber = tag.value >> 3;
        int wireType = tag.value & 0x07;

        if (wireType == 0) {
            // Varint - skip
            VarintResult val = readRawVarint(data, len, pos);
            if (!val.valid) break;
            pos = val.nextPos;
        } else if (wireType == 2) {
            // Length-delimited
            VarintResult vLen = readRawVarint(data, len, pos);
            if (!vLen.valid) break;
            pos = vLen.nextPos;
            size_t fieldLen = vLen.value;
            if (pos + fieldLen > len) break;
            if (fieldNumber == targetField) {
                *outLen = fieldLen;
                return data + pos;
            }
            pos += fieldLen;
        } else if (wireType == 5) {
            pos += 4;
        } else if (wireType == 1) {
            pos += 8;
        } else {
            break;
        }
    }
    *outLen = 0;
    return nullptr;
}

// ---------------------------------------------------------------------------
// High-level functions
// ---------------------------------------------------------------------------

size_t meshBuildWantConfig(uint8_t* buffer, size_t maxLen, uint32_t nonce) {
    // ToRadio.want_config_id = field 8, varint
    ProtoWriter w(buffer, maxLen);
    w.writeVarint(8, nonce);
    if (w.overflow()) return 0;
    return w.length();
}

size_t meshBuildDataPacket(uint8_t* buffer, size_t maxLen,
                           const uint8_t* payload, size_t payloadLen,
                           uint32_t packetId, uint16_t portnum) {
    // Build from inside out using scratch buffers on stack

    // 1. Data submessage
    //    Data.portnum = field 1, varint
    //    Data.payload = field 2, bytes
    uint8_t dataBuf[512];
    ProtoWriter dataW(dataBuf, sizeof(dataBuf));
    dataW.writeVarint(1, portnum);
    dataW.writeBytes(2, payload, payloadLen);
    if (dataW.overflow()) return 0;

    // 2. MeshPacket submessage
    //    MeshPacket.to = field 2, fixed32 (broadcast)
    //    MeshPacket.id = field 6, fixed32
    //    MeshPacket.decoded = field 4, bytes (Data submessage)
    uint8_t packetBuf[540];
    ProtoWriter packetW(packetBuf, sizeof(packetBuf));
    packetW.writeFixed32(2, MESH_BROADCAST_ADDR);
    if (packetId != 0) {
        packetW.writeFixed32(6, packetId);
    }
    packetW.writeBytes(4, dataBuf, dataW.length());
    if (packetW.overflow()) return 0;

    // 3. ToRadio outer
    //    ToRadio.packet = field 1, bytes (MeshPacket submessage)
    ProtoWriter toRadioW(buffer, maxLen);
    toRadioW.writeBytes(1, packetBuf, packetW.length());
    if (toRadioW.overflow()) return 0;

    return toRadioW.length();
}

bool meshIsConfigComplete(const uint8_t* data, size_t len, uint32_t nonce) {
    // FromRadio.config_complete_id = field 8, varint
    int32_t val = ProtoReader::extractVarint(data, len, 8);
    return (val >= 0 && (uint32_t)val == nonce);
}

const uint8_t* meshExtractPayload(const uint8_t* data, size_t len, size_t* payloadLen) {
    *payloadLen = 0;

    // FromRadio.packet = field 2, length-delimited (MeshPacket)
    size_t meshPacketLen = 0;
    const uint8_t* meshPacket = ProtoReader::extractLengthDelimited(data, len, 2, &meshPacketLen);
    if (!meshPacket) return nullptr;

    // MeshPacket.decoded = field 4, length-delimited (Data)
    size_t dataLen = 0;
    const uint8_t* dataMsg = ProtoReader::extractLengthDelimited(meshPacket, meshPacketLen, 4, &dataLen);
    if (!dataMsg) return nullptr;

    // Data.portnum = field 1, varint - must be PRIVATE_APP (256)
    int32_t portnum = ProtoReader::extractVarint(dataMsg, dataLen, 1);
    if (portnum != MESH_PORT_PRIVATE_APP) return nullptr;

    // Data.payload = field 2, length-delimited
    return ProtoReader::extractLengthDelimited(dataMsg, dataLen, 2, payloadLen);
}
