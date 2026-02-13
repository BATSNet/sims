/**
 * RadioLib Hardware Abstraction Layer for ESP32-S3
 * Uses ESP-IDF spi_master driver instead of direct register access
 */

#ifndef ESP_HAL_H
#define ESP_HAL_H

#include <RadioLib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_log.h"

// Arduino-style macros for RadioLib
#define LOW                         (0x0)
#define HIGH                        (0x1)
#define INPUT                       (0x01)
#define OUTPUT                      (0x03)
#define RISING                      (0x01)
#define FALLING                     (0x02)
#define NOP()                       asm volatile ("nop")

class EspHal : public RadioLibHal {
public:
    EspHal(int8_t sck, int8_t miso, int8_t mosi);

    void init() override;
    void term() override;

    // GPIO
    void pinMode(uint32_t pin, uint32_t mode) override;
    void digitalWrite(uint32_t pin, uint32_t value) override;
    uint32_t digitalRead(uint32_t pin) override;

    // Interrupts
    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) override;
    void detachInterrupt(uint32_t interruptNum) override;

    // Timing
    void delay(unsigned long ms) override;
    void delayMicroseconds(unsigned long us) override;
    unsigned long millis() override;
    unsigned long micros() override;
    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override;

    // SPI - using spi_master driver (works on all ESP32 variants)
    void spiBegin() override;
    void spiBeginTransaction() override;
    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override;
    void spiEndTransaction() override;
    void spiEnd() override;

private:
    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;
    spi_device_handle_t spiDevice;
    spi_host_device_t spiHost;
    bool spiInitialized;
};

#endif // ESP_HAL_H
