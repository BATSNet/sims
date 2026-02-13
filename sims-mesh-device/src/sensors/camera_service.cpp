/**
 * Camera Service Implementation
 */

#include "sensors/camera_service.h"

// Default JPEG quality if not defined
#ifndef CAMERA_JPEG_QUALITY
#define CAMERA_JPEG_QUALITY 20
#endif

CameraService::CameraService()
    : initialized(false), jpegQuality(CAMERA_JPEG_QUALITY) {
    #ifdef HAS_CAMERA
    frameBuffer = nullptr;
    #endif
}

CameraService::~CameraService() {
    clearImage();
}

bool CameraService::begin() {
    #ifdef HAS_CAMERA
    Serial.println("[Camera] Initializing camera...");

    // Camera configuration for XIAO ESP32S3 Sense
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = 15;
    config.pin_d1 = 17;
    config.pin_d2 = 18;
    config.pin_d3 = 16;
    config.pin_d4 = 14;
    config.pin_d5 = 12;
    config.pin_d6 = 11;
    config.pin_d7 = 48;
    config.pin_xclk = 10;
    config.pin_pclk = 13;
    config.pin_vsync = 38;
    config.pin_href = 47;
    config.pin_sscb_sda = 40;
    config.pin_sscb_scl = 39;
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = CAMERA_PIXEL_FORMAT;
    config.frame_size = CAMERA_FRAME_SIZE;
    config.jpeg_quality = jpegQuality;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] Init failed with error 0x%x\n", err);
        return false;
    }

    // Adjust sensor settings for better quality
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 0);     // -2 to 2
        s->set_contrast(s, 0);       // -2 to 2
        s->set_saturation(s, 0);     // -2 to 2
        s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect)
        s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
        s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
        s->set_wb_mode(s, 0);        // 0 to 4
        s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
        s->set_aec2(s, 0);           // 0 = disable , 1 = enable
        s->set_ae_level(s, 0);       // -2 to 2
        s->set_aec_value(s, 300);    // 0 to 1200
        s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
        s->set_agc_gain(s, 0);       // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
        s->set_bpc(s, 0);            // 0 = disable , 1 = enable
        s->set_wpc(s, 1);            // 0 = disable , 1 = enable
        s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
        s->set_lenc(s, 1);           // 0 = disable , 1 = enable
        s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
        s->set_vflip(s, 0);          // 0 = disable , 1 = enable
        s->set_dcw(s, 1);            // 0 = disable , 1 = enable
        s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
    }

    initialized = true;
    Serial.printf("[Camera] Camera initialized (JPEG quality: %d)\n", jpegQuality);
    return true;
    #else
    Serial.println("[Camera] Camera not available on this board");
    return false;
    #endif
}

bool CameraService::captureImage() {
    #ifdef HAS_CAMERA
    if (!initialized) {
        return false;
    }

    // Clear previous image
    clearImage();

    Serial.println("[Camera] Capturing image...");

    // Capture frame
    frameBuffer = esp_camera_fb_get();
    if (!frameBuffer) {
        Serial.println("[Camera] Failed to capture image");
        return false;
    }

    Serial.printf("[Camera] Image captured: %d bytes\n", frameBuffer->len);
    return true;
    #else
    return false;
    #endif
}

uint8_t* CameraService::getImageData() {
    #ifdef HAS_CAMERA
    if (frameBuffer) {
        return frameBuffer->buf;
    }
    #endif
    return nullptr;
}

size_t CameraService::getImageSize() {
    #ifdef HAS_CAMERA
    if (frameBuffer) {
        return frameBuffer->len;
    }
    #endif
    return 0;
}

bool CameraService::hasImage() {
    #ifdef HAS_CAMERA
    return frameBuffer != nullptr;
    #else
    return false;
    #endif
}

void CameraService::clearImage() {
    #ifdef HAS_CAMERA
    if (frameBuffer) {
        esp_camera_fb_return(frameBuffer);
        frameBuffer = nullptr;
    }
    #endif
}

void CameraService::setQuality(int quality) {
    jpegQuality = constrain(quality, 1, 63);
    #ifdef HAS_CAMERA
    if (initialized) {
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_quality(s, jpegQuality);
        }
    }
    #endif
}

void CameraService::setFrameSize(int frameSize) {
    #ifdef HAS_CAMERA
    if (initialized) {
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_framesize(s, (framesize_t)frameSize);
        }
    }
    #endif
}
