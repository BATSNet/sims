/**
 * XIAO ESP32S3 Sense - Camera and Audio Test
 *
 * Simple test app to verify camera and microphone functionality
 *
 * Serial Commands:
 * - 'p' or 'photo' - Take a photo
 * - 'a' or 'audio' - Record 10 seconds of audio
 * - 's' or 'status' - Print system status
 */

#include <Arduino.h>
#include <esp_camera.h>
#include <driver/i2s.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// Camera pins for XIAO ESP32S3 Sense (fixed)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// PDM Microphone pins (built-in)
#define MIC_PDM_CLK       42
#define MIC_PDM_DATA      41

// I2S configuration for PDM microphone
#define I2S_PORT          I2S_NUM_0
#define SAMPLE_RATE       16000  // 16kHz
#define BITS_PER_SAMPLE   16
#define AUDIO_DURATION_MS 10000  // 10 seconds
#define BUFFER_SIZE       512

bool cameraInitialized = false;
bool microphoneInitialized = false;

// Audio buffer (10 seconds at 16kHz, 16-bit)
const size_t audioBufferSize = SAMPLE_RATE * (AUDIO_DURATION_MS / 1000) * 2; // 2 bytes per sample
uint8_t* audioBuffer = nullptr;

bool initCamera() {
    Serial.println("[Camera] Initializing OV2640...");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_SVGA;  // 800x600
    config.jpeg_quality = 12;  // 0-63, lower = better quality
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_LATEST;

    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] ERROR: Camera init failed with error 0x%x\n", err);
        return false;
    }

    // Get camera sensor
    sensor_t* s = esp_camera_sensor_get();
    if (s == NULL) {
        Serial.println("[Camera] ERROR: Failed to get sensor");
        return false;
    }

    // Adjust sensor settings
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 = No Effect
    s->set_whitebal(s, 1);       // 0 = disable, 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable, 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4
    s->set_exposure_ctrl(s, 1);  // 0 = disable, 1 = enable
    s->set_aec2(s, 0);           // 0 = disable, 1 = enable
    s->set_ae_level(s, 0);       // -2 to 2
    s->set_aec_value(s, 300);    // 0 to 1200
    s->set_gain_ctrl(s, 1);      // 0 = disable, 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable, 1 = enable
    s->set_wpc(s, 1);            // 0 = disable, 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable, 1 = enable
    s->set_lenc(s, 1);           // 0 = disable, 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable, 1 = enable
    s->set_vflip(s, 0);          // 0 = disable, 1 = enable
    s->set_dcw(s, 1);            // 0 = disable, 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable, 1 = enable

    Serial.println("[Camera] OV2640 initialized successfully");
    return true;
}

bool initMicrophone() {
    Serial.println("[Microphone] Initializing PDM microphone...");

    // Allocate audio buffer
    audioBuffer = (uint8_t*)ps_malloc(audioBufferSize);
    if (audioBuffer == nullptr) {
        Serial.println("[Microphone] ERROR: Failed to allocate audio buffer");
        return false;
    }

    // I2S configuration for PDM microphone
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    // I2S pin configuration
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_PDM_CLK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_PDM_DATA
    };

    // Install and configure I2S driver
    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Microphone] ERROR: I2S driver install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[Microphone] ERROR: I2S set pin failed: %d\n", err);
        return false;
    }

    // Set PDM mode
    err = i2s_set_pdm_rx_down_sample(I2S_PORT, I2S_PDM_DSR_8S);
    if (err != ESP_OK) {
        Serial.printf("[Microphone] ERROR: PDM downsample failed: %d\n", err);
    }

    Serial.println("[Microphone] PDM microphone initialized successfully");
    return true;
}

void capturePhoto() {
    Serial.println("\n[Camera] Capturing photo...");

    if (!cameraInitialized) {
        Serial.println("[Camera] ERROR: Camera not initialized");
        return;
    }

    unsigned long startTime = millis();

    // Capture frame
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[Camera] ERROR: Camera capture failed");
        return;
    }

    unsigned long captureTime = millis() - startTime;

    // Print image info
    Serial.printf("[Camera] Photo captured in %lu ms\n", captureTime);
    Serial.printf("[Camera] Resolution: %dx%d\n", fb->width, fb->height);
    Serial.printf("[Camera] Format: %s\n", fb->format == PIXFORMAT_JPEG ? "JPEG" : "Unknown");
    Serial.printf("[Camera] Size: %d bytes\n", fb->len);

    // Print first 32 bytes as hex (JPEG header)
    Serial.print("[Camera] JPEG Header: ");
    for (int i = 0; i < min(32, (int)fb->len); i++) {
        Serial.printf("%02X ", fb->buf[i]);
    }
    Serial.println();

    // Return framebuffer
    esp_camera_fb_return(fb);

    Serial.println("[Camera] Photo capture complete");
}

