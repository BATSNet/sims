/**
 * Display Manager Implementation
 * ESP-IDF version using SSD1306 driver
 *
 * Status screen layout (128x64):
 * [O] IDLE    B:1   [batt]     <- y=0, top bar
 * ____________________________  <- y=10, divider
 * GPS: 12 sats                  <- y=14
 * Mesh: 3 nodes                 <- y=26
 * LoRa: -85dBm / 6.2dB         <- y=38
 * Q: 0          Rx: 15         <- y=50
 */

#include "display_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "Display";

unsigned long DisplayManager::millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

DisplayManager::DisplayManager()
    : display(nullptr), initialized(false), screenOn(true),
      currentScreen(SCREEN_NONE), screenLineCount(0),
      statusDrawn(false),
      txActive(false), txStartTime(0), txDurationMs(0),
      lastActivityTime(0) {
    memset(screenLines, 0, sizeof(screenLines));
    memset(screenLineYPos, 0, sizeof(screenLineYPos));
    memset(prevFields, 0, sizeof(prevFields));
}

bool DisplayManager::begin() {
    ESP_LOGI(TAG, "Initializing OLED display...");

    display = new SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_RST);

    if (!display->begin(SCREEN_ADDRESS, OLED_SDA, OLED_SCL)) {
        ESP_LOGE(TAG, "SSD1306 initialization failed");
        return false;
    }

    initialized = true;
    lastActivityTime = millis();

    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    display->setTextSize(1);
    display->display();

    ESP_LOGI(TAG, "OLED initialized successfully");
    return true;
}

// --- Boot screens (full redraw, blocking) ---

void DisplayManager::showBootScreen() {
    if (!initialized) return;

    currentScreen = SCREEN_BOOT;

    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);

    vTaskDelay(pdMS_TO_TICKS(50));

    // Title: "S.I.M.S." in large text (size 2)
    display->setTextSize(2);
    const char* title = "S.I.M.S.";
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int titleX = (SCREEN_WIDTH - w) / 2;
    int titleY = 8;

    sweepCursorAcrossLine(titleY, w, 80);
    display->setCursor(titleX, titleY);
    display->print(title);
    display->display();
    vTaskDelay(pdMS_TO_TICKS(40));

    // Subtitle lines in size 1
    display->setTextSize(1);

    const char* line1 = "Situation Incident";
    display->getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
    int line1X = (SCREEN_WIDTH - w) / 2;
    int line1Y = 28;
    sweepCursorAcrossLine(line1Y, w, 60);
    display->setCursor(line1X, line1Y);
    display->print(line1);
    display->display();
    vTaskDelay(pdMS_TO_TICKS(40));

    const char* line2 = "Management System";
    display->getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    int line2X = (SCREEN_WIDTH - w) / 2;
    int line2Y = 38;
    sweepCursorAcrossLine(line2Y, w, 60);
    display->setCursor(line2X, line2Y);
    display->print(line2);
    display->display();
    vTaskDelay(pdMS_TO_TICKS(40));

    // Divider line
    display->drawLine(20, 48, 108, 48, SSD1306_WHITE);
    display->display();
    vTaskDelay(pdMS_TO_TICKS(100));

    // Bottom info
    const char* descriptor = "Mesh Network";
    const char* version = "v1.0.0";

    display->setCursor(10, 52);
    display->print(descriptor);

    display->getTextBounds(version, 0, 0, &x1, &y1, &w, &h);
    display->setCursor(SCREEN_WIDTH - w - 10, 52);
    display->print(version);
    display->display();

    vTaskDelay(pdMS_TO_TICKS(1000));
}

void DisplayManager::showInitProgress(const char* step, int percent) {
    if (!initialized) return;

    currentScreen = SCREEN_INIT;

    display->clearDisplay();
    display->setTextSize(1);

    char statusLine[32];
    snprintf(statusLine, sizeof(statusLine), "> %s", step);

    int statusY = 18;
    int statusWidth = strlen(statusLine) * 6;

    sweepCursorAcrossLine(statusY, statusWidth, 60);

    display->setCursor(0, statusY);
    display->print(statusLine);
    display->display();

    // ASCII progress bar
    int barY = 34;
    char progressBar[22];

    int barLength = 18;
    int filledBlocks = (barLength * percent) / 100;

    progressBar[0] = '[';
    for (int i = 0; i < barLength; i++) {
        if (i < filledBlocks) {
            progressBar[i + 1] = '\xDB';  // full block
        } else {
            progressBar[i + 1] = '\xB0';  // light shade
        }
    }
    progressBar[barLength + 1] = ']';
    progressBar[barLength + 2] = '\0';

    int barWidth = strlen(progressBar) * 6;
    display->setCursor((SCREEN_WIDTH - barWidth) / 2, barY);
    display->print(progressBar);

    char percentStr[8];
    snprintf(percentStr, sizeof(percentStr), "%d%%", percent);
    int percentWidth = strlen(percentStr) * 6;
    display->setCursor((SCREEN_WIDTH - percentWidth) / 2, barY + 12);
    display->print(percentStr);
    display->display();

    vTaskDelay(pdMS_TO_TICKS(30));
}

