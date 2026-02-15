/**
 * Meshtastic BLE Service Implementation
 * ESP-IDF version using native NimBLE API
 *
 * Implements the official Meshtastic BLE protocol:
 * - Phone writes want_config_id to ToRadio
 * - Phone polls FromRadio repeatedly (READ only, no NOTIFY)
 * - State machine returns config messages in required order
 * - FromNum notifies phone when new data is available
 */

#include "meshtastic_ble.h"
#include "meshtastic_encoder.h"
#include "meshtastic_test.h"
#include <string.h>
#include <pb_decode.h>
#include "meshtastic/mesh.pb.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char* TAG = "MeshtasticBLE";

// Global pointer for C callbacks
static MeshtasticBLE* g_ble = nullptr;

// Meshtastic BLE UUIDs
static const ble_uuid128_t meshtasticServiceUuid =
    BLE_UUID128_INIT(0xfd, 0xea, 0x73, 0xe2, 0xca, 0x5d, 0xa8, 0x9f,
                     0x1f, 0x46, 0xa8, 0x15, 0x18, 0xb2, 0xa1, 0x6b);

static const ble_uuid128_t toRadioUuid =
    BLE_UUID128_INIT(0xe7, 0x01, 0x44, 0x12, 0x66, 0x78, 0xdd, 0xa1,
                     0xad, 0x4d, 0x9e, 0x12, 0xd2, 0x76, 0x5c, 0xf7);

static const ble_uuid128_t fromRadioUuid =
    BLE_UUID128_INIT(0x02, 0x00, 0x12, 0xac, 0x42, 0x02, 0x78, 0xb8,
                     0xed, 0x11, 0x93, 0x49, 0x9e, 0xe6, 0x55, 0x2c);

static const ble_uuid128_t fromNumUuid =
    BLE_UUID128_INIT(0x53, 0x44, 0xe3, 0x47, 0x75, 0xaa, 0x70, 0xa6,
                     0x66, 0x4f, 0x00, 0xa8, 0x8c, 0xa1, 0x9d, 0xed);

// Forward declarations for C callbacks
static int toradio_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static int fromradio_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int fromnum_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gap_event_cb(struct ble_gap_event *event, void *arg);
static void ble_host_task(void *param);
static void on_sync(void);
static void on_reset(int reason);
static void start_advertising(void);

// Attribute handles - NimBLE populates these during registration
static uint16_t fromRadioAttrHandle = 0;
static uint16_t fromNumAttrHandle = 0;

// GATT characteristic definitions
static struct ble_gatt_chr_def meshtastic_chars[] = {
    {
        // ToRadio - phone WRITES here
        .uuid = &toRadioUuid.u,
        .access_cb = toradio_access_cb,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        .min_key_size = 0,
        .val_handle = NULL,
    },
    {
        // FromRadio - phone READS here (NOT NOTIFY - critical!)
        .uuid = &fromRadioUuid.u,
        .access_cb = fromradio_access_cb,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ,
        .min_key_size = 0,
        .val_handle = &fromRadioAttrHandle,
    },
    {
        // FromNum - READ + NOTIFY to signal new data
        .uuid = &fromNumUuid.u,
        .access_cb = fromnum_access_cb,
        .arg = NULL,
        .descriptors = NULL,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &fromNumAttrHandle,
    },
    {
        0, // Terminator
    },
};

// Service definition
static struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &meshtasticServiceUuid.u,
        .includes = NULL,
        .characteristics = meshtastic_chars,
    },
    {
        0, // Terminator
    },
};

