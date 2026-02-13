/**
 * Meshtastic Adapter Implementation
 */

#include "meshtastic_adapter.h"
#include <string.h>

MeshtasticAdapter::MeshtasticAdapter() {
    deviceId = (uint32_t)ESP.getEfuseMac();
}

bool MeshtasticAdapter::simsToMeshtastic(
    float latitude,
    float longitude,
    const char* description,
    MeshtasticPacket& outPacket
) {
    // Create a text message packet with location info
    memset(&outPacket, 0, sizeof(MeshtasticPacket));

    outPacket.from = deviceId;
    outPacket.to = 0xFFFFFFFF;  // Broadcast
    outPacket.channel = 0;
    outPacket.portNum = MESHTASTIC_TEXT_MESSAGE;
    outPacket.hopLimit = 3;
    outPacket.wantAck = 0;

    // Format message: "description @ lat,lon"
    char message[237];
    snprintf(message, sizeof(message), "%s @ %.6f,%.6f",
             description, latitude, longitude);

    size_t msgLen = strlen(message);
    if (msgLen > sizeof(outPacket.payload)) {
        msgLen = sizeof(outPacket.payload);
    }

    memcpy(outPacket.payload, message, msgLen);
    outPacket.payloadSize = msgLen;

    Serial.printf("[Meshtastic] SIMS → Meshtastic: %s\n", message);
    return true;
}

bool MeshtasticAdapter::meshtasticToSims(
    const MeshtasticPacket& packet,
    float& outLatitude,
    float& outLongitude,
    char* outDescription,
    size_t descMaxLen
) {
    if (packet.portNum == MESHTASTIC_TEXT_MESSAGE) {
        // Extract text message
        size_t len = min((size_t)packet.payloadSize, descMaxLen - 1);
        memcpy(outDescription, packet.payload, len);
        outDescription[len] = '\0';

        // Try to extract coordinates if present (format: "text @ lat,lon")
        char* atSign = strstr(outDescription, " @ ");
        if (atSign != nullptr) {
            float lat, lon;
            if (sscanf(atSign + 3, "%f,%f", &lat, &lon) == 2) {
                outLatitude = lat;
                outLongitude = lon;
                *atSign = '\0';  // Truncate description at @ sign
                Serial.printf("[Meshtastic] Meshtastic → SIMS: %s at %.6f,%.6f\n",
                             outDescription, lat, lon);
                return true;
            }
        }

        // No coordinates found
        outLatitude = 0.0;
        outLongitude = 0.0;
        Serial.printf("[Meshtastic] Meshtastic → SIMS: %s (no location)\n", outDescription);
        return true;

    } else if (packet.portNum == MESHTASTIC_POSITION) {
        // Extract position data
        // TODO: Decode actual Meshtastic position protobuf
        // For now, just indicate position received
        snprintf(outDescription, descMaxLen, "Position update");
        outLatitude = 0.0;
        outLongitude = 0.0;
        Serial.println("[Meshtastic] Position packet received (decode not implemented)");
        return true;
    }

    return false;
}

size_t MeshtasticAdapter::encodePacket(const MeshtasticPacket& packet, uint8_t* buffer, size_t bufferSize) {
    // Simplified encoding (not actual Meshtastic protobuf)
    // TODO: Use nanopb to encode actual Meshtastic protobuf
    // For now, use a simple binary format

    if (bufferSize < packet.payloadSize + 20) {
        return 0;  // Not enough space
    }

    size_t offset = 0;

    // Header (simplified)
    memcpy(buffer + offset, &packet.from, 4); offset += 4;
    memcpy(buffer + offset, &packet.to, 4); offset += 4;
    buffer[offset++] = packet.channel;
    buffer[offset++] = packet.portNum;
    buffer[offset++] = packet.hopLimit;
    buffer[offset++] = packet.wantAck;
    buffer[offset++] = (uint8_t)(packet.payloadSize & 0xFF);
    buffer[offset++] = (uint8_t)((packet.payloadSize >> 8) & 0xFF);

    // Payload
    memcpy(buffer + offset, packet.payload, packet.payloadSize);
    offset += packet.payloadSize;

    Serial.printf("[Meshtastic] Encoded packet: %d bytes\n", offset);
    return offset;
}

