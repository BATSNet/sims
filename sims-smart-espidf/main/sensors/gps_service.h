/**
 * GPS Service - ESP-IDF Port
 *
 * Handles UART communication with GPS module and NMEA parsing
 * Uses TinyGPS++ library (C++ compatible, no Arduino dependency needed)
 */

#ifndef GPS_SERVICE_H
#define GPS_SERVICE_H

#include "driver/uart.h"
#include "config.h"
#include <TinyGPSPlus.h>

// GPS location data structure
struct GPSLocation {
    float latitude;
    float longitude;
    float altitude;
    float speed;
    float bearing;
    bool valid;
    unsigned long timestamp;  // millis when last updated
};

class GPSService {
public:
    GPSService();

    // Initialize GPS module
    bool begin(int rxPin = GPS_RX_PIN, int txPin = GPS_TX_PIN, int baud = GPS_BAUD);

    // Update GPS data (call periodically or from task)
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
    GPSLocation currentLocation;
    bool initialized;
    unsigned long lastUpdate;
    uart_port_t uartPort;
};

#endif // GPS_SERVICE_H