// --- Status screen with partial updates ---

void DisplayManager::drawStatusIcon(StatusIcon icon, int16_t x, int16_t y) {
    // 7x7 circle indicator
    // Draw circle outline
    display->setPixel(x + 2, y, SSD1306_WHITE);
    display->setPixel(x + 3, y, SSD1306_WHITE);
    display->setPixel(x + 4, y, SSD1306_WHITE);
    display->setPixel(x + 1, y + 1, SSD1306_WHITE);
    display->setPixel(x + 5, y + 1, SSD1306_WHITE);
    display->setPixel(x, y + 2, SSD1306_WHITE);
    display->setPixel(x + 6, y + 2, SSD1306_WHITE);
    display->setPixel(x, y + 3, SSD1306_WHITE);
    display->setPixel(x + 6, y + 3, SSD1306_WHITE);
    display->setPixel(x, y + 4, SSD1306_WHITE);
    display->setPixel(x + 6, y + 4, SSD1306_WHITE);
    display->setPixel(x + 1, y + 5, SSD1306_WHITE);
    display->setPixel(x + 5, y + 5, SSD1306_WHITE);
    display->setPixel(x + 2, y + 6, SSD1306_WHITE);
    display->setPixel(x + 3, y + 6, SSD1306_WHITE);
    display->setPixel(x + 4, y + 6, SSD1306_WHITE);

    if (icon == ICON_ACTIVE) {
        // Filled circle: fill interior
        display->fillRect(x + 2, y + 1, 3, 1, SSD1306_WHITE);
        display->fillRect(x + 1, y + 2, 5, 3, SSD1306_WHITE);
        display->fillRect(x + 2, y + 5, 3, 1, SSD1306_WHITE);
    } else if (icon == ICON_IDLE) {
        // Half-filled: fill bottom half
        display->fillRect(x + 1, y + 3, 5, 2, SSD1306_WHITE);
        display->fillRect(x + 2, y + 5, 3, 1, SSD1306_WHITE);
    }
    // ICON_DISCONNECTED: just the outline (already drawn)
}

void DisplayManager::drawBatteryIcon(int16_t x, int16_t y, int percent) {
    // Battery icon: 12x7 body + 2x3 nub on right = 14 wide total
    // Outer rectangle
    display->drawRect(x, y, 12, 7, SSD1306_WHITE);
    // Nub on right
    display->fillRect(x + 12, y + 2, 2, 3, SSD1306_WHITE);

    // Fill inside proportional to percentage (max inner width = 8)
    int fillWidth = (8 * percent) / 100;
    if (fillWidth < 0) fillWidth = 0;
    if (fillWidth > 8) fillWidth = 8;
    if (fillWidth > 0) {
        display->fillRect(x + 2, y + 2, fillWidth, 3, SSD1306_WHITE);
    }
}

void DisplayManager::drawTopBar(StatusIcon icon, const char* statusText,
                                 bool bleConnected, int bleClients,
                                 int batteryPercent) {
    // Clear top bar region (y=0 to y=9)
    display->clearRegion(0, 0, SCREEN_WIDTH, 10);

    // Status icon at (0, 1)
    drawStatusIcon(icon, 0, 1);

    // Status text at (10, 1)
    display->setTextSize(1);
    display->setCursor(10, 1);
    display->print(statusText);

    // BLE client count at (70, 1) - show "B:N" if connected
    if (bleConnected && bleClients > 0) {
        char bleBuf[16];
        snprintf(bleBuf, sizeof(bleBuf), "B:%d", bleClients);
        display->setCursor(70, 1);
        display->print(bleBuf);
    }

    // Battery percentage text + icon
    char batBuf[8];
    snprintf(batBuf, sizeof(batBuf), "%d%%", batteryPercent);
    int batTextLen = strlen(batBuf);
    // Right-align: icon(14px) at x=114, text before it
    int batTextX = 114 - batTextLen * 6 - 1;
    display->setCursor(batTextX, 1);
    display->print(batBuf);
    drawBatteryIcon(114, 0, batteryPercent);
}

