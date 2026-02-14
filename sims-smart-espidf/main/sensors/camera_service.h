/**
 * Camera Service - ESP-IDF Port
 *
 * OV2640 camera on XIAO ESP32S3 Sense
 * Captures grayscale frames and encodes to WebP for LoRa transmission.
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

    // Capture and encode image to WebP
    bool capture();

    // Release the last encoded image
    void release();

    // Get encoded WebP image data
    uint8_t* getImageBuffer();
    size_t getImageSize();

    // Check if camera is initialized
    bool isInitialized() const { return initialized; }

    // Power management
    void sleep();
    void wake();

private:
    bool initialized;
    uint8_t* encodedImage;
    size_t encodedImageSize;
};

#endif // CAMERA_SERVICE_H
