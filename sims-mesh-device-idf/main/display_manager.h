/**
 * Display Manager
 * Handles OLED display rendering for boot screen and status
 * ESP-IDF version using SSD1306 driver
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdint.h>
#include "ssd1306.h"
#include "config.h"

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

// Maximum lines per screen for animation
#define MAX_SCREEN_LINES 6

// Screen types for animation tracking
enum ScreenType {
    SCREEN_NONE,
    SCREEN_BOOT,
    SCREEN_INIT,
    SCREEN_STATUS,
    SCREEN_MESSAGE
};

class DisplayManager {
public:
    DisplayManager();

    // Initialize display
    bool begin();

    // Boot screens
    void showBootScreen();
    void showInitProgress(const char* step, int percent);

    // Runtime display
    void updateStatus(bool gpsValid, int satellites, int meshNodes,
                     int pendingMessages, int batteryPercent,
                     bool wifiConnected = false, int wifiRSSI = 0,
                     bool bleConnected = false,
                     int loraRSSI = 0, float loraSNR = 0.0,
                     int packetsReceived = 0);

    // Message display
    void showMessage(const char* message, int duration = 2000);

    // Clear display
    void clear();

private:
    SSD1306* display;
    bool initialized;
    unsigned long lastUpdate;

    // Screen state tracking
    ScreenType currentScreen;
    char screenLines[MAX_SCREEN_LINES][32];
    int screenLineYPos[MAX_SCREEN_LINES];
    int screenLineCount;

    // Helper methods
    void drawSIMSLogo();
    void drawProgressBar(int x, int y, int width, int height, int percent);
    void centerText(const char* text, int y);

    // Animation system
    void beginScreen(ScreenType screenType);
    void addLine(const char* text, int y);
    void animateScreen();
    bool isScreenTransition(ScreenType newScreen);

    // Animation helpers
    void sweepCursorAcrossLine(int y, int width, int durationMs);
    void revealLine(const char* text, int y);
    void drawAnimatedLogo();

    // Timing helper
    unsigned long millis();
};

#endif // DISPLAY_MANAGER_H
