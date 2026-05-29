#pragma once
#include <Arduino.h>

struct WebVisuState {
    bool wifiConnected;
    IPAddress ip;

    bool aufzugAktiv;
    bool loopAktiv;
    bool loopRichtung;
    bool roehreAktiv;
    bool roehreRichtung;
    bool servoAuf;
    bool lampenAn;

    bool taster0;
    bool taster1;
    bool taster2;
    bool taster3;
    bool schalterLinks;

    unsigned long uptimeMs;
    unsigned long loopStartMs;
    unsigned long roehreStartMs;
    unsigned long loopSchaltungen;
    unsigned long roehreSchaltungen;
    unsigned long servoSchaltungen;
};

namespace WebVisu {
    enum Command {
        CMD_NONE,
        CMD_LOOP_TOGGLE,
        CMD_ROEHRE_TOGGLE,
        CMD_SERVO_TOGGLE,
        CMD_LAMP_TOGGLE,
        CMD_ALL_STOP
    };

    void begin(const char* ssid, const char* pass);
    void update(const WebVisuState& state);
    Command consumeCommand();
}
