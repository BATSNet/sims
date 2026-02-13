/**
 * Message Storage Service
 * Handles persistent storage of pending messages using LittleFS
 */

#ifndef MESSAGE_STORAGE_H
#define MESSAGE_STORAGE_H

#include <Arduino.h>
#include <LITTLEFS.h>
#include "../config.h"

class MessageStorage {
public:
    MessageStorage();

    // Initialize storage
    bool begin();

    // Store message for retry
    bool storeMessage(const IncidentReport& incident);

    // Mark message as sent
    bool markAsSent(uint32_t sequenceNumber);

    // Get pending messages count
    int getPendingCount();

    // Get next pending message
    bool getNextPending(IncidentReport& incident);

    // Clear all pending messages
    void clearAll();

private:
    bool initialized;
    String getMessagePath(uint32_t sequenceNumber);
    bool writeIncident(const String& path, const IncidentReport& incident);
    bool readIncident(const String& path, IncidentReport& incident);
};

#endif // MESSAGE_STORAGE_H
