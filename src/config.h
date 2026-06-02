#pragma once
#include <Arduino.h>

#define PIN_R_MAUFZUG 17
#define PIN_G_MAUFZUG 16
#define PIN_R_MROEHRE 5
#define PIN_G_MROEHRE 4
#define PIN_R_MLOOP 7
#define PIN_G_MLOOP 6

#define PIN_SERVROEHRE 3

#define PIN_R_LAMPE_GRUEN 12
#define PIN_R_LAMPE_ROT 13

#define PIN_TASTER0 A0
#define PIN_TASTER1 A1
#define PIN_TASTER2 A2
#define PIN_TASTER3 A3

#define PIN_LINKS_STELLUNG_SCHALTER0 A4

#define WIFI_SSID "TP-Link_2B5A"
#define WIFI_PASS "65817742"

// DHCP ist robuster. Für feste IP auf true setzen.
const bool WIFI_USE_STATIC_IP = false;
const IPAddress WIFI_LOCAL_IP(192, 168, 0, 253);
const IPAddress WIFI_GATEWAY(192, 168, 0, 1);
const IPAddress WIFI_SUBNET(255, 255, 255, 0);
const IPAddress WIFI_DNS(192, 168, 0, 1);

const unsigned long WEBVISU_STATUS_MS = 10000;
const unsigned long WIFI_RECONNECT_MS = 10000;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long LAMP_BLINK_MS = 1000;
const unsigned long MOTOR_RUN_MS = 600;
const unsigned long BUTTON_DEBOUNCE_MS = 50;

const int SERVROEHRE_AUF = 20;
const int SERVROEHRE_ZU = 56;