// --- ToRadio access callback ---
static int toradio_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (!g_ble) return BLE_ATT_ERR_UNLIKELY;

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Read the written data
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len > 0 && len < 512) {
            uint8_t buf[512];
            uint16_t copied = 0;
            int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, len, &copied);
            if (rc == 0 && copied > 0) {
                g_ble->handleToRadio(buf, copied);
            }
        }
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// --- FromRadio access callback ---
static int fromradio_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (!g_ble) return BLE_ATT_ERR_UNLIKELY;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t buffer[256];
        size_t len = g_ble->getFromRadio(buffer, sizeof(buffer));

        if (len > 0) {
            int rc = os_mbuf_append(ctxt->om, buffer, len);
            ESP_LOGI(TAG, "FromRadio read: %zu bytes", len);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        } else {
            // Return empty - signals "no more data" to phone
            ESP_LOGD(TAG, "FromRadio read: empty (no data)");
            return 0;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// --- FromNum access callback ---
static int fromnum_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (!g_ble) return BLE_ATT_ERR_UNLIKELY;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t val[4];
        val[0] = g_ble->fromNum & 0xFF;
        val[1] = (g_ble->fromNum >> 8) & 0xFF;
        val[2] = (g_ble->fromNum >> 16) & 0xFF;
        val[3] = (g_ble->fromNum >> 24) & 0xFF;
        int rc = os_mbuf_append(ctxt->om, val, 4);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// --- GAP event callback ---
static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    if (!g_ble) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                g_ble->connHandle = event->connect.conn_handle;
                g_ble->onConnect();
            } else {
                start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            g_ble->onDisconnect();
            start_advertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == fromNumAttrHandle) {
                bool subscribed = event->subscribe.cur_notify;
                g_ble->onFromNumSubscribe(subscribed);
            }
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU updated: %d", event->mtu.value);
            break;

        default:
            break;
    }

    return 0;
}

// --- Advertising ---
static void start_advertising(void) {
    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0x0020;  // 20ms (fast discovery)
    adv_params.itvl_max = 0x0040;  // 40ms

    // Primary advertising data: flags + service UUID (21 bytes, well within 31-byte limit)
    // Name goes in scan response to avoid exceeding adv packet size
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = &meshtasticServiceUuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting adv fields: %d", rc);
        return;
    }

    // Scan response: device name (so phone can see it after active scan)
    struct ble_hs_adv_fields rsp_fields = {};
    rsp_fields.name = (uint8_t*)ble_svc_gap_device_name();
    rsp_fields.name_len = strlen(ble_svc_gap_device_name());
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGW(TAG, "Error setting scan response: %d", rc);
    }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising started (UUID in adv, name in scan response)");
    }
}

// --- NimBLE host callbacks ---
static void on_sync(void) {
    uint8_t addr_type;
    int rc = ble_hs_id_infer_auto(0, &addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "BLE address type: %d", addr_type);

    start_advertising();
}

static void on_reset(int reason) {
    ESP_LOGW(TAG, "BLE host reset, reason: %d", reason);
}

static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// --- MeshtasticBLE class implementation ---

MeshtasticBLE::MeshtasticBLE()
    : loraTransport(nullptr), meshProtocol(nullptr),
      initialized(false), connectedClients(0), connHandle(0),
      configState(STATE_SEND_NOTHING), configNonce(0), fromNum(0),
      channelIndex(0),
      queueHead(0), queueTail(0), queueCount(0),
      fromNumValHandle(0), fromRadioValHandle(0) {
    memset(messageQueue, 0, sizeof(messageQueue));
    memset(messageQueueLen, 0, sizeof(messageQueueLen));
    memset(storedDeviceName, 0, sizeof(storedDeviceName));
    memset(storedShortName, 0, sizeof(storedShortName));
}

bool MeshtasticBLE::begin(const char* deviceName, LoRaTransport* lora, MeshProtocol* mesh) {
    loraTransport = lora;
    meshProtocol = mesh;
    g_ble = this;

    // Store device name for NodeInfo responses
    strncpy(storedDeviceName, deviceName, sizeof(storedDeviceName) - 1);
    storedDeviceName[sizeof(storedDeviceName) - 1] = '\0';
    // Extract short name: last 4 chars (the hex suffix after "SIMS-")
    const char* dash = strrchr(deviceName, '-');
    if (dash && strlen(dash + 1) >= 4) {
        strncpy(storedShortName, dash + 1, 4);
    } else {
        strncpy(storedShortName, "SIMS", 4);
    }
    storedShortName[4] = '\0';

    ESP_LOGI(TAG, "Initializing NimBLE...");

    // Initialize NimBLE
    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return false;
    }

    // Configure host callbacks
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    // Initialize GATT services (must come before setting device name,
    // because ble_svc_gap_init() resets the name to the default)
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Set device name AFTER gap_init (gap_init resets it to default)
    ble_svc_gap_device_name_set(deviceName);

    // Register our custom GATT service
    rc = ble_gatts_count_cfg(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return false;
    }

    rc = ble_gatts_add_svcs(gatt_services);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return false;
    }

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    initialized = true;
    ESP_LOGI(TAG, "BLE service started, advertising as: %s", deviceName);

    return true;
}

