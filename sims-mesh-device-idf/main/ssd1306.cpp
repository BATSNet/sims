/**
 * Minimal SSD1306 128x64 OLED I2C Driver for ESP-IDF
 */

#include "ssd1306.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "SSD1306";

// 5x7 font (ASCII 32-127), stored as 5 bytes per character
static const uint8_t font5x7[] = {
    0x00,0x00,0x00,0x00,0x00, // 32 space
    0x00,0x00,0x5F,0x00,0x00, // 33 !
    0x00,0x07,0x00,0x07,0x00, // 34 "
    0x14,0x7F,0x14,0x7F,0x14, // 35 #
    0x24,0x2A,0x7F,0x2A,0x12, // 36 $
    0x23,0x13,0x08,0x64,0x62, // 37 %
    0x36,0x49,0x55,0x22,0x50, // 38 &
    0x00,0x05,0x03,0x00,0x00, // 39 '
    0x00,0x1C,0x22,0x41,0x00, // 40 (
    0x00,0x41,0x22,0x1C,0x00, // 41 )
    0x08,0x2A,0x1C,0x2A,0x08, // 42 *
    0x08,0x08,0x3E,0x08,0x08, // 43 +
    0x00,0x50,0x30,0x00,0x00, // 44 ,
    0x08,0x08,0x08,0x08,0x08, // 45 -
    0x00,0x60,0x60,0x00,0x00, // 46 .
    0x20,0x10,0x08,0x04,0x02, // 47 /
    0x3E,0x51,0x49,0x45,0x3E, // 48 0
    0x00,0x42,0x7F,0x40,0x00, // 49 1
    0x42,0x61,0x51,0x49,0x46, // 50 2
    0x21,0x41,0x45,0x4B,0x31, // 51 3
    0x18,0x14,0x12,0x7F,0x10, // 52 4
    0x27,0x45,0x45,0x45,0x39, // 53 5
    0x3C,0x4A,0x49,0x49,0x30, // 54 6
    0x01,0x71,0x09,0x05,0x03, // 55 7
    0x36,0x49,0x49,0x49,0x36, // 56 8
    0x06,0x49,0x49,0x29,0x1E, // 57 9
    0x00,0x36,0x36,0x00,0x00, // 58 :
    0x00,0x56,0x36,0x00,0x00, // 59 ;
    0x00,0x08,0x14,0x22,0x41, // 60 <
    0x14,0x14,0x14,0x14,0x14, // 61 =
    0x41,0x22,0x14,0x08,0x00, // 62 >
    0x02,0x01,0x51,0x09,0x06, // 63 ?
    0x32,0x49,0x79,0x41,0x3E, // 64 @
    0x7E,0x11,0x11,0x11,0x7E, // 65 A
    0x7F,0x49,0x49,0x49,0x36, // 66 B
    0x3E,0x41,0x41,0x41,0x22, // 67 C
    0x7F,0x41,0x41,0x22,0x1C, // 68 D
    0x7F,0x49,0x49,0x49,0x41, // 69 E
    0x7F,0x09,0x09,0x01,0x01, // 70 F
    0x3E,0x41,0x41,0x51,0x32, // 71 G
    0x7F,0x08,0x08,0x08,0x7F, // 72 H
    0x00,0x41,0x7F,0x41,0x00, // 73 I
    0x20,0x40,0x41,0x3F,0x01, // 74 J
    0x7F,0x08,0x14,0x22,0x41, // 75 K
    0x7F,0x40,0x40,0x40,0x40, // 76 L
    0x7F,0x02,0x04,0x02,0x7F, // 77 M
    0x7F,0x04,0x08,0x10,0x7F, // 78 N
    0x3E,0x41,0x41,0x41,0x3E, // 79 O
    0x7F,0x09,0x09,0x09,0x06, // 80 P
    0x3E,0x41,0x51,0x21,0x5E, // 81 Q
    0x7F,0x09,0x19,0x29,0x46, // 82 R
    0x46,0x49,0x49,0x49,0x31, // 83 S
    0x01,0x01,0x7F,0x01,0x01, // 84 T
    0x3F,0x40,0x40,0x40,0x3F, // 85 U
    0x1F,0x20,0x40,0x20,0x1F, // 86 V
    0x7F,0x20,0x18,0x20,0x7F, // 87 W
    0x63,0x14,0x08,0x14,0x63, // 88 X
    0x03,0x04,0x78,0x04,0x03, // 89 Y
    0x61,0x51,0x49,0x45,0x43, // 90 Z
    0x00,0x00,0x7F,0x41,0x41, // 91 [
    0x02,0x04,0x08,0x10,0x20, // 92 backslash
    0x41,0x41,0x7F,0x00,0x00, // 93 ]
    0x04,0x02,0x01,0x02,0x04, // 94 ^
    0x40,0x40,0x40,0x40,0x40, // 95 _
    0x00,0x01,0x02,0x04,0x00, // 96 `
    0x20,0x54,0x54,0x54,0x78, // 97 a
    0x7F,0x48,0x44,0x44,0x38, // 98 b
    0x38,0x44,0x44,0x44,0x20, // 99 c
    0x38,0x44,0x44,0x48,0x7F, // 100 d
    0x38,0x54,0x54,0x54,0x18, // 101 e
    0x08,0x7E,0x09,0x01,0x02, // 102 f
    0x08,0x14,0x54,0x54,0x3C, // 103 g
    0x7F,0x08,0x04,0x04,0x78, // 104 h
    0x00,0x44,0x7D,0x40,0x00, // 105 i
    0x20,0x40,0x44,0x3D,0x00, // 106 j
    0x00,0x7F,0x10,0x28,0x44, // 107 k
    0x00,0x41,0x7F,0x40,0x00, // 108 l
    0x7C,0x04,0x18,0x04,0x78, // 109 m
    0x7C,0x08,0x04,0x04,0x78, // 110 n
    0x38,0x44,0x44,0x44,0x38, // 111 o
    0x7C,0x14,0x14,0x14,0x08, // 112 p
    0x08,0x14,0x14,0x18,0x7C, // 113 q
    0x7C,0x08,0x04,0x04,0x08, // 114 r
    0x48,0x54,0x54,0x54,0x20, // 115 s
    0x04,0x3F,0x44,0x40,0x20, // 116 t
    0x3C,0x40,0x40,0x20,0x7C, // 117 u
    0x1C,0x20,0x40,0x20,0x1C, // 118 v
    0x3C,0x40,0x30,0x40,0x3C, // 119 w
    0x44,0x28,0x10,0x28,0x44, // 120 x
    0x0C,0x50,0x50,0x50,0x3C, // 121 y
    0x44,0x64,0x54,0x4C,0x44, // 122 z
    0x00,0x08,0x36,0x41,0x00, // 123 {
    0x00,0x00,0x7F,0x00,0x00, // 124 |
    0x00,0x41,0x36,0x08,0x00, // 125 }
    0x08,0x08,0x2A,0x1C,0x08, // 126 ~
    0x08,0x1C,0x2A,0x08,0x08, // 127 DEL
};

