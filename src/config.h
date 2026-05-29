#pragma once
#include <Arduino.h>

// Motoren
#define PIN_R_MAUFZUG      17
#define PIN_G_MAUFZUG      16
#define PIN_R_MROEHRE      5
#define PIN_G_MROEHRE      4
#define PIN_R_MLOOP        7
#define PIN_G_MLOOP        6

// Servos
#define PIN_SERVROEHRE     3

// Lampen
#define PIN_R_LAMPE_GRUEN  12
#define PIN_R_LAMPE_ROT    13

// Taster
#define PIN_TASTER0        A0
#define PIN_TASTER1        A1
#define PIN_TASTER2        A2
#define PIN_TASTER3        A3

// Schalter
#define PIN_LINKS_STELLUNG_SCHALTER0 A4

// Constants
const unsigned long LAMP_BLINK_MS = 1000;
const unsigned long MOTOR_RUN_MS = 600;
const unsigned long BUTTON_DEBOUNCE_MS = 50;

const int SERVROEHRE_AUF = 20;
const int SERVROEHRE_ZU = 56;