#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    pinMode(35, OUTPUT);  // Status LED

    delay(1000);
    Serial.println("===== MINIMAL TEST START =====");
    Serial.println("If you see this, board is working!");
}

void loop() {
    digitalWrite(35, HIGH);
    Serial.println("LED ON");
    delay(500);
    digitalWrite(35, LOW);
    Serial.println("LED OFF");
    delay(500);
}
