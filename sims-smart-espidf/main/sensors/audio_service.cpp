/**
 * Audio Service Implementation - ESP-IDF Port
 *
 * I2S PDM microphone driver using new ESP-IDF v5.x API.
 * The legacy i2s.h driver does not work for PDM RX on ESP-IDF v5.5.
 */

#include "sensors/audio_service.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <cmath>

static const char *TAG = "Audio";

static unsigned long millis_now(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

AudioService::AudioService()
    : initialized(false), rxHandle(NULL) {
}

AudioService::~AudioService() {
    end();
}

bool AudioService::begin() {
    ESP_LOGI(TAG, "Initializing I2S PDM microphone (new driver)...");

    // Step 1: Create RX channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 512;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rxHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        return false;
    }

    // Step 2: Configure PDM RX mode
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = (gpio_num_t)MIC_PDM_CLK_PIN,
            .din = (gpio_num_t)MIC_PDM_DATA_PIN,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    err = i2s_channel_init_pdm_rx_mode(rxHandle, &pdm_rx_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_pdm_rx_mode failed: %s", esp_err_to_name(err));
        i2s_del_channel(rxHandle);
        rxHandle = NULL;
        return false;
    }

    // Step 3: Enable the channel
    err = i2s_channel_enable(rxHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        i2s_del_channel(rxHandle);
        rxHandle = NULL;
        return false;
    }

    // Verify with a test read
    int16_t testBuf[64];
    size_t testBytes = 0;
    ESP_LOGI(TAG, "Testing I2S PDM read (1s timeout)...");
    err = i2s_channel_read(rxHandle, testBuf, sizeof(testBuf), &testBytes, pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "I2S test read: err=%s, bytes=%d", esp_err_to_name(err), (int)testBytes);

    if (testBytes == 0) {
        ESP_LOGW(TAG, "No PDM data received - check mic hardware");
    } else {
        // Show a few sample values to verify data
        ESP_LOGI(TAG, "First samples: %d, %d, %d, %d",
                 testBuf[0], testBuf[1], testBuf[2], testBuf[3]);
    }

    initialized = true;
    ESP_LOGI(TAG, "PDM microphone initialized (CLK=%d, DATA=%d, %dHz)",
             MIC_PDM_CLK_PIN, MIC_PDM_DATA_PIN, AUDIO_SAMPLE_RATE);
    return true;
}

void AudioService::end() {
    if (initialized && rxHandle) {
        i2s_channel_disable(rxHandle);
        i2s_del_channel(rxHandle);
        rxHandle = NULL;
        initialized = false;
        ESP_LOGI(TAG, "I2S PDM driver stopped");
    }
}

size_t AudioService::read(int16_t *buffer, size_t maxSamples) {
    if (!initialized || !rxHandle) {
        return 0;
    }

    size_t bytesRead = 0;
    size_t bytesToRead = maxSamples * sizeof(int16_t);

    esp_err_t err = i2s_channel_read(rxHandle, buffer, bytesToRead, &bytesRead, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S read failed: %s", esp_err_to_name(err));
        return 0;
    }

    return bytesRead / sizeof(int16_t);
}

uint8_t* AudioService::record(uint32_t maxDurationMs, size_t *outSize, uint32_t silenceTimeoutMs) {
    if (!initialized || !rxHandle) {
        ESP_LOGE(TAG, "Audio not initialized");
        *outSize = 0;
        return nullptr;
    }

    // Calculate max buffer size
    size_t totalSamples = (AUDIO_SAMPLE_RATE * maxDurationMs) / 1000;
    size_t bufferSize = totalSamples * sizeof(int16_t);

    ESP_LOGI(TAG, "Recording up to %lu ms (silence timeout %lu ms)...",
             maxDurationMs, silenceTimeoutMs);

    // Allocate buffer in PSRAM for large recordings
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) {
        // Fall back to internal RAM
        buffer = (uint8_t*)malloc(bufferSize);
        if (!buffer) {
            ESP_LOGE(TAG, "Failed to allocate recording buffer (%d bytes)", (int)bufferSize);
            *outSize = 0;
            return nullptr;
        }
        ESP_LOGW(TAG, "Using internal RAM for recording buffer");
    }

    // Silence detection parameters
    const int SILENCE_RMS_THRESHOLD = 2000;  // Ambient ~1400, speech well above this
    const uint32_t MIN_RECORD_MS = 1000;     // Minimum recording before silence-stop

    size_t bytesRecorded = 0;
    size_t bytesRead = 0;
    bool speechDetected = false;
    unsigned long lastSoundTime = millis_now();
    unsigned long recordStartTime = millis_now();

    while (bytesRecorded < bufferSize) {
        size_t bytesToRead = 512;
        if (bytesRecorded + bytesToRead > bufferSize) {
            bytesToRead = bufferSize - bytesRecorded;
        }

        esp_err_t err = i2s_channel_read(rxHandle, buffer + bytesRecorded, bytesToRead,
                                         &bytesRead, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S read error during recording: %s", esp_err_to_name(err));
            break;
        }
        bytesRecorded += bytesRead;

        // Calculate RMS for this chunk
        int16_t* samples = (int16_t*)(buffer + bytesRecorded - bytesRead);
        size_t numSamples = bytesRead / sizeof(int16_t);
        int64_t sum = 0;
        for (size_t i = 0; i < numSamples; i++) {
            sum += (int64_t)samples[i] * samples[i];
        }
        int rms = (numSamples > 0) ? (int)sqrt((double)sum / numSamples) : 0;

        unsigned long now = millis_now();
        unsigned long elapsedMs = now - recordStartTime;

        if (rms > SILENCE_RMS_THRESHOLD) {
            lastSoundTime = now;
            if (!speechDetected) {
                speechDetected = true;
                ESP_LOGI(TAG, "Speech detected (rms=%d)", rms);
            }
        }

        // Check silence timeout after minimum recording time
        if (elapsedMs >= MIN_RECORD_MS) {
            unsigned long silentMs = now - lastSoundTime;
            if (silentMs >= silenceTimeoutMs) {
                ESP_LOGI(TAG, "Silence detected for %lu ms - stopping recording", silentMs);
                break;
            }
        }

        // Log progress every second
        if (bytesRecorded % (AUDIO_SAMPLE_RATE * 2) == 0) {
            unsigned long silentMs = millis_now() - lastSoundTime;
            ESP_LOGI(TAG, "Recording: %lu ms, rms=%d, silent=%lu ms, speech=%s",
                     elapsedMs, rms, silentMs, speechDetected ? "yes" : "no");
        }
    }

    unsigned long totalMs = millis_now() - recordStartTime;
    *outSize = bytesRecorded;
    ESP_LOGI(TAG, "Recording complete: %d bytes in %lu ms (speech=%s)",
             (int)bytesRecorded, totalMs, speechDetected ? "yes" : "no");
    return buffer;
}
