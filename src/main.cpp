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
  int ballsStarted = 0;
  int ballsFinished = 0;
  int ballsSecondPass = 0;
  
  int startRichtungMode = 0; // 0=Alternating, 1=Rechts, 2=Links
  int streckenMode = 0; // 0=Manuell, 1=Aussortieren, 2=Rampe, 3=Looping, 4=Gerade, 5=Zufall, 6=Gleichmaessig
  int kugelnSeitReset = 0;
  bool servoStartIstRechts = true;
  
  bool aussortierenAktiv = false;
  unsigned long aufzugStopMs = 0;
  
  // Servo Auslass Queue
  int servoReleasePhase = 0; // 0=Idle, 1=Wait5s, 2=Auf1, 3=Zu1, 4=Wait3s, 5=Auf2, 6=Zu2, 7=Wait3s, 8=Auf3, 9=Zu3/Done
  unsigned long servoReleaseNextMs = 0;
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

void schalteMotor(bool &aktiv, bool &richtung, unsigned long &startMs,
                  int pinR, int pinG, unsigned long jetzt) {
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
  if (!(anlage.raceState == WebVisu::RACE_RUNNING && anlage.aussortierenAktiv)) {
    anlage.servoAuf = true;
    Actuators::setServoWinkel(SERVROEHRE_AUF);
  }

  switch(typ) {
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
    anlage.kugelnSeitReset++;
    
    int printKugelNummer = ((anlage.kugelnSeitReset - 1) % 3) + 1;
    printLichtschranke("Oben", printKugelNummer);

    if (anlage.anlageScharf) {
      if (anlage.streckenMode == 0) { // Manuell
        if (anlage.startRichtungMode == 0) { // Alternierend
          anlage.servoStartIstRechts = !anlage.servoStartIstRechts;
          Actuators::setServoStartWinkel(anlage.servoStartIstRechts ? SERVOSTART_RECHTS : SERVOSTART_LINKS);
        }
      } else if (anlage.streckenMode == 5) { // Zufall
        int r = random(2, 5); // 2, 3, 4
        setStreckeIntern(r, jetzt);
      } else if (anlage.streckenMode == 6) { // Gleichmäßig
        static int lastGleich = 1;
        lastGleich++;
        if (lastGleich > 4) lastGleich = 2;
        setStreckeIntern(lastGleich, jetzt);
      }

      // Zeitmessung starten
      int idx = anlage.ballsStarted % 3;
      kugelHistorie[idx].startMs = jetzt;
      kugelHistorie[idx].endMs = 0;
      kugelHistorie[idx].dauerMs = 0;
      kugelHistorie[idx].aktiv = true;
      kugelHistorie[idx].abgeschlossen = false;
      
      anlage.ballsStarted++; // Immer hochzählen, resetRace setzt es auf 0

      // Rennen-Logik
      if (anlage.raceState == WebVisu::RACE_RUNNING) {
        
        if (!anlage.aussortierenAktiv) {
          // Aussortieren AUS
          if (anlage.ballsStarted == 3) {
             anlage.aufzugStopMs = jetzt + DELAY_AUFZUG_STOP_MS;
             anlage.raceState = WebVisu::RACE_FINISHED;
          }
        } else {
          // Aussortieren AN
          if (anlage.ballsStarted == 4) {
             // 1. Kugel rollt zum 2. Mal oben durch (Trigger 4)
             anlage.streckenMode = 1; // Aussortieren
             setStreckeIntern(1, jetzt);
             
             // Jetzt Röhre schließen zum Sammeln
             anlage.servoAuf = false;
             Actuators::setServoWinkel(SERVROEHRE_ZU);
          }
          if (anlage.ballsStarted == 6) {
             // 3. Kugel rollt zum 2. Mal oben durch (Trigger 6) -> auslassen beginnen
             anlage.aufzugStopMs = jetzt + DELAY_AUFZUG_STOP_MS;
             anlage.servoReleasePhase = 1;
             anlage.servoReleaseNextMs = jetzt + SERVO_RELEASE_DELAY_MS;
          }
        }
      }
    }
  }

  if (lichtschrankeUnten.triggered()) {
    if (anlage.ballsFinished < anlage.kugelnSeitReset) {
      anlage.ballsFinished++;
      
      int printKugelNummer = ((anlage.ballsFinished - 1) % 3) + 1;
      printLichtschranke("Unten", printKugelNummer);

      if (anlage.anlageScharf) {
        if (anlage.ballsFinished <= anlage.ballsStarted) {
          int idx = (anlage.ballsFinished - 1) % 3;
          if (kugelHistorie[idx].aktiv) {
            unsigned long dauer = jetzt - kugelHistorie[idx].startMs;
            
            kugelHistorie[idx].endMs = jetzt;
            kugelHistorie[idx].dauerMs = dauer;
            kugelHistorie[idx].aktiv = false;
            kugelHistorie[idx].abgeschlossen = true;
            
            anlage.messungAnzahl++;
          }
        }
      }
    } else {
      Serial.println("LS Unten: Erkannt");
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
    switch (anlage.servoReleasePhase) {
      case 1: // Wait5s passed -> Auf1 (3 Kugeln drin)
        anlage.servoAuf = true;
        Actuators::setServoWinkel(SERVROEHRE_AUF);
        anlage.servoReleasePhase = 2;
        anlage.servoReleaseNextMs = jetzt + SERVO_OPEN3BALL_MS;
        break;
      case 2: // Auf1 passed -> Zu1
        anlage.servoAuf = false;
        Actuators::setServoWinkel(SERVROEHRE_ZU);
        anlage.servoReleasePhase = 3;
        anlage.servoReleaseNextMs = jetzt + SERVO_WAIT_MS;
        break;
      case 3: // Wait3s passed -> Auf2 (2 Kugeln drin)
        anlage.servoAuf = true;
        Actuators::setServoWinkel(SERVROEHRE_AUF);
        anlage.servoReleasePhase = 4;
        anlage.servoReleaseNextMs = jetzt + SERVO_OPEN2BALL_MS;
        break;
      case 4: // Auf2 passed -> Zu2
        anlage.servoAuf = false;
        Actuators::setServoWinkel(SERVROEHRE_ZU);
        anlage.servoReleasePhase = 5;
        anlage.servoReleaseNextMs = jetzt + SERVO_WAIT_MS;
        break;
      case 5: // Wait3s passed -> Auf3 (1 Kugel drin)
        anlage.servoAuf = true;
        Actuators::setServoWinkel(SERVROEHRE_AUF);
        anlage.servoReleasePhase = 6;
        anlage.servoReleaseNextMs = jetzt + SERVO_OPEN1BALL_MS;
        break;
      case 6: // Auf3 passed -> Zu3
        anlage.servoAuf = false;
        Actuators::setServoWinkel(SERVROEHRE_ZU);
        anlage.servoReleasePhase = 0;
        anlage.aufzugStopMs = jetzt + DELAY_AUFZUG_STOP_MS;
        anlage.raceState = WebVisu::RACE_FINISHED;
        break;
    }
  }
}

void resetRace() {
  anlage.raceState = WebVisu::RACE_IDLE;
  anlage.ballsStarted = 0;
  anlage.ballsFinished = 0;
  anlage.ballsSecondPass = 0;
  anlage.kugelnSeitReset = 0;
  for(int i=0; i<3; i++) {
    kugelHistorie[i] = {0,0,0,false,false};
  }
  if (anlage.aufzugAktiv) {
    anlage.aufzugAktiv = false;
    Actuators::stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
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
  case WebVisu::CMD_RACE_START:
    resetRace();
    anlage.raceState = WebVisu::RACE_RUNNING;
    anlage.streckenMode = 5; // Zufall
    setStreckeIntern(random(2, 5), jetzt); // Sofort Strecke stellen, damit nichts auf Aussortieren stehen bleibt
    anlage.aufzugStopMs = 0;
    anlage.servoReleasePhase = 0;
    
    if (!anlage.aufzugAktiv) {
      anlage.aufzugAktiv = true;
      Actuators::setMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG, true);
    }
    break;
  case WebVisu::CMD_RACE_RESET:
    resetRace();
    break;
  case WebVisu::CMD_TOGGLE_AUSSORTIEREN:
    anlage.aussortierenAktiv = !anlage.aussortierenAktiv;
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
  s.raceState = anlage.raceState;
  s.ballsSecondPass = anlage.ballsSecondPass;
  s.startRichtungMode = anlage.startRichtungMode;
  s.kugelnSeitReset = anlage.kugelnSeitReset;
  s.aussortierenAktiv = anlage.aussortierenAktiv;
  s.streckenMode = anlage.streckenMode;

  s.loopStartMs = anlage.loopStartMs;
  s.roehreStartMs = anlage.roehreStartMs;
  return s;
}

} // namespace

void setup() {
  Actuators::begin();
  Actuators::setServoStartWinkel(anlage.servoStartIstRechts ? SERVOSTART_RECHTS : SERVOSTART_LINKS);

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
  updateRaceTasks(jetzt);
  updateWebCommand(jetzt);
  stoppeZeitMotoren(jetzt);
  updateLampen(jetzt);
  updateShocksensor(jetzt);
  updateLEDs();

  WebVisu::update(webStatus(jetzt));
}
