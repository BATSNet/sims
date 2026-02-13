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

    // Set advertising data
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t*)ble_svc_gap_device_name();
    fields.name_len = strlen(ble_svc_gap_device_name());
    fields.name_is_complete = 1;
    // Advertise the Meshtastic service UUID
    fields.uuids128 = &meshtasticServiceUuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting adv fields: %d", rc);
        // Try without UUID (might not fit in 31 bytes)
        fields.uuids128 = NULL;
        fields.num_uuids128 = 0;
        fields.uuids128_is_complete = 0;
        rc = ble_gap_adv_set_fields(&fields);
        if (rc != 0) {
            ESP_LOGE(TAG, "Error setting minimal adv fields: %d", rc);
            return;
        }

        // Put UUID in scan response
        struct ble_hs_adv_fields rsp_fields = {};
        rsp_fields.uuids128 = &meshtasticServiceUuid;
        rsp_fields.num_uuids128 = 1;
        rsp_fields.uuids128_is_complete = 1;
        ble_gap_adv_rsp_set_fields(&rsp_fields);
    }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising started");
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
      queueHead(0), queueTail(0), queueCount(0),
      fromNumValHandle(0), fromRadioValHandle(0) {
    memset(messageQueue, 0, sizeof(messageQueue));
    memset(messageQueueLen, 0, sizeof(messageQueueLen));
}

bool MeshtasticBLE::begin(const char* deviceName, LoRaTransport* lora, MeshProtocol* mesh) {
    loraTransport = lora;
    meshProtocol = mesh;
    g_ble = this;

    ESP_LOGI(TAG, "Initializing NimBLE...");

    // Initialize NimBLE
    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return false;
    }

    // Set device name
    ble_svc_gap_device_name_set(deviceName);

    // Configure host callbacks
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    // Initialize GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

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
            len = buildFromRadio_NodeInfo(buffer, maxLen, deviceId, "SIMS-MESH", "SIMS");
            if (len > 0) {
                configState = STATE_SEND_COMPLETE_ID;
                ESP_LOGI(TAG, "OWN_NODEINFO sent (%zu bytes), next: COMPLETE_ID", len);
            }
            break;

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
        ESP_LOGI(TAG, "Mesh packet from app");
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
