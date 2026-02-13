/**
 * LoRa Transport Layer
 * Handles low-level LoRa SX1262 radio communication
 * ESP-IDF version using RadioLib with custom HAL
 */

#ifndef LORA_TRANSPORT_H
#define LORA_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <RadioLib.h>
#include "esp_attr.h"
#include "config.h"

class LoRaTransport {
public:
    LoRaTransport();
    ~LoRaTransport();

    // Initialize LoRa radio
    bool begin(int cs, int rst, int dio1, int busy);

    // Send raw data
    bool send(uint8_t* data, size_t length);

    // Receive data (non-blocking)
    bool receive(uint8_t* buffer, size_t* length);

    // Check if data is available
    bool available();

    // Get RSSI of last received packet
    int getRSSI();

    // Get SNR of last received packet
    float getSNR();

    // Set transmission power (dBm)
    void setTxPower(int power);

    // Set spreading factor (7-12)
    void setSpreadingFactor(int sf);

    // Enter sleep mode
    void sleep();

    // Wake from sleep mode
    void wake();

    // Get radio statistics
    void getStats(int& rssi, float& snr, uint32_t& packetsSent,
                  uint32_t& packetsReceived, uint32_t& txErrors);

private:
    SX1262* radio;
    int _csPin, _rstPin, _dio1Pin, _busyPin;

    uint32_t packetsSent;
    uint32_t packetsReceived;
    uint32_t txErrors;
    int lastRSSI;
    float lastSNR;

    void setupRadio();
    void setInterrupt();
    static void IRAM_ATTR onReceive();
};

#endif // LORA_TRANSPORT_H
