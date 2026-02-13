/**
 * LoRa Transport Layer Implementation
 * ESP-IDF version using RadioLib with custom EspHal
 */

#include "lora_transport.h"
#include "esp_hal.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "LoRa";

// HAL instance - must persist for the lifetime of the radio
static EspHal* hal = nullptr;

// Static instance for interrupt handling
static LoRaTransport* _instance = nullptr;
static volatile bool receivedFlag = false;

LoRaTransport::LoRaTransport()
    : radio(nullptr), packetsSent(0), packetsReceived(0),
      txErrors(0), lastRSSI(0), lastSNR(0.0) {
    _instance = this;
}

LoRaTransport::~LoRaTransport() {
    radio = nullptr;
    hal = nullptr;
}

bool LoRaTransport::begin(int cs, int rst, int dio1, int busy) {
    _csPin = cs;
    _rstPin = rst;
    _dio1Pin = dio1;
    _busyPin = busy;

    // Create HAL with SPI pins
    hal = new EspHal(LORA_SCK, LORA_MISO, LORA_MOSI);

    // Create radio instance with HAL
    radio = new SX1262(new Module(hal, cs, dio1, rst, busy));

    // Initialize radio
    ESP_LOGI(TAG, "Initializing SX1262...");
    int state = radio->begin(LORA_FREQUENCY / 1e6,
                            LORA_BANDWIDTH,
                            LORA_SPREADING_FACTOR,
                            LORA_CODING_RATE,
                            LORA_SYNC_WORD,
                            LORA_TX_POWER,
                            LORA_PREAMBLE_LENGTH);

    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "SX1262 init failed, code %d", state);
        return false;
    }

    ESP_LOGI(TAG, "SX1262 init success");

    setupRadio();
    setInterrupt();

    ESP_LOGI(TAG, "Radio configured and ready");
    return true;
}

void LoRaTransport::setupRadio() {
    radio->setCRC(true);
    radio->setOutputPower(LORA_TX_POWER);
    radio->setCurrentLimit(140);

    ESP_LOGI(TAG, "Frequency: %.1f MHz", LORA_FREQUENCY / 1e6);
    ESP_LOGI(TAG, "Bandwidth: %.1f kHz", LORA_BANDWIDTH);
    ESP_LOGI(TAG, "Spreading Factor: %d", LORA_SPREADING_FACTOR);
    ESP_LOGI(TAG, "TX Power: %d dBm", LORA_TX_POWER);
}

void LoRaTransport::setInterrupt() {
    radio->setPacketReceivedAction(onReceive);
    radio->startReceive();
}

void IRAM_ATTR LoRaTransport::onReceive() {
    receivedFlag = true;
}

bool LoRaTransport::send(uint8_t* data, size_t length) {
    if (!radio || length > MAX_PACKET_SIZE) {
        return false;
    }

    int state = radio->transmit(data, length);

    if (state == RADIOLIB_ERR_NONE) {
        packetsSent++;
        ESP_LOGI(TAG, "Packet sent successfully (%d bytes)", length);
        return true;
    } else {
        txErrors++;
        ESP_LOGE(TAG, "Transmission failed, code %d", state);
        return false;
    }
}

bool LoRaTransport::receive(uint8_t* buffer, size_t* length) {
    if (!receivedFlag) {
        return false;
    }

    receivedFlag = false;

    int state = radio->readData(buffer, MAX_PACKET_SIZE);

    if (state == RADIOLIB_ERR_NONE) {
        *length = radio->getPacketLength();
        lastRSSI = radio->getRSSI();
        lastSNR = radio->getSNR();
        packetsReceived++;

        ESP_LOGI(TAG, "Received packet: %d bytes, RSSI: %d dBm, SNR: %.2f dB",
                 *length, lastRSSI, lastSNR);

        radio->startReceive();
        return true;
    } else {
        ESP_LOGE(TAG, "Read failed, code %d", state);
        radio->startReceive();
        return false;
    }
}

bool LoRaTransport::available() {
    return receivedFlag;
}

int LoRaTransport::getRSSI() {
    return lastRSSI;
}

float LoRaTransport::getSNR() {
    return lastSNR;
}

void LoRaTransport::setTxPower(int power) {
    if (radio) {
        radio->setOutputPower(power);
    }
}

void LoRaTransport::setSpreadingFactor(int sf) {
    if (radio && sf >= 7 && sf <= 12) {
        radio->setSpreadingFactor(sf);
    }
}

void LoRaTransport::sleep() {
    if (radio) {
        radio->sleep();
    }
}

void LoRaTransport::wake() {
    if (radio) {
        radio->standby();
        radio->startReceive();
    }
}

void LoRaTransport::getStats(int& rssi, float& snr, uint32_t& sent,
                            uint32_t& received, uint32_t& errors) {
    rssi = lastRSSI;
    snr = lastSNR;
    sent = packetsSent;
    received = packetsReceived;
    errors = txErrors;
}
