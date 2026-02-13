/**
 * GPS Service
 * Handles UART communication with GPS module and NMEA parsing
 * ESP-IDF version using uart_driver
 */

#ifndef GPS_SERVICE_H
#define GPS_SERVICE_H

#include <stdint.h>
#include <TinyGPS++.h>
#include "../config.h"

class GPSService {
public:
    GPSService();

    bool begin(int rxPin, int txPin);
    void update();

    GPSLocation getLocation();
    bool hasFix();
    int getSatellites();
    void getStats(int& satellites, float& hdop, unsigned long& age);

private:
    TinyGPSPlus gps;
    GPSLocation currentLocation;
    bool initialized;
    unsigned long lastUpdate;
    int uartNum;
};

#endif // GPS_SERVICE_H
