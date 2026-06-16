#pragma once
#include <Arduino.h>

struct WebVisuState {
    bool wifiConnected;
    IPAddress ip;

    bool aufzugAktiv;
    bool anlageScharf;
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
    bool lichtschrankeOben;
    bool lichtschrankeUnten;

    unsigned long uptimeMs;
    
    struct KugelData {
        unsigned long startMs;
        unsigned long endMs;
        unsigned long dauerMs;
        bool aktiv;
        bool abgeschlossen;
    };
    KugelData kugeln[3];
    unsigned long messungAnzahl;

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
        CMD_AUFZUG_TOGGLE,
        CMD_LAMP_TOGGLE,
        CMD_ALL_STOP
    };

    void begin(const char* ssid, const char* pass);
    void update(const WebVisuState& state);
    Command consumeCommand();
}
