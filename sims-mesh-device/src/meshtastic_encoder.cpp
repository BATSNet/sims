/**
 * Meshtastic Protobuf Encoder Implementation
 */

#include "meshtastic_encoder.h"
#include "meshtastic/mesh.pb.h"
#include <pb_encode.h>
#include <string.h>
#include <stdio.h>
#include <Arduino.h>

// Helper function to encode string fields using callbacks
static bool encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    const char *str = (const char*)(*arg);
    if (!str) {
        str = "";
    }

    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }

    return pb_encode_string(stream, (const uint8_t*)str, strlen(str));
}

// Build FromRadio with MyNodeInfo
size_t buildFromRadio_MyNodeInfo(uint8_t* buffer, size_t maxLen, uint32_t deviceId) {
    if (!buffer || maxLen == 0) {
        return 0;
    }

    // Initialize FromRadio message
    meshtastic_FromRadio fromRadio = meshtastic_FromRadio_init_zero;
    fromRadio.which_payload_variant = meshtastic_FromRadio_my_info_tag;
    fromRadio.id = 1;  // Packet ID

    // Set MyNodeInfo fields
    meshtastic_MyNodeInfo* myInfo = &fromRadio.payload_variant.my_info;
    myInfo->my_node_num = deviceId;
    myInfo->reboot_count = 0;
    myInfo->min_app_version = 30200;  // 3.2.0
    myInfo->firmware_edition = meshtastic_FirmwareEdition_VANILLA;
    myInfo->nodedb_count = 1;  // Just this node

    // Encode to buffer
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, maxLen);
    bool status = pb_encode(&stream, meshtastic_FromRadio_fields, &fromRadio);

    if (!status) {
        Serial.printf("[Encoder] MyNodeInfo encode failed: %s\n", PB_GET_ERROR(&stream));
        return 0;
    }

    Serial.printf("[Encoder] MyNodeInfo: %zu bytes, id=%u, nodeNum=%u\n",
                  stream.bytes_written, (unsigned int)fromRadio.id, (unsigned int)myInfo->my_node_num);

    // Hex dump first 32 bytes for debugging
    Serial.print("[Encoder]   Hex: ");
    for (size_t i = 0; i < stream.bytes_written && i < 32; i++) {
        Serial.printf("%02x", buffer[i]);
    }
    Serial.println();

    return stream.bytes_written;
}

// Build FromRadio with NodeInfo
size_t buildFromRadio_NodeInfo(uint8_t* buffer, size_t maxLen, uint32_t deviceId,
                                 const char* longName, const char* shortName) {
    if (!buffer || maxLen == 0 || !longName || !shortName) {
        return 0;
    }

    // Initialize FromRadio message
    meshtastic_FromRadio fromRadio = meshtastic_FromRadio_init_zero;
    fromRadio.which_payload_variant = meshtastic_FromRadio_node_info_tag;
    fromRadio.id = 2;  // Packet ID

    // Set NodeInfo fields
    meshtastic_NodeInfo* nodeInfo = &fromRadio.payload_variant.node_info;
    nodeInfo->num = deviceId;
    nodeInfo->has_user = true;

    // Set User fields with string callbacks
    meshtastic_User* user = &nodeInfo->user;

    // Generate node ID string (e.g., "!ed020f3c")
    static char nodeIdStr[16];
    snprintf(nodeIdStr, sizeof(nodeIdStr), "!%08x", (unsigned int)deviceId);

    // Setup string callbacks
    user->id.funcs.encode = &encode_string;
    user->id.arg = (void*)nodeIdStr;

    user->long_name.funcs.encode = &encode_string;
    user->long_name.arg = (void*)longName;

    user->short_name.funcs.encode = &encode_string;
    user->short_name.arg = (void*)shortName;

    user->hw_model = meshtastic_HardwareModel_HELTEC_V3;
    user->is_licensed = false;
    user->role = meshtastic_Config_DeviceConfig_Role_CLIENT;

    // Encode to buffer
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, maxLen);
    bool status = pb_encode(&stream, meshtastic_FromRadio_fields, &fromRadio);

    if (!status) {
        Serial.printf("[Encoder] NodeInfo encode failed: %s\n", PB_GET_ERROR(&stream));
        return 0;
    }

    Serial.printf("[Encoder] NodeInfo: %zu bytes, id=%u, nodeNum=%u\n",
                  stream.bytes_written, (unsigned int)fromRadio.id, (unsigned int)nodeInfo->num);

    // Hex dump first 32 bytes for debugging
    Serial.print("[Encoder]   Hex: ");
    for (size_t i = 0; i < stream.bytes_written && i < 32; i++) {
        Serial.printf("%02x", buffer[i]);
    }
    Serial.println();

    return stream.bytes_written;
}

// Build FromRadio with config_complete_id
size_t buildFromRadio_ConfigComplete(uint8_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) {
        return 0;
    }

    // Initialize FromRadio message
    meshtastic_FromRadio fromRadio = meshtastic_FromRadio_init_zero;
    fromRadio.which_payload_variant = meshtastic_FromRadio_config_complete_id_tag;
    fromRadio.id = 3;  // Packet ID
    fromRadio.payload_variant.config_complete_id = 1;

    // Encode to buffer
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, maxLen);
    bool status = pb_encode(&stream, meshtastic_FromRadio_fields, &fromRadio);

    if (!status) {
        Serial.printf("[Encoder] ConfigComplete encode failed: %s\n", PB_GET_ERROR(&stream));
        return 0;
    }

    Serial.printf("[Encoder] ConfigComplete: %zu bytes, id=%u\n",
                  stream.bytes_written, (unsigned int)fromRadio.id);

    // Hex dump for debugging
    Serial.print("[Encoder]   Hex: ");
    for (size_t i = 0; i < stream.bytes_written; i++) {
        Serial.printf("%02x", buffer[i]);
    }
    Serial.println();

    return stream.bytes_written;
}
