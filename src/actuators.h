#pragma once

namespace Actuators {
    void begin();

    void setMotor(int pinR, int pinG, bool richtung);
    void stopMotor(int pinR, int pinG);

    void setServoWinkel(int winkel);
    void setServoStartWinkel(int winkel);
    void setLampen(bool an);
}