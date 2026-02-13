/**
 * XIAO ESP32S3 Sense - Camera & Audio Test with Web Server
 *
 * Captures photo and audio, then serves them via web browser
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include <driver/i2s.h>
#include <base64.h>

// WiFi credentials
const char* ssid = "iPhone";
const char* password = "letsrock";

// Camera pins for XIAO ESP32S3 Sense
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

// PDM Microphone
#define MIC_PDM_CLK       42
#define MIC_PDM_DATA      41
#define I2S_PORT          I2S_NUM_0
#define SAMPLE_RATE       16000
#define AUDIO_DURATION_MS 10000

// Storage
uint8_t* imageBuffer = nullptr;
size_t imageSize = 0;
uint8_t* audioBuffer = nullptr;
size_t audioBufferSize = SAMPLE_RATE * (AUDIO_DURATION_MS / 1000) * 2;
size_t audioSize = 0;

WebServer server(80);

bool initCamera() {
    Serial.println("[Camera] Initializing...");

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
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_LATEST;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("[Camera] Init failed!");
        return false;
    }

    Serial.println("[Camera] OK!");
    return true;
}

bool initMicrophone() {
    Serial.println("[Microphone] Initializing...");

    audioBuffer = (uint8_t*)ps_malloc(audioBufferSize);
    if (!audioBuffer) {
        Serial.println("[Microphone] Buffer allocation failed!");
        return false;
    }

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

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = MIC_PDM_CLK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = MIC_PDM_DATA
    };

    if (i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL) != ESP_OK) {
        Serial.println("[Microphone] Driver install failed!");
        return false;
    }

    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_set_pdm_rx_down_sample(I2S_PORT, I2S_PDM_DSR_8S);

    Serial.println("[Microphone] OK!");
    return true;
}

void capturePhoto() {
    Serial.println("[Capture] Taking photo...");
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[Capture] Photo failed!");
        return;
    }

    // Copy to buffer
    if (imageBuffer) free(imageBuffer);
    imageBuffer = (uint8_t*)malloc(fb->len);
    memcpy(imageBuffer, fb->buf, fb->len);
    imageSize = fb->len;

    esp_camera_fb_return(fb);
    Serial.printf("[Capture] Photo: %d bytes\n", imageSize);
}

void recordAudio() {
    Serial.println("[Capture] Recording 10 seconds...");
    size_t bytesRecorded = 0;
    size_t bytesRead = 0;

    while (bytesRecorded < audioBufferSize) {
        size_t bytesToRead = min((size_t)512, audioBufferSize - bytesRecorded);
        i2s_read(I2S_PORT, audioBuffer + bytesRecorded, bytesToRead, &bytesRead, portMAX_DELAY);
        bytesRecorded += bytesRead;

        if (bytesRecorded % 32000 == 0) {
            Serial.printf("[Capture] %d%%\n", (bytesRecorded * 100) / audioBufferSize);
        }
    }

    audioSize = bytesRecorded;
    Serial.printf("[Capture] Audio: %d bytes\n", audioSize);
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>XIAO ESP32S3</title>";
    html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;}";
    html += "h1{color:#333;}.container{background:white;padding:20px;border-radius:10px;margin:10px 0;}";
    html += "img{max-width:100%;border:2px solid #ddd;border-radius:5px;}";
    html += "button{background:#4CAF50;color:white;padding:10px 20px;border:none;";
    html += "border-radius:5px;cursor:pointer;font-size:16px;margin:5px;}";
    html += "button:hover{background:#45a049;}.info{color:#666;font-size:14px;}</style></head><body>";
    html += "<h1>XIAO ESP32S3 Sense - Captured Media</h1>";
    html += "<div class='container'><h2>Camera (OV2640)</h2>";
    html += "<p class='info'>Resolution: 800x600 | Format: JPEG</p>";
    html += "<img src='/image.jpg' alt='Captured Photo'><br>";
    html += "<button onclick=\"window.open('/image.jpg','_blank')\">Download Photo</button></div>";
    html += "<div class='container'><h2>Microphone (PDM)</h2>";
    html += "<p class='info'>Sample Rate: 16kHz | Duration: 10 seconds</p>";
    html += "<audio controls style='width:100%;'><source src='/audio.wav' type='audio/wav'>";
    html += "Your browser does not support audio.</audio><br>";
    html += "<button onclick=\"window.open('/audio.wav','_blank')\">Download Audio</button></div>";
    html += "<div class='container'><h2>Actions</h2>";
    html += "<button onclick=\"location.href='/capture'\">Capture New</button>";
    html += "<button onclick='location.reload()'>Refresh</button></div></body></html>";
    server.send(200, "text/html", html);
}

void handleImage() {
    if (!imageBuffer || imageSize == 0) {
        server.send(404, "text/plain", "No image captured");
        return;
    }
    server.send_P(200, "image/jpeg", (const char*)imageBuffer, imageSize);
}

void handleAudio() {
    if (!audioBuffer || audioSize == 0) {
        server.send(404, "text/plain", "No audio captured");
        return;
    }

    // Create WAV header
    uint8_t wavHeader[44];
    uint32_t fileSize = audioSize + 36;
    uint32_t dataSize = audioSize;
    uint16_t numChannels = 1;
    uint32_t sampleRate = SAMPLE_RATE;
    uint16_t bitsPerSample = 16;
    uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
    uint16_t blockAlign = numChannels * bitsPerSample / 8;

    memcpy(wavHeader, "RIFF", 4);
    memcpy(wavHeader + 4, &fileSize, 4);
    memcpy(wavHeader + 8, "WAVE", 4);
    memcpy(wavHeader + 12, "fmt ", 4);
    uint32_t fmtSize = 16;
    memcpy(wavHeader + 16, &fmtSize, 4);
    uint16_t audioFormat = 1; // PCM
    memcpy(wavHeader + 20, &audioFormat, 2);
    memcpy(wavHeader + 22, &numChannels, 2);
    memcpy(wavHeader + 24, &sampleRate, 4);
    memcpy(wavHeader + 28, &byteRate, 4);
    memcpy(wavHeader + 32, &blockAlign, 2);
    memcpy(wavHeader + 34, &bitsPerSample, 2);
    memcpy(wavHeader + 36, "data", 4);
    memcpy(wavHeader + 40, &dataSize, 4);

    // Send header + audio data
    WiFiClient client = server.client();
    server.setContentLength(44 + audioSize);
    server.send(200, "audio/wav", "");
    client.write(wavHeader, 44);
    client.write(audioBuffer, audioSize);
}

void handleCapture() {
    capturePhoto();
    recordAudio();
    server.sendHeader("Location", "/");
    server.send(303);
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("XIAO ESP32S3 - Camera & Audio Web Test");
    Serial.println("========================================\n");

    // Initialize hardware
    if (!initCamera()) {
        Serial.println("ERROR: Camera init failed!");
        return;
    }

    if (!initMicrophone()) {
        Serial.println("ERROR: Microphone init failed!");
        return;
    }

    // Capture initial photo and audio
    capturePhoto();
    recordAudio();

    // Connect to WiFi
    Serial.printf("[WiFi] Connecting to '%s'...\n", ssid);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.print("[WiFi] IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.println("\n========================================");
        Serial.println("Open this URL in your browser:");
        Serial.print("http://");
        Serial.println(WiFi.localIP());
        Serial.println("========================================\n");

        // Start web server
        server.on("/", handleRoot);
        server.on("/image.jpg", handleImage);
        server.on("/audio.wav", handleAudio);
        server.on("/capture", handleCapture);
        server.begin();
        Serial.println("[Server] Web server started!");
    } else {
        Serial.println("\n[WiFi] Connection failed!");
        Serial.println("Check SSID and password");
    }
}

void loop() {
    server.handleClient();
    delay(1);
}
