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
  unsigned long loopStartMs = 0;
  unsigned long roehreStartMs = 0;
  unsigned long lampenWechselMs = 0;
  unsigned long messungAnzahl = 0;

  WebVisu::RaceState raceState = WebVisu::RACE_IDLE;
  int raceBallsStarted = 0;
  int raceBallsFinished = 0;
  int raceBallsSecondPass = 0;

  int startRichtungMode = 0; // 0=Alternating, 1=Rechts, 2=Links
  int streckenMode = 0;      // 0=Manuell, 1=Aussortieren, 2=Rampe, 3=Looping,
                             // 4=Gerade, 5=Zufall, 6=Gleichmaessig
  int raceStreckenMode = 5;  // 5=Zufall, 6=Gleichmaessig

  // Gesamtkugeln
  int kugelnSeitReset = 0;
  int kugelnUntenSeitReset = 0;

  bool servoStartIstRechts = true;

  bool aussortierenAktiv = false;
  bool ledFarbeToggle = false;
  unsigned long aufzugStopMs = 0;

  // Servo Auslass Queue
  // Auslass-Sequenz: 0=Idle, 1=Strecke0+Auf, 2=Zu, 3=Strecke1, 4=Auf, 5=Zu,
  // 6=Strecke2, 7=Auf, 8=Zu/Done
  int servoReleasePhase = 0;
  unsigned long servoReleaseNextMs = 0;

  unsigned long time3Ball = SERVO_OPEN3BALL_MS;
  unsigned long time2Ball = SERVO_OPEN2BALL_MS;
  unsigned long time1Ball = SERVO_OPEN1BALL_MS;
  int servoAufWinkel = SERVROEHRE_AUF;
  int servoZuWinkel = SERVROEHRE_ZU;
  int winnerIndex = 0;
} anlage;

WebVisuState::KugelData kugelHistorie[3] = {};

bool aktivLow(int pin) { return digitalRead(pin) == LOW; }

// (printUptime entfernt - ist in webvisu_backend.cpp vorhanden)

void printLichtschranke(const char *name, int kugelNummer) {
  Serial.print("LS ");
  Serial.print(name);
  if (kugelNummer > 0) {
    Serial.print(": Kugel ");
    Serial.println(kugelNummer);
  } else {
    Serial.println("");
  }
}

void setLampen(bool an) {
  anlage.lampenAn = an;
  Actuators::setLampen(an);
}

void schalteMotor(bool &aktiv, bool &richtung, unsigned long &startMs, int pinR,
                  int pinG, unsigned long jetzt) {
  richtung = !richtung;
  aktiv = true;
  startMs = jetzt;
  Actuators::setMotor(pinR, pinG, richtung);
}

void schalteLoop(unsigned long jetzt) {
  schalteMotor(anlage.loopAktiv, anlage.loopRichtung, anlage.loopStartMs,
               PIN_R_MLOOP, PIN_G_MLOOP, jetzt);
}

void schalteRoehre(unsigned long jetzt) {
  schalteMotor(anlage.roehreAktiv, anlage.roehreRichtung, anlage.roehreStartMs,
               PIN_R_MROEHRE, PIN_G_MROEHRE, jetzt);
}

