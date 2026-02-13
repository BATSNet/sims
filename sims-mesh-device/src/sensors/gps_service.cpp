/**
 * GPS Service Implementation
 */

#include "sensors/gps_service.h"

GPSService::GPSService()
    : gpsSerial(nullptr), initialized(false), lastUpdate(0) {
    currentLocation.valid = false;
}

bool GPSService::begin(int rxPin, int txPin) {
    Serial.println("[GPS] Initializing GPS service...");

    // Initialize hardware serial for GPS
    gpsSerial = new HardwareSerial(1);
    gpsSerial->begin(GPS_BAUD_RATE, SERIAL_8N1, rxPin, txPin);

    initialized = true;
    Serial.println("[GPS] GPS service initialized");
    Serial.println("[GPS] Waiting for GPS fix...");

    return true;
}

void GPSService::update() {
    if (!initialized || !gpsSerial) {
        return;
    }

    // Read GPS data
    while (gpsSerial->available() > 0) {
        char c = gpsSerial->read();
        gps.encode(c);
    }

    // Update location data if we have a fix
    if (gps.location.isValid()) {
        currentLocation.latitude = gps.location.lat();
        currentLocation.longitude = gps.location.lng();
        currentLocation.altitude = gps.altitude.meters();
        currentLocation.speed = gps.speed.kmph();
        currentLocation.bearing = gps.course.deg();
        currentLocation.valid = true;
        currentLocation.timestamp = millis();
        lastUpdate = millis();

        // Log GPS fix on first acquisition
        static bool firstFix = true;
        if (firstFix) {
            Serial.printf("[GPS] First fix acquired: %.6f, %.6f\n",
                         currentLocation.latitude, currentLocation.longitude);
            firstFix = false;
        }
    } else {
        // Check if data is stale
        if (millis() - lastUpdate > GPS_TIMEOUT) {
            currentLocation.valid = false;
        }
    }

    // Log GPS status periodically
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 30000) {  // Every 30 seconds
        Serial.printf("[GPS] Status: %s, Satellites: %d, HDOP: %.2f\n",
                     hasFix() ? "FIX" : "NO FIX",
                     gps.satellites.value(),
                     gps.hdop.hdop());
        lastLog = millis();
    }
}

GPSLocation GPSService::getLocation() {
    return currentLocation;
}

bool GPSService::hasFix() {
    return currentLocation.valid && gps.location.isValid();
}

int GPSService::getSatellites() {
    return gps.satellites.value();
}

void GPSService::getStats(int& satellites, float& hdop, unsigned long& age) {
    satellites = gps.satellites.value();
    hdop = gps.hdop.hdop();
    age = gps.location.age();
}
