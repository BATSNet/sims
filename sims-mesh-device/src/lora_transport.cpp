/**
 * LoRa Transport Layer Implementation
 */

#include "lora_transport.h"

// Static instance for interrupt handling
static LoRaTransport* _instance = nullptr;
static volatile bool receivedFlag = false;

LoRaTransport::LoRaTransport()
    : radio(nullptr), packetsSent(0), packetsReceived(0),
      txErrors(0), lastRSSI(0), lastSNR(0.0) {
    _instance = this;
}

LoRaTransport::~LoRaTransport() {
    if (radio) {
        delete radio;
    }
}

bool LoRaTransport::begin(int cs, int rst, int dio1, int busy) {
    _csPin = cs;
    _rstPin = rst;
    _dio1Pin = dio1;
    _busyPin = busy;

    // Create radio instance (SX1262 for Heltec LoRa 32 V3)
    radio = new SX1262(new Module(cs, dio1, rst, busy));

    // Initialize radio
    Serial.print("[LoRa] Initializing SX1262... ");
    int state = radio->begin(LORA_FREQUENCY / 1e6,  // Frequency in MHz
                            LORA_BANDWIDTH,
                            LORA_SPREADING_FACTOR,
                            LORA_CODING_RATE,
                            LORA_SYNC_WORD,
                            LORA_TX_POWER,
                            LORA_PREAMBLE_LENGTH);

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("failed, code %d\n", state);
        return false;
    }

    Serial.println("success!");

    // Configure radio
    setupRadio();

    // Set interrupt for receiving
    setInterrupt();

    Serial.println("[LoRa] Radio configured and ready");
    return true;
}

void LoRaTransport::setupRadio() {
    // Set CRC enabled
    radio->setCRC(true);

    // Set output power
    radio->setOutputPower(LORA_TX_POWER);

    // Set current limit to 140 mA (safe for most modules)
    radio->setCurrentLimit(140);

    Serial.printf("[LoRa] Frequency: %.1f MHz\n", LORA_FREQUENCY / 1e6);
    Serial.printf("[LoRa] Bandwidth: %.1f kHz\n", LORA_BANDWIDTH);
    Serial.printf("[LoRa] Spreading Factor: %d\n", LORA_SPREADING_FACTOR);
    Serial.printf("[LoRa] TX Power: %d dBm\n", LORA_TX_POWER);
}

void LoRaTransport::setInterrupt() {
    // Set the interrupt handler for receiving packets
    radio->setPacketReceivedAction(onReceive);

    // Start listening
    radio->startReceive();
}

void IRAM_ATTR LoRaTransport::onReceive() {
    receivedFlag = true;
}

bool LoRaTransport::send(uint8_t* data, size_t length) {
    if (!radio || length > MAX_PACKET_SIZE) {
        return false;
    }

    // Transmit the packet
    int state = radio->transmit(data, length);

    if (state == RADIOLIB_ERR_NONE) {
        packetsSent++;
        Serial.printf("[LoRa] Packet sent successfully (%d bytes)\n", length);
        return true;
    } else {
        txErrors++;
        Serial.printf("[LoRa] Transmission failed, code %d\n", state);
        return false;
    }
}

bool LoRaTransport::receive(uint8_t* buffer, size_t* length) {
    if (!receivedFlag) {
        return false;
    }

    receivedFlag = false;

    // Read the received packet
    int state = radio->readData(buffer, MAX_PACKET_SIZE);

    if (state == RADIOLIB_ERR_NONE) {
        *length = radio->getPacketLength();
        lastRSSI = radio->getRSSI();
        lastSNR = radio->getSNR();
        packetsReceived++;

        Serial.printf("[LoRa] Received packet: %d bytes, RSSI: %d dBm, SNR: %.2f dB\n",
                     *length, lastRSSI, lastSNR);

        // Resume listening
        radio->startReceive();
        return true;
    } else {
        Serial.printf("[LoRa] Read failed, code %d\n", state);
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