void DisplayManager::updateField(DisplayField field, int16_t x, int16_t y,
                                  const char* newText) {
    if (strcmp(prevFields[field], newText) == 0 && statusDrawn) {
        return;  // No change
    }

    // Clear the old text region
    int oldLen = strlen(prevFields[field]);
    if (oldLen > 0) {
        display->clearRegion(x, y, oldLen * 6, 8);
    }
    // Clear the new text region too (in case new text is shorter)
    int newLen = strlen(newText);
    int maxLen = oldLen > newLen ? oldLen : newLen;
    if (maxLen > 0) {
        display->clearRegion(x, y, maxLen * 6, 8);
    }

    // Draw new text
    display->setTextSize(1);
    display->setCursor(x, y);
    display->print(newText);

    // Save for next comparison
    strncpy(prevFields[field], newText, 31);
    prevFields[field][31] = '\0';
}

void DisplayManager::updateStatus(bool gpsValid, int satellites, int meshNodes,
                                   int pendingMessages, int batteryPercent,
                                   bool bleConnected, int bleClients,
                                   int loraRSSI, float loraSNR,
                                   int packetsReceived) {
    if (!initialized || !screenOn) return;

    bool isTransition = (currentScreen != SCREEN_STATUS);

    if (isTransition) {
        // Full clear + redraw on screen transition
        display->clearDisplay();
        display->setTextSize(1);
        display->setTextColor(SSD1306_WHITE);
        memset(prevFields, 0, sizeof(prevFields));
        statusDrawn = false;
        currentScreen = SCREEN_STATUS;
    }

    // Determine status icon and text
    StatusIcon icon;
    const char* statusText;

    // Check if TX notification is active
    if (txActive) {
        if (millis() - txStartTime < (unsigned long)txDurationMs) {
            icon = ICON_ACTIVE;
            statusText = "TX";
        } else {
            txActive = false;
            icon = ICON_IDLE;
            statusText = "IDLE";
        }
    } else if (pendingMessages > 0) {
        icon = ICON_ACTIVE;
        statusText = "ACTIVE";
    } else {
        icon = ICON_IDLE;
        statusText = "IDLE";
    }

    // Draw top bar (always redraw - it has icons, not just text)
    drawTopBar(icon, statusText, bleConnected, bleClients, batteryPercent);

    // Divider line at y=10
    if (!statusDrawn) {
        display->drawLine(0, 10, 127, 10, SSD1306_WHITE);
    }

    // Field updates - only redraw what changed
    char line[32];

    // GPS (y=14)
    if (gpsValid) {
        snprintf(line, sizeof(line), "GPS: %d sats", satellites);
    } else {
        snprintf(line, sizeof(line), "GPS: NO FIX");
    }
    updateField(FIELD_GPS, 0, 14, line);

    // Mesh (y=26)
    snprintf(line, sizeof(line), "Mesh: %d nodes", meshNodes);
    updateField(FIELD_MESH, 0, 26, line);

    // LoRa (y=38)
    if (loraRSSI != 0) {
        snprintf(line, sizeof(line), "LoRa: %ddBm/%.1fdB", loraRSSI, loraSNR);
    } else {
        snprintf(line, sizeof(line), "LoRa: READY");
    }
    updateField(FIELD_LORA, 0, 38, line);

    // Queue + Rx (y=50)
    snprintf(line, sizeof(line), "Q:%d       Rx:%d", pendingMessages, packetsReceived);
    updateField(FIELD_QUEUE, 0, 50, line);

    statusDrawn = true;

    // Send only changed pages to display
    display->displayDirty();
}

// --- Non-blocking TX notification ---

void DisplayManager::notifyTx(int durationMs) {
    txActive = true;
    txStartTime = millis();
    txDurationMs = durationMs;
    registerActivity();
}

// --- Idle screen ---

void DisplayManager::showIdleScreen(int batteryPercent) {
    if (!initialized || !screenOn) return;
    if (currentScreen == SCREEN_IDLE) return;

    currentScreen = SCREEN_IDLE;
    statusDrawn = false;

    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);

    // Centered "SIMS" in large text
    display->setTextSize(2);
    const char* text = "SIMS";
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display->setCursor((SCREEN_WIDTH - w) / 2, 20);
    display->print(text);

    // Battery icon centered below
    drawBatteryIcon((SCREEN_WIDTH - 14) / 2, 45, batteryPercent);

    display->display();
}

// --- Sleep screen ---

