#include <Arduino.h>
#include <WiFi.h>

#include "actuators.h"
#include "buttons.h"
#include "config.h"
#include "sensors.h"
#include "webvisu.h"

namespace {
Button tasterLoop(PIN_TASTER0);
Button tasterRoehre(PIN_TASTER1);
Button tasterServoBlockade(PIN_TASTER2);
Button tasterAufzug(PIN_TASTER3);
AnalogLightBarrier lichtschrankeOben(PIN_SENSOR_LICHTSCHRANKE_OBEN,
                                     LICHTSCHRANKE_ADC_SCHWELLE,
                                     LICHTSCHRANKE_DEBOUNCE_MS);
AnalogLightBarrier lichtschrankeUnten(PIN_SENSOR_LICHTSCHRANKE_UNTEN,
                                      LICHTSCHRANKE_ADC_SCHWELLE,
                                      LICHTSCHRANKE_DEBOUNCE_MS);

struct Anlage {
  bool loopRichtung = false;
  bool roehreRichtung = false;
  bool loopAktiv = false;
  bool roehreAktiv = false;
  bool aufzugAktiv = false;
  bool anlageScharf = false;
  bool servoAuf = false;
  bool lampenAn = false;
  bool messungAktiv = false;
  unsigned long loopStartMs = 0;
  unsigned long roehreStartMs = 0;
  unsigned long messungStartMs = 0;
  unsigned long messungLetzteMs = 0;
  unsigned long lampenWechselMs = 0;
  unsigned long messungAnzahl = 0;
  unsigned long loopSchaltungen = 0;
  unsigned long roehreSchaltungen = 0;
  unsigned long servoSchaltungen = 0;
} anlage;

WebVisuState::KugelData kugelHistorie[3] = {};

bool aktivLow(int pin) { return digitalRead(pin) == LOW; }

void printUptime(unsigned long jetzt) {
  unsigned long totalSeconds = jetzt / 1000;
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds / 60) % 60;
  unsigned long seconds = totalSeconds % 60;

  if (hours < 10)
    Serial.print('0');
  Serial.print(hours);
  Serial.print(':');
  if (minutes < 10)
    Serial.print('0');
  Serial.print(minutes);
  Serial.print(':');
  if (seconds < 10)
    Serial.print('0');
  Serial.print(seconds);
  Serial.print(' ');
}

void printLichtschranke(const char *name, unsigned long jetzt) {
  printUptime(jetzt);
  Serial.print("Lichtschranke ");
  Serial.print(name);
  Serial.println(" erkannt");
}

void setLampen(bool an) {
  anlage.lampenAn = an;
  Actuators::setLampen(an);
}

void schalteMotor(bool &aktiv, bool &richtung, unsigned long &startMs,
                  unsigned long &zaehler, int pinR, int pinG,
                  unsigned long jetzt) {
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

void schalteAufzug() {
  anlage.aufzugAktiv = !anlage.aufzugAktiv;
  if (anlage.aufzugAktiv) {
    Actuators::setMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG, true);
  } else {
    Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
  }
}

void stoppeAlles() {
  Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
  Actuators::stopMotor(PIN_R_MLOOP, PIN_G_MLOOP);
  Actuators::stopMotor(PIN_R_MROEHRE, PIN_G_MROEHRE);
  anlage.aufzugAktiv = false;
  anlage.loopAktiv = false;
  anlage.roehreAktiv = false;
}

void updateSchalter() {
  anlage.anlageScharf = aktivLow(PIN_LINKS_STELLUNG_SCHALTER0);
  if (!anlage.anlageScharf) {
    anlage.messungAktiv = false;
    stoppeAlles();
  }
}

void updateTaster(unsigned long jetzt) {
  bool pLoop = tasterLoop.pressed();
  bool pRoehre = tasterRoehre.pressed();
  bool pServo = tasterServoBlockade.pressed();
  bool pAufzug = tasterAufzug.pressed();

  if (!anlage.anlageScharf) {
    return;
  }

  if (pLoop)
    schalteLoop(jetzt);
  if (pRoehre)
    schalteRoehre(jetzt);
  if (pServo)
    schalteServo();
  if (pAufzug)
    schalteAufzug();
}

void updateLichtschranken(unsigned long jetzt) {
  if (lichtschrankeOben.triggered()) {
    printLichtschranke("oben", jetzt);
    if (anlage.anlageScharf) {
      anlage.messungAktiv = true;
      anlage.messungStartMs = jetzt;

      // Historie verschieben
      kugelHistorie[2] = kugelHistorie[1];
      kugelHistorie[1] = kugelHistorie[0];

      // Neue Kugel eintragen
      kugelHistorie[0].startMs = jetzt;
      kugelHistorie[0].endMs = 0;
      kugelHistorie[0].dauerMs = 0;
      kugelHistorie[0].aktiv = true;
      kugelHistorie[0].abgeschlossen = false;
    }
  }

  if (lichtschrankeUnten.triggered()) {
    printLichtschranke("unten", jetzt);
    if (anlage.anlageScharf && anlage.messungAktiv) {
      anlage.messungLetzteMs = jetzt - anlage.messungStartMs;
      anlage.messungAnzahl++;
      anlage.messungAktiv = false;

      // Letzte Kugel aktualisieren
      kugelHistorie[0].endMs = jetzt;
      kugelHistorie[0].dauerMs = anlage.messungLetzteMs;
      kugelHistorie[0].aktiv = false;
      kugelHistorie[0].abgeschlossen = true;

      printUptime(jetzt);
      Serial.print("Zeitmessung: ");
      Serial.print(anlage.messungLetzteMs);
      Serial.println(" ms");
    }
  }
}

