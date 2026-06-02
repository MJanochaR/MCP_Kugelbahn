#include <Arduino.h>
#include <WiFi.h>

#include "actuators.h"
#include "buttons.h"
#include "config.h"
#include "webvisu.h"

namespace {
    Button taster0(PIN_TASTER0);
    Button taster1(PIN_TASTER1);
    Button taster2(PIN_TASTER2);
    Button taster3(PIN_TASTER3);

    struct Anlage {
        bool loopRichtung = false;
        bool roehreRichtung = false;
        bool loopAktiv = false;
        bool roehreAktiv = false;
        bool aufzugAktiv = false;
        bool servoAuf = false;
        bool lampenAn = false;
        unsigned long loopStartMs = 0;
        unsigned long roehreStartMs = 0;
        unsigned long lampenWechselMs = 0;
        unsigned long loopSchaltungen = 0;
        unsigned long roehreSchaltungen = 0;
        unsigned long servoSchaltungen = 0;
    } anlage;

    bool aktivLow(int pin) {
        return digitalRead(pin) == LOW;
    }

    void setLampen(bool an) {
        anlage.lampenAn = an;
        Actuators::setLampen(an);
    }

    void schalteMotor(bool& aktiv, bool& richtung, unsigned long& startMs,
                      unsigned long& zaehler, int pinR, int pinG, unsigned long jetzt) {
        richtung = !richtung;
        aktiv = true;
        startMs = jetzt;
        zaehler++;
        Actuators::setMotor(pinR, pinG, richtung);
    }

    void schalteLoop(unsigned long jetzt) {
        schalteMotor(anlage.loopAktiv, anlage.loopRichtung, anlage.loopStartMs,
                     anlage.loopSchaltungen, PIN_R_MLOOP, PIN_G_MLOOP, jetzt);
    }

    void schalteRoehre(unsigned long jetzt) {
        schalteMotor(anlage.roehreAktiv, anlage.roehreRichtung, anlage.roehreStartMs,
                     anlage.roehreSchaltungen, PIN_R_MROEHRE, PIN_G_MROEHRE, jetzt);
    }

    void schalteServo() {
        anlage.servoAuf = !anlage.servoAuf;
        anlage.servoSchaltungen++;
        Actuators::setServoWinkel(anlage.servoAuf ? SERVROEHRE_AUF : SERVROEHRE_ZU);
    }

    void stoppeAlles() {
        Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
        Actuators::stopMotor(PIN_R_MLOOP, PIN_G_MLOOP);
        Actuators::stopMotor(PIN_R_MROEHRE, PIN_G_MROEHRE);
        anlage.aufzugAktiv = false;
        anlage.loopAktiv = false;
        anlage.roehreAktiv = false;
    }

    void updateAufzug() {
        anlage.aufzugAktiv = aktivLow(PIN_LINKS_STELLUNG_SCHALTER0);

        if (anlage.aufzugAktiv) {
            Actuators::setMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG, true);
        } else {
            Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
        }
    }

    void updateTaster(unsigned long jetzt) {
        if (taster0.pressed()) schalteLoop(jetzt);
        if (taster1.pressed()) schalteRoehre(jetzt);
        if (taster2.pressed()) schalteServo();
    }

    void updateWebCommand(unsigned long jetzt) {
        switch (WebVisu::consumeCommand()) {
            case WebVisu::CMD_LOOP_TOGGLE: schalteLoop(jetzt); break;
            case WebVisu::CMD_ROEHRE_TOGGLE: schalteRoehre(jetzt); break;
            case WebVisu::CMD_SERVO_TOGGLE: schalteServo(); break;
            case WebVisu::CMD_LAMP_TOGGLE:
                anlage.lampenWechselMs = jetzt;
                setLampen(!anlage.lampenAn);
                break;
            case WebVisu::CMD_ALL_STOP: stoppeAlles(); break;
            case WebVisu::CMD_NONE: break;
        }
    }

    void stoppeZeitMotoren(unsigned long jetzt) {
        if (anlage.loopAktiv && jetzt - anlage.loopStartMs >= MOTOR_RUN_MS) {
            Actuators::stopMotor(PIN_R_MLOOP, PIN_G_MLOOP);
            anlage.loopAktiv = false;
        }

        if (anlage.roehreAktiv && jetzt - anlage.roehreStartMs >= MOTOR_RUN_MS) {
            Actuators::stopMotor(PIN_R_MROEHRE, PIN_G_MROEHRE);
            anlage.roehreAktiv = false;
        }
    }

    void updateLampen(unsigned long jetzt) {
        if (jetzt - anlage.lampenWechselMs >= LAMP_BLINK_MS) {
            anlage.lampenWechselMs = jetzt;
            setLampen(!anlage.lampenAn);
        }
    }

    WebVisuState webStatus(unsigned long jetzt) {
        WebVisuState s = {};
        s.wifiConnected = WiFi.status() == WL_CONNECTED;
        s.ip = WiFi.localIP();
        s.aufzugAktiv = anlage.aufzugAktiv;
        s.loopAktiv = anlage.loopAktiv;
        s.loopRichtung = anlage.loopRichtung;
        s.roehreAktiv = anlage.roehreAktiv;
        s.roehreRichtung = anlage.roehreRichtung;
        s.servoAuf = anlage.servoAuf;
        s.lampenAn = anlage.lampenAn;
        s.taster0 = aktivLow(PIN_TASTER0);
        s.taster1 = aktivLow(PIN_TASTER1);
        s.taster2 = aktivLow(PIN_TASTER2);
        s.taster3 = aktivLow(PIN_TASTER3);
        s.schalterLinks = aktivLow(PIN_LINKS_STELLUNG_SCHALTER0);
        s.uptimeMs = jetzt;
        s.loopStartMs = anlage.loopStartMs;
        s.roehreStartMs = anlage.roehreStartMs;
        s.loopSchaltungen = anlage.loopSchaltungen;
        s.roehreSchaltungen = anlage.roehreSchaltungen;
        s.servoSchaltungen = anlage.servoSchaltungen;
        return s;
    }
}

void setup() {
    Actuators::begin();

    taster0.begin();
    taster1.begin();
    taster2.begin();
    taster3.begin();
    pinMode(PIN_LINKS_STELLUNG_SCHALTER0, INPUT_PULLUP);

    WebVisu::begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
    unsigned long jetzt = millis();

    updateAufzug();
    updateTaster(jetzt);
    updateWebCommand(jetzt);
    stoppeZeitMotoren(jetzt);
    updateLampen(jetzt);

    WebVisu::update(webStatus(jetzt));
}
