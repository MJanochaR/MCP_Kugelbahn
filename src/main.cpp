#include <Arduino.h>
#include <mbed.h>

// Outputs - Dual Pins
#define PIN_R_MAUFZUG      17
#define PIN_G_MAUFZUG      16
#define PIN_R_MROEHRE      5
#define PIN_G_MROEHRE      4
#define PIN_R_MLOOP        7
#define PIN_G_MLOOP        6

// Servo
#define PIN_SERVO          3

// Outputs - Single Pins
#define PIN_R_LAMPE_GRUEN  12
#define PIN_R_LAMPE_ROT    13

// Inputs - Single Pins
#define PIN_TASTER0        A0
#define PIN_TASTER1        A1
#define PIN_TASTER2        A2
#define PIN_TASTER3        A3

// Inputs - Multi Pins
#define PIN_LINKS_STELLUNG_Schalter0 A4

// Servo PWM
mbed::PwmOut servoPin(digitalPinToPinName(PIN_SERVO));

const int SERVO_ZU = 40;
const int SERVO_AUF = 60;
bool servoAuf = false;

// Variablen
unsigned long letzterWechsel = 0;
const unsigned long wechselIntervall = 1000;
bool lampenAn = false;

// Motor-Test Tastersteuerung
bool richtungLoop = false;
bool richtungRoehre = false;

bool motorLoopAktiv = false;
bool motorRoehreAktiv = false;

unsigned long startLoop = 0;
unsigned long startRoehre = 0;

const unsigned long motorLaufzeit = 1000;

bool letzterTaster0 = HIGH;
bool letzterTaster1 = HIGH;
bool letzterTaster2 = HIGH;

void setMotor(int pinR, int pinG, bool richtung) {
    if (richtung) {
        digitalWrite(pinR, HIGH);
        digitalWrite(pinG, LOW);
    } else {
        digitalWrite(pinR, LOW);
        digitalWrite(pinG, HIGH);
    }
}

void stopMotor(int pinR, int pinG) {
    digitalWrite(pinR, LOW);
    digitalWrite(pinG, LOW);
}

void setServoWinkel(int winkel) {
    int puls = map(winkel, 0, 180, 500, 2500);
    servoPin.period_ms(20);
    servoPin.pulsewidth_us(puls);
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_R_MAUFZUG, OUTPUT);
    pinMode(PIN_G_MAUFZUG, OUTPUT);
    pinMode(PIN_R_MROEHRE, OUTPUT);
    pinMode(PIN_G_MROEHRE, OUTPUT);
    pinMode(PIN_R_MLOOP, OUTPUT);
    pinMode(PIN_G_MLOOP, OUTPUT);

    pinMode(PIN_R_LAMPE_GRUEN, OUTPUT);
    pinMode(PIN_R_LAMPE_ROT, OUTPUT);

    pinMode(PIN_TASTER0, INPUT_PULLUP);
    pinMode(PIN_TASTER1, INPUT_PULLUP);
    pinMode(PIN_TASTER2, INPUT_PULLUP);
    pinMode(PIN_TASTER3, INPUT_PULLUP);

    pinMode(PIN_LINKS_STELLUNG_Schalter0, INPUT_PULLUP);

    stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
    stopMotor(PIN_R_MROEHRE, PIN_G_MROEHRE);
    stopMotor(PIN_R_MLOOP, PIN_G_MLOOP);

    setServoWinkel(SERVO_ZU);
}

void loop() {
    unsigned long jetzt = millis();

    // Aufzugsmotor steuern
    if (digitalRead(PIN_LINKS_STELLUNG_Schalter0) == LOW) {
        setMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG, true);
    } else {
        stopMotor(PIN_R_MAUFZUG, PIN_G_MAUFZUG);
    }

    // Taster lesen
    bool taster0 = digitalRead(PIN_TASTER0);
    bool taster1 = digitalRead(PIN_TASTER1);
    bool taster2 = digitalRead(PIN_TASTER2);

    // Taster0 -> Loop Motor
    if (letzterTaster0 == HIGH && taster0 == LOW) {
        richtungLoop = !richtungLoop;
        motorLoopAktiv = true;
        startLoop = millis();

        setMotor(PIN_R_MLOOP, PIN_G_MLOOP, richtungLoop);
    }

    // Taster1 -> Roehre Motor
    if (letzterTaster1 == HIGH && taster1 == LOW) {
        richtungRoehre = !richtungRoehre;
        motorRoehreAktiv = true;
        startRoehre = millis();

        setMotor(PIN_R_MROEHRE, PIN_G_MROEHRE, richtungRoehre);
    }

    // Taster2 -> Servo Auf / Zu
    if (letzterTaster2 == HIGH && taster2 == LOW) {
        servoAuf = !servoAuf;

        if (servoAuf) {
            setServoWinkel(SERVO_AUF);
        } else {
            setServoWinkel(SERVO_ZU);
        }
    }

    // Loop Motor nach 1 Sekunde stoppen
    if (motorLoopAktiv && millis() - startLoop >= motorLaufzeit) {
        stopMotor(PIN_R_MLOOP, PIN_G_MLOOP);
        motorLoopAktiv = false;
    }

    // Roehre Motor nach 1 Sekunde stoppen
    if (motorRoehreAktiv && millis() - startRoehre >= motorLaufzeit) {
        stopMotor(PIN_R_MROEHRE, PIN_G_MROEHRE);
        motorRoehreAktiv = false;
    }

    // alte Tasterwerte speichern
    letzterTaster0 = taster0;
    letzterTaster1 = taster1;
    letzterTaster2 = taster2;

    // Lampen blinken
    if (jetzt - letzterWechsel >= wechselIntervall) {
        letzterWechsel = jetzt;

        lampenAn = !lampenAn;
        digitalWrite(PIN_R_LAMPE_GRUEN, lampenAn ? HIGH : LOW);
        digitalWrite(PIN_R_LAMPE_ROT, lampenAn ? HIGH : LOW);
    }   
}