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

    void printWebVisuStatus(const char* message, bool force = false) {
        unsigned long now = millis();
        if (!force && now - lastStatusMs < WEBVISU_STATUS_MS) return;

        lastStatusMs = now;
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
            return;
        }

        printWebVisuStatus("WLAN getrennt");

        unsigned long now = millis();
        if (now - lastReconnectMs < WIFI_RECONNECT_MS) return;

        lastReconnectMs = now;
        WiFi.disconnect();
        connectWifi(WIFI_SSID, WIFI_PASS);
    }

    bool startsWith(const String& line, const char* path) {
        return line.startsWith(path);
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
        client.print("\"loopStartMs\":"); client.print(lastState.loopStartMs); client.print(',');
        client.print("\"roehreStartMs\":"); client.print(lastState.roehreStartMs); client.print(',');
        client.print("\"aufzugAktiv\":"); client.print(onOff(lastState.aufzugAktiv)); client.print(',');
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
        client.print("\"loopSchaltungen\":"); client.print(lastState.loopSchaltungen); client.print(',');
        client.print("\"roehreSchaltungen\":"); client.print(lastState.roehreSchaltungen); client.print(',');
        client.print("\"servoSchaltungen\":"); client.print(lastState.servoSchaltungen);
        client.print('}');
    }

    void rememberCommand(const String& requestLine) {
        if (requestLine.indexOf("do=loop") >= 0) pendingCommand = WebVisu::CMD_LOOP_TOGGLE;
        else if (requestLine.indexOf("do=roehre") >= 0) pendingCommand = WebVisu::CMD_ROEHRE_TOGGLE;
        else if (requestLine.indexOf("do=servo") >= 0) pendingCommand = WebVisu::CMD_SERVO_TOGGLE;
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

    void handleClient(WiFiClient& client) {
        client.setTimeout(50);

        String requestLine = client.readStringUntil('\r');
        client.readStringUntil('\n');
        skipHeaders(client);

        if (startsWith(requestLine, "GET /api/status")) {
            sendJson(client);
        } else if (startsWith(requestLine, "GET /cmd")) {
            rememberCommand(requestLine);
            sendPlain(client, "200 OK", "OK");
        } else if (startsWith(requestLine, "GET / ")) {
            WebVisuFrontend::sendIndex(client);
        } else {
            sendPlain(client, "404 Not Found", "Nicht gefunden");
        }

        client.flush();
        client.stop();
    }
}

namespace WebVisu {

void begin(const char* ssid, const char* pass) {
    Serial.begin(115200);
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
