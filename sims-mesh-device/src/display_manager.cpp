/**
 * Display Manager Implementation
 */

#include "display_manager.h"
#include <string.h>

// Boot screen content - professional centered layout
static const char* BOOT_LOGO[] = {
    "S.I.M.S.",                  // Main title
    "Situation Incident",        // Full name line 1
    "Management System",         // Full name line 2
    "Mesh Network",              // Descriptor
    "v1.0.0"                     // Version
};

static const int LOGO_LINES = 5;
static const int LOGO_START_Y = 10; // Start position for centered content

DisplayManager::DisplayManager()
    : display(nullptr), initialized(false), lastUpdate(0),
      currentScreen(SCREEN_NONE), screenLineCount(0) {
    // Initialize screen line storage
    memset(screenLines, 0, sizeof(screenLines));
    memset(screenLineYPos, 0, sizeof(screenLineYPos));
}

bool DisplayManager::begin() {
    Serial.println("[Display] Initializing OLED display...");

    // Create display instance
    display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

    // Initialize display
    if (!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("[Display] SSD1306 allocation failed");
        return false;
    }

    initialized = true;

    // Initial setup
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    display->setTextSize(1);
    display->display();

    Serial.println("[Display] OLED initialized successfully");
    return true;
}

void DisplayManager::showBootScreen() {
    if (!initialized) return;

    currentScreen = SCREEN_BOOT;

    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);

    // Initial delay (matches aivos-watch)
    delay(50);

    // Title: "S.I.M.S." in large text (size 2)
    display->setTextSize(2);
    const char* title = "S.I.M.S.";
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int titleX = (SCREEN_WIDTH - w) / 2;
    int titleY = 8;

    // Animate title with cursor sweep
    sweepCursorAcrossLine(titleY, w, 80);
    display->setCursor(titleX, titleY);
    display->print(title);
    display->display();
    delay(40);

    // Subtitle lines in size 1
    display->setTextSize(1);

    // Line 1: "Situation Incident"
    const char* line1 = "Situation Incident";
    display->getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
    int line1X = (SCREEN_WIDTH - w) / 2;
    int line1Y = 28;
    sweepCursorAcrossLine(line1Y, w, 60);
    display->setCursor(line1X, line1Y);
    display->print(line1);
    display->display();
    delay(40);

    // Line 2: "Management System"
    const char* line2 = "Management System";
    display->getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    int line2X = (SCREEN_WIDTH - w) / 2;
    int line2Y = 38;
    sweepCursorAcrossLine(line2Y, w, 60);
    display->setCursor(line2X, line2Y);
    display->print(line2);
    display->display();
    delay(40);

    // Divider line
    display->drawLine(20, 48, 108, 48, SSD1306_WHITE);
    display->display();
    delay(100);

    // Bottom info
    const char* descriptor = "Mesh Network";
    const char* version = "v1.0.0";

    // Descriptor on left
    display->setCursor(10, 52);
    display->print(descriptor);

    // Version on right
    display->getTextBounds(version, 0, 0, &x1, &y1, &w, &h);
    display->setCursor(SCREEN_WIDTH - w - 10, 52);
    display->print(version);
    display->display();

    // Final pause to view complete screen
    delay(1000);
}

