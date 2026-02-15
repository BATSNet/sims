/**
 * Mesh BLE Client Implementation
 *
 * NimBLE Central (GAP client) that connects to Meshtastic mesh devices.
 * Replicates the protocol from Flutter's MeshtasticBleService using
 * the same NimBLE API style as the mesh device server side.
 */

#include "mesh/mesh_ble_client.h"
#include "mesh/mesh_packet_encoder.h"
#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_random.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"

static const char* TAG = "MeshBLE";

// Global pointer for C callbacks
static MeshBleClient* g_client = nullptr;

// Meshtastic service UUID (same bytes as server side, reversed for 128-bit)
static const ble_uuid128_t meshServiceUuid =
    BLE_UUID128_INIT(0xfd, 0xea, 0x73, 0xe2, 0xca, 0x5d, 0xa8, 0x9f,
                     0x1f, 0x46, 0xa8, 0x15, 0x18, 0xb2, 0xa1, 0x6b);

// Characteristic UUIDs
static const ble_uuid128_t toRadioUuid =
    BLE_UUID128_INIT(0xe7, 0x01, 0x44, 0x12, 0x66, 0x78, 0xdd, 0xa1,
                     0xad, 0x4d, 0x9e, 0x12, 0xd2, 0x76, 0x5c, 0xf7);

static const ble_uuid128_t fromRadioUuid =
    BLE_UUID128_INIT(0x02, 0x00, 0x12, 0xac, 0x42, 0x02, 0x78, 0xb8,
                     0xed, 0x11, 0x93, 0x49, 0x9e, 0xe6, 0x55, 0x2c);

static const ble_uuid128_t fromNumUuid =
    BLE_UUID128_INIT(0x53, 0x44, 0xe3, 0x47, 0x75, 0xaa, 0x70, 0xa6,
                     0x66, 0x4f, 0x00, 0xa8, 0x8c, 0xa1, 0x9d, 0xed);

// ---------------------------------------------------------------------------
// Forward declarations for NimBLE C callbacks
// ---------------------------------------------------------------------------

static int gap_event_cb(struct ble_gap_event* event, void* arg);
static void on_sync(void);
static void on_reset(int reason);
static void ble_host_task(void* param);
static int chr_disc_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                        const struct ble_gatt_chr* chr, void* arg);
static int svc_disc_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                        const struct ble_gatt_svc* service, void* arg);
static int dsc_disc_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                        uint16_t chr_val_handle, const struct ble_gatt_dsc* dsc,
                        void* arg);
static int on_read_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                       struct ble_gatt_attr* attr, void* arg);
static int on_write_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                        struct ble_gatt_attr* attr, void* arg);
static int on_subscribe_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                             struct ble_gatt_attr* attr, void* arg);

// ---------------------------------------------------------------------------
// Helper: UUID comparison for 128-bit UUIDs
// ---------------------------------------------------------------------------

static bool uuid128_eq(const ble_uuid128_t* a, const uint8_t* b) {
    return memcmp(a->value, b, 16) == 0;
}

// ---------------------------------------------------------------------------
// GAP event callback - track Meshtastic devices across ad + scan response
// ---------------------------------------------------------------------------

// Addresses seen with Meshtastic service UUID (from primary advertisement).
// The device name typically arrives in a separate scan response event.
#define MAX_SEEN_ADDRS 8
static struct {
    uint8_t addr[6];
    uint8_t addrType;
    int8_t  rssi;
    bool    used;
} s_meshAddrs[MAX_SEEN_ADDRS];

static void markMeshtasticAddr(const uint8_t* addr, uint8_t addrType, int8_t rssi) {
    // Check if already tracked
    for (int i = 0; i < MAX_SEEN_ADDRS; i++) {
        if (s_meshAddrs[i].used && memcmp(s_meshAddrs[i].addr, addr, 6) == 0) {
            s_meshAddrs[i].rssi = rssi;
            return;
        }
    }
    // Add to first empty slot
    for (int i = 0; i < MAX_SEEN_ADDRS; i++) {
        if (!s_meshAddrs[i].used) {
            memcpy(s_meshAddrs[i].addr, addr, 6);
            s_meshAddrs[i].addrType = addrType;
            s_meshAddrs[i].rssi = rssi;
            s_meshAddrs[i].used = true;
            return;
        }
    }
}

