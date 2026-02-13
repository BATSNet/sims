/**
 * Camera Service - ESP-IDF Port
 *
 * OV2640 camera on XIAO ESP32S3 Sense
 * Uses esp_camera component (already ESP-IDF native)
 */

#ifndef CAMERA_SERVICE_H
#define CAMERA_SERVICE_H

#include "esp_camera.h"
#include "config.h"

class CameraService {
public:
    CameraService();
    ~CameraService();

    // Initialize camera
    bool begin();

    // Capture a JPEG image
    // Returns pointer to JPEG buffer (caller must call release() when done)
    bool capture();

    // Release the last captured frame buffer
    void release();

    // Get captured image data
    uint8_t* getImageBuffer();
    size_t getImageSize();

    // Check if camera is initialized
    bool isInitialized() const { return initialized; }

    // Power management
    void sleep();
    void wake();

private:
    bool initialized;
    camera_fb_t* currentFrame;
};

#endif // CAMERA_SERVICE_H
