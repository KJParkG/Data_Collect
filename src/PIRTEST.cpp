#include <Arduino.h>

const int inputpin = 18; // Lolin S3 Pro의 GPIO 18

void setup() {
    pinMode(inputpin, INPUT);
    Serial.begin(115200);
    Serial.println("Lolin S3 Pro - PIR Sensor Test");
    Serial.println("Waiting for sensor to settle (20 seconds)...");
    delay(20000); // 센서 안정화 시간
    Serial.println("Sensor ready!");
}

void loop() {
    int sensor_output = digitalRead(inputpin);

    if (sensor_output == HIGH) {
        Serial.println("Object detected!");
    } else {
        Serial.println("No object in sight.");
    }
    delay(500); // 0.5초 간격으로 상태 확인
}