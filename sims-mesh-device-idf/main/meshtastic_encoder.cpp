/**
 * Meshtastic Protobuf Encoder Implementation
 * ESP-IDF version - uses ESP_LOG instead of Serial
 */

#include "meshtastic_encoder.h"
#include "meshtastic/mesh.pb.h"
#include <pb_encode.h>
#include <string.h>
#include <stdio.h>
#include "esp_log.h"

static const char* TAG = "Encoder";

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

size_t buildFromRadio_MyNodeInfo(uint8_t* buffer, size_t maxLen, uint32_t deviceId) {
    if (!buffer || maxLen == 0) return 0;

    meshtastic_FromRadio fromRadio = meshtastic_FromRadio_init_zero;
    fromRadio.which_payload_variant = meshtastic_FromRadio_my_info_tag;
    fromRadio.id = 1;

    meshtastic_MyNodeInfo* myInfo = &fromRadio.payload_variant.my_info;
    myInfo->my_node_num = deviceId;
    myInfo->reboot_count = 0;
    myInfo->min_app_version = 30200;
    myInfo->firmware_edition = meshtastic_FirmwareEdition_VANILLA;
    myInfo->nodedb_count = 1;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, maxLen);
    bool status = pb_encode(&stream, meshtastic_FromRadio_fields, &fromRadio);

    if (!status) {
        ESP_LOGE(TAG, "MyNodeInfo encode failed: %s", PB_GET_ERROR(&stream));
        return 0;
    }

    ESP_LOGI(TAG, "MyNodeInfo: %zu bytes, id=%u, nodeNum=%u",
             stream.bytes_written, (unsigned int)fromRadio.id, (unsigned int)myInfo->my_node_num);

    return stream.bytes_written;
}

size_t buildFromRadio_NodeInfo(uint8_t* buffer, size_t maxLen, uint32_t deviceId,
                                 const char* longName, const char* shortName) {
    if (!buffer || maxLen == 0 || !longName || !shortName) return 0;

    meshtastic_FromRadio fromRadio = meshtastic_FromRadio_init_zero;
    fromRadio.which_payload_variant = meshtastic_FromRadio_node_info_tag;
    fromRadio.id = 2;

    meshtastic_NodeInfo* nodeInfo = &fromRadio.payload_variant.node_info;
    nodeInfo->num = deviceId;
    nodeInfo->has_user = true;

    meshtastic_User* user = &nodeInfo->user;

    static char nodeIdStr[16];
    snprintf(nodeIdStr, sizeof(nodeIdStr), "!%08x", (unsigned int)deviceId);

    user->id.funcs.encode = &encode_string;
    user->id.arg = (void*)nodeIdStr;

    user->long_name.funcs.encode = &encode_string;
    user->long_name.arg = (void*)longName;

    user->short_name.funcs.encode = &encode_string;
    user->short_name.arg = (void*)shortName;

    user->hw_model = meshtastic_HardwareModel_HELTEC_V3;
    user->is_licensed = false;
    user->role = meshtastic_Config_DeviceConfig_Role_CLIENT;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, maxLen);
    bool status = pb_encode(&stream, meshtastic_FromRadio_fields, &fromRadio);

    if (!status) {
        ESP_LOGE(TAG, "NodeInfo encode failed: %s", PB_GET_ERROR(&stream));
        return 0;
    }

    ESP_LOGI(TAG, "NodeInfo: %zu bytes, id=%u, nodeNum=%u",
             stream.bytes_written, (unsigned int)fromRadio.id, (unsigned int)nodeInfo->num);

    return stream.bytes_written;
}

size_t buildFromRadio_ConfigComplete(uint8_t* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return 0;

    meshtastic_FromRadio fromRadio = meshtastic_FromRadio_init_zero;
    fromRadio.which_payload_variant = meshtastic_FromRadio_config_complete_id_tag;
    fromRadio.id = 3;
    fromRadio.payload_variant.config_complete_id = 1;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, maxLen);
    bool status = pb_encode(&stream, meshtastic_FromRadio_fields, &fromRadio);

    if (!status) {
        ESP_LOGE(TAG, "ConfigComplete encode failed: %s", PB_GET_ERROR(&stream));
        return 0;
    }

    ESP_LOGI(TAG, "ConfigComplete: %zu bytes, id=%u",
             stream.bytes_written, (unsigned int)fromRadio.id);

    return stream.bytes_written;
}
