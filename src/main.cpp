#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "actuators.h"
#include "buttons.h"
#include "webvisu.h"

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
bool motorAufzugAktiv = false;

unsigned long startLoop = 0;
unsigned long startRoehre = 0;

bool servoAuf = false;

bool lampenAn = false;
unsigned long letzterLampenWechsel = 0;

unsigned long loopSchaltungen = 0;
unsigned long roehreSchaltungen = 0;
unsigned long servoSchaltungen = 0;

// --------------------------------------------------------------------------------------------
void schalteLoop(unsigned long jetzt) {
    richtungLoop = !richtungLoop;
    motorLoopAktiv = true;
    startLoop = jetzt;
    loopSchaltungen++;

    Actuators::setMotor(PIN_R_MLOOP, PIN_G_MLOOP, richtungLoop);
}

void schalteRoehre(unsigned long jetzt) {
    richtungRoehre = !richtungRoehre;
    motorRoehreAktiv = true;
    startRoehre = jetzt;
    roehreSchaltungen++;

    Actuators::setMotor(PIN_R_MROEHRE, PIN_G_MROEHRE, richtungRoehre);
}

void schalteServo() {
    servoAuf = !servoAuf;
    servoSchaltungen++;
    Actuators::setServoWinkel(servoAuf ? SERVROEHRE_AUF : SERVROEHRE_ZU);
}

void setzeLampen(bool an) {
    lampenAn = an;
    Actuators::setLampen(lampenAn);
}

void stoppeAlles() {
    Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
    Actuators::stopMotor(PIN_R_MLOOP, PIN_G_MLOOP);
    Actuators::stopMotor(PIN_R_MROEHRE, PIN_G_MROEHRE);
    motorAufzugAktiv = false;
    motorLoopAktiv = false;
    motorRoehreAktiv = false;
}

WebVisuState baueWebStatus(unsigned long jetzt) {
    WebVisuState s;
    s.wifiConnected = WiFi.status() == WL_CONNECTED;
    s.ip = WiFi.localIP();

    s.aufzugAktiv = motorAufzugAktiv;
    s.loopAktiv = motorLoopAktiv;
    s.loopRichtung = richtungLoop;
    s.roehreAktiv = motorRoehreAktiv;
    s.roehreRichtung = richtungRoehre;
    s.servoAuf = servoAuf;
    s.lampenAn = lampenAn;

    // INPUT_PULLUP: gedrückt/aktiv bedeutet LOW, daher invertieren.
    s.taster0 = digitalRead(PIN_TASTER0) == LOW;
    s.taster1 = digitalRead(PIN_TASTER1) == LOW;
    s.taster2 = digitalRead(PIN_TASTER2) == LOW;
    s.taster3 = digitalRead(PIN_TASTER3) == LOW;
    s.schalterLinks = digitalRead(PIN_LINKS_STELLUNG_SCHALTER0) == LOW;

    s.uptimeMs = jetzt;
    s.loopStartMs = startLoop;
    s.roehreStartMs = startRoehre;
    s.loopSchaltungen = loopSchaltungen;
    s.roehreSchaltungen = roehreSchaltungen;
    s.servoSchaltungen = servoSchaltungen;
    return s;
}

// --------------------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    Actuators::begin();

    taster0.begin();
    taster1.begin();
    taster2.begin();
    taster3.begin();

    pinMode(PIN_LINKS_STELLUNG_SCHALTER0, INPUT_PULLUP);

    WebVisu::begin(WIFI_SSID, WIFI_PASS);
}

// --------------------------------------------------------------------------------------------
void loop() {
    unsigned long jetzt = millis();

    // Aufzug steuern
    if (digitalRead(PIN_LINKS_STELLUNG_SCHALTER0) == LOW) {
        Actuators::setMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG, true);
        motorAufzugAktiv = true;
    } else {
        Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
        motorAufzugAktiv = false;
    }

    // Motor Loop Stellung wechseln: physischer Taster oder WebVisu
    if (taster0.pressed()) {
        schalteLoop(jetzt);
    }

    // Motor Röhre Stellung wechseln: physischer Taster oder WebVisu
    if (taster1.pressed()) {
        schalteRoehre(jetzt);
    }

    // Servo Röhre Stellung wechseln: physischer Taster oder WebVisu
    if (taster2.pressed()) {
        schalteServo();
    }

    // Web-Kommandos ausführen
    switch (WebVisu::consumeCommand()) {
        case WebVisu::CMD_LOOP_TOGGLE:
            schalteLoop(jetzt);
            break;
        case WebVisu::CMD_ROEHRE_TOGGLE:
            schalteRoehre(jetzt);
            break;
        case WebVisu::CMD_SERVO_TOGGLE:
            schalteServo();
            break;
        case WebVisu::CMD_LAMP_TOGGLE:
            letzterLampenWechsel = jetzt;
            setzeLampen(!lampenAn);
            break;
        case WebVisu::CMD_ALL_STOP:
            stoppeAlles();
            break;
        case WebVisu::CMD_NONE:
        default:
            break;
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
        setzeLampen(!lampenAn);
    }

    // WebVisu muss regelmäßig laufen, damit Browser und API reagieren.
    WebVisu::update(baueWebStatus(jetzt));
}