void DisplayManager::showSleepScreen() {
    if (!initialized) return;

    currentScreen = SCREEN_SLEEP;

    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    display->setTextSize(2);

    const char* text = "SLEEP";
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display->setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display->print(text);
    display->display();

    vTaskDelay(pdMS_TO_TICKS(500));
}

// --- Display power ---

void DisplayManager::setScreenPower(bool on) {
    if (!initialized) return;

    screenOn = on;
    display->setDisplayOn(on);

    if (on) {
        // Force full redraw on next update
        statusDrawn = false;
        currentScreen = SCREEN_NONE;
        registerActivity();
    }
}

bool DisplayManager::isDisplayOn() {
    return screenOn && initialized;
}

// --- Activity tracking ---

void DisplayManager::registerActivity() {
    lastActivityTime = millis();
}

bool DisplayManager::isIdle(unsigned long timeoutMs) {
    return (millis() - lastActivityTime) >= timeoutMs;
}

// --- Message display (blocking, for init-time use) ---

void DisplayManager::showMessage(const char* message, int duration) {
    if (!initialized) return;

    bool isTransition = isScreenTransition(SCREEN_MESSAGE);

    beginScreen(SCREEN_MESSAGE);

    display->setTextSize(2);

    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
    int centerY = (SCREEN_HEIGHT - h) / 2;

    addLine(message, centerY);

    if (isTransition) {
        int textWidth = strlen(message) * 12;
        sweepCursorAcrossLine(centerY, textWidth, 60);

        display->setCursor((SCREEN_WIDTH - w) / 2, centerY);
        display->print(message);
        display->display();
    } else {
        display->setCursor((SCREEN_WIDTH - w) / 2, centerY);
        display->print(message);
        display->display();
    }

    if (duration > 0) {
        vTaskDelay(pdMS_TO_TICKS(duration));
    }
}

void DisplayManager::clear() {
    if (!initialized) return;
    display->clearDisplay();
    display->display();
}

// --- Private helpers ---

void DisplayManager::drawProgressBar(int x, int y, int width, int height, int percent) {
    display->drawRect(x, y, width, height, SSD1306_WHITE);
    int fillWidth = (width - 4) * percent / 100;
    display->fillRect(x + 2, y + 2, fillWidth, height - 4, SSD1306_WHITE);
}

void DisplayManager::centerText(const char* text, int y) {
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display->setCursor((SCREEN_WIDTH - w) / 2, y);
    display->print(text);
}

bool DisplayManager::isScreenTransition(ScreenType newScreen) {
    return currentScreen != newScreen;
}

void DisplayManager::beginScreen(ScreenType screenType) {
    if (!initialized) return;

    bool shouldAnimate = isScreenTransition(screenType);

    currentScreen = screenType;
    screenLineCount = 0;

    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);

    if (shouldAnimate) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void DisplayManager::addLine(const char* text, int y) {
    if (!initialized) return;
    if (screenLineCount >= MAX_SCREEN_LINES) return;

    strncpy(screenLines[screenLineCount], text, 31);
    screenLines[screenLineCount][31] = '\0';
    screenLineYPos[screenLineCount] = y;
    screenLineCount++;
}

void DisplayManager::animateScreen() {
    if (!initialized) return;

    for (int i = 0; i < screenLineCount; i++) {
        revealLine(screenLines[i], screenLineYPos[i]);
    }
}

void DisplayManager::sweepCursorAcrossLine(int y, int width, int durationMs) {
    if (!initialized) return;

    const int cursorSize = 12;
    const int frameDelay = 5;

    unsigned long startTime = millis();
    int lastX = 0;

    while (millis() - startTime < (unsigned long)durationMs) {
        unsigned long elapsed = millis() - startTime;

        int cursorX = (width * elapsed) / durationMs;

        if (cursorX != lastX && cursorX < width) {
            if (lastX > 0) {
                display->fillRect(lastX, y, cursorSize, cursorSize, SSD1306_BLACK);
            }

            display->fillRect(cursorX, y, cursorSize, cursorSize, SSD1306_WHITE);
            display->display();

            lastX = cursorX;
        }

        vTaskDelay(pdMS_TO_TICKS(frameDelay));
    }

    display->fillRect(lastX, y, cursorSize, cursorSize, SSD1306_BLACK);
    display->display();
}

void DisplayManager::revealLine(const char* text, int y) {
    if (!initialized) return;

    int textWidth = strlen(text) * 6;

    sweepCursorAcrossLine(y, textWidth, 60);

    display->setCursor(0, y);
    display->print(text);
    display->display();

    vTaskDelay(pdMS_TO_TICKS(40));
}
