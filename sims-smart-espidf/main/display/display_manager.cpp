/**
 * Display Manager Implementation for SIMS-SMART Device
 */

#include "display/display_manager.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "Display";

DisplayManager::DisplayManager()
    : oled_(OLED_WIDTH, OLED_HEIGHT, -1),
      initialized_(false),
      currentScreen_(SCREEN_IDLE),
      wifiConnected_(false),
      gpsFix_(false),
      meshConnected_(false),
      batteryPct_(0) {
    transcription_[0] = '\0';
    incidentId_[0] = '\0';
    errorMsg_[0] = '\0';
    meshFrom_[0] = '\0';
    meshMsg_[0] = '\0';
}

DisplayManager::~DisplayManager() {}

bool DisplayManager::begin(int sdaPin, int sclPin, uint8_t addr) {
    ESP_LOGI(TAG, "Initializing OLED display (SDA=%d, SCL=%d, addr=0x%02X)",
             sdaPin, sclPin, addr);

    if (!oled_.begin(addr, sdaPin, sclPin)) {
        ESP_LOGE(TAG, "OLED init failed");
        return false;
    }

    initialized_ = true;

    // Show boot screen
    oled_.clearDisplay();
    oled_.setTextColor(SSD1306_WHITE);
    drawCentered("SIMS-SMART", 10, 2);
    drawCentered("Initializing...", 40);
    oled_.display();

    ESP_LOGI(TAG, "OLED display initialized");
    return true;
}

void DisplayManager::showScreen(Screen screen) {
    if (!initialized_) return;

    currentScreen_ = screen;
    oled_.clearDisplay();

    switch (screen) {
        case SCREEN_IDLE:        drawIdleScreen(); break;
        case SCREEN_RECORDING:   drawRecordingScreen(); break;
        case SCREEN_LISTENING:   drawListeningScreen(); break;
        case SCREEN_PREVIEW:     drawPreviewScreen(); break;
        case SCREEN_CAPTURING:   drawCapturingScreen(); break;
        case SCREEN_SENDING:     drawSendingScreen(); break;
        case SCREEN_SUCCESS:     drawSuccessScreen(); break;
        case SCREEN_ERROR:       drawErrorScreen(); break;
        case SCREEN_MESH:        drawMeshScreen(); break;
        default: break;
    }

    oled_.display();
}

void DisplayManager::setStatusFlags(bool wifi, bool gps, bool mesh, int batteryPct) {
    wifiConnected_ = wifi;
    gpsFix_ = gps;
    meshConnected_ = mesh;
    batteryPct_ = batteryPct;
}

void DisplayManager::setTranscription(const char* text) {
    if (text) {
        strncpy(transcription_, text, sizeof(transcription_) - 1);
        transcription_[sizeof(transcription_) - 1] = '\0';
    } else {
        transcription_[0] = '\0';
    }
}

void DisplayManager::setIncidentId(const char* id) {
    if (id) {
        strncpy(incidentId_, id, sizeof(incidentId_) - 1);
        incidentId_[sizeof(incidentId_) - 1] = '\0';
    } else {
        incidentId_[0] = '\0';
    }
}

void DisplayManager::setErrorMessage(const char* msg) {
    if (msg) {
        strncpy(errorMsg_, msg, sizeof(errorMsg_) - 1);
        errorMsg_[sizeof(errorMsg_) - 1] = '\0';
    } else {
        errorMsg_[0] = '\0';
    }
}

void DisplayManager::setMeshMessage(const char* from, const char* msg) {
    if (from) {
        strncpy(meshFrom_, from, sizeof(meshFrom_) - 1);
        meshFrom_[sizeof(meshFrom_) - 1] = '\0';
    }
    if (msg) {
        strncpy(meshMsg_, msg, sizeof(meshMsg_) - 1);
        meshMsg_[sizeof(meshMsg_) - 1] = '\0';
    }
}

