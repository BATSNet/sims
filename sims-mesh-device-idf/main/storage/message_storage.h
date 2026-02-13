/**
 * Message Storage Service
 * Handles persistent storage of pending messages using SPIFFS (VFS)
 * ESP-IDF version
 */

#ifndef MESSAGE_STORAGE_H
#define MESSAGE_STORAGE_H

#include <stdint.h>
#include <string>
#include "../config.h"

class MessageStorage {
public:
    MessageStorage();

    bool begin();
    bool storeMessage(const IncidentReport& incident);
    bool markAsSent(uint32_t sequenceNumber);
    int getPendingCount();
    bool getNextPending(IncidentReport& incident);
    void clearAll();

private:
    bool initialized;
    std::string getMessagePath(uint32_t sequenceNumber);
    bool writeIncident(const std::string& path, const IncidentReport& incident);
    bool readIncident(const std::string& path, IncidentReport& incident);
};

#endif // MESSAGE_STORAGE_H
