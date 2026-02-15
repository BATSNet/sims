/**
 * Mesh BLE Client - NimBLE Central for connecting to Meshtastic mesh nodes
 *
 * Scans for Meshtastic BLE peripherals (SIMS-XXXX mesh devices),
 * connects, performs config exchange, and sends PRIVATE_APP data packets.
 *
 * Protocol flow (same as Flutter app):
 * 1. Active scan for Meshtastic service UUID
 * 2. Connect to strongest RSSI device, request MTU 512
 * 3. Discover service + 3 characteristics (ToRadio/FromRadio/FromNum)
 * 4. Subscribe to FromNum notifications (CCCD 0x0001)
 * 5. Write want_config_id to ToRadio
 * 6. Poll FromRadio (100ms interval) until config_complete_id
 * 7. Ready - can send PRIVATE_APP packets
 *
 * Uses global pointer pattern for C callbacks (same as server side).
 */

#ifndef MESH_BLE_CLIENT_H
#define MESH_BLE_CLIENT_H

#include <stdint.h>
#include <stddef.h>

class MeshBleClient {
public:
    enum State {
        STATE_IDLE,         // Not started or disconnected
        STATE_SCANNING,     // Active scan for mesh devices
        STATE_CONNECTING,   // GAP connection in progress
        STATE_CONFIGURING,  // Config exchange (want_config -> poll FromRadio)
        STATE_READY,        // Connected and configured - can send packets
        STATE_ERROR         // Error state (will auto-recover)
    };

    MeshBleClient();

    // Initialize NimBLE stack (Central role, no GATT server)
    bool begin();

    // Start scanning and connect to the strongest mesh device
    bool scanAndConnect();

    // Send binary payload as PRIVATE_APP mesh packet (broadcast)
    // Returns true if write was successful
    bool sendPayload(const uint8_t* data, size_t len);

    // Send a text message as TEXT_MESSAGE_APP (portnum 1)
    // These messages are visible in the Meshtastic phone app
    bool sendTextMessage(const char* text);

    // Call from main loop - handles reconnect backoff
    void update();

    // State queries
    State getState() const { return state_; }
    bool isReady() const { return state_ == STATE_READY; }
    bool isConnected() const { return state_ == STATE_READY || state_ == STATE_CONFIGURING; }
    const char* getConnectedDeviceName() const { return connectedDeviceName_; }
    const char* getStateString() const;

    // -- Callbacks called from NimBLE C event handlers --
    void onScanResult(const uint8_t* addr, uint8_t addrType, int8_t rssi,
                      const uint8_t* advData, uint8_t advLen,
                      const char* name);
    void onScanComplete(int reason);
    void onConnected(uint16_t connHandle);
    void onDisconnected(uint16_t connHandle, int reason);
    void onMtuChanged(uint16_t connHandle, uint16_t mtu);
    void onDiscoveryComplete(uint16_t connHandle, int status);
    void onFromNumNotify(uint16_t connHandle, const uint8_t* data, size_t len);
    void onCharacteristicDiscovered(uint16_t connHandle, uint16_t valHandle,
                                     const uint8_t* uuid128);

    // Called from C callbacks - must be public
    void startConfigExchange();
    void handleFromRadioData(const uint8_t* data, size_t len);

private:
    State state_;
    bool initialized_;

    // Connection state
    uint16_t connHandle_;
    uint16_t currentMtu_;

    // Discovered characteristic handles
    uint16_t toRadioHandle_;
    uint16_t fromRadioHandle_;
    uint16_t fromNumHandle_;
    uint16_t fromNumCccdHandle_;  // CCCD descriptor for FromNum notifications
    int charsDiscovered_;         // Count of discovered chars (need 3)

    // Scan result tracking
    uint8_t bestAddr_[6];
    uint8_t bestAddrType_;
    int8_t bestRssi_;
    bool foundDevice_;

    // Config exchange
    uint32_t configNonce_;
    uint32_t configStartTime_;
    uint32_t lastPollTime_;
    bool configComplete_;

    // Reconnect backoff
    uint32_t reconnectDelay_;
    uint32_t lastReconnectTime_;
    bool autoReconnect_;
    int reconnectAttempts_;

    // Packet ID counter
    uint32_t packetIdCounter_;

    // Connected device info
    char connectedDeviceName_[32];

    // Internal methods
    void pollFromRadio();
    void scheduleReconnect();
    void resetConnection();
    static uint32_t millis();
};

#endif // MESH_BLE_CLIENT_H
