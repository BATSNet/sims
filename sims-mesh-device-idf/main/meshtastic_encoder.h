/**
 * Meshtastic Protobuf Encoder
 * Helper functions to encode Meshtastic FromRadio messages using nanopb
 * ESP-IDF version - identical API, just removed Arduino dependency
 */

#ifndef MESHTASTIC_ENCODER_H
#define MESHTASTIC_ENCODER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t buildFromRadio_MyNodeInfo(uint8_t* buffer, size_t maxLen, uint32_t deviceId);

size_t buildFromRadio_NodeInfo(uint8_t* buffer, size_t maxLen, uint32_t deviceId,
                                 const char* longName, const char* shortName);

size_t buildFromRadio_Channel(uint8_t* buffer, size_t maxLen,
                                int channelIndex, int role,
                                const char* name,
                                const uint8_t* psk, size_t pskLen);

size_t buildFromRadio_ConfigComplete(uint8_t* buffer, size_t maxLen);

#ifdef __cplusplus
}
#endif

#endif // MESHTASTIC_ENCODER_H
