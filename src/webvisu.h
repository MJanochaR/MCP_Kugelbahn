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
        int strecke;
    };
    KugelData kugeln[3];
    unsigned long messungAnzahl;
    
    int raceState;
    int ballsSecondPass;
    
    int startRichtungMode;
    int kugelnSeitReset;
    bool aussortierenAktiv;
    int streckenMode; // 0=Manuell, 1=Aussortieren, 2=Rampe, 3=Looping, 4=Gerade, 5=Zufall, 6=Gleichmaessig
    int raceStreckenMode;

    unsigned long alltimeFastestMs;
    int alltimeFastestStrecke;
    unsigned long fastestMsPerStrecke[5];
    int runsPerStrecke[5];
    int aussortierteKugelnGesamt;

    unsigned long loopStartMs;
    unsigned long roehreStartMs;
};

namespace WebVisu {
    enum RaceState {
        RACE_IDLE,
        RACE_RUNNING,
        RACE_FINISHED
    };
    
    enum Command {
        CMD_NONE,
        CMD_LOOP_TOGGLE,
        CMD_ROEHRE_TOGGLE,
        CMD_SERVO_TOGGLE,
        CMD_AUFZUG_TOGGLE,
        CMD_LAMP_TOGGLE,
        CMD_ALL_STOP,
        CMD_RACE_START,
        CMD_RACE_RESET,
        CMD_STARTRICHTUNG_SET,
        CMD_RESET_STATS,
        CMD_TOGGLE_AUSSORTIEREN,
        CMD_STRECKE_SET,
        CMD_RACE_STRECKE_SET,
        CMD_TEST_RELEASE
    };

    void begin(const char* ssid, const char* pass);
    void update(const WebVisuState& state);
    Command consumeCommand();
    int consumeCommandArg();
}
