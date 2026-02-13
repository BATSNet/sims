/**
 * RadioLib Hardware Abstraction Layer for ESP32-S3
 * Uses ESP-IDF spi_master driver (chip-agnostic, works on S3/S2/C3/etc.)
 */

#include "esp_hal.h"
#include <string.h>

static const char* TAG = "EspHal";

EspHal::EspHal(int8_t sck, int8_t miso, int8_t mosi)
    : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
      spiSCK(sck), spiMISO(miso), spiMOSI(mosi),
      spiDevice(nullptr), spiHost(SPI2_HOST), spiInitialized(false) {
}

void EspHal::init() {
    spiBegin();
}

void EspHal::term() {
    spiEnd();
}

void EspHal::pinMode(uint32_t pin, uint32_t mode) {
    if (pin == RADIOLIB_NC) return;

    gpio_config_t conf = {};
    conf.pin_bit_mask = (1ULL << pin);
    conf.mode = (gpio_mode_t)mode;
    conf.pull_up_en = GPIO_PULLUP_DISABLE;
    conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&conf);
}

void EspHal::digitalWrite(uint32_t pin, uint32_t value) {
    if (pin == RADIOLIB_NC) return;
    gpio_set_level((gpio_num_t)pin, value);
}

uint32_t EspHal::digitalRead(uint32_t pin) {
    if (pin == RADIOLIB_NC) return 0;
    return gpio_get_level((gpio_num_t)pin);
}

void EspHal::attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void), uint32_t mode) {
    if (interruptNum == RADIOLIB_NC) return;

    gpio_install_isr_service((int)ESP_INTR_FLAG_IRAM);
    gpio_set_intr_type((gpio_num_t)interruptNum, (gpio_int_type_t)(mode & 0x7));
    gpio_isr_handler_add((gpio_num_t)interruptNum, (void (*)(void*))interruptCb, NULL);
}

void EspHal::detachInterrupt(uint32_t interruptNum) {
    if (interruptNum == RADIOLIB_NC) return;
    gpio_isr_handler_remove((gpio_num_t)interruptNum);
    gpio_wakeup_disable((gpio_num_t)interruptNum);
    gpio_set_intr_type((gpio_num_t)interruptNum, GPIO_INTR_DISABLE);
}

void EspHal::delay(unsigned long ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void EspHal::delayMicroseconds(unsigned long us) {
    uint64_t m = (uint64_t)esp_timer_get_time();
    if (us) {
        uint64_t e = (m + us);
        if (m > e) { // overflow
            while ((uint64_t)esp_timer_get_time() > e) { NOP(); }
        }
        while ((uint64_t)esp_timer_get_time() < e) { NOP(); }
    }
}

unsigned long EspHal::millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

unsigned long EspHal::micros() {
    return (unsigned long)(esp_timer_get_time());
}

long EspHal::pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) {
    if (pin == RADIOLIB_NC) return 0;

    this->pinMode(pin, INPUT);
    uint32_t start = this->micros();
    uint32_t curtick = this->micros();

    while (this->digitalRead(pin) == state) {
        if ((this->micros() - curtick) > timeout) {
            return 0;
        }
    }

    return this->micros() - start;
}

void EspHal::spiBegin() {
    if (spiInitialized) return;

    // Configure SPI bus
    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = spiMOSI;
    busConfig.miso_io_num = spiMISO;
    busConfig.sclk_io_num = spiSCK;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz = 256;

    esp_err_t ret = spi_bus_initialize(spiHost, &busConfig, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Add SPI device (no CS - RadioLib handles CS via GPIO)
    spi_device_interface_config_t devConfig = {};
    devConfig.clock_speed_hz = 2000000;  // 2 MHz
    devConfig.mode = 0;                   // SPI mode 0 (CPOL=0, CPHA=0)
    devConfig.spics_io_num = -1;          // CS handled by RadioLib
    devConfig.queue_size = 1;
    devConfig.flags = 0;

    ret = spi_bus_add_device(spiHost, &devConfig, &spiDevice);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return;
    }

    spiInitialized = true;
    ESP_LOGI(TAG, "SPI initialized (SCK=%d, MISO=%d, MOSI=%d)", spiSCK, spiMISO, spiMOSI);
}

void EspHal::spiBeginTransaction() {
    // No-op - config set at init time
}

void EspHal::spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
    if (len == 0) return;

    spi_transaction_t trans = {};
    trans.length = len * 8;
    trans.tx_buffer = out;
    trans.rx_buffer = in;

    spi_device_transmit(spiDevice, &trans);
}

void EspHal::spiEndTransaction() {
    // No-op
}

void EspHal::spiEnd() {
    if (!spiInitialized) return;

    if (spiDevice) {
        spi_bus_remove_device(spiDevice);
        spiDevice = nullptr;
    }
    spi_bus_free(spiHost);
    spiInitialized = false;
}