// Block characters for progress bar (Adafruit GFX-compatible)
// 0xDB = full block, 0xB0 = light shade
static const uint8_t font_block_full[] = {0x7F,0x7F,0x7F,0x7F,0x7F}; // full block
static const uint8_t font_block_light[] = {0x55,0x2A,0x55,0x2A,0x55}; // light shade

SSD1306::SSD1306(int width, int height, int rstPin)
    : width_(width), height_(height), rstPin_(rstPin),
      addr_(0x3C), busHandle_(nullptr), devHandle_(nullptr),
      initialized_(false),
      dirtyPages_(0), displayOn_(true),
      cursorX_(0), cursorY_(0), textSize_(1), textColor_(SSD1306_WHITE) {
    memset(buffer_, 0, sizeof(buffer_));
}

SSD1306::~SSD1306() {
    if (devHandle_) {
        i2c_master_bus_rm_device(devHandle_);
    }
    if (busHandle_) {
        i2c_del_master_bus(busHandle_);
    }
}

bool SSD1306::begin(uint8_t addr, int sdaPin, int sclPin) {
    addr_ = addr;

    // Reset display if reset pin is set
    if (rstPin_ >= 0) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << rstPin_);
        io_conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&io_conf);

        gpio_set_level((gpio_num_t)rstPin_, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level((gpio_num_t)rstPin_, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)rstPin_, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Initialize I2C master bus
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = (gpio_num_t)sdaPin;
    bus_config.scl_io_num = (gpio_num_t)sclPin;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &busHandle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    // Add SSD1306 device
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = addr_;
    dev_config.scl_speed_hz = 400000;

    err = i2c_master_bus_add_device(busHandle_, &dev_config, &devHandle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(err));
        return false;
    }

    // SSD1306 initialization sequence
    static const uint8_t initCmds[] = {
        0xAE,       // Display OFF
        0xD5, 0x80, // Set display clock div
        0xA8, 0x3F, // Set multiplex (64-1)
        0xD3, 0x00, // Set display offset
        0x40,       // Set start line
        0x8D, 0x14, // Charge pump ON (internal VCC)
        0x20, 0x00, // Memory addressing mode: horizontal
        0xA1,       // Segment remap (flip horizontal)
        0xC8,       // COM scan direction (flip vertical)
        0xDA, 0x12, // COM pins config
        0x81, 0xCF, // Set contrast
        0xD9, 0xF1, // Set precharge
        0xDB, 0x40, // Set VCOMH deselect level
        0xA4,       // Entire display ON (follow RAM)
        0xA6,       // Normal display (not inverted)
        0xAF,       // Display ON
    };

    for (size_t i = 0; i < sizeof(initCmds); i++) {
        sendCommand(initCmds[i]);
    }

    clearDisplay();
    display();

    initialized_ = true;
    ESP_LOGI(TAG, "Display initialized at 0x%02x", addr_);
    return true;
}

