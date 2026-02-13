/**
 * Power Manager - ESP-IDF
 *
 * Power management for battery optimization.
 * Uses ESP-IDF PM (Power Management) and sleep APIs.
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "esp_pm.h"
#include "esp_sleep.h"
#include "config.h"
#include <stdint.h>

class PowerManager {
public:
    PowerManager();

    // Initialize power management
    bool begin();

    // Enable/disable dynamic frequency scaling
    bool enableLightSleep();
    void disableLightSleep();

    // Battery monitoring
    int getBatteryPercent();
    int getBatteryVoltage();  // in mV
    bool isLowBattery();

    // Power control for peripherals
    void enableWiFiPower();
    void disableWiFiPower();
    void enableCameraPower();
    void disableCameraPower();

    // Deep sleep (for future use)
    void enterDeepSleep(uint64_t sleepTimeUs);

private:
    bool pmConfigured;
    int readBatteryADC();
};

#endif // POWER_MANAGER_H
