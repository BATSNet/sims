/**
 * Simple Hello World Test
 * Just prints to serial to verify communication works
 */

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial to initialize

    Serial.println("\n\n\n");
    Serial.println("========================================");
    Serial.println("HELLO FROM XIAO ESP32S3!");
    Serial.println("========================================");
    Serial.println("Serial communication working!");
    Serial.println();
    Serial.printf("Chip: %s\n", ESP.getChipModel());
    Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("\nType anything and press Enter...");
}

void loop() {
    // Echo back anything received
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        Serial.println("========================================");
        Serial.printf("You typed: '%s'\n", input.c_str());
        Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
        Serial.println("========================================");
        Serial.println();
    }

    // Print heartbeat every 5 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        Serial.println("[HEARTBEAT] Still alive... (type something!)");
        lastPrint = millis();
    }

    delay(100);
}