void recordAudio() {
    Serial.println("\n[Microphone] Recording 10 seconds of audio...");

    if (!microphoneInitialized) {
        Serial.println("[Microphone] ERROR: Microphone not initialized");
        return;
    }

    unsigned long startTime = millis();
    size_t bytesRecorded = 0;
    size_t bytesRead = 0;

    // Clear buffer
    memset(audioBuffer, 0, audioBufferSize);

    // Record audio in chunks
    while (bytesRecorded < audioBufferSize && (millis() - startTime) < AUDIO_DURATION_MS) {
        size_t bytesToRead = min((size_t)BUFFER_SIZE, audioBufferSize - bytesRecorded);

        esp_err_t result = i2s_read(I2S_PORT,
                                     audioBuffer + bytesRecorded,
                                     bytesToRead,
                                     &bytesRead,
                                     portMAX_DELAY);

        if (result == ESP_OK) {
            bytesRecorded += bytesRead;

            // Print progress every 1 second
            if ((millis() - startTime) % 1000 < 20) {
                Serial.printf("[Microphone] Progress: %d%%\n",
                             (int)((bytesRecorded * 100) / audioBufferSize));
            }
        } else {
            Serial.printf("[Microphone] ERROR: I2S read failed: %d\n", result);
            break;
        }
    }

    unsigned long recordTime = millis() - startTime;

    // Calculate audio statistics
    int16_t* samples = (int16_t*)audioBuffer;
    size_t numSamples = bytesRecorded / 2;
    int32_t sum = 0;
    int16_t minVal = 32767;
    int16_t maxVal = -32768;

    for (size_t i = 0; i < numSamples; i++) {
        int16_t sample = samples[i];
        sum += abs(sample);
        if (sample < minVal) minVal = sample;
        if (sample > maxVal) maxVal = sample;
    }

    int16_t avgAmplitude = sum / numSamples;

    // Print recording info
    Serial.printf("[Microphone] Recording complete in %lu ms\n", recordTime);
    Serial.printf("[Microphone] Bytes recorded: %d / %d\n", bytesRecorded, audioBufferSize);
    Serial.printf("[Microphone] Samples: %d\n", numSamples);
    Serial.printf("[Microphone] Sample rate: %d Hz\n", SAMPLE_RATE);
    Serial.printf("[Microphone] Duration: %.2f seconds\n", (float)numSamples / SAMPLE_RATE);
    Serial.printf("[Microphone] Average amplitude: %d\n", avgAmplitude);
    Serial.printf("[Microphone] Min: %d, Max: %d, Range: %d\n", minVal, maxVal, maxVal - minVal);

    // Check if audio was actually recorded (not just silence)
    if (avgAmplitude < 10) {
        Serial.println("[Microphone] WARNING: Audio level very low - check microphone!");
    } else if (avgAmplitude > 5000) {
        Serial.println("[Microphone] Audio level good");
    }

    // Print first 32 samples
    Serial.print("[Microphone] First 32 samples: ");
    for (int i = 0; i < min(32, (int)numSamples); i++) {
        Serial.printf("%d ", samples[i]);
    }
    Serial.println();
}

void printStatus() {
    Serial.println("\n========== SYSTEM STATUS ==========");
    Serial.printf("Board: XIAO ESP32S3 Sense\n");
    Serial.printf("Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
    Serial.println();
    Serial.printf("Camera: %s\n", cameraInitialized ? "Initialized" : "Not initialized");
    Serial.printf("Microphone: %s\n", microphoneInitialized ? "Initialized" : "Not initialized");
    Serial.println("===================================\n");
}

void printHelp() {
    Serial.println("\n========== COMMANDS ==========");
    Serial.println("p or photo   - Take a photo");
    Serial.println("a or audio   - Record 10 seconds of audio");
    Serial.println("s or status  - Print system status");
    Serial.println("h or help    - Show this help");
    Serial.println("==============================\n");
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("XIAO ESP32S3 Sense - Camera & Audio Test");
    Serial.println("========================================\n");

    // Initialize camera
    cameraInitialized = initCamera();
    if (!cameraInitialized) {
        Serial.println("[ERROR] Camera initialization failed - photo capture disabled");
    }

    // Initialize microphone
    microphoneInitialized = initMicrophone();
    if (!microphoneInitialized) {
        Serial.println("[ERROR] Microphone initialization failed - audio recording disabled");
    }

    if (!cameraInitialized && !microphoneInitialized) {
        Serial.println("\n[CRITICAL] Both camera and microphone initialization failed!");
        Serial.println("Check hardware connections and PSRAM availability");
    }

    Serial.println("\nInitialization complete!");
    printHelp();
    printStatus();
}

void loop() {
    // Check for serial commands
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        command.toLowerCase();

        if (command == "p" || command == "photo") {
            capturePhoto();
        }
        else if (command == "a" || command == "audio") {
            recordAudio();
        }
        else if (command == "s" || command == "status") {
            printStatus();
        }
        else if (command == "h" || command == "help") {
            printHelp();
        }
        else if (command.length() > 0) {
            Serial.printf("Unknown command: '%s'\n", command.c_str());
            Serial.println("Type 'help' for available commands");
        }
    }

    delay(10);
}