void MeshtasticBLE::update() {
    // Event-driven - nothing to do here
}

void MeshtasticBLE::notifyFromNum() {
    if (!initialized || connectedClients == 0) return;

    fromNum++;
    uint8_t val[4];
    val[0] = fromNum & 0xFF;
    val[1] = (fromNum >> 8) & 0xFF;
    val[2] = (fromNum >> 16) & 0xFF;
    val[3] = (fromNum >> 24) & 0xFF;

    // Send notification using the static attribute handle
    struct os_mbuf *om = ble_hs_mbuf_from_flat(val, 4);
    if (om) {
        int rc = ble_gatts_notify_custom(connHandle, fromNumAttrHandle, om);
        if (rc != 0) {
            ESP_LOGW(TAG, "FromNum notify failed: %d", rc);
        } else {
            ESP_LOGI(TAG, "FromNum notify: %u", (unsigned int)fromNum);
        }
    }
}

bool MeshtasticBLE::isConnected() {
    return connectedClients > 0;
}

int MeshtasticBLE::getConnectedCount() {
    return connectedClients;
}

size_t MeshtasticBLE::getFromRadio(uint8_t* buffer, size_t maxLen) {
    uint32_t deviceId = meshProtocol ? meshProtocol->getDeviceId() : 0xed020f3c;
    size_t len = 0;

    switch (configState) {
        case STATE_SEND_MY_INFO:
            len = buildFromRadio_MyNodeInfo(buffer, maxLen, deviceId);
            if (len > 0) {
                configState = STATE_SEND_OWN_NODEINFO;
                ESP_LOGI(TAG, "MY_INFO sent (%zu bytes), next: OWN_NODEINFO", len);
            }
            break;

        case STATE_SEND_OWN_NODEINFO:
            len = buildFromRadio_NodeInfo(buffer, maxLen, deviceId, storedDeviceName, storedShortName);
            if (len > 0) {
                channelIndex = 0;
                configState = STATE_SEND_CHANNELS;
                ESP_LOGI(TAG, "OWN_NODEINFO sent (%zu bytes), next: CHANNELS", len);
            }
            break;

        case STATE_SEND_CHANNELS: {
            // Channel 0: PRIMARY, default Meshtastic PSK (1 byte = 0x01)
            // Channel 1: SECONDARY, AES-256 "SIMS" encrypted
            static const uint8_t defaultPsk[] = { 0x01 };  // Meshtastic default key shorthand
            static const uint8_t simsPsk[32] = {  // AES-256 key for SIMS channel
                0x53, 0x49, 0x4D, 0x53, 0x2D, 0x4D, 0x45, 0x53,  // "SIMS-MES"
                0x48, 0x2D, 0x53, 0x45, 0x43, 0x55, 0x52, 0x45,  // "H-SECURE"
                0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6, 0x07, 0x18,
                0x29, 0x3A, 0x4B, 0x5C, 0x6D, 0x7E, 0x8F, 0x90
            };

            if (channelIndex == 0) {
                len = buildFromRadio_Channel(buffer, maxLen, 0,
                    meshtastic_Channel_Role_PRIMARY, "", defaultPsk, sizeof(defaultPsk));
            } else if (channelIndex == 1) {
                len = buildFromRadio_Channel(buffer, maxLen, 1,
                    meshtastic_Channel_Role_SECONDARY, "SIMS", simsPsk, sizeof(simsPsk));
            }

            if (len > 0) {
                channelIndex++;
                if (channelIndex >= 2) {
                    configState = STATE_SEND_COMPLETE_ID;
                    ESP_LOGI(TAG, "All channels sent, next: COMPLETE_ID");
                }
            }
            break;
        }

        case STATE_SEND_COMPLETE_ID:
            len = buildFromRadio_ConfigComplete(buffer, maxLen);
            if (len > 0) {
                configState = STATE_SEND_PACKETS;
                ESP_LOGI(TAG, "CONFIG_COMPLETE sent (%zu bytes), entering steady state", len);
            }
            break;

        case STATE_SEND_PACKETS:
            if (queueCount > 0) {
                len = messageQueueLen[queueHead];
                memcpy(buffer, messageQueue[queueHead], len);
                queueHead = (queueHead + 1) % MAX_QUEUE;
                queueCount--;
                ESP_LOGI(TAG, "Packet sent (%zu bytes), %d remaining", len, queueCount);
            }
            break;

        case STATE_SEND_NOTHING:
        default:
            break;
    }

    return len;
}