void SSD1306::sendCommand(uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd}; // Co=0, D/C#=0 (command)
    i2c_master_transmit(devHandle_, buf, 2, 100);
}

void SSD1306::clearDisplay() {
    memset(buffer_, 0, sizeof(buffer_));
    dirtyPages_ = 0xFF;  // Mark all pages dirty so displayDirty() sends them
}

void SSD1306::display() {
    if (!devHandle_) return;

    // Set column address range
    sendCommand(0x21); // Column addr
    sendCommand(0);    // Start
    sendCommand(127);  // End

    // Set page address range
    sendCommand(0x22); // Page addr
    sendCommand(0);    // Start
    sendCommand(7);    // End (64/8 - 1)

    // Send framebuffer in chunks (I2C has buffer limits)
    // Each I2C write: 0x40 (data prefix) + up to 128 bytes
    for (int i = 0; i < 1024; i += 128) {
        uint8_t buf[129];
        buf[0] = 0x40; // Co=0, D/C#=1 (data)
        memcpy(buf + 1, buffer_ + i, 128);
        i2c_master_transmit(devHandle_, buf, 129, 100);
    }
}

void SSD1306::setPixel(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;

    int page = y / 8;
    if (color) {
        buffer_[x + page * width_] |= (1 << (y & 7));
    } else {
        buffer_[x + page * width_] &= ~(1 << (y & 7));
    }
    dirtyPages_ |= (1 << page);
}

void SSD1306::setTextSize(uint8_t size) {
    textSize_ = (size > 0) ? size : 1;
}

void SSD1306::setTextColor(uint8_t color) {
    textColor_ = color;
}

