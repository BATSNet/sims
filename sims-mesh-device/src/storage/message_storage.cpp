/**
 * Message Storage Implementation
 */

#include "storage/message_storage.h"
#include <LittleFS.h>
#define LITTLEFS LittleFS

MessageStorage::MessageStorage() : initialized(false) {
}

bool MessageStorage::begin() {
    Serial.println("[Storage] Initializing message storage...");

    if (!LITTLEFS.begin(true)) {
        Serial.println("[Storage] Failed to mount LittleFS");
        return false;
    }

    // Create storage directory if it doesn't exist
    if (!LITTLEFS.exists(STORAGE_PATH)) {
        LITTLEFS.mkdir(STORAGE_PATH);
    }

    initialized = true;

    // Count existing pending messages
    int pending = getPendingCount();
    Serial.printf("[Storage] Storage initialized, %d pending messages\n", pending);

    return true;
}

bool MessageStorage::storeMessage(const IncidentReport& incident) {
    if (!initialized) {
        return false;
    }

    String path = getMessagePath(incident.timestamp);

    Serial.printf("[Storage] Storing message: %s\n", path.c_str());

    return writeIncident(path, incident);
}

bool MessageStorage::markAsSent(uint32_t sequenceNumber) {
    if (!initialized) {
        return false;
    }

    String path = getMessagePath(sequenceNumber);

    if (LITTLEFS.exists(path)) {
        LITTLEFS.remove(path);
        Serial.printf("[Storage] Message marked as sent: %s\n", path.c_str());
        return true;
    }

    return false;
}

int MessageStorage::getPendingCount() {
    if (!initialized) {
        return 0;
    }

    int count = 0;
    File dir = LITTLEFS.open(STORAGE_PATH);
    if (dir) {
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                count++;
            }
            file = dir.openNextFile();
        }
        dir.close();
    }

    return count;
}

bool MessageStorage::getNextPending(IncidentReport& incident) {
    if (!initialized) {
        return false;
    }

    File dir = LITTLEFS.open(STORAGE_PATH);
    if (!dir) {
        return false;
    }

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String path = file.path();
            dir.close();
            return readIncident(path, incident);
        }
        file = dir.openNextFile();
    }

    dir.close();
    return false;
}

void MessageStorage::clearAll() {
    if (!initialized) {
        return;
    }

    Serial.println("[Storage] Clearing all pending messages...");

    File dir = LITTLEFS.open(STORAGE_PATH);
    if (dir) {
        File file = dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                LITTLEFS.remove(file.path());
            }
            file = dir.openNextFile();
        }
        dir.close();
    }

    Serial.println("[Storage] All messages cleared");
}

String MessageStorage::getMessagePath(uint32_t sequenceNumber) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/msg_%lu.dat",
             STORAGE_PATH, sequenceNumber);
    return String(filename);
}

bool MessageStorage::writeIncident(const String& path, const IncidentReport& incident) {
    File file = LITTLEFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("[Storage] Failed to open file for writing: %s\n", path.c_str());
        return false;
    }

    // Write incident data (binary format)
    // TODO: Use Protobuf for proper serialization
    file.write((uint8_t*)&incident.deviceId, sizeof(incident.deviceId));
    file.write((uint8_t*)&incident.latitude, sizeof(incident.latitude));
    file.write((uint8_t*)&incident.longitude, sizeof(incident.longitude));
    file.write((uint8_t*)&incident.altitude, sizeof(incident.altitude));
    file.write((uint8_t*)&incident.timestamp, sizeof(incident.timestamp));
    file.write((uint8_t*)&incident.priority, sizeof(incident.priority));
    file.write((uint8_t*)&incident.category, sizeof(incident.category));

    // Write description
    file.write((uint8_t*)incident.description, sizeof(incident.description));

    // Note: Image and audio data are NOT stored to save space
    // Only metadata is persisted for retry

    file.close();
    return true;
}

bool MessageStorage::readIncident(const String& path, IncidentReport& incident) {
    File file = LITTLEFS.open(path, FILE_READ);
    if (!file) {
        Serial.printf("[Storage] Failed to open file for reading: %s\n", path.c_str());
        return false;
    }

    // Read incident data
    file.read((uint8_t*)&incident.deviceId, sizeof(incident.deviceId));
    file.read((uint8_t*)&incident.latitude, sizeof(incident.latitude));
    file.read((uint8_t*)&incident.longitude, sizeof(incident.longitude));
    file.read((uint8_t*)&incident.altitude, sizeof(incident.altitude));
    file.read((uint8_t*)&incident.timestamp, sizeof(incident.timestamp));
    file.read((uint8_t*)&incident.priority, sizeof(incident.priority));
    file.read((uint8_t*)&incident.category, sizeof(incident.category));
    file.read((uint8_t*)incident.description, sizeof(incident.description));

    // Clear media flags (not stored)
    incident.hasImage = false;
    incident.hasAudio = false;

    file.close();
    return true;
}
