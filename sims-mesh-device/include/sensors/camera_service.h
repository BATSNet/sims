/**
 * Camera Service
 * Handles OV2640 camera capture and JPEG compression
 * Only available on XIAO ESP32S3 Sense
 */

#ifndef CAMERA_SERVICE_H
#define CAMERA_SERVICE_H

#include <Arduino.h>
#include "../config.h"

#ifdef HAS_CAMERA
#include "esp_camera.h"
#endif

class CameraService {
public:
    CameraService();
    ~CameraService();

    // Initialize camera
    bool begin();

    // Capture image (JPEG)
    bool captureImage();

    // Get captured image data
    uint8_t* getImageData();
    size_t getImageSize();

    // Check if image is available
    bool hasImage();

    // Clear image buffer
    void clearImage();

    // Set JPEG quality (1-63, lower = higher compression)
    void setQuality(int quality);

    // Set frame size
    void setFrameSize(int frameSize);

private:
    #ifdef HAS_CAMERA
    camera_fb_t* frameBuffer;
    #endif
    bool initialized;
    int jpegQuality;
};

#endif // CAMERA_SERVICE_H