void DisplayManager::showInitProgress(const char* step, int percent) {
    if (!initialized) return;

    currentScreen = SCREEN_INIT;

    display->clearDisplay();
    display->setTextSize(1);

    // Format status line with retro terminal prompt
    char statusLine[32];
    snprintf(statusLine, sizeof(statusLine), "> %s", step);

    int statusY = 18;
    int statusWidth = strlen(statusLine) * 6;

    // Fast cursor sweep across status line (aivos-watch style)
    sweepCursorAcrossLine(statusY, statusWidth, 60);

    // Reveal status text
    display->setCursor(0, statusY);
    display->print(statusLine);
    display->display();

    // ASCII progress bar
    int barY = 34;
    char progressBar[22];  // 20 chars + brackets + null terminator

    // Calculate filled blocks (18 chars for the bar itself)
    int barLength = 18;
    int filledBlocks = (barLength * percent) / 100;

    // Build progress bar string
    progressBar[0] = '[';
    for (int i = 0; i < barLength; i++) {
        if (i < filledBlocks) {
            progressBar[i + 1] = '\xDB';  // █ full block
        } else {
            progressBar[i + 1] = '\xB0';  // ░ light shade
        }
    }
    progressBar[barLength + 1] = ']';
    progressBar[barLength + 2] = '\0';

    // Draw ASCII progress bar centered
    int barWidth = strlen(progressBar) * 6;
    display->setCursor((SCREEN_WIDTH - barWidth) / 2, barY);
    display->print(progressBar);

    // Show percentage centered below bar
    char percentStr[8];
    snprintf(percentStr, sizeof(percentStr), "%d%%", percent);
    int percentWidth = strlen(percentStr) * 6;
    display->setCursor((SCREEN_WIDTH - percentWidth) / 2, barY + 12);
    display->print(percentStr);
    display->display();

    // Brief pause to show progress
    delay(30);
}

