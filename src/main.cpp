#include <Arduino.h>

#include "config.h"
#include "actuators.h"
#include "buttons.h"

// BUTTONS --------------------------------------------------------------------------------------------
Button taster0(PIN_TASTER0);
Button taster1(PIN_TASTER1);
Button taster2(PIN_TASTER2);
Button taster3(PIN_TASTER3);

// VAR --------------------------------------------------------------------------------------------
bool richtungLoop = false;
bool richtungRoehre = false;

bool motorLoopAktiv = false;
bool motorRoehreAktiv = false;

unsigned long startLoop = 0;
unsigned long startRoehre = 0;

bool servoAuf = false;

bool lampenAn = false;
unsigned long letzterLampenWechsel = 0;


// --------------------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    Actuators::begin();

    taster0.begin();
    taster1.begin();
    taster2.begin();
    taster3.begin();

    pinMode(PIN_LINKS_STELLUNG_SCHALTER0, INPUT_PULLUP);
}
// --------------------------------------------------------------------------------------------
void loop() {
    unsigned long jetzt = millis();

    // Aufzug steuern
    if (digitalRead(PIN_LINKS_STELLUNG_SCHALTER0) == LOW) {
        Actuators::setMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG, true);
    } else {
        Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
    }
    
    // Motor Loop Stellung wechseln
    if (taster0.pressed()) {
        richtungLoop = !richtungLoop;
        motorLoopAktiv = true;
        startLoop = jetzt;

        Actuators::setMotor(PIN_R_MLOOP, PIN_G_MLOOP, richtungLoop);
    }

    // Motor Röhre Stellung wechseln
    if (taster1.pressed()) {
        richtungRoehre = !richtungRoehre;
        motorRoehreAktiv = true;
        startRoehre = jetzt;

        Actuators::setMotor(PIN_R_MROEHRE, PIN_G_MROEHRE, richtungRoehre);
    }

    // Servo Röhre Stellung wechseln
    if (taster2.pressed()) {
        servoAuf = !servoAuf;
        Actuators::setServoWinkel(servoAuf ? SERVROEHRE_AUF : SERVROEHRE_ZU);
    }

    // Motoren nach vorgegebener Zeit stoppen. Zeit für Endposition
    if (motorLoopAktiv && jetzt - startLoop >= MOTOR_RUN_MS) {
        Actuators::stopMotor(PIN_R_MLOOP, PIN_G_MLOOP);
        motorLoopAktiv = false;
    }
    if (motorRoehreAktiv && jetzt - startRoehre >= MOTOR_RUN_MS) {
        Actuators::stopMotor(PIN_R_MROEHRE, PIN_G_MROEHRE);
        motorRoehreAktiv = false;
    }

    // Blinken der LEDs
    if (jetzt - letzterLampenWechsel >= LAMP_BLINK_MS) {
        letzterLampenWechsel = jetzt;
        lampenAn = !lampenAn;

        Actuators::setLampen(lampenAn);
    }


    



}