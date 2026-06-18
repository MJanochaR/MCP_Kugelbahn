#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "webvisu.h"
#include "webvisu_frontend.h"

namespace {
    WiFiServer server(80);
    WebVisu::Command pendingCommand = WebVisu::CMD_NONE;
    int pendingCommandArg = 0;
    WebVisuState lastState = {};

    bool serverStarted = false;
    unsigned long lastReconnectMs = 0;
    unsigned long lastStatusMs = 0;
    bool lastSerialState = false;

    const char* onOff(bool value) {
        return value ? "true" : "false";
    }

    void printUptime(unsigned long now) {
        unsigned long totalSeconds = now / 1000;
        unsigned long hours = totalSeconds / 3600;
        unsigned long minutes = (totalSeconds / 60) % 60;
        unsigned long seconds = totalSeconds % 60;

        if (hours < 10) Serial.print('0');
        Serial.print(hours);
        Serial.print(':');
        if (minutes < 10) Serial.print('0');
        Serial.print(minutes);
        Serial.print(':');
        if (seconds < 10) Serial.print('0');
        Serial.print(seconds);
        Serial.print(' ');
    }

    const char* getWifiStatusString(int status) {
        switch (status) {
            case WL_NO_SHIELD: return "Kein WiFi-Modul gefunden";
            case WL_IDLE_STATUS: return "Verbindung wird aufgebaut...";
            case WL_NO_SSID_AVAIL: return "SSID nicht gefunden";
            case WL_SCAN_COMPLETED: return "Scan abgeschlossen";
            case WL_CONNECTED: return "Verbunden";
            case WL_CONNECT_FAILED: return "Verbindung fehlgeschlagen";
            case WL_CONNECTION_LOST: return "Verbindung verloren";
            case WL_DISCONNECTED: return "Getrennt / Warte auf Reconnect";
            default: return "Unbekannt";
        }
    }

    bool isWifiActive() {
        int status = WiFi.status();
        // Fallback: If IP is not 0.0.0.0, we are connected or listening
        IPAddress ip = WiFi.localIP();
        bool hasIP = (ip[0] != 0);
        return (status == WL_CONNECTED || status == WL_AP_LISTENING || status == WL_AP_CONNECTED || (WIFI_AP_MODE && hasIP));
    }

    void printWebVisuStatus(const char* message, bool force = false) {
        unsigned long now = millis();
        bool active = isWifiActive();
        unsigned long interval = active ? 300000 : 10000; // 5 Minuten wenn aktiv, 10 Sekunden wenn inaktiv

        if (!force && now - lastStatusMs < interval) return;

        lastStatusMs = now;
        printUptime(now);
        Serial.print("WebVisu: ");
        Serial.print(message);
        Serial.print(" | WLAN=");
        Serial.print(active ? (WIFI_AP_MODE ? "AP_ACTIVE" : "OK") : "OFF");
        if (!active) {
            Serial.print(" (Grund: ");
            Serial.print(getWifiStatusString(WiFi.status()));
            Serial.print(")");
        }
        Serial.print(" | IP=");
        Serial.println(WiFi.localIP());
    }

    void checkSerialMonitorTrigger() {
        bool currentSerialState = Serial;
        if (currentSerialState && !lastSerialState) {
            printWebVisuStatus("Serial Monitor verbunden (Status-Dump)", true);
        }
        lastSerialState = currentSerialState;
    }

    void configureWifi() {
        if (WIFI_USE_STATIC_IP) {
            WiFi.config(WIFI_LOCAL_IP, WIFI_DNS, WIFI_GATEWAY, WIFI_SUBNET);
        }
    }

    void startServer() {
        if (serverStarted) return;

        server.begin();
        serverStarted = true;
        printWebVisuStatus("Server bereit", true);
        Serial.print("--> Web-Interface erreichbar unter: http://");
        Serial.println(WiFi.localIP());
    }

