# Heltec WiFi LoRa 32 V3 - Vext Power Control Fix

## Problem

OLED display remained blank and firmware hung during I2C initialization at `Wire.begin()`.

## Root Cause

GPIO36 (Vext_Ctrl) controls power to external peripherals including:
- OLED display
- I2C bus
- External sensors

The Vext rail uses a **P-channel FET** that requires:
- **GPIO36 = LOW** to enable power (FET conducts)
- **GPIO36 = HIGH** to disable power (FET off)

Without enabling Vext, the OLED has no power and I2C communication fails.

## Solution

Enable Vext before initializing I2C or OLED:

```cpp
// CRITICAL: Enable Vext power for OLED/I2C (GPIO36 LOW = power ON)
pinMode(36, OUTPUT);
digitalWrite(36, LOW);
delay(100);  // Give power time to stabilize

// Now safe to initialize I2C
Wire.begin(OLED_SDA, OLED_SCL);
```

## Correct Initialization Sequence

1. Enable Vext power (GPIO36 LOW)
2. Wait 100ms for power stabilization
3. Initialize I2C with Wire.begin()
4. Initialize display with display.begin()

## Pin Summary

- GPIO17: OLED_SDA (I2C data)
- GPIO18: OLED_SCL (I2C clock)
- GPIO21: OLED_RST (reset pin)
- GPIO36: Vext_Ctrl (power control - **must be LOW**)

## References

- [Heltec Community: V3 Vext GPIO36](http://community.heltec.cn/t/wifi-lora-32-v3-vext-not-connected-to-pin-gpio36/13469)
- [ESPHome Heltec V3 Config](https://devices.esphome.io/devices/heltec-wifi-lora-32-v3/)
- [Official Heltec ESP32 Library](https://github.com/HelTecAutomation/Heltec_ESP32)

## Power Management

To save power, you can disable Vext when OLED is not needed:

```cpp
// Turn off OLED and I2C power
digitalWrite(36, HIGH);

// Turn back on
digitalWrite(36, LOW);
delay(100);
// Re-initialize display
```

This is useful for battery-powered mesh nodes.
