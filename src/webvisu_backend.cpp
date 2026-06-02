#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "webvisu.h"
#include "webvisu_frontend.h"

namespace {
    WiFiServer server(80);
    WebVisu::Command pendingCommand = WebVisu::CMD_NONE;
    WebVisuState lastState = {};

    bool serverStarted = false;
    unsigned long lastReconnectMs = 0;
    unsigned long lastStatusMs = 0;

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

    void printWebVisuStatus(const char* message, bool force = false) {
        unsigned long now = millis();
        if (!force && now - lastStatusMs < WEBVISU_STATUS_MS) return;

        lastStatusMs = now;
        printUptime(now);
        Serial.print("WebVisu: ");
        Serial.print(message);
        Serial.print(" | WLAN=");
        Serial.print(WiFi.status() == WL_CONNECTED ? "OK" : "OFF");
        Serial.print(" | IP=");
        Serial.println(WiFi.localIP());
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
    }

    void connectWifi(const char* ssid, const char* pass) {
        configureWifi();
        WiFi.begin(ssid, pass);
    }

    void reconnectIfNeeded() {
        if (WiFi.status() == WL_CONNECTED) {
            startServer();
            printWebVisuStatus("Server aktiv");
            return;
        }

        printWebVisuStatus("WLAN getrennt");

        unsigned long now = millis();
        if (now - lastReconnectMs < WIFI_RECONNECT_MS) return;

        lastReconnectMs = now;
        WiFi.disconnect();
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
        client.print("\"wifi\":"); client.print(onOff(lastState.wifiConnected)); client.print(',');
        client.print("\"ip\":\""); client.print(lastState.ip); client.print("\",");
        client.print("\"uptimeMs\":"); client.print(lastState.uptimeMs); client.print(',');
        client.print("\"messungAktiv\":"); client.print(onOff(lastState.messungAktiv)); client.print(',');
        client.print("\"messungStartMs\":"); client.print(lastState.messungStartMs); client.print(',');
        client.print("\"messungLetzteMs\":"); client.print(lastState.messungLetzteMs); client.print(',');
        client.print("\"messungAnzahl\":"); client.print(lastState.messungAnzahl); client.print(',');
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
        client.print("\"lichtschrankeUnten\":"); client.print(onOff(lastState.lichtschrankeUnten)); client.print(',');
        client.print("\"loopSchaltungen\":"); client.print(lastState.loopSchaltungen); client.print(',');
        client.print("\"roehreSchaltungen\":"); client.print(lastState.roehreSchaltungen); client.print(',');
        client.print("\"servoSchaltungen\":"); client.print(lastState.servoSchaltungen);
        client.print('}');
    }

    void rememberCommand(const String& requestLine) {
        if (requestLine.indexOf("do=loop") >= 0) pendingCommand = WebVisu::CMD_LOOP_TOGGLE;
        else if (requestLine.indexOf("do=roehre") >= 0) pendingCommand = WebVisu::CMD_ROEHRE_TOGGLE;
        else if (requestLine.indexOf("do=servo") >= 0) pendingCommand = WebVisu::CMD_SERVO_TOGGLE;
        else if (requestLine.indexOf("do=aufzug") >= 0) pendingCommand = WebVisu::CMD_AUFZUG_TOGGLE;
        else if (requestLine.indexOf("do=lamp") >= 0) pendingCommand = WebVisu::CMD_LAMP_TOGGLE;
        else if (requestLine.indexOf("do=stop") >= 0) pendingCommand = WebVisu::CMD_ALL_STOP;
    }

    void skipHeaders(WiFiClient& client) {
        unsigned long start = millis();
        while (client.connected() && millis() - start < 50) {
            if (!client.available()) continue;
            String line = client.readStringUntil('\n');
            if (line == "\r" || line.length() == 0) return;
        }
    }

    String readRequestLine(WiFiClient& client) {
        unsigned long start = millis();
        while (client.connected() && millis() - start < 200) {
            if (!client.available()) continue;

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
    unsigned long serialStart = millis();
    while (!Serial && millis() - serialStart < 2000) {
        delay(10);
    }

    printWebVisuStatus("Start", true);
    connectWifi(ssid, pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        startServer();
    } else {
        printWebVisuStatus("WLAN Start fehlgeschlagen", true);
    }
}

void update(const WebVisuState& state) {
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

}
