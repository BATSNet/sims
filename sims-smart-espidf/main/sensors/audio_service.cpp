/**
 * Audio Service Implementation - ESP-IDF Port
 *
 * I2S PDM microphone driver.
 * Configuration from working camera_audio_web test.
 */

#include "sensors/audio_service.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "Audio";

AudioService::AudioService()
    : initialized(false), i2sPort(I2S_NUM_0) {
}

AudioService::~AudioService() {
    end();
}

bool AudioService::begin() {
    ESP_LOGI(TAG, "Initializing I2S PDM microphone...");

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = AUDIO_BUFFER_COUNT,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    esp_err_t err = i2s_driver_install(i2sPort, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_PDM_CLK_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_PDM_DATA_PIN
    };

    err = i2s_set_pin(i2sPort, &pin_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S set pin failed: %s", esp_err_to_name(err));
        i2s_driver_uninstall(i2sPort);
        return false;
    }

    // Set PDM downsample rate for better quality
    i2s_set_pdm_rx_down_sample(i2sPort, I2S_PDM_DSR_8S);

    initialized = true;
    ESP_LOGI(TAG, "PDM microphone initialized (CLK=%d, DATA=%d, %dHz)",
             MIC_PDM_CLK_PIN, MIC_PDM_DATA_PIN, AUDIO_SAMPLE_RATE);
    return true;
}

void AudioService::end() {
    if (initialized) {
        i2s_driver_uninstall(i2sPort);
        initialized = false;
        ESP_LOGI(TAG, "I2S driver uninstalled");
    }
}

size_t AudioService::read(int16_t *buffer, size_t maxSamples) {
    if (!initialized) {
        return 0;
    }

    size_t bytesRead = 0;
    size_t bytesToRead = maxSamples * sizeof(int16_t);

    esp_err_t err = i2s_read(i2sPort, buffer, bytesToRead, &bytesRead, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(err));
        return 0;
    }

    return bytesRead / sizeof(int16_t);
}

uint8_t* AudioService::record(uint32_t durationMs, size_t *outSize) {
    if (!initialized) {
        ESP_LOGE(TAG, "Audio not initialized");
        *outSize = 0;
        return nullptr;
    }

    // Calculate buffer size
    size_t totalSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    size_t bufferSize = totalSamples * sizeof(int16_t);

    ESP_LOGI(TAG, "Recording %lu ms (%d samples, %d bytes)...",
             durationMs, totalSamples, bufferSize);

    // Allocate buffer in PSRAM for large recordings
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        // Fall back to internal RAM
        buffer = (uint8_t*)malloc(bufferSize);
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate recording buffer (%d bytes)", bufferSize);
            *outSize = 0;
            return nullptr;
        }
        ESP_LOGW(TAG, "Using internal RAM for recording buffer");
    }

    // Record audio
    size_t bytesRecorded = 0;
    size_t bytesRead = 0;

    while (bytesRecorded < bufferSize) {
        size_t bytesToRead = 512;
        if (bytesRecorded + bytesToRead > bufferSize) {
            bytesToRead = bufferSize - bytesRecorded;
        }

        esp_err_t err = i2s_read(i2sPort, buffer + bytesRecorded, bytesToRead,
                                 &bytesRead, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S read error during recording: %s", esp_err_to_name(err));
            break;
        }
        bytesRecorded += bytesRead;

        // Log progress every second
        if (bytesRecorded % (AUDIO_SAMPLE_RATE * 2) == 0) {
            ESP_LOGI(TAG, "Recording: %d%%", (int)((bytesRecorded * 100) / bufferSize));
        }
    }

    *outSize = bytesRecorded;
    ESP_LOGI(TAG, "Recording complete: %d bytes", bytesRecorded);
    return buffer;
}
