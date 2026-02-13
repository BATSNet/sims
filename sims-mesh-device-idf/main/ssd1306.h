/**
 * Minimal SSD1306 128x64 OLED I2C Driver for ESP-IDF
 * Provides Adafruit_SSD1306-compatible API subset
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stddef.h>
#include "driver/i2c_master.h"

#define SSD1306_WHITE 1
#define SSD1306_BLACK 0

class SSD1306 {
public:
    SSD1306(int width, int height, int rstPin);
    ~SSD1306();

    bool begin(uint8_t addr, int sdaPin, int sclPin);

    void clearDisplay();
    void display();

    void setTextSize(uint8_t size);
    void setTextColor(uint8_t color);
    void setCursor(int16_t x, int16_t y);

    void print(const char* text);
    void print(int value);

    void getTextBounds(const char* text, int16_t x, int16_t y,
                       int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

private:
    int width_;
    int height_;
    int rstPin_;
    uint8_t addr_;

    i2c_master_bus_handle_t busHandle_;
    i2c_master_dev_handle_t devHandle_;
    bool initialized_;

    // Frame buffer: 128x64 / 8 = 1024 bytes
    uint8_t buffer_[1024];

    // Text state
    int16_t cursorX_;
    int16_t cursorY_;
    uint8_t textSize_;
    uint8_t textColor_;

    void sendCommand(uint8_t cmd);
    void sendCommands(const uint8_t* cmds, size_t len);
    void setPixel(int16_t x, int16_t y, uint8_t color);
    void drawChar(int16_t x, int16_t y, char c, uint8_t color, uint8_t size);
};

#endif // SSD1306_H
