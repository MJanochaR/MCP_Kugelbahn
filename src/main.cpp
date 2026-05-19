#include <Arduino.h>

// const int Pin_Motor_Aufzug = 4;
int testLEDPinHigh = 4;
int testLEDPinLow = 5;



void setup() {
    //pinMode(Pin_Motor_Aufzug, OUTPUT);
    pinMode(testLEDPinHigh, OUTPUT);
    pinMode(testLEDPinLow, OUTPUT);
    digitalWrite(testLEDPinHigh, HIGH);
    digitalWrite(testLEDPinLow, LOW);
    Serial.begin(115200);
}

void loop() {
    //digitalWrite(Pin_Motor_Aufzug, HIGH);
    delay(500);
    Serial.println("Sketch läuft");

    //digitalWrite(Pin_Motor_Aufzug, LOW);
    delay(500);
}