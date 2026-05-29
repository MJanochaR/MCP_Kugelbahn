#include <Arduino.h>
#include <mbed.h>

#include "config.h"
#include "actuators.h"

namespace {
    mbed::PwmOut servoPin(digitalPinToPinName(PIN_SERVROEHRE));
}

namespace Actuators {

void begin() {
    pinMode(PIN_R_MAUFZUG, OUTPUT);
    pinMode(PIN_G_MAUFZUG, OUTPUT);
    pinMode(PIN_R_MROEHRE, OUTPUT);
    pinMode(PIN_G_MROEHRE, OUTPUT);
    pinMode(PIN_R_MLOOP, OUTPUT);
    pinMode(PIN_G_MLOOP, OUTPUT);

    pinMode(PIN_R_LAMPE_GRUEN, OUTPUT);
    pinMode(PIN_R_LAMPE_ROT, OUTPUT);

    servoPin.period_ms(20);

    stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
    stopMotor(PIN_R_MROEHRE, PIN_G_MROEHRE);
    stopMotor(PIN_R_MLOOP, PIN_G_MLOOP);

    setLampen(false);
    setServoWinkel(SERVROEHRE_ZU);
}

void setMotor(int pinR, int pinG, bool richtung) {
    digitalWrite(pinR, richtung ? HIGH : LOW);
    digitalWrite(pinG, richtung ? LOW : HIGH);
}

void stopMotor(int pinR, int pinG) {
    digitalWrite(pinR, LOW);
    digitalWrite(pinG, LOW);
}

void setServoWinkel(int winkel) {
    int puls = map(winkel, 0, 180, 500, 2500);
    servoPin.pulsewidth_us(puls);
}

void setLampen(bool an) {
    digitalWrite(PIN_R_LAMPE_GRUEN, an ? HIGH : LOW);
    digitalWrite(PIN_R_LAMPE_ROT, an ? HIGH : LOW);
}

}