void DisplayManager::updateStatus(bool gpsValid, int satellites, int meshNodes,
                                  int pendingMessages, int batteryPercent,
                                  bool wifiConnected, int wifiRSSI,
                                  bool bleConnected,
                                  int loraRSSI, float loraSNR,
                                  int packetsReceived) {
    if (!initialized) return;

    bool isTransition = isScreenTransition(SCREEN_STATUS);

    // Begin screen (clears display, sets defaults)
    beginScreen(SCREEN_STATUS);

    // Title bar (not animated - static header)
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

    // Battery indicator (right side)
    display->setCursor(56, 0);
    display->print("BAT:");
    display->print(batteryPercent);
    display->print("%");

    // Divider line
    display->drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Build animated content as lines
    char line[32];

    // GPS Status line
    if (gpsValid) {
        snprintf(line, sizeof(line), "GPS:%d sats", satellites);
    } else {
        snprintf(line, sizeof(line), "GPS:NO FIX");
    }
    addLine(line, 14);

    // Mesh Network Status line (nodes + packets)
    if (packetsReceived > 0) {
        snprintf(line, sizeof(line), "Mesh:%d/%dpkts", meshNodes, packetsReceived);
    } else {
        snprintf(line, sizeof(line), "Mesh:%d nodes", meshNodes);
    }
    addLine(line, 26);

    // LoRa Signal Strength line (RSSI/SNR)
    if (loraRSSI != 0) {
        snprintf(line, sizeof(line), "LoRa:%ddBm/%.1fdB", loraRSSI, loraSNR);
    } else {
        snprintf(line, sizeof(line), "LoRa:READY");
    }
    addLine(line, 38);

    // Status line (queue or idle)
    if (pendingMessages > 0) {
        snprintf(line, sizeof(line), "Q:%d msgs", pendingMessages);
    } else {
        snprintf(line, sizeof(line), "IDLE");
    }
    addLine(line, 50);

    // Animate on screen transition, otherwise just display
    if (isTransition) {
        animateScreen();
    } else {
        // Just draw lines without animation (fast update)
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

    // Begin screen
    beginScreen(SCREEN_MESSAGE);

    // Use larger text for messages
    display->setTextSize(2);

    // Calculate centered position
    int16_t x1, y1;
    uint16_t w, h;
    display->getTextBounds(message, 0, 0, &x1, &y1, &w, &h);
    int centerY = (SCREEN_HEIGHT - h) / 2;

    // Add message as a line
    addLine(message, centerY);

    // Animate on transition
    if (isTransition) {
        // For centered text, sweep from center position
        int textWidth = strlen(message) * 12;  // 12 pixels per char at size 2
        sweepCursorAcrossLine(centerY, textWidth, 60);

        // Show message centered
        display->setCursor((SCREEN_WIDTH - w) / 2, centerY);
        display->print(message);
        display->display();
    } else {
        // Just show without animation
        display->setCursor((SCREEN_WIDTH - w) / 2, centerY);
        display->print(message);
        display->display();
    }

    if (duration > 0) {
        delay(duration);
    }
}

void DisplayManager::clear() {
    if (!initialized) return;
    display->clearDisplay();
    display->display();
}

void DisplayManager::drawSIMSLogo() {
    // Draw "SIMS" in large text
    display->setTextSize(3);
    display->setCursor(20, 10);
    display->print("SIMS");

    // Subtitle
    display->setTextSize(1);
    centerText("Mesh Network", 38);
    centerText("Incident Reporting", 48);
}

void DisplayManager::drawProgressBar(int x, int y, int width, int height, int percent) {
    // Draw border
    display->drawRect(x, y, width, height, SSD1306_WHITE);

    // Fill progress
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

// Centralized animation system (aivos-watch style)

bool DisplayManager::isScreenTransition(ScreenType newScreen) {
    return currentScreen != newScreen;
}

void DisplayManager::beginScreen(ScreenType screenType) {
    if (!initialized) return;

    // Only animate on screen transitions
    bool shouldAnimate = isScreenTransition(screenType);

    currentScreen = screenType;
    screenLineCount = 0;

    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);

    // Small delay on transitions (matches aivos-watch initial delay)
    if (shouldAnimate) {
        delay(50);
    }
}

void DisplayManager::addLine(const char* text, int y) {
    if (!initialized) return;
    if (screenLineCount >= MAX_SCREEN_LINES) return;

    // Store line info for animation
    strncpy(screenLines[screenLineCount], text, 31);
    screenLines[screenLineCount][31] = '\0';
    screenLineYPos[screenLineCount] = y;
    screenLineCount++;
}

void DisplayManager::animateScreen() {
    if (!initialized) return;

    // Animate each line with cursor sweep
    for (int i = 0; i < screenLineCount; i++) {
        revealLine(screenLines[i], screenLineYPos[i]);
    }
}

// Animation helpers for aivos-watch style boot screen

void DisplayManager::sweepCursorAcrossLine(int y, int width, int durationMs) {
    if (!initialized) return;

    const int cursorSize = 12;  // 12x12 cursor (proportional to aivos-watch 20x20 on 240x240)
    const int frameDelay = 5;   // 5ms per frame = smooth animation

    unsigned long startTime = millis();
    int lastX = 0;

    while (millis() - startTime < durationMs) {
        unsigned long elapsed = millis() - startTime;

        // Linear interpolation: cursor position based on elapsed time
        int cursorX = (width * elapsed) / durationMs;

        // Only redraw if cursor moved (reduces I2C traffic)
        if (cursorX != lastX && cursorX < width) {
            // Clear previous cursor
            if (lastX > 0) {
                display->fillRect(lastX, y, cursorSize, cursorSize, SSD1306_BLACK);
            }

            // Draw cursor at new position
            display->fillRect(cursorX, y, cursorSize, cursorSize, SSD1306_WHITE);
            display->display();

            lastX = cursorX;
        }

        delay(frameDelay);
    }

    // Clear final cursor
    display->fillRect(lastX, y, cursorSize, cursorSize, SSD1306_BLACK);
    display->display();
}

void DisplayManager::revealLine(const char* text, int y) {
    if (!initialized) return;

    // Calculate text width (6 pixels per character)
    int textWidth = strlen(text) * 6;

    // Sweep cursor across line (60ms = aivos-watch timing)
    sweepCursorAcrossLine(y, textWidth, 60);

    // Reveal text after cursor sweep
    display->setCursor(0, y);
    display->print(text);
    display->display();

    // Pause before next line (40ms = aivos-watch timing)
    delay(40);
}

void DisplayManager::drawAnimatedLogo() {
    if (!initialized) return;

    for (int i = 0; i < LOGO_LINES; i++) {
        int yPos = LOGO_START_Y + (i * 10);
        revealLine(BOOT_LOGO[i], yPos);
    }
}