void schalteServo() {
  anlage.servoAuf = !anlage.servoAuf;
  Actuators::setServoWinkel(anlage.servoAuf ? anlage.servoAufWinkel
                                            : anlage.servoZuWinkel);
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
    if (anlage.raceState == WebVisu::RACE_RUNNING) {
      anlage.raceState = WebVisu::RACE_IDLE;
    }
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

  if (pLoop || pRoehre || pServo) {
    anlage.streckenMode = 0; // Manuell
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

void setStreckeIntern(int typ, unsigned long jetzt) {
  // Servo NICHT öffnen wenn Release-Sequenz läuft ODER Aussortieren-Sammlung aktiv (ab Kugel 4)
  bool releaseAktiv = anlage.servoReleasePhase > 0;
  bool sammlung = (anlage.raceState == WebVisu::RACE_RUNNING && 
                   anlage.aussortierenAktiv && 
                   anlage.raceBallsStarted >= 3);
  if (!releaseAktiv && !sammlung) {
    anlage.servoAuf = true;
    Actuators::setServoWinkel(anlage.servoAufWinkel);
  }

  switch (typ) {
  case 1: // Aussortieren
    anlage.servoStartIstRechts = false;
    Actuators::setServoStartWinkel(SERVOSTART_LINKS);
    anlage.roehreRichtung = true;
    anlage.roehreAktiv = true;
    anlage.roehreStartMs = jetzt;
    Actuators::setMotor(PIN_R_MROEHRE, PIN_G_MROEHRE, anlage.roehreRichtung);
    break;
  case 2: // Rampe
    anlage.servoStartIstRechts = false;
    Actuators::setServoStartWinkel(SERVOSTART_LINKS);
    anlage.roehreRichtung = false;
    anlage.roehreAktiv = true;
    anlage.roehreStartMs = jetzt;
    Actuators::setMotor(PIN_R_MROEHRE, PIN_G_MROEHRE, anlage.roehreRichtung);
    break;
  case 3: // Looping
    anlage.servoStartIstRechts = true;
    Actuators::setServoStartWinkel(SERVOSTART_RECHTS);
    anlage.loopRichtung = true;
    anlage.loopAktiv = true;
    anlage.loopStartMs = jetzt;
    Actuators::setMotor(PIN_R_MLOOP, PIN_G_MLOOP, anlage.loopRichtung);
    break;
  case 4: // Gerade
    anlage.servoStartIstRechts = true;
    Actuators::setServoStartWinkel(SERVOSTART_RECHTS);
    anlage.loopRichtung = false;
    anlage.loopAktiv = true;
    anlage.loopStartMs = jetzt;
    Actuators::setMotor(PIN_R_MLOOP, PIN_G_MLOOP, anlage.loopRichtung);
    break;
  }
}

void updateLichtschranken(unsigned long jetzt) {
  if (lichtschrankeOben.triggered()) {
    anlage.kugelnSeitReset++; // Globale Statistik

    int printKugelNummer = (anlage.raceState == WebVisu::RACE_RUNNING)
                               ? ((anlage.raceBallsStarted % 3) + 1)
                               : (((anlage.kugelnSeitReset - 1) % 3) + 1);
    printLichtschranke("Oben", printKugelNummer);

    if (anlage.anlageScharf) {
      // Wenn wir im Aussortieren-Rennen in der Sammelphase sind (ab Kugel 4), 
      // überschreiben wir die Streckenlogik hart auf "Aussortieren" (1).
      bool forceAussortieren = (anlage.raceState == WebVisu::RACE_RUNNING && 
                                anlage.aussortierenAktiv && 
                                anlage.raceBallsStarted >= 3);

      if (forceAussortieren) {
        setStreckeIntern(1, jetzt);
      } else {
        if (anlage.streckenMode == 0) {        // Manuell
          if (anlage.startRichtungMode == 0) { // Alternierend
            anlage.servoStartIstRechts = !anlage.servoStartIstRechts;
            Actuators::setServoStartWinkel(anlage.servoStartIstRechts
                                               ? SERVOSTART_RECHTS
                                               : SERVOSTART_LINKS);
          }
        } else if (anlage.streckenMode == 5) { // Zufall
          int r = random(2, 5);                // 2, 3, 4
          setStreckeIntern(r, jetzt);
        } else if (anlage.streckenMode == 6) { // Gleichmäßig
          static int lastGleich = 1;
          lastGleich++;
          if (lastGleich > 4)
            lastGleich = 2;
          setStreckeIntern(lastGleich, jetzt);
        }
      }

      // Rennen-Logik
      if (anlage.raceState == WebVisu::RACE_RUNNING) {
        // Zeitmessung starten (Nur für die ersten 3 Kugeln im Rennen)
        if (anlage.raceBallsStarted < 3) {
          int idx = anlage.raceBallsStarted;
          kugelHistorie[idx].startMs = jetzt;
          kugelHistorie[idx].endMs = 0;
          kugelHistorie[idx].dauerMs = 0;
          kugelHistorie[idx].aktiv = true;
          kugelHistorie[idx].abgeschlossen = false;
        }

        anlage.raceBallsStarted++;

        if (!anlage.aussortierenAktiv) {
          // Aussortieren AUS
          if (anlage.raceBallsStarted == 3) {
            anlage.aufzugStopMs = jetzt + DELAY_AUFZUG_STOP_MS;
            Serial.print("3 Kugeln erkannt. Aufzug stoppt in ");
            Serial.print(DELAY_AUFZUG_STOP_MS);
            Serial.println(" ms.");
            // Wir setzen RACE_FINISHED hier nicht mehr sofort, sondern warten,
            // bis Kugeln unten ankommen (oder timeout).
          }
        } else {
          // Aussortieren AN
          if (anlage.raceBallsStarted == 4) {
            // 1. Kugel zum 2. Mal oben → Strecke ist durch forceAussortieren bereits auf Oben Links
            // Servo zu (Sammlung beginnt)
            anlage.servoAuf = false;
            Actuators::setServoWinkel(SERVROEHRE_ZU);
          }
          if (anlage.raceBallsStarted == 6) {
            // 3. Kugel zum 2. Mal oben → Aufzug stoppen und Auslass starten
            anlage.aufzugStopMs = jetzt + DELAY_AUFZUG_STOP_MS;
            anlage.servoReleasePhase = 1;
            anlage.servoReleaseNextMs = jetzt + SERVO_RELEASE_DELAY_MS;
            Serial.print("6 Kugeln erkannt. Aufzug stoppt in ");
            Serial.print(DELAY_AUFZUG_STOP_MS);
            Serial.println(" ms. Release-Sequenz startet in ");
            Serial.print(SERVO_RELEASE_DELAY_MS);
            Serial.println(" ms.");
          }
        }
      }
    }
  }

  if (lichtschrankeUnten.triggered()) {
    anlage.kugelnUntenSeitReset++; // Globale Statistik

    int printKugelNummer = (anlage.raceState == WebVisu::RACE_RUNNING)
                               ? ((anlage.raceBallsFinished % 3) + 1)
                               : (((anlage.kugelnUntenSeitReset - 1) % 3) + 1);
    printLichtschranke("Unten", printKugelNummer);

    if (anlage.anlageScharf) {
      if (anlage.raceState == WebVisu::RACE_RUNNING) {
        // Nur so viele unten zählen, wie oben gestartet sind (oder max 3)
        if (anlage.raceBallsFinished < anlage.raceBallsStarted &&
            anlage.raceBallsFinished < 3) {
          int idx = anlage.raceBallsFinished;

          if (kugelHistorie[idx].aktiv) {
            unsigned long dauer = jetzt - kugelHistorie[idx].startMs;

            kugelHistorie[idx].endMs = jetzt;
            kugelHistorie[idx].dauerMs = dauer;
            kugelHistorie[idx].aktiv = false;
            kugelHistorie[idx].abgeschlossen = true;

            anlage.messungAnzahl++;
          }
          anlage.raceBallsFinished++;

          if (!anlage.aussortierenAktiv) {
            // Rennen ohne Aussortieren: Wenn 3. Kugel unten ist, ist das Rennen
            // fertig
            if (anlage.raceBallsFinished == 3) {
              anlage.raceState = WebVisu::RACE_FINISHED;
            }
          } else {
            // Rennen mit Aussortieren: Wenn 3 Kugeln unten sind, Gewinner
            // berechnen
            if (anlage.raceBallsFinished == 3) {
              anlage.winnerIndex = 0;
              unsigned long bestTime = 0xFFFFFFFF;
              for (int i = 0; i < 3; i++) {
                if (kugelHistorie[i].dauerMs > 0 &&
                    kugelHistorie[i].dauerMs < bestTime) {
                  bestTime = kugelHistorie[i].dauerMs;
                  anlage.winnerIndex = i;
                }
              }
            }
          }
        }
      }
    }
  }
}

void updateRaceTasks(unsigned long jetzt) {
  if (anlage.aufzugStopMs > 0 && jetzt >= anlage.aufzugStopMs) {
    anlage.aufzugStopMs = 0;
    if (anlage.aufzugAktiv) {
      anlage.aufzugAktiv = false;
      Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
    }
  }

  if (anlage.servoReleasePhase > 0 && jetzt >= anlage.servoReleaseNextMs) {
    // Kugel-Index für diese Phase (0=unten, 1=mitte, 2=oben)
    int kugelIdx = (anlage.servoReleasePhase - 1) / 2; // 0,0 -> 1,1 -> 2,2
    int subPhase =
        (anlage.servoReleasePhase - 1) % 2; // 0=Strecke+Auf, 1=Zu+Warten

    switch (subPhase) {
    case 0: { // Strecke für aktuelle Kugel einstellen + Servo öffnen
      // Nur Motorrichtung für Röhre einstellen, ohne ServoStart/Weiche neu zu
      // triggern
      bool isWinner = (anlage.winnerIndex == kugelIdx);
      anlage.roehreRichtung =
          isWinner ? false : true; // false=Rampe(Gewinner), true=Aussortieren
      anlage.roehreAktiv = true;
      anlage.roehreStartMs = jetzt;
      Actuators::setMotor(PIN_R_MROEHRE, PIN_G_MROEHRE, anlage.roehreRichtung);

      anlage.servoAuf = true;
      Actuators::setServoWinkel(anlage.servoAufWinkel);
      unsigned long openTime = (kugelIdx == 0)   ? anlage.time3Ball
                               : (kugelIdx == 1) ? anlage.time2Ball
                                                 : anlage.time1Ball;
      anlage.servoReleaseNextMs = jetzt + openTime;
      anlage.servoReleasePhase++;
      Serial.print("Auslass ");
      Serial.print(kugelIdx + 1);
      Serial.print(" Auf! Zeit: ");
      Serial.println(openTime);
      break;
    }
    case 1: { // Servo schließen, warten bis Kugel an Weiche ist
      anlage.servoAuf = false;
      Actuators::setServoWinkel(anlage.servoZuWinkel);
      Serial.print("Auslass ");
      Serial.print(kugelIdx + 1);
      Serial.println(" Zu.");
      if (kugelIdx == 2) {
        // Letzte Kugel -> fertig
        anlage.servoReleasePhase = 0;
        anlage.raceState = WebVisu::RACE_FINISHED;
      } else {
        anlage.servoReleaseNextMs =
            jetzt + DELAY_KUGEL_BIS_WEICHE_MS; // Danach direkt Phase 0 für
                                               // nächste Kugel (Weiche dreht
                                               // sich parallel zum Kugel-Fall)
        anlage.servoReleasePhase++;
      }
      break;
    }
    }
  }
}

void resetRace() {
  anlage.raceState = WebVisu::RACE_IDLE;
  anlage.raceBallsStarted = 0;
  anlage.raceBallsFinished = 0;
  anlage.raceBallsSecondPass = 0;
  for (int i = 0; i < 3; i++) {
    kugelHistorie[i] = {0, 0, 0, false, false};
  }
  anlage.aufzugStopMs = 0;
  anlage.servoReleasePhase = 0;
  anlage.servoReleaseNextMs = 0;
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
  case WebVisu::CMD_RACE_START:
    resetRace();
    anlage.raceState = WebVisu::RACE_RUNNING;
    anlage.streckenMode = anlage.raceStreckenMode;
    if (anlage.streckenMode == 5) {
      setStreckeIntern(random(2, 5),
                       jetzt); // Sofort Strecke stellen, damit nichts auf
                               // Aussortieren stehen bleibt
    } else if (anlage.streckenMode == 6) {
      setStreckeIntern(2, jetzt); // Start mit Default-Wert für Gleichmäßig
    } else {
      setStreckeIntern(anlage.streckenMode, jetzt);
    }
    anlage.aufzugStopMs = 0;
    anlage.servoReleasePhase = 0;

    if (!anlage.aufzugAktiv) {
      anlage.aufzugAktiv = true;
      Actuators::setMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG, true);
    }
    break;
  case WebVisu::CMD_RACE_RESET:
    resetRace();
    if (anlage.aufzugAktiv) {
      anlage.aufzugAktiv = false;
      Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
    }
    break;
  case WebVisu::CMD_TOGGLE_AUSSORTIEREN:
    anlage.aussortierenAktiv = !anlage.aussortierenAktiv;
    break;
  case WebVisu::CMD_TEST_RELEASE:
    if (anlage.aufzugAktiv) {
      anlage.aufzugAktiv = false;
      Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
    }
    anlage.servoAuf = false;
    Actuators::setServoWinkel(anlage.servoZuWinkel);
    anlage.winnerIndex = 0;
    anlage.servoReleasePhase = 1;
    anlage.servoReleaseNextMs = jetzt;
    break;
  case WebVisu::CMD_RACE_STRECKE_SET:
    anlage.raceStreckenMode = WebVisu::consumeCommandArg();
    break;

  case WebVisu::CMD_STRECKE_SET:
    anlage.streckenMode = WebVisu::consumeCommandArg();
    if (anlage.streckenMode >= 1 && anlage.streckenMode <= 4) {
      setStreckeIntern(anlage.streckenMode, jetzt);
    }
    break;
  case WebVisu::CMD_STARTRICHTUNG_SET:
    anlage.streckenMode = 0; // Wechsel in Manuell
    anlage.startRichtungMode = WebVisu::consumeCommandArg();
    if (anlage.startRichtungMode == 1) { // Immer Rechts
      anlage.servoStartIstRechts = true;
      Actuators::setServoStartWinkel(SERVOSTART_RECHTS);
    } else if (anlage.startRichtungMode == 2) { // Immer Links
      anlage.servoStartIstRechts = false;
      Actuators::setServoStartWinkel(SERVOSTART_LINKS);
    }
    break;
  case WebVisu::CMD_RESET_STATS:
    anlage.kugelnSeitReset = 0;
    anlage.kugelnUntenSeitReset = 0;
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
volatile int shockZustandISR = 0;

void shockISR() {
  shockZustandISR = digitalRead(PIN_Shocksensor_IN);
  shockFlankeErkannt = true;
}

void updateShocksensor(unsigned long jetzt) {
  if (shockFlankeErkannt) {
    shockFlankeErkannt = false;
    static unsigned long lastShockMs = 0;
    if (jetzt - lastShockMs >= SHOCKSENSOR_DEBOUNCE_MS) {
      lastShockMs = jetzt;
      Serial.println("Shocksensor Flanke erkannt - LED Toggle!");
      anlage.ledFarbeToggle = !anlage.ledFarbeToggle;
    }
  }
}

void updateLEDs() {
  bool scharf = anlage.anlageScharf;
  digitalWrite(PIN_LED_LICHTSCHRANKE_OBEN, scharf ? HIGH : LOW);
  digitalWrite(PIN_LED_LICHTSCHRANKE_UNTEN, scharf ? HIGH : LOW);

  if (scharf) {
    digitalWrite(PIN_2FarbigeLED_Out1, anlage.ledFarbeToggle ? LOW : HIGH);
    digitalWrite(PIN_2FarbigeLED_Out2, anlage.ledFarbeToggle ? HIGH : LOW);
  } else {
    digitalWrite(PIN_2FarbigeLED_Out1, LOW);
    digitalWrite(PIN_2FarbigeLED_Out2, LOW);
  }
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
  s.raceState = anlage.raceState;
  s.ballsSecondPass = anlage.raceBallsSecondPass;
  s.startRichtungMode = anlage.startRichtungMode;
  s.kugelnSeitReset = anlage.kugelnSeitReset;
  s.aussortierenAktiv = anlage.aussortierenAktiv;
  s.streckenMode = anlage.streckenMode;
  s.raceStreckenMode = anlage.raceStreckenMode;

  s.loopStartMs = anlage.loopStartMs;
  s.roehreStartMs = anlage.roehreStartMs;
  return s;
}

} // namespace

void setup() {
  Actuators::begin();
  Actuators::setServoStartWinkel(anlage.servoStartIstRechts ? SERVOSTART_RECHTS
                                                            : SERVOSTART_LINKS);

  tasterLoop.begin();
  tasterRoehre.begin();
  tasterServoBlockade.begin();
  tasterAufzug.begin();
  pinMode(PIN_LINKS_STELLUNG_SCHALTER0, INPUT_PULLUP);
  pinMode(PIN_LED_LICHTSCHRANKE_OBEN, OUTPUT);
  pinMode(PIN_LED_LICHTSCHRANKE_UNTEN, OUTPUT);
  pinMode(PIN_2FarbigeLED_Out1, OUTPUT);
  // Shocksensor (Hardware Interrupt)
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
  updateRaceTasks(jetzt);
  updateWebCommand(jetzt);
  stoppeZeitMotoren(jetzt);
  updateLampen(jetzt);
  updateShocksensor(jetzt);
  updateLEDs();

  WebVisu::update(webStatus(jetzt));
}
