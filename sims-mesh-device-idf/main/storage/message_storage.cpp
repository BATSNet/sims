/**
 * Message Storage Implementation
 * ESP-IDF version using SPIFFS via VFS (POSIX file API)
 */

#include "storage/message_storage.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static const char* TAG = "Storage";

MessageStorage::MessageStorage() : initialized(false) {
}

bool MessageStorage::begin() {
    ESP_LOGI(TAG, "Initializing message storage...");

    // SPIFFS is mounted in main.cpp - verify by checking partition info
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS not mounted");
        return false;
    }
    ESP_LOGI(TAG, "SPIFFS available: %d/%d bytes used", used, total);

    initialized = true;

    int pending = getPendingCount();
    ESP_LOGI(TAG, "Storage initialized, %d pending messages", pending);

    return true;
}

bool MessageStorage::storeMessage(const IncidentReport& incident) {
    if (!initialized) return false;

    std::string path = getMessagePath(incident.timestamp);
    ESP_LOGI(TAG, "Storing message: %s", path.c_str());

    return writeIncident(path, incident);
}

bool MessageStorage::markAsSent(uint32_t sequenceNumber) {
    if (!initialized) return false;

    std::string path = getMessagePath(sequenceNumber);

    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        remove(path.c_str());
        ESP_LOGI(TAG, "Message marked as sent: %s", path.c_str());
        return true;
    }

    return false;
}

int MessageStorage::getPendingCount() {
    if (!initialized) return 0;

    int count = 0;
    DIR* dir = opendir(STORAGE_PATH);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                count++;
            }
        }
        closedir(dir);
    }

    return count;
}

bool MessageStorage::getNextPending(IncidentReport& incident) {
    if (!initialized) return false;

    DIR* dir = opendir(STORAGE_PATH);
    if (!dir) return false;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            std::string path = std::string(STORAGE_PATH) + "/" + entry->d_name;
            closedir(dir);
            return readIncident(path, incident);
        }
    }

    closedir(dir);
    return false;
}

void MessageStorage::clearAll() {
    if (!initialized) return;

    ESP_LOGI(TAG, "Clearing all pending messages...");

    DIR* dir = opendir(STORAGE_PATH);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                std::string path = std::string(STORAGE_PATH) + "/" + entry->d_name;
                remove(path.c_str());
            }
        }
        closedir(dir);
    }

    ESP_LOGI(TAG, "All messages cleared");
}

std::string MessageStorage::getMessagePath(uint32_t sequenceNumber) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/msg_%lu.dat",
             STORAGE_PATH, (unsigned long)sequenceNumber);
    return std::string(filename);
}

bool MessageStorage::writeIncident(const std::string& path, const IncidentReport& incident) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path.c_str());
        return false;
    }

    fwrite(&incident.deviceId, sizeof(incident.deviceId), 1, f);
    fwrite(&incident.latitude, sizeof(incident.latitude), 1, f);
    fwrite(&incident.longitude, sizeof(incident.longitude), 1, f);
    fwrite(&incident.altitude, sizeof(incident.altitude), 1, f);
    fwrite(&incident.timestamp, sizeof(incident.timestamp), 1, f);
    fwrite(&incident.priority, sizeof(incident.priority), 1, f);
    fwrite(&incident.category, sizeof(incident.category), 1, f);
    fwrite(incident.description, sizeof(incident.description), 1, f);

    fclose(f);
    return true;
}

bool MessageStorage::readIncident(const std::string& path, IncidentReport& incident) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", path.c_str());
        return false;
    }

    fread(&incident.deviceId, sizeof(incident.deviceId), 1, f);
    fread(&incident.latitude, sizeof(incident.latitude), 1, f);
    fread(&incident.longitude, sizeof(incident.longitude), 1, f);
    fread(&incident.altitude, sizeof(incident.altitude), 1, f);
    fread(&incident.timestamp, sizeof(incident.timestamp), 1, f);
    fread(&incident.priority, sizeof(incident.priority), 1, f);
    fread(&incident.category, sizeof(incident.category), 1, f);
    fread(incident.description, sizeof(incident.description), 1, f);

    incident.hasImage = false;
    incident.hasAudio = false;

    fclose(f);
    return true;
}
