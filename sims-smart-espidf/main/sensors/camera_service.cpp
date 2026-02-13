/**
 * Camera Service Implementation - ESP-IDF Port
 *
 * Uses esp_camera component directly (already ESP-IDF compatible).
 * Pin configuration from working camera_audio_web test.
 */

#include "sensors/camera_service.h"
#include "esp_log.h"

static const char *TAG = "Camera";

CameraService::CameraService()
    : initialized(false), currentFrame(nullptr) {
}

CameraService::~CameraService() {
    if (currentFrame) {
        esp_camera_fb_return(currentFrame);
        currentFrame = nullptr;
    }
    if (initialized) {
        esp_camera_deinit();
    }
}

bool CameraService::begin() {
    ESP_LOGI(TAG, "Initializing camera...");

    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAM_PIN_D0;
    config.pin_d1 = CAM_PIN_D1;
    config.pin_d2 = CAM_PIN_D2;
    config.pin_d3 = CAM_PIN_D3;
    config.pin_d4 = CAM_PIN_D4;
    config.pin_d5 = CAM_PIN_D5;
    config.pin_d6 = CAM_PIN_D6;
    config.pin_d7 = CAM_PIN_D7;
    config.pin_xclk = CAM_PIN_XCLK;
    config.pin_pclk = CAM_PIN_PCLK;
    config.pin_vsync = CAM_PIN_VSYNC;
    config.pin_href = CAM_PIN_HREF;
    config.pin_sccb_sda = CAM_PIN_SIOD;
    config.pin_sccb_scl = CAM_PIN_SIOC;
    config.pin_pwdn = CAM_PIN_PWDN;
    config.pin_reset = CAM_PIN_RESET;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = CAMERA_PIXEL_FORMAT;
    config.frame_size = CAMERA_FRAME_SIZE;
    config.jpeg_quality = CAMERA_JPEG_QUALITY;
    config.fb_count = CAMERA_FB_COUNT;
    config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s (0x%x)", esp_err_to_name(err), err);
        return false;
    }

    initialized = true;
    ESP_LOGI(TAG, "Camera initialized successfully");
    return true;
}

bool CameraService::capture() {
    if (!initialized) {
        ESP_LOGE(TAG, "Camera not initialized");
        return false;
    }

    // Release previous frame if not yet released
    if (currentFrame) {
        esp_camera_fb_return(currentFrame);
        currentFrame = nullptr;
    }

    ESP_LOGI(TAG, "Capturing image...");
    currentFrame = esp_camera_fb_get();
    if (!currentFrame) {
        ESP_LOGE(TAG, "Capture failed - no frame buffer");
        return false;
    }

    ESP_LOGI(TAG, "Image captured: %d bytes (%dx%d)",
             currentFrame->len, currentFrame->width, currentFrame->height);
    return true;
}

void CameraService::release() {
    if (currentFrame) {
        esp_camera_fb_return(currentFrame);
        currentFrame = nullptr;
    }
}

uint8_t* CameraService::getImageBuffer() {
    return currentFrame ? currentFrame->buf : nullptr;
}

size_t CameraService::getImageSize() {
    return currentFrame ? currentFrame->len : 0;
}

void CameraService::sleep() {
    // Power down camera sensor to save power
    // On XIAO ESP32S3, PWDN is -1 (not connected), so we can only deinit
    if (initialized) {
        if (currentFrame) {
            esp_camera_fb_return(currentFrame);
            currentFrame = nullptr;
        }
        esp_camera_deinit();
        initialized = false;
        ESP_LOGI(TAG, "Camera powered down");
    }
}

void CameraService::wake() {
    if (!initialized) {
        begin();
    }
}