    void connectWifi(const char* ssid, const char* pass) {
        if (!WIFI_AP_MODE) {
            WiFi.disconnect();
            delay(1000); // Dem Modul Zeit zum Zurücksetzen geben
        }

        if (WIFI_AP_MODE) {
            if (WIFI_USE_STATIC_IP) {
                WiFi.config(WIFI_LOCAL_IP, WIFI_LOCAL_IP, WIFI_SUBNET);
            }
            if (pass == nullptr || strlen(pass) == 0) {
                Serial.println("WebVisu Warning: Kein Passwort angegeben. AP wird OFFEN gestartet.");
                WiFi.beginAP(ssid, "");
            } else if (strlen(pass) < 8) {
                Serial.println("WebVisu Warning: WPA2 erfordert mind. 8 Zeichen. AP-Passwort wird auf '12345678' gesetzt.");
                WiFi.beginAP(ssid, "12345678");
            } else {
                int result = WiFi.beginAP(ssid, pass);
                if (result != WL_AP_LISTENING) {
                    Serial.print("AP Start Fehler-Code: ");
                    Serial.println(result);
                }
            }
        } else {
            configureWifi();
            WiFi.begin(ssid, pass);
        }
    }

    void reconnectIfNeeded() {
        if (isWifiActive()) {
            startServer();
            printWebVisuStatus(WIFI_AP_MODE ? "Access Point aktiv" : "Server aktiv");
            return;
        }

        printWebVisuStatus(WIFI_AP_MODE ? "Access Point inaktiv" : "WLAN getrennt");

        unsigned long now = millis();
        if (now - lastReconnectMs < WIFI_RECONNECT_MS) return;

        lastReconnectMs = now;
        connectWifi(WIFI_SSID, WIFI_PASS);
    }

    String requestPath(const String& line) {
        int firstSpace = line.indexOf(' ');
        if (firstSpace < 0) return "";

        int secondSpace = line.indexOf(' ', firstSpace + 1);
        if (secondSpace < 0) {
            return line.substring(firstSpace + 1);
        }

        return line.substring(firstSpace + 1, secondSpace);
    }

    bool isRootPath(const String& path) {
        return path == "/" || path == "/index.html" || path.startsWith("/?");
    }

    void sendHeader(WiFiClient& client, const char* status, const char* type) {
        client.print("HTTP/1.1 ");
        client.println(status);
        client.print("Content-Type: ");
        client.println(type);
        client.println("Cache-Control: no-store");
        client.println("Connection: close");
        client.println();
    }

    void sendPlain(WiFiClient& client, const char* status, const char* text) {
        sendHeader(client, status, "text/plain; charset=utf-8");
        client.println(text);
    }