void SSD1306::setCursor(int16_t x, int16_t y) {
    cursorX_ = x;
    cursorY_ = y;
}

void SSD1306::drawChar(int16_t x, int16_t y, char c, uint8_t color, uint8_t size) {
    if (c < 32 || c > 127) {
        // Handle special block characters used by progress bar
        const uint8_t* glyph = nullptr;
        if ((uint8_t)c == 0xDB) {
            glyph = font_block_full;
        } else if ((uint8_t)c == 0xB0) {
            glyph = font_block_light;
        } else {
            return;
        }
        for (int8_t col = 0; col < 5; col++) {
            uint8_t line = glyph[col];
            for (int8_t row = 0; row < 7; row++) {
                if (line & (1 << row)) {
                    if (size == 1) {
                        setPixel(x + col, y + row, color);
                    } else {
                        fillRect(x + col * size, y + row * size, size, size, color);
                    }
                }
            }
        }
        return;
    }

    const uint8_t* glyph = &font5x7[(c - 32) * 5];

    for (int8_t col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (int8_t row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                if (size == 1) {
                    setPixel(x + col, y + row, color);
                } else {
                    fillRect(x + col * size, y + row * size, size, size, color);
                }
            }
        }
    }
}

void SSD1306::print(const char* text) {
    if (!text) return;

    while (*text) {
        drawChar(cursorX_, cursorY_, *text, textColor_, textSize_);
        cursorX_ += 6 * textSize_; // 5 pixels + 1 space per char
        text++;
    }
}

void SSD1306::print(int value) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    print(buf);
}

void SSD1306::getTextBounds(const char* text, int16_t x, int16_t y,
                            int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    if (!text) {
        *x1 = x; *y1 = y; *w = 0; *h = 0;
        return;
    }

    int len = strlen(text);
    *x1 = x;
    *y1 = y;
    *w = len * 6 * textSize_ - textSize_; // subtract last spacing
    if (*w < 0) *w = 0;
    *h = 7 * textSize_;
}

void SSD1306::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    // Bresenham's line algorithm
    int16_t dx = abs(x1 - x0);
    int16_t dy = -abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    while (true) {
        setPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void SSD1306::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    for (int16_t j = y; j < y + h; j++) {
        for (int16_t i = x; i < x + w; i++) {
            setPixel(i, j, color);
        }
    }
}

void SSD1306::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    drawLine(x, y, x + w - 1, y, color);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
    drawLine(x + w - 1, y + h - 1, x, y + h - 1, color);
    drawLine(x, y + h - 1, x, y, color);
}

void SSD1306::displayDirty() {
    if (!devHandle_ || dirtyPages_ == 0) return;

    for (int page = 0; page < 8; page++) {
        if (!(dirtyPages_ & (1 << page))) continue;

        // Set column address range: full width
        sendCommand(0x21);  // Column addr
        sendCommand(0);     // Start
        sendCommand(127);   // End

        // Set page address range: just this page
        sendCommand(0x22);  // Page addr
        sendCommand(page);  // Start
        sendCommand(page);  // End

        // Send 128 bytes for this page
        uint8_t buf[129];
        buf[0] = 0x40;  // Data prefix
        memcpy(buf + 1, buffer_ + page * 128, 128);
        i2c_master_transmit(devHandle_, buf, 129, 100);
    }

    dirtyPages_ = 0;
}

void SSD1306::setDisplayOn(bool on) {
    if (!devHandle_) return;
    sendCommand(on ? 0xAF : 0xAE);
    displayOn_ = on;
}

void SSD1306::clearRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    for (int16_t j = y; j < y + h && j < height_; j++) {
        for (int16_t i = x; i < x + w && i < width_; i++) {
            if (i >= 0 && j >= 0) {
                int page = j / 8;
                buffer_[i + page * width_] &= ~(1 << (j & 7));
                dirtyPages_ |= (1 << page);
            }
        }
    }
}