void DisplayManager::cycleMode() {
    // Cycle between idle and mesh screens
    if (currentScreen_ == SCREEN_IDLE) {
        showScreen(SCREEN_MESH);
    } else {
        showScreen(SCREEN_IDLE);
    }
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

void DisplayManager::drawStatusBar() {
    // Top status bar (10px height)
    oled_.setTextSize(1);
    oled_.setTextColor(SSD1306_WHITE);

    int x = 0;

    // WiFi indicator
    oled_.setCursor(x, 0);
    oled_.print(wifiConnected_ ? "W" : "w");
    x += 8;

    // GPS indicator
    oled_.setCursor(x, 0);
    oled_.print(gpsFix_ ? "G" : "g");
    x += 8;

    // Mesh indicator
    oled_.setCursor(x, 0);
    oled_.print(meshConnected_ ? "M" : "m");
    x += 8;

    // Battery on right side
    if (batteryPct_ > 0) {
        char bat[16];
        snprintf(bat, sizeof(bat), "%d%%", batteryPct_);
        oled_.setCursor(128 - strlen(bat) * 6, 0);
        oled_.print(bat);
    }

    // Separator line
    oled_.drawLine(0, 9, 127, 9, SSD1306_WHITE);
}

void DisplayManager::drawIdleScreen() {
    drawStatusBar();

    drawCentered("SIMS-SMART", 16, 1);
    drawCentered("Say \"Hi ESP\"", 32, 1);
    drawCentered("or press ACTION", 44, 1);
}

void DisplayManager::drawRecordingScreen() {
    drawStatusBar();

    drawCentered("RECORDING", 16, 2);
    drawCentered("Speak now...", 40);

    // Draw a simple progress indicator
    oled_.drawRect(14, 52, 100, 8, SSD1306_WHITE);
}

void DisplayManager::drawListeningScreen() {
    drawStatusBar();

    drawCentered("LISTENING", 16, 2);
    drawCentered("Say commands...", 40);

    // Draw a simple progress indicator
    oled_.drawRect(14, 52, 100, 8, SSD1306_WHITE);
}

void DisplayManager::drawCapturingScreen() {
    drawStatusBar();

    drawCentered("CAPTURE", 20, 2);
    drawCentered("Taking photo...", 45);
}

void DisplayManager::drawPreviewScreen() {
    drawStatusBar();

    oled_.setCursor(0, 12);
    oled_.setTextSize(1);
    oled_.print("Preview:");

    // Show transcription text wrapped
    drawWrapped(transcription_, 0, 22, 128);
}

void DisplayManager::drawSendingScreen() {
    drawStatusBar();

    drawCentered("Sending", 20, 2);
    if (meshConnected_) {
        drawCentered("Via mesh network...", 45);
    } else {
        drawCentered("Via WiFi...", 45);
    }
}

void DisplayManager::drawSuccessScreen() {
    drawCentered("SENT!", 10, 2);

    if (strlen(incidentId_) > 0) {
        // Show truncated incident ID
        char shortId[22];
        strncpy(shortId, incidentId_, 21);
        shortId[21] = '\0';
        drawCentered("ID:", 35);
        drawCentered(shortId, 45);
    }
}

void DisplayManager::drawErrorScreen() {
    drawCentered("ERROR", 10, 2);

    if (strlen(errorMsg_) > 0) {
        drawWrapped(errorMsg_, 0, 35, 128);
    }
}

void DisplayManager::drawMeshScreen() {
    drawStatusBar();

    oled_.setCursor(0, 12);
    oled_.setTextSize(1);
    oled_.print("Mesh Messages:");

    if (strlen(meshMsg_) > 0) {
        // Show sender
        if (strlen(meshFrom_) > 0) {
            oled_.setCursor(0, 24);
            oled_.print("From: ");
            oled_.print(meshFrom_);
        }
        // Show message wrapped
        drawWrapped(meshMsg_, 0, 34, 128);
    } else {
        drawCentered("No messages", 35);
    }
}

void DisplayManager::drawCentered(const char* text, int y, uint8_t size) {
    oled_.setTextSize(size);
    oled_.setTextColor(SSD1306_WHITE);

    int16_t x1, y1;
    uint16_t w, h;
    oled_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

    int x = (128 - (int)w) / 2;
    if (x < 0) x = 0;

    oled_.setCursor(x, y);
    oled_.print(text);
}

void DisplayManager::drawWrapped(const char* text, int x, int y, int maxWidth) {
    if (!text || strlen(text) == 0) return;

    oled_.setTextSize(1);
    oled_.setTextColor(SSD1306_WHITE);

    int charsPerLine = maxWidth / 6;  // 6px per char at size 1
    int len = strlen(text);
    int pos = 0;
    int lineY = y;

    while (pos < len && lineY < 64 - 7) {
        // Find how many chars fit on this line
        int lineLen = charsPerLine;
        if (pos + lineLen > len) lineLen = len - pos;

        // Try to break at space if not at end
        if (pos + lineLen < len) {
            int lastSpace = -1;
            for (int i = lineLen - 1; i >= 0; i--) {
                if (text[pos + i] == ' ') {
                    lastSpace = i;
                    break;
                }
            }
            if (lastSpace > 0) lineLen = lastSpace + 1;
        }

        // Draw this line
        char lineBuf[32];
        int copyLen = lineLen;
        if (copyLen >= (int)sizeof(lineBuf)) copyLen = sizeof(lineBuf) - 1;
        memcpy(lineBuf, text + pos, copyLen);
        lineBuf[copyLen] = '\0';

        oled_.setCursor(x, lineY);
        oled_.print(lineBuf);

        pos += lineLen;
        lineY += 10; // line height
    }
}