    void sendJson(WiFiClient& client) {
        sendHeader(client, "200 OK", "application/json; charset=utf-8");
        client.print('{');
        client.print("\"uptimeMs\":"); client.print(lastState.uptimeMs); client.print(',');
        client.print("\"kugeln\":[");
        for(int i=0; i<3; i++) {
            client.print("{\"startMs\":"); client.print(lastState.kugeln[i].startMs);
            client.print(",\"endMs\":"); client.print(lastState.kugeln[i].endMs);
            client.print(",\"dauerMs\":"); client.print(lastState.kugeln[i].dauerMs);
            client.print(",\"aktiv\":"); client.print(onOff(lastState.kugeln[i].aktiv));
            client.print(",\"abgeschlossen\":"); client.print(onOff(lastState.kugeln[i].abgeschlossen));
            client.print(",\"strecke\":"); client.print(lastState.kugeln[i].strecke);
            client.print("}");
            if (i < 2) client.print(",");
        }
        client.print("],");
        client.print("\"messungAnzahl\":"); client.print(lastState.messungAnzahl); client.print(',');
        client.print("\"raceState\":"); client.print(lastState.raceState); client.print(',');
        client.print("\"ballsSecondPass\":"); client.print(lastState.ballsSecondPass); client.print(',');
        client.print("\"aussortierenAktiv\":"); client.print(onOff(lastState.aussortierenAktiv)); client.print(',');
        client.print("\"startRichtungMode\":"); client.print(lastState.startRichtungMode); client.print(',');
        client.print("\"streckenMode\":"); client.print(lastState.streckenMode); client.print(',');
        client.print("\"raceStreckenMode\":"); client.print(lastState.raceStreckenMode); client.print(',');
        client.print("\"kugelnSeitReset\":"); client.print(lastState.kugelnSeitReset); client.print(',');
        client.print("\"alltimeFastestMs\":"); client.print(lastState.alltimeFastestMs); client.print(',');
        client.print("\"alltimeFastestStrecke\":"); client.print(lastState.alltimeFastestStrecke); client.print(',');
        
        client.print("\"fastestMsPerStrecke\":[");
        for(int i=0; i<5; i++) {
            client.print(lastState.fastestMsPerStrecke[i]);
            if(i<4) client.print(",");
        }
        client.print("],");

        client.print("\"runsPerStrecke\":[");
        for(int i=0; i<5; i++) {
            client.print(lastState.runsPerStrecke[i]);
            if(i<4) client.print(",");
        }
        client.print("],");

        client.print("\"aussortierteKugelnGesamt\":"); client.print(lastState.aussortierteKugelnGesamt); client.print(',');

        client.print("\"loopStartMs\":"); client.print(lastState.loopStartMs); client.print(',');
        client.print("\"roehreStartMs\":"); client.print(lastState.roehreStartMs); client.print(',');
        client.print("\"aufzugAktiv\":"); client.print(onOff(lastState.aufzugAktiv)); client.print(',');
        client.print("\"anlageScharf\":"); client.print(onOff(lastState.anlageScharf)); client.print(',');
        client.print("\"loopAktiv\":"); client.print(onOff(lastState.loopAktiv)); client.print(',');
        client.print("\"loopRichtung\":"); client.print(onOff(lastState.loopRichtung)); client.print(',');
        client.print("\"roehreAktiv\":"); client.print(onOff(lastState.roehreAktiv)); client.print(',');
        client.print("\"roehreRichtung\":"); client.print(onOff(lastState.roehreRichtung)); client.print(',');
        client.print("\"servoAuf\":"); client.print(onOff(lastState.servoAuf)); client.print(',');
        client.print("\"lampenAn\":"); client.print(onOff(lastState.lampenAn)); client.print(',');


        client.print("\"taster0\":"); client.print(onOff(lastState.taster0)); client.print(',');
        client.print("\"taster1\":"); client.print(onOff(lastState.taster1)); client.print(',');
        client.print("\"taster2\":"); client.print(onOff(lastState.taster2)); client.print(',');
        client.print("\"taster3\":"); client.print(onOff(lastState.taster3)); client.print(',');
        client.print("\"schalterLinks\":"); client.print(onOff(lastState.schalterLinks)); client.print(',');
        client.print("\"lichtschrankeOben\":"); client.print(onOff(lastState.lichtschrankeOben)); client.print(',');
        client.print("\"lichtschrankeUnten\":"); client.print(onOff(lastState.lichtschrankeUnten));
        client.print('}');
    }

    void parseArg(const String& req, WebVisu::Command cmd, const char* key) {
        pendingCommand = cmd;
        int posIdx = req.indexOf(key);
        if (posIdx > 0) {
            int endIdx = req.indexOf(' ', posIdx);
            if (endIdx < 0) endIdx = req.indexOf('&', posIdx);
            if (endIdx < 0) endIdx = req.length();
            pendingCommandArg = req.substring(posIdx + strlen(key), endIdx).toInt();
        }
    }

