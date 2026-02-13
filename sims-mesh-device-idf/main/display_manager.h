/**
 * Display Manager
 * Handles OLED display rendering for boot screen and status
 * ESP-IDF version using SSD1306 driver
 *
 * Features:
 * - Field-based partial updates (no full-screen flicker)
 * - Top bar with status icon, BLE count, battery icon
 * - Non-blocking TX notification
 * - Idle screen after timeout
 * - Display on/off control
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

// Field IDs for partial update tracking
enum DisplayField {
    FIELD_STATUS_ICON = 0,
    FIELD_STATUS_TEXT,
    FIELD_BLE_COUNT,
    FIELD_BATTERY_ICON,
    FIELD_GPS,
    FIELD_MESH,
    FIELD_LORA,
    FIELD_QUEUE,
    FIELD_COUNT
};

// Screen types
enum ScreenType {
    SCREEN_NONE,
    SCREEN_BOOT,
    SCREEN_INIT,
    SCREEN_STATUS,
    SCREEN_MESSAGE,
    SCREEN_IDLE,
    SCREEN_SLEEP
};

// Status icon style
enum StatusIcon {
    ICON_DISCONNECTED,  // Empty circle
    ICON_IDLE,          // Half-filled circle
    ICON_ACTIVE         // Filled circle
};

class DisplayManager {
public:
    DisplayManager();

    // Initialize display
    bool begin();

    // Boot screens (full redraw, blocking - used only at startup)
    void showBootScreen();
    void showInitProgress(const char* step, int percent);

    // Runtime status display (partial updates)
    void updateStatus(bool gpsValid, int satellites, int meshNodes,
                     int pendingMessages, int batteryPercent,
                     bool bleConnected, int bleClients,
                     int loraRSSI, float loraSNR,
                     int packetsReceived);

    // Message display (blocking - for init-time errors only)
    void showMessage(const char* message, int duration = 2000);

    // Non-blocking TX indicator
    void notifyTx(int durationMs = 1500);

    // Idle screen
    void showIdleScreen(int batteryPercent);

    // Sleep screen (brief text before deep sleep)
    void showSleepScreen();

    // Display power
    void setScreenPower(bool on);
    bool isDisplayOn();

    // Activity tracking for idle timeout
    void registerActivity();
    bool isIdle(unsigned long timeoutMs);

    // Clear display
    void clear();

private:
    SSD1306* display;
    bool initialized;
    bool screenOn;

    // Screen state tracking
    ScreenType currentScreen;
    char screenLines[MAX_SCREEN_LINES][32];
    int screenLineYPos[MAX_SCREEN_LINES];
    int screenLineCount;

    // Field-based partial update tracking
    char prevFields[FIELD_COUNT][32];
    bool statusDrawn;  // true after first full status draw

    // TX notification state
    bool txActive;
    unsigned long txStartTime;
    int txDurationMs;

    // Activity tracking
    unsigned long lastActivityTime;

    // Helper methods
    void drawProgressBar(int x, int y, int width, int height, int percent);
    void centerText(const char* text, int y);

    // Status screen drawing
    void drawTopBar(StatusIcon icon, const char* statusText,
                    bool bleConnected, int bleClients, int batteryPercent);
    void drawStatusIcon(StatusIcon icon, int16_t x, int16_t y);
    void drawBatteryIcon(int16_t x, int16_t y, int percent);
    void updateField(DisplayField field, int16_t x, int16_t y,
                     const char* newText);

    // Animation system (boot/init only)
    void beginScreen(ScreenType screenType);
    void addLine(const char* text, int y);
    void animateScreen();
    bool isScreenTransition(ScreenType newScreen);

    // Animation helpers
    void sweepCursorAcrossLine(int y, int width, int durationMs);
    void revealLine(const char* text, int y);

    // Timing helper
    unsigned long millis();
};

#endif // DISPLAY_MANAGER_H
