/**
 * Mesh Packet Encoder - hand-encoded protobuf for Meshtastic protocol
 *
 * Encodes/decodes the minimal subset of Meshtastic protobufs needed:
 * - ToRadio { want_config_id | packet: MeshPacket { to, decoded: Data { portnum, payload } } }
 * - FromRadio { config_complete_id | packet: MeshPacket { decoded: Data { portnum, payload } } }
 *
 * No nanopb dependency - direct protobuf wire format encoding.
 * Ported from Flutter's mesh_packet_builder.dart.
 */

#ifndef MESH_PACKET_ENCODER_H
#define MESH_PACKET_ENCODER_H

#include <stdint.h>
#include <stddef.h>

// Meshtastic protocol constants
#define MESH_BROADCAST_ADDR      0xFFFFFFFF
#define MESH_PORT_TEXT_MESSAGE    1
#define MESH_PORT_PRIVATE_APP    256

/**
 * Build a ToRadio message with want_config_id.
 * Used during BLE config exchange to request device configuration.
 *
 * Returns bytes written, or 0 on error.
 */
size_t meshBuildWantConfig(uint8_t* buffer, size_t maxLen, uint32_t nonce);

/**
 * Build a ToRadio message wrapping a data packet.
 * Broadcasts payload to all mesh nodes (dest = 0xFFFFFFFF).
 * portnum defaults to PRIVATE_APP (256). Use MESH_PORT_TEXT_MESSAGE (1)
 * for messages visible in the Meshtastic phone app.
 *
 * Returns bytes written, or 0 on error.
 */
size_t meshBuildDataPacket(uint8_t* buffer, size_t maxLen,
                           const uint8_t* payload, size_t payloadLen,
                           uint32_t packetId,
                           uint16_t portnum = MESH_PORT_PRIVATE_APP);

/**
 * Check if a FromRadio message contains config_complete_id matching nonce.
 * FromRadio.config_complete_id = field 8, varint.
 *
 * Returns true if this is a config_complete with matching nonce.
 */
bool meshIsConfigComplete(const uint8_t* data, size_t len, uint32_t nonce);

/**
 * Extract PRIVATE_APP payload from a FromRadio message.
 * Walks: FromRadio.packet(field 2) -> MeshPacket.decoded(field 4) -> Data.payload(field 2)
 * Checks Data.portnum(field 1) == PRIVATE_APP(256).
 *
 * Returns pointer into the input data buffer (no copy), and sets payloadLen.
 * Returns nullptr if not a PRIVATE_APP packet.
 */
const uint8_t* meshExtractPayload(const uint8_t* data, size_t len, size_t* payloadLen);

// ---- Low-level helpers (exposed for testing) ----

/**
 * ProtoWriter - writes protobuf wire format fields into a buffer.
 */
class ProtoWriter {
public:
    ProtoWriter(uint8_t* buf, size_t maxLen);

    void writeVarint(int fieldNumber, uint32_t value);
    void writeFixed32(int fieldNumber, uint32_t value);
    void writeBytes(int fieldNumber, const uint8_t* data, size_t len);

    size_t length() const { return pos_; }
    bool overflow() const { return overflow_; }

private:
    uint8_t* buf_;
    size_t maxLen_;
    size_t pos_;
    bool overflow_;

    void writeTag(int fieldNumber, int wireType);
    void writeRawVarint(uint32_t value);
    void writeByte(uint8_t b);
};

/**
 * ProtoReader - reads protobuf wire format fields from a buffer.
 */
class ProtoReader {
public:
    // Extract a varint field value. Returns -1 if not found.
    static int32_t extractVarint(const uint8_t* data, size_t len, int targetField);

    // Extract a length-delimited field (submessage/bytes).
    // Returns pointer into data, sets outLen. Returns nullptr if not found.
    static const uint8_t* extractLengthDelimited(const uint8_t* data, size_t len,
                                                  int targetField, size_t* outLen);

private:
    struct VarintResult {
        uint32_t value;
        size_t nextPos;
        bool valid;
    };

    static VarintResult readRawVarint(const uint8_t* data, size_t len, size_t pos);
};

#endif // MESH_PACKET_ENCODER_H
