/**
 * Basic WiFi Connection Test
 */

#include <Arduino.h>
#include <WiFi.h>

const char* ssid = "iPhone";
const char* password = "letsrock";

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("\n========================================");
    Serial.println("Basic WiFi Connection Test");
    Serial.println("========================================");
    Serial.printf("SSID: '%s' (length: %d)\n", ssid, strlen(ssid));
    Serial.printf("Password: '%s' (length: %d)\n", password, strlen(password));
    Serial.println();

    // Print character codes for SSID
    Serial.print("SSID bytes: ");
    for(int i = 0; i < strlen(ssid); i++) {
        Serial.printf("%02X ", (unsigned char)ssid[i]);
    }
    Serial.println();

    // Print character codes for password
    Serial.print("Password bytes: ");
    for(int i = 0; i < strlen(password); i++) {
        Serial.printf("%02X ", (unsigned char)password[i]);
    }
    Serial.println("\n");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("Starting connection...");
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        Serial.print(WiFi.status());
        Serial.print(" ");
        attempts++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n========================================");
        Serial.println("SUCCESS! WiFi Connected!");
        Serial.println("========================================");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        Serial.println("========================================");
    } else {
        Serial.println("\n========================================");
        Serial.println("FAILED! Could not connect");
        Serial.println("========================================");
        Serial.print("Final status code: ");
        Serial.println(WiFi.status());
        Serial.println("\nStatus codes:");
        Serial.println("0 = WL_IDLE_STATUS");
        Serial.println("1 = WL_NO_SSID_AVAIL (SSID not found)");
        Serial.println("3 = WL_CONNECTED");
        Serial.println("4 = WL_CONNECT_FAILED");
        Serial.println("6 = WL_DISCONNECTED");
        Serial.println("========================================");
    }
}

void loop() {
    delay(1000);
}
