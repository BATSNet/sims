/**
 * Display Manager Implementation
 * ESP-IDF version using SSD1306 driver
 */

#include "display_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "Display";

// Boot screen content
static const char* BOOT_LOGO[] = {
    "S.I.M.S.",
    "Situation Incident",
    "Management System",
    "Mesh Network",
    "v1.0.0"
};

static const int LOGO_LINES = 5;
static const int LOGO_START_Y = 10;

unsigned long DisplayManager::millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}

DisplayManager::DisplayManager()
    : display(nullptr), initialized(false), lastUpdate(0),
      currentScreen(SCREEN_NONE), screenLineCount(0) {
    memset(screenLines, 0, sizeof(screenLines));
    memset(screenLineYPos, 0, sizeof(screenLineYPos));
}

bool DisplayManager::begin() {
    ESP_LOGI(TAG, "Initializing OLED display...");

    display = new SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_RST);

    if (!display->begin(SCREEN_ADDRESS, OLED_SDA, OLED_SCL)) {
        ESP_LOGE(TAG, "SSD1306 initialization failed");
        return false;
    }

    initialized = true;

    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    display->setTextSize(1);
    display->display();

    ESP_LOGI(TAG, "OLED initialized successfully");
    return true;
}

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

void DisplayManager::updateStatus(bool gpsValid, int satellites, int meshNodes,
                                  int pendingMessages, int batteryPercent,
                                  bool wifiConnected, int wifiRSSI,
                                  bool bleConnected,
                                  int loraRSSI, float loraSNR,
                                  int packetsReceived) {
    if (!initialized) return;

    bool isTransition = isScreenTransition(SCREEN_STATUS);

    beginScreen(SCREEN_STATUS);

    // Title bar
    display->setCursor(0, 0);
    display->print("SIMS");

    // WiFi indicator
    if (wifiConnected) {
        int bars = 3;
        if (wifiRSSI > -55) bars = 4;
        else if (wifiRSSI > -70) bars = 3;
        else if (wifiRSSI > -80) bars = 2;
        else bars = 1;

        int wifiX = 30;
        for (int i = 0; i < bars; i++) {
            display->fillRect(wifiX + i * 3, 0 + (3 - i), 2, 1 + i, SSD1306_WHITE);
        }
    }

    // BLE indicator
    if (bleConnected) {
        display->setCursor(48, 0);
        display->print("B");
    }

    // Battery indicator
    display->setCursor(56, 0);
    display->print("BAT:");
    display->print(batteryPercent);
    display->print("%");

    // Divider line
    display->drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Build animated content as lines
    char line[32];

    // GPS Status
    if (gpsValid) {
        snprintf(line, sizeof(line), "GPS:%d sats", satellites);
    } else {
        snprintf(line, sizeof(line), "GPS:NO FIX");
    }
    addLine(line, 14);

    // Mesh Network Status
    if (packetsReceived > 0) {
        snprintf(line, sizeof(line), "Mesh:%d/%dpkts", meshNodes, packetsReceived);
    } else {
        snprintf(line, sizeof(line), "Mesh:%d nodes", meshNodes);
    }
    addLine(line, 26);

    // LoRa Signal Strength
    if (loraRSSI != 0) {
        snprintf(line, sizeof(line), "LoRa:%ddBm/%.1fdB", loraRSSI, loraSNR);
    } else {
        snprintf(line, sizeof(line), "LoRa:READY");
    }
    addLine(line, 38);

    // Status line
    if (pendingMessages > 0) {
        snprintf(line, sizeof(line), "Q:%d msgs", pendingMessages);
    } else {
        snprintf(line, sizeof(line), "IDLE");
    }
    addLine(line, 50);

    if (isTransition) {
        animateScreen();
    } else {
        for (int i = 0; i < screenLineCount; i++) {
            display->setCursor(0, screenLineYPos[i]);
            display->print(screenLines[i]);
        }
        display->display();
    }
}

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

void DisplayManager::drawSIMSLogo() {
    display->setTextSize(3);
    display->setCursor(20, 10);
    display->print("SIMS");

    display->setTextSize(1);
    centerText("Mesh Network", 38);
    centerText("Incident Reporting", 48);
}

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

void DisplayManager::drawAnimatedLogo() {
    if (!initialized) return;

    for (int i = 0; i < LOGO_LINES; i++) {
        int yPos = LOGO_START_Y + (i * 10);
        revealLine(BOOT_LOGO[i], yPos);
    }
}