static bool isMeshtasticAddr(const uint8_t* addr) {
    for (int i = 0; i < MAX_SEEN_ADDRS; i++) {
        if (s_meshAddrs[i].used && memcmp(s_meshAddrs[i].addr, addr, 6) == 0) {
            return true;
        }
    }
    return false;
}

static void clearMeshtasticAddrs() {
    for (int i = 0; i < MAX_SEEN_ADDRS; i++) s_meshAddrs[i].used = false;
}

static int gap_event_cb(struct ble_gap_event* event, void* arg) {
    if (!g_client) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            const struct ble_gap_disc_desc* desc = &event->disc;

            struct ble_hs_adv_fields fields;
            int rc = ble_hs_adv_parse_fields(&fields, desc->data, desc->length_data);
            if (rc != 0) break;

            // Check for Meshtastic service UUID (typically in primary advertisement)
            bool hasService = false;
            for (int i = 0; i < fields.num_uuids128; i++) {
                if (ble_uuid_cmp(&fields.uuids128[i].u, &meshServiceUuid.u) == 0) {
                    hasService = true;
                    break;
                }
            }

            // Extract name (typically in scan response)
            const char* name = "";
            char nameBuf[32] = {0};
            if (fields.name != NULL && fields.name_len > 0) {
                int copyLen = fields.name_len;
                if (copyLen >= (int)sizeof(nameBuf)) copyLen = sizeof(nameBuf) - 1;
                memcpy(nameBuf, fields.name, copyLen);
                nameBuf[copyLen] = '\0';
                name = nameBuf;
            }

            if (hasService) {
                // Primary ad with service UUID - remember this address
                markMeshtasticAddr(desc->addr.val, desc->addr.type, desc->rssi);
                // If name is also present, forward immediately
                if (strlen(name) > 0) {
                    g_client->onScanResult(desc->addr.val, desc->addr.type,
                                            desc->rssi, desc->data, desc->length_data,
                                            name);
                }
            } else if (strlen(name) > 0 && isMeshtasticAddr(desc->addr.val)) {
                // Scan response for a previously-seen Meshtastic device - now has name
                g_client->onScanResult(desc->addr.val, desc->addr.type,
                                        desc->rssi, desc->data, desc->length_data,
                                        name);
            }
            break;
        }

        case BLE_GAP_EVENT_DISC_COMPLETE:
            g_client->onScanComplete(event->disc_complete.reason);
            break;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                g_client->onConnected(event->connect.conn_handle);
            } else {
                ESP_LOGW(TAG, "Connection failed: %d", event->connect.status);
                g_client->onDisconnected(0, event->connect.status);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            g_client->onDisconnected(event->disconnect.conn.conn_handle,
                                      event->disconnect.reason);
            break;

        case BLE_GAP_EVENT_MTU:
            g_client->onMtuChanged(event->mtu.conn_handle, event->mtu.value);
            break;

        case BLE_GAP_EVENT_NOTIFY_RX: {
            // FromNum notification received
            uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
            if (len > 0 && len <= 4) {
                uint8_t buf[4];
                uint16_t copied = 0;
                ble_hs_mbuf_to_flat(event->notify_rx.om, buf, len, &copied);
                g_client->onFromNumNotify(event->notify_rx.conn_handle, buf, copied);
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Service discovery callback
// ---------------------------------------------------------------------------

static int svc_disc_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                        const struct ble_gatt_svc* service, void* arg) {
    if (error->status == 0 && service != NULL) {
        ESP_LOGI(TAG, "Service found, discovering characteristics...");
        // Discover characteristics within this service
        int rc = ble_gattc_disc_all_chrs(conn_handle,
                                          service->start_handle,
                                          service->end_handle,
                                          chr_disc_cb, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Characteristic discovery failed: %d", rc);
        }
    } else if (error->status == BLE_HS_EDONE) {
        // Discovery complete
        ESP_LOGI(TAG, "Service discovery done");
    } else {
        ESP_LOGE(TAG, "Service discovery error: %d", error->status);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Characteristic discovery callback
// ---------------------------------------------------------------------------

static int chr_disc_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                        const struct ble_gatt_chr* chr, void* arg) {
    if (!g_client) return 0;

    if (error->status == 0 && chr != NULL) {
        // Check if this is one of our target characteristics
        if (chr->uuid.u.type == BLE_UUID_TYPE_128) {
            const ble_uuid128_t* uuid = (const ble_uuid128_t*)&chr->uuid;
            g_client->onCharacteristicDiscovered(conn_handle, chr->val_handle,
                                                   uuid->value);
        }
    } else if (error->status == BLE_HS_EDONE) {
        g_client->onDiscoveryComplete(conn_handle, 0);
    } else {
        ESP_LOGE(TAG, "Characteristic discovery error: %d", error->status);
        g_client->onDiscoveryComplete(conn_handle, error->status);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Descriptor discovery callback (for finding FromNum CCCD)
// ---------------------------------------------------------------------------

// CCCD UUID (0x2902) - stored as variable to avoid C++ rvalue address issue
static const ble_uuid16_t cccd_uuid = BLE_UUID16_INIT(0x2902);

static int dsc_disc_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                        uint16_t chr_val_handle, const struct ble_gatt_dsc* dsc,
                        void* arg) {
    if (!g_client) return 0;

    if (error->status == 0 && dsc != NULL) {
        // Check for CCCD (UUID 0x2902)
        if (ble_uuid_cmp(&dsc->uuid.u, &cccd_uuid.u) == 0) {
            ESP_LOGI(TAG, "Found FromNum CCCD at handle %d", dsc->handle);
            // Subscribe to notifications by writing 0x0001 to CCCD
            uint8_t val[2] = {0x01, 0x00};
            struct os_mbuf* om = ble_hs_mbuf_from_flat(val, 2);
            if (om) {
                int rc = ble_gattc_write(conn_handle, dsc->handle,
                                          om, on_subscribe_cb, NULL);
                if (rc != 0) {
                    ESP_LOGE(TAG, "CCCD write failed: %d", rc);
                }
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Read / Write callbacks
// ---------------------------------------------------------------------------

static int on_read_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                       struct ble_gatt_attr* attr, void* arg) {
    if (!g_client) return 0;

    if (error->status == 0 && attr != NULL && attr->om != NULL) {
        uint16_t len = OS_MBUF_PKTLEN(attr->om);
        if (len > 0) {
            uint8_t buf[512];
            uint16_t copied = 0;
            ble_hs_mbuf_to_flat(attr->om, buf, sizeof(buf), &copied);
            g_client->handleFromRadioData(buf, copied);
        } else {
            // Empty read - no more data
            g_client->handleFromRadioData(nullptr, 0);
        }
    } else if (error->status == BLE_HS_EDONE || error->status != 0) {
        // Read failed or no data
        g_client->handleFromRadioData(nullptr, 0);
    }
    return 0;
}

static int on_write_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                        struct ble_gatt_attr* attr, void* arg) {
    if (error->status != 0) {
        ESP_LOGE(TAG, "Write failed: %d", error->status);
    }
    return 0;
}

static int on_subscribe_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                             struct ble_gatt_attr* attr, void* arg) {
    if (!g_client) return 0;

    if (error->status == 0) {
        ESP_LOGI(TAG, "Subscribed to FromNum notifications");
        // Now start config exchange
        g_client->startConfigExchange();
    } else {
        ESP_LOGE(TAG, "FromNum subscribe failed: %d", error->status);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// NimBLE host callbacks
// ---------------------------------------------------------------------------

static void on_sync(void) {
    uint8_t addr_type;
    int rc = ble_hs_id_infer_auto(0, &addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "BLE host synced, addr type: %d", addr_type);

    // Auto-start scan if requested
    if (g_client) {
        g_client->scanAndConnect();
    }
}

static void on_reset(int reason) {
    ESP_LOGW(TAG, "BLE host reset, reason: %d", reason);
}

static void ble_host_task(void* param) {
    ESP_LOGI(TAG, "NimBLE host task started (client mode)");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ---------------------------------------------------------------------------
// MeshBleClient implementation
// ---------------------------------------------------------------------------

MeshBleClient::MeshBleClient()
    : state_(STATE_IDLE), initialized_(false),
      connHandle_(0), currentMtu_(23),
      toRadioHandle_(0), fromRadioHandle_(0), fromNumHandle_(0),
      fromNumCccdHandle_(0), charsDiscovered_(0),
      bestAddrType_(0), bestRssi_(-127), foundDevice_(false),
      configNonce_(0), configStartTime_(0), lastPollTime_(0),
      configComplete_(false),
      reconnectDelay_(BLE_RECONNECT_INITIAL_MS),
      lastReconnectTime_(0), autoReconnect_(true), reconnectAttempts_(0),
      packetIdCounter_(0) {
    memset(bestAddr_, 0, sizeof(bestAddr_));
    memset(connectedDeviceName_, 0, sizeof(connectedDeviceName_));
}

uint32_t MeshBleClient::millis() {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

const char* MeshBleClient::getStateString() const {
    switch (state_) {
        case STATE_IDLE:        return "IDLE";
        case STATE_SCANNING:    return "SCANNING";
        case STATE_CONNECTING:  return "CONNECTING";
        case STATE_CONFIGURING: return "CONFIGURING";
        case STATE_READY:       return "READY";
        case STATE_ERROR:       return "ERROR";
        default:                return "UNKNOWN";
    }
}

bool MeshBleClient::begin() {
    if (initialized_) return true;

    g_client = this;

    ESP_LOGI(TAG, "Initializing NimBLE (Central mode)...");

    int rc = nimble_port_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", rc);
        return false;
    }

    // Configure host callbacks
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    // Set initialized before starting host task so on_sync callback can scan
    initialized_ = true;

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "NimBLE initialized (Central mode)");
    return true;
}

bool MeshBleClient::scanAndConnect() {
    if (!initialized_) return false;
    if (state_ == STATE_SCANNING || state_ == STATE_CONNECTING) return false;

    ESP_LOGI(TAG, "Starting BLE scan for mesh devices...");
    state_ = STATE_SCANNING;
    foundDevice_ = false;
    bestRssi_ = -127;
    memset(bestAddr_, 0, sizeof(bestAddr_));
    clearMeshtasticAddrs();

    // Configure active scan (to get scan responses with device names)
    struct ble_gap_disc_params scanParams = {};
    scanParams.passive = 0;  // Active scan
    scanParams.itvl = 0x0010;  // 10ms
    scanParams.window = 0x0010;
    scanParams.filter_duplicates = 0;  // Don't filter - we want best RSSI
    scanParams.limited = 0;

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_SCAN_DURATION_MS,
                           &scanParams, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Scan start failed: %d", rc);
        state_ = STATE_IDLE;
        return false;
    }

    return true;
}

void MeshBleClient::update() {
    if (!initialized_) return;

    // Handle config exchange polling
    if (state_ == STATE_CONFIGURING && !configComplete_) {
        uint32_t now = millis();

        // Check for config timeout
        if (now - configStartTime_ > BLE_CONFIG_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Config exchange timed out - proceeding anyway");
            configComplete_ = true;
            state_ = STATE_READY;
            ESP_LOGI(TAG, "Mesh BLE READY (config timeout)");
            reconnectAttempts_ = 0;
            reconnectDelay_ = BLE_RECONNECT_INITIAL_MS;
            return;
        }

        // Poll FromRadio at configured interval
        if (now - lastPollTime_ >= BLE_CONFIG_POLL_MS) {
            lastPollTime_ = now;
            pollFromRadio();
        }
    }

    // Handle auto-reconnect
    if (state_ == STATE_IDLE && autoReconnect_ && reconnectAttempts_ > 0) {
        uint32_t now = millis();
        if (now - lastReconnectTime_ >= reconnectDelay_) {
            ESP_LOGI(TAG, "Auto-reconnect attempt %d (delay %lu ms)",
                     reconnectAttempts_, (unsigned long)reconnectDelay_);
            scanAndConnect();
        }
    }
}

bool MeshBleClient::sendPayload(const uint8_t* data, size_t len) {
    if (state_ != STATE_READY || toRadioHandle_ == 0) {
        ESP_LOGW(TAG, "Cannot send: mesh not ready (state: %s)", getStateString());
        return false;
    }

    // Build ToRadio protobuf wrapping the payload
    uint8_t toRadioBuf[600];
    packetIdCounter_++;
    size_t toRadioLen = meshBuildDataPacket(toRadioBuf, sizeof(toRadioBuf),
                                            data, len, packetIdCounter_);
    if (toRadioLen == 0) {
        ESP_LOGE(TAG, "Failed to build mesh data packet");
        return false;
    }

    ESP_LOGI(TAG, "Sending mesh packet: %d bytes (payload: %d, id: %u)",
             (int)toRadioLen, (int)len, (unsigned)packetIdCounter_);

    // Write to ToRadio characteristic
    struct os_mbuf* om = ble_hs_mbuf_from_flat(toRadioBuf, toRadioLen);
    if (!om) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for send");
        return false;
    }

    int rc = ble_gattc_write(connHandle_, toRadioHandle_, om, on_write_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT write failed: %d", rc);
        return false;
    }

    ESP_LOGI(TAG, "Mesh packet sent successfully");
    return true;
}

bool MeshBleClient::sendTextMessage(const char* text) {
    if (state_ != STATE_READY || toRadioHandle_ == 0) {
        ESP_LOGW(TAG, "Cannot send text: mesh not ready (state: %s)", getStateString());
        return false;
    }

    if (!text || strlen(text) == 0) {
        ESP_LOGW(TAG, "Empty text message, not sending");
        return false;
    }

    size_t textLen = strlen(text);

    // Build ToRadio protobuf with TEXT_MESSAGE_APP portnum
    uint8_t toRadioBuf[600];
    packetIdCounter_++;
    size_t toRadioLen = meshBuildDataPacket(toRadioBuf, sizeof(toRadioBuf),
                                            (const uint8_t*)text, textLen,
                                            packetIdCounter_,
                                            MESH_PORT_TEXT_MESSAGE);
    if (toRadioLen == 0) {
        ESP_LOGE(TAG, "Failed to build text message packet");
        return false;
    }

    ESP_LOGI(TAG, "Sending text message: \"%s\" (%d bytes, id: %u)",
             text, (int)textLen, (unsigned)packetIdCounter_);

    // Write to ToRadio characteristic
    struct os_mbuf* om = ble_hs_mbuf_from_flat(toRadioBuf, toRadioLen);
    if (!om) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for text message");
        return false;
    }

    int rc = ble_gattc_write(connHandle_, toRadioHandle_, om, on_write_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Text message GATT write failed: %d", rc);
        return false;
    }

    ESP_LOGI(TAG, "Text message sent via mesh");
    return true;
}

// ---------------------------------------------------------------------------
// Callbacks from NimBLE events
// ---------------------------------------------------------------------------

void MeshBleClient::onScanResult(const uint8_t* addr, uint8_t addrType,
                                  int8_t rssi, const uint8_t* advData,
                                  uint8_t advLen, const char* name) {
    // Only connect to our own SIMS radios (ignore other Meshtastic devices)
    if (!name || strncmp(name, "SIMS-", 5) != 0) {
        if (name && strlen(name) > 0) {
            ESP_LOGD(TAG, "Ignoring non-SIMS device: %s (RSSI: %d)", name, rssi);
        }
        return;
    }

    if (rssi > bestRssi_) {
        bestRssi_ = rssi;
        memcpy(bestAddr_, addr, 6);
        bestAddrType_ = addrType;
        foundDevice_ = true;

        strncpy(connectedDeviceName_, name, sizeof(connectedDeviceName_) - 1);

        ESP_LOGI(TAG, "Found SIMS mesh device: %s (RSSI: %d)", name, rssi);
    }
}

void MeshBleClient::onScanComplete(int reason) {
    ESP_LOGI(TAG, "Scan complete (reason: %d)", reason);

    if (!foundDevice_) {
        ESP_LOGW(TAG, "No mesh devices found");
        state_ = STATE_IDLE;
        scheduleReconnect();
        return;
    }

    // Connect to the best device
    state_ = STATE_CONNECTING;
    ESP_LOGI(TAG, "Connecting to mesh device (RSSI: %d)...", bestRssi_);

    // Stop any ongoing scan first
    ble_gap_disc_cancel();

    ble_addr_t peerAddr;
    peerAddr.type = bestAddrType_;
    memcpy(peerAddr.val, bestAddr_, 6);

    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peerAddr,
                              BLE_CONNECT_TIMEOUT_MS,
                              NULL,  // default connection params
                              gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Connect failed: %d", rc);
        state_ = STATE_IDLE;
        scheduleReconnect();
    }
}

void MeshBleClient::onConnected(uint16_t connHandle) {
    connHandle_ = connHandle;
    ESP_LOGI(TAG, "Connected to mesh device (handle: %d)", connHandle);

    // Request larger MTU
    int rc = ble_att_set_preferred_mtu(BLE_MTU_DESIRED);
    if (rc != 0) {
        ESP_LOGW(TAG, "Set preferred MTU failed: %d", rc);
    }
    rc = ble_gattc_exchange_mtu(connHandle, NULL, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "MTU exchange failed: %d", rc);
    }

    // Discover Meshtastic service
    charsDiscovered_ = 0;
    toRadioHandle_ = 0;
    fromRadioHandle_ = 0;
    fromNumHandle_ = 0;
    fromNumCccdHandle_ = 0;

    rc = ble_gattc_disc_svc_by_uuid(connHandle, &meshServiceUuid.u,
                                     svc_disc_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Service discovery start failed: %d", rc);
        ble_gap_terminate(connHandle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

void MeshBleClient::onDisconnected(uint16_t connHandle, int reason) {
    ESP_LOGW(TAG, "Disconnected from mesh device (reason: %d)", reason);
    resetConnection();
    scheduleReconnect();
}

void MeshBleClient::onMtuChanged(uint16_t connHandle, uint16_t mtu) {
    currentMtu_ = mtu;
    ESP_LOGI(TAG, "MTU updated: %d", mtu);
}

void MeshBleClient::onCharacteristicDiscovered(uint16_t connHandle,
                                                 uint16_t valHandle,
                                                 const uint8_t* uuid128) {
    if (uuid128_eq(&toRadioUuid, uuid128)) {
        toRadioHandle_ = valHandle;
        charsDiscovered_++;
        ESP_LOGI(TAG, "ToRadio handle: %d", valHandle);
    } else if (uuid128_eq(&fromRadioUuid, uuid128)) {
        fromRadioHandle_ = valHandle;
        charsDiscovered_++;
        ESP_LOGI(TAG, "FromRadio handle: %d", valHandle);
    } else if (uuid128_eq(&fromNumUuid, uuid128)) {
        fromNumHandle_ = valHandle;
        charsDiscovered_++;
        ESP_LOGI(TAG, "FromNum handle: %d", valHandle);
    }
}

void MeshBleClient::onDiscoveryComplete(uint16_t connHandle, int status) {
    if (status != 0) {
        ESP_LOGE(TAG, "Discovery failed: %d", status);
        ble_gap_terminate(connHandle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    if (charsDiscovered_ < 3) {
        ESP_LOGE(TAG, "Missing characteristics (found %d/3)", charsDiscovered_);
        ble_gap_terminate(connHandle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    ESP_LOGI(TAG, "All 3 characteristics discovered, subscribing to FromNum...");

    // Discover descriptors for FromNum to find CCCD
    // FromNum handle + 1 should be the CCCD, but let's discover properly
    int rc = ble_gattc_disc_all_dscs(connHandle, fromNumHandle_,
                                      fromNumHandle_ + 2,
                                      dsc_disc_cb, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "Descriptor discovery failed: %d, trying direct CCCD write", rc);
        // Fallback: try handle + 1 as CCCD
        uint8_t val[2] = {0x01, 0x00};
        struct os_mbuf* om = ble_hs_mbuf_from_flat(val, 2);
        if (om) {
            rc = ble_gattc_write(connHandle, fromNumHandle_ + 1,
                                  om, on_subscribe_cb, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Fallback CCCD write failed: %d", rc);
            }
        }
    }
}

void MeshBleClient::onFromNumNotify(uint16_t connHandle,
                                      const uint8_t* data, size_t len) {
    if (len >= 4) {
        uint32_t fromNum = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        ESP_LOGI(TAG, "FromNum notification: %u", (unsigned)fromNum);
    }

    // If in ready state, poll for incoming packets
    if (state_ == STATE_READY) {
        pollFromRadio();
    }
}

// ---------------------------------------------------------------------------
// Config exchange
// ---------------------------------------------------------------------------

void MeshBleClient::startConfigExchange() {
    state_ = STATE_CONFIGURING;
    configNonce_ = (esp_random() & 0x7FFFFFFF) + 1;
    configStartTime_ = millis();
    lastPollTime_ = 0;
    configComplete_ = false;

    ESP_LOGI(TAG, "Starting config exchange (nonce: %u)", (unsigned)configNonce_);

    // Build and write want_config_id to ToRadio
    uint8_t wantConfigBuf[16];
    size_t wantConfigLen = meshBuildWantConfig(wantConfigBuf, sizeof(wantConfigBuf),
                                               configNonce_);
    if (wantConfigLen == 0) {
        ESP_LOGE(TAG, "Failed to build want_config");
        return;
    }

    struct os_mbuf* om = ble_hs_mbuf_from_flat(wantConfigBuf, wantConfigLen);
    if (!om) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for want_config");
        return;
    }

    int rc = ble_gattc_write(connHandle_, toRadioHandle_, om, on_write_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "want_config write failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "want_config_id written (%d bytes)", (int)wantConfigLen);
    }
}

void MeshBleClient::pollFromRadio() {
    if (fromRadioHandle_ == 0) return;

    int rc = ble_gattc_read(connHandle_, fromRadioHandle_, on_read_cb, NULL);
    if (rc != 0) {
        ESP_LOGW(TAG, "FromRadio read failed: %d", rc);
    }
}

void MeshBleClient::handleFromRadioData(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        // Empty read - no more data
        return;
    }

    ESP_LOGI(TAG, "FromRadio: %d bytes", (int)len);

    // During config exchange, check for config_complete_id
    if (state_ == STATE_CONFIGURING && !configComplete_) {
        if (meshIsConfigComplete(data, len, configNonce_)) {
            configComplete_ = true;
            state_ = STATE_READY;
            reconnectAttempts_ = 0;
            reconnectDelay_ = BLE_RECONNECT_INITIAL_MS;
            ESP_LOGI(TAG, "Config exchange complete - mesh BLE READY");
            return;
        }
        // Otherwise it's MyNodeInfo or NodeInfo - continue polling
        return;
    }

    // In ready state, check for PRIVATE_APP payloads
    if (state_ == STATE_READY) {
        size_t payloadLen = 0;
        const uint8_t* payload = meshExtractPayload(data, len, &payloadLen);
        if (payload && payloadLen > 0) {
            ESP_LOGI(TAG, "Received PRIVATE_APP payload: %d bytes", (int)payloadLen);
            // Could forward to main task if needed in the future
        }
    }
}

// ---------------------------------------------------------------------------
// Reconnect logic
// ---------------------------------------------------------------------------

void MeshBleClient::scheduleReconnect() {
    if (!autoReconnect_) return;

    reconnectAttempts_++;
    lastReconnectTime_ = millis();

    // Exponential backoff: 5s, 10s, 20s, 40s, 60s (capped)
    reconnectDelay_ = BLE_RECONNECT_INITIAL_MS;
    for (int i = 1; i < reconnectAttempts_ && reconnectDelay_ < BLE_RECONNECT_MAX_MS; i++) {
        reconnectDelay_ *= 2;
    }
    if (reconnectDelay_ > BLE_RECONNECT_MAX_MS) {
        reconnectDelay_ = BLE_RECONNECT_MAX_MS;
    }

    ESP_LOGI(TAG, "Reconnect scheduled in %lu ms (attempt %d)",
             (unsigned long)reconnectDelay_, reconnectAttempts_);
}

void MeshBleClient::resetConnection() {
    state_ = STATE_IDLE;
    connHandle_ = 0;
    currentMtu_ = 23;
    toRadioHandle_ = 0;
    fromRadioHandle_ = 0;
    fromNumHandle_ = 0;
    fromNumCccdHandle_ = 0;
    charsDiscovered_ = 0;
    configComplete_ = false;
    connectedDeviceName_[0] = '\0';
}