    void rememberCommand(const String& requestLine) {
        if (requestLine.indexOf("do=loop") >= 0) pendingCommand = WebVisu::CMD_LOOP_TOGGLE;
        else if (requestLine.indexOf("do=roehre") >= 0) pendingCommand = WebVisu::CMD_ROEHRE_TOGGLE;
        else if (requestLine.indexOf("do=startrichtung_set") >= 0) {
            pendingCommand = WebVisu::CMD_STARTRICHTUNG_SET;
            int posIdx = requestLine.indexOf("mode=");
            if (posIdx >= 0) {
                int endIdx = requestLine.indexOf(' ', posIdx);
                if (endIdx < 0) endIdx = requestLine.indexOf('&', posIdx);
                if (endIdx < 0) endIdx = requestLine.length();
                pendingCommandArg = requestLine.substring(posIdx + 5, endIdx).toInt();
            }
        }
        else if (requestLine.indexOf("do=servo") >= 0) pendingCommand = WebVisu::CMD_SERVO_TOGGLE;
        else if (requestLine.indexOf("do=aufzug") >= 0) pendingCommand = WebVisu::CMD_AUFZUG_TOGGLE;
        else if (requestLine.indexOf("do=lamp") >= 0) pendingCommand = WebVisu::CMD_LAMP_TOGGLE;
        else if (requestLine.indexOf("do=stop") >= 0) pendingCommand = WebVisu::CMD_ALL_STOP;
        else if (requestLine.indexOf("do=race_start") >= 0) pendingCommand = WebVisu::CMD_RACE_START;
        else if (requestLine.indexOf("do=race_reset") >= 0) pendingCommand = WebVisu::CMD_RACE_RESET;
        else if (requestLine.indexOf("do=reset_stats") >= 0) pendingCommand = WebVisu::CMD_RESET_STATS;
        else if (requestLine.indexOf("do=toggle_aussortieren") >= 0) pendingCommand = WebVisu::CMD_TOGGLE_AUSSORTIEREN;
        else if (requestLine.indexOf("do=test_release") >= 0) pendingCommand = WebVisu::CMD_TEST_RELEASE;
        else if (requestLine.indexOf("do=race_strecke_set") >= 0) parseArg(requestLine, WebVisu::CMD_RACE_STRECKE_SET, "mode=");
        else if (requestLine.indexOf("do=strecke_set") >= 0) parseArg(requestLine, WebVisu::CMD_STRECKE_SET, "mode=");

    }

    void skipHeaders(WiFiClient& client) {
        unsigned long start = millis();
        while (client.connected() && millis() - start < 30) {
            if (!client.available()) {
                delay(1);
                continue;
            }
            String line = client.readStringUntil('\n');
            if (line == "\r" || line.length() == 0) return;
        }
    }

    String readRequestLine(WiFiClient& client) {
        unsigned long start = millis();
        while (client.connected() && millis() - start < 150) {
            if (!client.available()) {
                delay(1);
                continue;
            }

            String line = client.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) return line;
        }

        return "";
    }

    void handleClient(WiFiClient& client) {
        client.setTimeout(200);

        String requestLine = readRequestLine(client);
        skipHeaders(client);
        String path = requestPath(requestLine);

        if (requestLine.length() == 0) {
            Serial.println("WebVisu: leerer HTTP-Request");
            sendPlain(client, "400 Bad Request", "Leerer Request");
        } else if (path.startsWith("/api/status")) {
            sendJson(client);
        } else if (path.startsWith("/cmd")) {
            rememberCommand(requestLine);
            sendPlain(client, "200 OK", "OK");
        } else if (isRootPath(path)) {
            WebVisuFrontend::sendIndex(client);
        } else if (path == "/favicon.ico") {
            sendPlain(client, "204 No Content", "");
        } else {
            Serial.print("WebVisu: unbekannter Pfad ");
            Serial.println(path);
            sendPlain(client, "404 Not Found", "Nicht gefunden");
        }

        client.flush();
        client.stop();
    }
}

namespace WebVisu {

void begin(const char* ssid, const char* pass) {
    Serial.begin(115200);
    printWebVisuStatus("Start", true);
    connectWifi(ssid, pass);

    unsigned long start = millis();
    while (!isWifiActive() && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(100);
    }

    if (isWifiActive()) {
        startServer();
    } else {
        printWebVisuStatus(WIFI_AP_MODE ? "AP Start fehlgeschlagen" : "WLAN Start fehlgeschlagen", true);
    }
}

void update(const WebVisuState& state) {
    checkSerialMonitorTrigger();
    lastState = state;
    reconnectIfNeeded();

    WiFiClient client = server.accept();
    if (client) handleClient(client);
}

Command consumeCommand() {
    Command command = pendingCommand;
    pendingCommand = CMD_NONE;
    return command;
}

int consumeCommandArg() {
    int arg = pendingCommandArg;
    pendingCommandArg = 0;
    return arg;
}

}
