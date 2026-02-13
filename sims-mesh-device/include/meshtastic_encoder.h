/**
 * Meshtastic Protobuf Encoder
 *
 * Provides helper functions to encode Meshtastic FromRadio messages using nanopb.
 * Used by BLE layer to send properly formatted protobuf messages to the Meshtastic app.
 */

#ifndef MESHTASTIC_ENCODER_H
#define MESHTASTIC_ENCODER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Build a FromRadio message containing MyNodeInfo
 *
 * @param buffer Output buffer for encoded protobuf message
 * @param maxLen Maximum size of output buffer
 * @param deviceId Device node ID (e.g., 0xed020f3c)
 * @return Number of bytes written to buffer, or 0 on error
 */
size_t buildFromRadio_MyNodeInfo(uint8_t* buffer, size_t maxLen, uint32_t deviceId);

/**
 * Build a FromRadio message containing NodeInfo
 *
 * @param buffer Output buffer for encoded protobuf message
 * @param maxLen Maximum size of output buffer
 * @param deviceId Device node ID
 * @param longName Long device name (e.g., "SIMS-MESH")
 * @param shortName Short device name (e.g., "SIMS")
 * @return Number of bytes written to buffer, or 0 on error
 */
size_t buildFromRadio_NodeInfo(uint8_t* buffer, size_t maxLen, uint32_t deviceId,
                                 const char* longName, const char* shortName);

/**
 * Build a FromRadio message with config_complete_id
 * Signals to app that all configuration messages have been sent
 *
 * @param buffer Output buffer for encoded protobuf message
 * @param maxLen Maximum size of output buffer
 * @return Number of bytes written to buffer, or 0 on error
 */
size_t buildFromRadio_ConfigComplete(uint8_t* buffer, size_t maxLen);

#ifdef __cplusplus
}
#endif

#endif // MESHTASTIC_ENCODER_H