// --- Protobuf field extraction helpers ---
// Extract a length-delimited field from raw protobuf bytes.
// Returns pointer to field data and sets outLen. Returns NULL if not found.
static const uint8_t* extractLengthDelimited(const uint8_t* data, size_t len,
                                               int targetField, size_t* outLen) {
    size_t pos = 0;
    while (pos < len) {
        // Read tag varint
        uint32_t tag = 0;
        int shift = 0;
        while (pos < len) {
            uint8_t b = data[pos++];
            tag |= (uint32_t)(b & 0x7F) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
        }

        int fieldNumber = tag >> 3;
        int wireType = tag & 0x07;

        if (wireType == 0) {
            // Varint - skip
            while (pos < len && (data[pos++] & 0x80)) {}
        } else if (wireType == 2) {
            // Length-delimited
            uint32_t fieldLen = 0;
            shift = 0;
            while (pos < len) {
                uint8_t b = data[pos++];
                fieldLen |= (uint32_t)(b & 0x7F) << shift;
                if ((b & 0x80) == 0) break;
                shift += 7;
            }
            if (fieldNumber == targetField && pos + fieldLen <= len) {
                *outLen = fieldLen;
                return &data[pos];
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
    return NULL;
}

// Extract a varint field value. Returns -1 if not found.
static int32_t extractVarint(const uint8_t* data, size_t len, int targetField) {
    size_t pos = 0;
    while (pos < len) {
        uint32_t tag = 0;
        int shift = 0;
        while (pos < len) {
            uint8_t b = data[pos++];
            tag |= (uint32_t)(b & 0x7F) << shift;
            if ((b & 0x80) == 0) break;
            shift += 7;
        }

        int fieldNumber = tag >> 3;
        int wireType = tag & 0x07;

        if (wireType == 0) {
            uint32_t value = 0;
            shift = 0;
            while (pos < len) {
                uint8_t b = data[pos++];
                value |= (uint32_t)(b & 0x7F) << shift;
                if ((b & 0x80) == 0) break;
                shift += 7;
            }
            if (fieldNumber == targetField) return (int32_t)value;
        } else if (wireType == 2) {
            uint32_t fieldLen = 0;
            shift = 0;
            while (pos < len) {
                uint8_t b = data[pos++];
                fieldLen |= (uint32_t)(b & 0x7F) << shift;
                if ((b & 0x80) == 0) break;
                shift += 7;
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
    return -1;
}

void MeshtasticBLE::handleToRadio(const uint8_t* data, size_t len) {
    if (len < 1) return;

    ESP_LOGI(TAG, "ToRadio: %d bytes", len);

    meshtastic_ToRadio toRadio = meshtastic_ToRadio_init_zero;
    pb_istream_t stream = pb_istream_from_buffer(data, len);
    bool decoded = pb_decode(&stream, meshtastic_ToRadio_fields, &toRadio);

    if (!decoded) {
        ESP_LOGE(TAG, "ToRadio decode failed: %s", PB_GET_ERROR(&stream));
        return;
    }

    if (toRadio.which_payload_variant == meshtastic_ToRadio_want_config_id_tag) {
        configNonce = toRadio.payload_variant.want_config_id;
        ESP_LOGI(TAG, "WANT_CONFIG received, nonce=%u", (unsigned int)configNonce);

        configState = STATE_SEND_MY_INFO;

        queueHead = 0;
        queueTail = 0;
        queueCount = 0;

        notifyFromNum();

    } else if (toRadio.which_payload_variant == meshtastic_ToRadio_packet_tag) {
        // Extract raw MeshPacket bytes from ToRadio and forward directly over LoRa.
        // This preserves the full Meshtastic packet (from, to, portnum, payload, etc.)
        size_t meshPacketLen = 0;
        const uint8_t* meshPacket = extractLengthDelimited(data, len, 1, &meshPacketLen);
        if (!meshPacket || meshPacketLen == 0) {
            ESP_LOGW(TAG, "Could not extract MeshPacket from ToRadio");
            return;
        }

        ESP_LOGI(TAG, "MeshPacket from app: %d bytes, forwarding raw over LoRa", meshPacketLen);

        // Send raw MeshPacket bytes directly over LoRa
        if (loraTransport) {
            if (loraTransport->send((uint8_t*)meshPacket, meshPacketLen)) {
                ESP_LOGI(TAG, "Raw MeshPacket forwarded to LoRa (%d bytes)", meshPacketLen);
            } else {
                ESP_LOGE(TAG, "Failed to forward MeshPacket to LoRa");
            }
        }
    } else {
        ESP_LOGW(TAG, "Unknown ToRadio variant: %d", toRadio.which_payload_variant);
    }
}

void MeshtasticBLE::onConnect() {
    connectedClients++;
    ESP_LOGI(TAG, "Client connected (total: %d)", connectedClients);

    configState = STATE_SEND_NOTHING;
    configNonce = 0;
}

void MeshtasticBLE::onDisconnect() {
    connectedClients--;
    if (connectedClients < 0) connectedClients = 0;
    ESP_LOGI(TAG, "Client disconnected (total: %d)", connectedClients);

    configState = STATE_SEND_NOTHING;
}

void MeshtasticBLE::onFromNumSubscribe(bool subscribed) {
    if (subscribed) {
        ESP_LOGI(TAG, "Client subscribed to FromNum - waiting for want_config_id");
    } else {
        ESP_LOGI(TAG, "Client unsubscribed from FromNum");
    }
}

void MeshtasticBLE::queueReceivedPayload(const uint8_t* payload, size_t len, uint32_t fromNodeId) {
    // Legacy method - forward to queueRawMeshPacket if it's a raw MeshPacket
    queueRawMeshPacket(payload, len);
}

void MeshtasticBLE::queueRawMeshPacket(const uint8_t* meshPacketData, size_t meshPacketLen) {
    if (configState != STATE_SEND_PACKETS) {
        ESP_LOGW(TAG, "Not in steady state, dropping received packet");
        return;
    }
    if (queueCount >= MAX_QUEUE) {
        ESP_LOGW(TAG, "BLE queue full, dropping received packet");
        return;
    }
    if (meshPacketLen > 240) {
        ESP_LOGW(TAG, "MeshPacket too large (%d bytes), dropping", meshPacketLen);
        return;
    }

    // Wrap raw MeshPacket bytes in FromRadio { packet = <raw bytes> }
    // FromRadio.packet = field 2, length-delimited (wire type 2)
    uint8_t* buf = messageQueue[queueTail];
    size_t pos = 0;

    buf[pos++] = (2 << 3) | 2;  // field 2, wire type 2
    // Encode length as varint
    size_t pLen = meshPacketLen;
    while (pLen > 0x7F) { buf[pos++] = (pLen & 0x7F) | 0x80; pLen >>= 7; }
    buf[pos++] = pLen & 0x7F;
    memcpy(&buf[pos], meshPacketData, meshPacketLen);
    pos += meshPacketLen;

    messageQueueLen[queueTail] = pos;
    queueTail = (queueTail + 1) % MAX_QUEUE;
    queueCount++;

    ESP_LOGI(TAG, "Queued raw MeshPacket for BLE: %d bytes (FromRadio: %d)", (int)meshPacketLen, (int)pos);

    notifyFromNum();
}
