/**
 * GPS Service
 * Handles UART communication with GPS module and NMEA parsing
 */

#ifndef GPS_SERVICE_H
#define GPS_SERVICE_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "../config.h"

class GPSService {
public:
    GPSService();

    // Initialize GPS module
    bool begin(int rxPin, int txPin);

    // Update GPS data (call in main loop)
    void update();

    // Get current location
    GPSLocation getLocation();

    // Check if GPS has valid fix
    bool hasFix();

    // Get number of satellites
    int getSatellites();

    // Get GPS statistics
    void getStats(int& satellites, float& hdop, unsigned long& age);

private:
    TinyGPSPlus gps;
    HardwareSerial* gpsSerial;
    GPSLocation currentLocation;
    bool initialized;
    unsigned long lastUpdate;
};

#endif // GPS_SERVICE_H
