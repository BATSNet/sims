/**
 * WiFi Scanner - See all available networks
 */

#include <Arduino.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n========================================");
    Serial.println("WiFi Network Scanner");
    Serial.println("========================================\n");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("Scanning for WiFi networks...\n");

    int n = WiFi.scanNetworks();

    Serial.println("========================================");
    Serial.printf("Found %d networks:\n", n);
    Serial.println("========================================\n");

    for (int i = 0; i < n; i++) {
        Serial.printf("%d: SSID: \"%s\"\n", i + 1, WiFi.SSID(i).c_str());
        Serial.printf("   Signal: %d dBm\n", WiFi.RSSI(i));
        Serial.printf("   Channel: %d\n", WiFi.channel(i));
        Serial.printf("   Encryption: %s\n",
            WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Secured");
        Serial.println();
    }

    Serial.println("========================================");
    Serial.println("Look for your iPhone hotspot above!");
    Serial.println("========================================");
}

void loop() {
    delay(5000);

    Serial.println("\n[Scanning again...]\n");

    int n = WiFi.scanNetworks();

    Serial.println("========================================");
    Serial.printf("Found %d networks:\n", n);
    Serial.println("========================================\n");

    for (int i = 0; i < n; i++) {
        Serial.printf("%d: SSID: \"%s\"\n", i + 1, WiFi.SSID(i).c_str());
        Serial.printf("   Signal: %d dBm\n", WiFi.RSSI(i));
        Serial.printf("   Channel: %d\n", WiFi.channel(i));
        Serial.println();
    }
}
