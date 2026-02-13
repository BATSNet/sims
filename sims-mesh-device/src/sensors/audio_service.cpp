/**
 * Audio Service Implementation
 */

#include "sensors/audio_service.h"

// I2S configuration
#define I2S_PORT I2S_NUM_0
#define I2S_SAMPLE_RATE AUDIO_SAMPLE_RATE
#define I2S_SAMPLE_BITS I2S_BITS_PER_SAMPLE_16BIT
#define I2S_CHANNELS 1
#define I2S_DMA_BUF_COUNT 4
#define I2S_DMA_BUF_LEN 1024

// I2S pins (INMP441 microphone)
#define I2S_SCK 41   // Serial Clock
#define I2S_WS 42    // Word Select (LRCLK)
#define I2S_SD 2     // Serial Data

AudioService::AudioService()
    : initialized(false), recording(false), audioBuffer(nullptr),
      audioBufferSize(0), audioDataSize(0), recordingStartTime(0) {
}

AudioService::~AudioService() {
    if (recording) {
        stopRecording();
    }
    freeBuffer();
}

bool AudioService::begin() {
    Serial.println("[Audio] Initializing audio service...");

    // I2S configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_SAMPLE_BITS,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = I2S_DMA_BUF_COUNT,
        .dma_buf_len = I2S_DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    // I2S pin configuration
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };

    // Install and start I2S driver
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Failed to install I2S driver: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[Audio] Failed to set I2S pins: %d\n", err);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    // Allocate audio buffer
    allocateBuffer();

    initialized = true;
    Serial.println("[Audio] Audio service initialized");
    return true;
}

bool AudioService::startRecording() {
    if (!initialized || recording) {
        return false;
    }

    Serial.println("[Audio] Starting recording...");

    // Clear buffer
    audioDataSize = 0;
    recordingStartTime = millis();
    recording = true;

    // Start I2S
    i2s_zero_dma_buffer(I2S_PORT);

    return true;
}

bool AudioService::stopRecording() {
    if (!recording) {
        return false;
    }

    recording = false;
    unsigned long duration = millis() - recordingStartTime;

    Serial.printf("[Audio] Recording stopped: %d bytes, %lu ms\n",
                 audioDataSize, duration);

    // Read remaining data from I2S
    size_t bytesRead = 0;
    size_t remainingSpace = audioBufferSize - audioDataSize;

    if (remainingSpace > 0) {
        i2s_read(I2S_PORT, audioBuffer + audioDataSize,
                remainingSpace, &bytesRead, 100);
        audioDataSize += bytesRead;
    }

    // TODO: Apply Voice Activity Detection (VAD) to remove silence
    // TODO: Apply audio compression (Opus codec or similar)

    return true;
}

bool AudioService::isRecording() {
    if (!recording) {
        return false;
    }

    // Read audio data while recording
    size_t bytesRead = 0;
    size_t remainingSpace = audioBufferSize - audioDataSize;

    if (remainingSpace > 0) {
        i2s_read(I2S_PORT, audioBuffer + audioDataSize,
                min(remainingSpace, (size_t)AUDIO_BUFFER_SIZE),
                &bytesRead, 0);
        audioDataSize += bytesRead;
    }

    // Check if maximum duration reached
    if (millis() - recordingStartTime > AUDIO_MAX_DURATION) {
        Serial.println("[Audio] Maximum recording duration reached");
        stopRecording();
        return false;
    }

    // Check if buffer is full
    if (audioDataSize >= audioBufferSize) {
        Serial.println("[Audio] Audio buffer full");
        stopRecording();
        return false;
    }

    return true;
}

uint8_t* AudioService::getAudioData() {
    return audioBuffer;
}

size_t AudioService::getAudioSize() {
    return audioDataSize;
}

bool AudioService::hasAudio() {
    return audioDataSize > 0;
}

void AudioService::clearAudio() {
    audioDataSize = 0;
}

void AudioService::allocateBuffer() {
    // Calculate buffer size for maximum recording duration
    // 16-bit mono at 16kHz = 32 KB/s
    audioBufferSize = (AUDIO_SAMPLE_RATE * AUDIO_BITS_PER_SAMPLE / 8 *
                      AUDIO_MAX_DURATION) / 1000;

    audioBuffer = (uint8_t*)malloc(audioBufferSize);

    if (audioBuffer) {
        Serial.printf("[Audio] Allocated %d bytes for audio buffer\n", audioBufferSize);
    } else {
        Serial.println("[Audio] ERROR: Failed to allocate audio buffer!");
    }
}

void AudioService::freeBuffer() {
    if (audioBuffer) {
        free(audioBuffer);
        audioBuffer = nullptr;
    }
}
