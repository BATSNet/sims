/**
 * Camera Service Implementation - ESP-IDF Port
 *
 * Captures grayscale QVGA frames from OV2640, converts to RGB888,
 * and encodes as WebP for compact LoRa transmission.
 */

#include "sensors/camera_service.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "webp/encode.h"

static const char *TAG = "Camera";

CameraService::CameraService()
    : initialized(false),
      encodedImage(nullptr), encodedImageSize(0) {
}

CameraService::~CameraService() {
    release();
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

    // Release previous encoded image
    release();

    ESP_LOGI(TAG, "Capturing image...");
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Capture failed - no frame buffer");
        return false;
    }

    int width = fb->width;
    int height = fb->height;
    size_t pixelCount = width * height;
    ESP_LOGI(TAG, "Raw grayscale frame: %d bytes (%dx%d)", fb->len, width, height);

    // Allocate RGB888 buffer in PSRAM for WebP encoder input
    size_t rgbSize = pixelCount * 3;
    uint8_t* rgb = (uint8_t*)heap_caps_malloc(rgbSize, MALLOC_CAP_SPIRAM);
    if (!rgb) {
        ESP_LOGE(TAG, "Failed to allocate RGB buffer (%d bytes)", rgbSize);
        esp_camera_fb_return(fb);
        return false;
    }

    // Convert grayscale to RGB888 (R=G=B=gray for each pixel)
    const uint8_t* gray = fb->buf;
    for (size_t i = 0; i < pixelCount; i++) {
        rgb[i * 3]     = gray[i];
        rgb[i * 3 + 1] = gray[i];
        rgb[i * 3 + 2] = gray[i];
    }

    // Return camera frame buffer - no longer needed
    esp_camera_fb_return(fb);

    // Encode to WebP
    uint8_t* webpOut = nullptr;
    size_t webpSize = WebPEncodeRGB(rgb, width, height, width * 3,
                                     (float)CAMERA_WEBP_QUALITY, &webpOut);

    // Free RGB buffer
    heap_caps_free(rgb);

    if (webpSize == 0 || !webpOut) {
        ESP_LOGE(TAG, "WebP encoding failed");
        return false;
    }

    encodedImage = webpOut;
    encodedImageSize = webpSize;
    ESP_LOGI(TAG, "WebP encoded: %d bytes (%.1f%% of raw)",
             encodedImageSize, (float)encodedImageSize / pixelCount * 100.0f);
    return true;
}

void CameraService::release() {
    if (encodedImage) {
        WebPFree(encodedImage);
        encodedImage = nullptr;
        encodedImageSize = 0;
    }
}

uint8_t* CameraService::getImageBuffer() {
    return encodedImage;
}

size_t CameraService::getImageSize() {
    return encodedImageSize;
}

void CameraService::sleep() {
    if (initialized) {
        release();
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
