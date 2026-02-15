/**
 * Display Manager for SIMS-SMART Device
 *
 * Manages OLED display screens for the incident reporting workflow.
 * Different from sims-mesh-device DisplayManager - this one has
 * screens for recording, transcription, and incident upload flow.
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "display/ssd1306.h"
#include <stdint.h>

class DisplayManager {
public:
    // Display screens
    enum Screen {
        SCREEN_IDLE,          // Status bar + "Say Hi ESP"
        SCREEN_RECORDING,     // "Speak now..." + animation (legacy, unused)
        SCREEN_LISTENING,     // "Listening for command..."
        SCREEN_PREVIEW,       // Show command words
        SCREEN_CAPTURING,     // "Taking photo..."
        SCREEN_SENDING,       // "Sending via mesh/WiFi..."
        SCREEN_SUCCESS,       // "Sent!" + incident ID
        SCREEN_ERROR,         // Error message
        SCREEN_MESH,          // Last mesh message
        SCREEN_COUNT
    };

    DisplayManager();
    ~DisplayManager();

    // Initialize the OLED display
    bool begin(int sdaPin, int sclPin, uint8_t addr = 0x3C);

    // Show a specific screen
    void showScreen(Screen screen);

    // Update dynamic content without full redraw
    void setStatusFlags(bool wifi, bool gps, bool mesh, int batteryPct);
    void setTranscription(const char* text);
    void setIncidentId(const char* id);
    void setErrorMessage(const char* msg);
    void setMeshMessage(const char* from, const char* msg);

    // Cycle through screens (for mode button)
    void cycleMode();

    // Get current screen
    Screen getCurrentScreen() const { return currentScreen_; }

    // Check if display is available
    bool isAvailable() const { return initialized_; }

private:
    SSD1306 oled_;
    bool initialized_;
    Screen currentScreen_;

    // Cached status flags
    bool wifiConnected_;
    bool gpsFix_;
    bool meshConnected_;
    int batteryPct_;

    // Cached content
    char transcription_[256];
    char incidentId_[64];
    char errorMsg_[128];
    char meshFrom_[32];
    char meshMsg_[128];

    // Drawing helpers
    void drawStatusBar();
    void drawIdleScreen();
    void drawRecordingScreen();
    void drawListeningScreen();
    void drawPreviewScreen();
    void drawCapturingScreen();
    void drawSendingScreen();
    void drawSuccessScreen();
    void drawErrorScreen();
    void drawMeshScreen();
    void drawCentered(const char* text, int y, uint8_t size = 1);
    void drawWrapped(const char* text, int x, int y, int maxWidth);
};

#endif // DISPLAY_MANAGER_H