void updateWebCommand(unsigned long jetzt) {
  WebVisu::Command cmd = WebVisu::consumeCommand();
  if (!anlage.anlageScharf) {
    return;
  }
  switch (cmd) {
  case WebVisu::CMD_LOOP_TOGGLE:
    schalteLoop(jetzt);
    break;
  case WebVisu::CMD_ROEHRE_TOGGLE:
    schalteRoehre(jetzt);
    break;
  case WebVisu::CMD_SERVO_TOGGLE:
    schalteServo();
    break;
  case WebVisu::CMD_AUFZUG_TOGGLE:
    schalteAufzug();
    break;
  case WebVisu::CMD_LAMP_TOGGLE:
    anlage.lampenWechselMs = jetzt;
    setLampen(!anlage.lampenAn);
    break;
  case WebVisu::CMD_ALL_STOP:
    stoppeAlles();
    break;
  case WebVisu::CMD_NONE:
    break;
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
  if (!anlage.anlageScharf) {
    anlage.lampenWechselMs = jetzt;
    if (anlage.lampenAn)
      setLampen(false);
    return;
  }

  if (jetzt - anlage.lampenWechselMs >= LAMP_BLINK_MS) {
    anlage.lampenWechselMs = jetzt;
    setLampen(!anlage.lampenAn);
  }
}

volatile bool shockFlankeErkannt = false;
volatile bool shockZustandISR = false;

void shockISR() {
  shockZustandISR = digitalRead(PIN_Shocksensor_IN);
  shockFlankeErkannt = true;
}

void updateShocksensor(unsigned long jetzt) {
  if (shockFlankeErkannt) {
    shockFlankeErkannt = false;
    printUptime(jetzt);
    Serial.println("Shocksensor Flanke erkannt");
  }
}

void updateLEDs() {
  bool scharf = anlage.anlageScharf;
  digitalWrite(PIN_LED_LICHTSCHRANKE_OBEN, scharf ? HIGH : LOW);
  digitalWrite(PIN_LED_LICHTSCHRANKE_UNTEN, scharf ? HIGH : LOW);
  digitalWrite(PIN_2FarbigeLED_Out1, scharf ? HIGH : LOW);
  digitalWrite(PIN_2FarbigeLED_Out2, LOW);
}

WebVisuState webStatus(unsigned long jetzt) {
  WebVisuState s = {};
  s.wifiConnected =
      (WiFi.status() == WL_CONNECTED || WiFi.status() == WL_AP_LISTENING);
  s.ip = WiFi.localIP();
  s.aufzugAktiv = anlage.aufzugAktiv;
  s.anlageScharf = anlage.anlageScharf;
  s.loopAktiv = anlage.loopAktiv;
  s.loopRichtung = anlage.loopRichtung;
  s.roehreAktiv = anlage.roehreAktiv;
  s.roehreRichtung = anlage.roehreRichtung;
  s.servoAuf = anlage.servoAuf;
  s.lampenAn = anlage.lampenAn;
  s.taster0 = tasterLoop.isPressed();
  s.taster1 = tasterRoehre.isPressed();
  s.taster2 = tasterServoBlockade.isPressed();
  s.taster3 = tasterAufzug.isPressed();
  s.schalterLinks = aktivLow(PIN_LINKS_STELLUNG_SCHALTER0);
  s.lichtschrankeOben = lichtschrankeOben.isActive();
  s.lichtschrankeUnten = lichtschrankeUnten.isActive();
  s.uptimeMs = jetzt;

  for (int i = 0; i < 3; i++) {
    s.kugeln[i] = kugelHistorie[i];
  }
  s.messungAnzahl = anlage.messungAnzahl;

  s.loopStartMs = anlage.loopStartMs;
  s.roehreStartMs = anlage.roehreStartMs;
  s.loopSchaltungen = anlage.loopSchaltungen;
  s.roehreSchaltungen = anlage.roehreSchaltungen;
  s.servoSchaltungen = anlage.servoSchaltungen;
  return s;
}
} // namespace

void setup() {
  Actuators::begin();

  tasterLoop.begin();
  tasterRoehre.begin();
  tasterServoBlockade.begin();
  tasterAufzug.begin();
  pinMode(PIN_LINKS_STELLUNG_SCHALTER0, INPUT_PULLUP);
  pinMode(PIN_LED_LICHTSCHRANKE_OBEN, OUTPUT);
  pinMode(PIN_LED_LICHTSCHRANKE_UNTEN, OUTPUT);
  pinMode(PIN_2FarbigeLED_Out1, OUTPUT);
  pinMode(PIN_2FarbigeLED_Out2, OUTPUT);
  pinMode(PIN_Shocksensor_IN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_Shocksensor_IN), shockISR, CHANGE);
  digitalWrite(PIN_LED_LICHTSCHRANKE_OBEN, HIGH);
  digitalWrite(PIN_LED_LICHTSCHRANKE_UNTEN, HIGH);
  lichtschrankeOben.begin();
  lichtschrankeUnten.begin();

  WebVisu::begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  unsigned long jetzt = millis();

  updateSchalter();
  updateTaster(jetzt);
  updateLichtschranken(jetzt);
  updateWebCommand(jetzt);
  stoppeZeitMotoren(jetzt);
  updateLampen(jetzt);
  updateShocksensor(jetzt);
  updateLEDs();

  WebVisu::update(webStatus(jetzt));
}