bool MeshtasticAdapter::decodePacket(const uint8_t* data, size_t dataSize, MeshtasticPacket& outPacket) {
    // Simplified decoding (not actual Meshtastic protobuf)
    // TODO: Use nanopb to decode actual Meshtastic protobuf

    if (dataSize < 14) {
        return false;  // Too short
    }

    size_t offset = 0;

    memcpy(&outPacket.from, data + offset, 4); offset += 4;
    memcpy(&outPacket.to, data + offset, 4); offset += 4;
    outPacket.channel = data[offset++];
    outPacket.portNum = data[offset++];
    outPacket.hopLimit = data[offset++];
    outPacket.wantAck = data[offset++];
    outPacket.payloadSize = data[offset] | (data[offset + 1] << 8); offset += 2;

    if (offset + outPacket.payloadSize > dataSize) {
        return false;  // Invalid payload size
    }

    memcpy(outPacket.payload, data + offset, outPacket.payloadSize);

    Serial.printf("[Meshtastic] Decoded packet: from=0x%08X, portNum=%d, %d bytes\n",
                 outPacket.from, outPacket.portNum, outPacket.payloadSize);
    return true;
}

bool MeshtasticAdapter::createPositionPacket(
    float latitude,
    float longitude,
    float altitude,
    MeshtasticPacket& outPacket
) {
    memset(&outPacket, 0, sizeof(MeshtasticPacket));

    outPacket.from = deviceId;
    outPacket.to = 0xFFFFFFFF;  // Broadcast
    outPacket.channel = 0;
    outPacket.portNum = MESHTASTIC_POSITION;
    outPacket.hopLimit = 3;
    outPacket.wantAck = 0;

    // TODO: Encode actual Meshtastic position protobuf
    // For now, use simplified format
    MeshtasticPosition pos;
    pos.latitudeI = latitudeToInt(latitude);
    pos.longitudeI = longitudeToInt(longitude);
    pos.altitude = (int32_t)altitude;
    pos.time = millis() / 1000;  // Convert to seconds

    memcpy(outPacket.payload, &pos, sizeof(pos));
    outPacket.payloadSize = sizeof(pos);

    Serial.printf("[Meshtastic] Position packet created: %.6f,%.6f\n", latitude, longitude);
    return true;
}

bool MeshtasticAdapter::createTextMessagePacket(
    const char* text,
    MeshtasticPacket& outPacket
) {
    memset(&outPacket, 0, sizeof(MeshtasticPacket));

    outPacket.from = deviceId;
    outPacket.to = 0xFFFFFFFF;  // Broadcast
    outPacket.channel = 0;
    outPacket.portNum = MESHTASTIC_TEXT_MESSAGE;
    outPacket.hopLimit = 3;
    outPacket.wantAck = 0;

    size_t textLen = strlen(text);
    if (textLen > sizeof(outPacket.payload)) {
        textLen = sizeof(outPacket.payload);
    }

    memcpy(outPacket.payload, text, textLen);
    outPacket.payloadSize = textLen;

    Serial.printf("[Meshtastic] Text packet created: %s\n", text);
    return true;
}

int32_t MeshtasticAdapter::latitudeToInt(float lat) {
    return (int32_t)(lat * 1e7);
}

int32_t MeshtasticAdapter::longitudeToInt(float lon) {
    return (int32_t)(lon * 1e7);
}

float MeshtasticAdapter::intToLatitude(int32_t latI) {
    return (float)latI / 1e7;
}

float MeshtasticAdapter::intToLongitude(int32_t lonI) {
    return (float)lonI / 1e7;
}
