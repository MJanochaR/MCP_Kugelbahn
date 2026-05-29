#include <Arduino.h>
#include <WiFi.h>

#include "webvisu.h"
#include "config.h"

namespace {
    WiFiServer server(80);
    WebVisu::Command pendingCommand = WebVisu::CMD_NONE;
    WebVisuState lastState = {};

    unsigned long letzterIpDruck = 0;
    unsigned long letzterReconnectVersuch = 0;
    bool serverGestartet = false;

    const char* wifiStatusText(int status) {
        switch (status) {
            case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
            case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
            case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
            case WL_CONNECTED: return "WL_CONNECTED";
            case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
            case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
            case WL_DISCONNECTED: return "WL_DISCONNECTED";
            default: return "UNKNOWN";
        }
    }

    String boolText(bool v) {
        return v ? "true" : "false";
    }

    void druckeWifiDebug() {
        int status = WiFi.status();

        Serial.println();
        Serial.println("========== WebVisu WLAN Debug ==========");
        Serial.print("Status: ");
        Serial.print(status);
        Serial.print(" / ");
        Serial.println(wifiStatusText(status));

        Serial.print("SSID: ");
        Serial.println(WIFI_SSID);

        Serial.print("IP: http://");
        Serial.println(WiFi.localIP());

        Serial.print("Gateway: ");
        Serial.println(WiFi.gatewayIP());

        Serial.print("Subnet: ");
        Serial.println(WiFi.subnetMask());

        Serial.print("RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");

        Serial.print("Server Port: ");
        Serial.println("80");

        Serial.println("Browser-Test:");
        Serial.print("  http://");
        Serial.print(WiFi.localIP());
        Serial.println("/");

        Serial.print("API-Test:");
        Serial.print("  http://");
        Serial.print(WiFi.localIP());
        Serial.println("/api/status");

        Serial.println("Hinweis: Browser muss im gleichen WLAN/Subnetz sein.");
        Serial.println("Wichtig: wirklich http:// benutzen, nicht https://");
        Serial.println("========================================");
        Serial.println();
    }

    void sendHeader(WiFiClient& client, const char* contentType) {
        client.println("HTTP/1.1 200 OK");
        client.print("Content-Type: ");
        client.println(contentType);
        client.println("Cache-Control: no-store, no-cache, must-revalidate");
        client.println("Pragma: no-cache");
        client.println("Access-Control-Allow-Origin: *");
        client.println("Connection: close");
        client.println();
    }

    void sendPlain(WiFiClient& client, const char* text) {
        sendHeader(client, "text/plain; charset=utf-8");
        client.println(text);
    }

    void sendNotFound(WiFiClient& client) {
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-Type: text/plain; charset=utf-8");
        client.println("Connection: close");
        client.println();
        client.println("404 - Nicht gefunden");
    }

    void sendJson(WiFiClient& client) {
        sendHeader(client, "application/json; charset=utf-8");

        client.print("{");
        client.print("\"wifi\":"); client.print(boolText(lastState.wifiConnected)); client.print(',');
        client.print("\"ip\":\""); client.print(lastState.ip); client.print("\",");
        client.print("\"uptimeMs\":"); client.print(lastState.uptimeMs); client.print(',');
        client.print("\"loopStartMs\":"); client.print(lastState.loopStartMs); client.print(',');
        client.print("\"roehreStartMs\":"); client.print(lastState.roehreStartMs); client.print(',');
        client.print("\"aufzugAktiv\":"); client.print(boolText(lastState.aufzugAktiv)); client.print(',');
        client.print("\"loopAktiv\":"); client.print(boolText(lastState.loopAktiv)); client.print(',');
        client.print("\"loopRichtung\":"); client.print(boolText(lastState.loopRichtung)); client.print(',');
        client.print("\"roehreAktiv\":"); client.print(boolText(lastState.roehreAktiv)); client.print(',');
        client.print("\"roehreRichtung\":"); client.print(boolText(lastState.roehreRichtung)); client.print(',');
        client.print("\"servoAuf\":"); client.print(boolText(lastState.servoAuf)); client.print(',');
        client.print("\"lampenAn\":"); client.print(boolText(lastState.lampenAn)); client.print(',');
        client.print("\"taster0\":"); client.print(boolText(lastState.taster0)); client.print(',');
        client.print("\"taster1\":"); client.print(boolText(lastState.taster1)); client.print(',');
        client.print("\"taster2\":"); client.print(boolText(lastState.taster2)); client.print(',');
        client.print("\"taster3\":"); client.print(boolText(lastState.taster3)); client.print(',');
        client.print("\"schalterLinks\":"); client.print(boolText(lastState.schalterLinks)); client.print(',');
        client.print("\"loopSchaltungen\":"); client.print(lastState.loopSchaltungen); client.print(',');
        client.print("\"roehreSchaltungen\":"); client.print(lastState.roehreSchaltungen); client.print(',');
        client.print("\"servoSchaltungen\":"); client.print(lastState.servoSchaltungen);
        client.print("}");
    }

    void sendIndex(WiFiClient& client) {
        sendHeader(client, "text/html; charset=utf-8");

        client.println(R"rawliteral(
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kugelbahn WebVisu</title>
<style>
:root{font-family:system-ui,Arial,sans-serif;color:#17202a;background:#eef3f7}
body{margin:0}
.wrap{max-width:980px;margin:auto;padding:18px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px}
.card{background:white;border-radius:16px;padding:16px;box-shadow:0 6px 20px #0001}
.big{font-size:28px;font-weight:700}
.muted{color:#667085;font-size:14px}
.ok{color:#16803c}
.bad{color:#b42318}
.pill{display:inline-block;padding:4px 10px;border-radius:999px;background:#edf2ff;margin:2px 0}
button{border:0;border-radius:12px;padding:12px 14px;margin:5px;background:#1f6feb;color:white;font-weight:700;cursor:pointer}
button.stop{background:#b42318}
.value{font-size:18px;font-weight:700}
.bar{height:10px;background:#e5e7eb;border-radius:999px;overflow:hidden}
.bar>span{display:block;height:100%;background:#1f6feb;width:0%}
pre{background:#111827;color:#e5e7eb;padding:12px;border-radius:12px;overflow:auto}
</style>
</head>
<body>
<div class="wrap">
<h1>Fischertechnik Kugelbahn WebVisu</h1>
<p class="muted">Live-Status, Steuerung und einfache Statistik vom Arduino GIGA R1 WiFi</p>

<div class="grid">
 <div class="card"><div class="muted">WLAN / IP</div><div id="wifi" class="big">...</div><div id="ip" class="muted"></div></div>
 <div class="card"><div class="muted">Uptime</div><div id="uptime" class="big">...</div></div>
 <div class="card"><div class="muted">Aufzug</div><div id="aufzug" class="big">...</div></div>
 <div class="card"><div class="muted">Lampen</div><div id="lampen" class="big">...</div></div>
</div>

<h2>Aktoren</h2>
<div class="grid">
 <div class="card">
   <h3>Motor Loop</h3>
   <p>Status: <span id="loop" class="value"></span></p>
   <p>Richtung: <span id="loopR"></span></p>
   <div class="bar"><span id="loopBar"></span></div>
   <button onclick="cmd('loop')">Loop schalten</button>
 </div>

 <div class="card">
   <h3>Motor Röhre</h3>
   <p>Status: <span id="roehre" class="value"></span></p>
   <p>Richtung: <span id="roehreR"></span></p>
   <div class="bar"><span id="roehreBar"></span></div>
   <button onclick="cmd('roehre')">Röhre schalten</button>
 </div>

 <div class="card">
   <h3>Servo Röhre</h3>
   <p>Stellung: <span id="servo" class="value"></span></p>
   <button onclick="cmd('servo')">Servo umschalten</button>
 </div>

 <div class="card">
   <h3>Notbedienung</h3>
   <button onclick="cmd('lamp')">Lampen umschalten</button>
   <button class="stop" onclick="cmd('stop')">Alle stoppen</button>
 </div>
</div>

<h2>Eingänge / Statistik</h2>
<div class="grid">
 <div class="card">
   <h3>Eingänge</h3>
   <div id="inputs"></div>
 </div>

 <div class="card">
   <h3>Zähler</h3>
   <p>Loop-Schaltungen: <b id="cntLoop"></b></p>
   <p>Röhre-Schaltungen: <b id="cntRoehre"></b></p>
   <p>Servo-Schaltungen: <b id="cntServo"></b></p>
 </div>
</div>

<h2>Debug</h2>
<div class="card">
  <p class="muted">Wenn diese Seite lädt, aber Werte nicht aktualisieren, steht hier der letzte Fehler:</p>
  <pre id="debug">warte...</pre>
</div>

</div>

<script>
const MOTOR_RUN_MS = 600;

function txt(v){
  return v ? 'AN' : 'AUS';
}

function cls(el,v){
  el.className = v ? 'big ok' : 'big bad';
}

function ms(t){
  let s = Math.floor(t / 1000);
  let m = Math.floor(s / 60);
  s %= 60;
  let h = Math.floor(m / 60);
  m %= 60;
  return `${h}h ${m}m ${s}s`;
}

async function cmd(c){
  try {
    const r = await fetch('/cmd?do=' + c + '&t=' + Date.now());
    debug.textContent = 'Command OK: ' + c + '\\nHTTP ' + r.status;
    await load();
  } catch(e) {
    debug.textContent = 'Command Fehler: ' + e;
  }
}

async function load(){
  try {
    const r = await fetch('/api/status?t=' + Date.now());

    if (!r.ok) {
      debug.textContent = 'API HTTP Fehler: ' + r.status;
      return;
    }

    const d = await r.json();

    wifi.textContent = d.wifi ? 'Verbunden' : 'Offline';
    cls(wifi, d.wifi);

    ip.textContent = d.ip;
    uptime.textContent = ms(d.uptimeMs);

    aufzug.textContent = txt(d.aufzugAktiv);
    cls(aufzug, d.aufzugAktiv);

    lampen.textContent = txt(d.lampenAn);
    cls(lampen, d.lampenAn);

    loop.textContent = txt(d.loopAktiv);
    loopR.textContent = d.loopRichtung ? 'Richtung A' : 'Richtung B';
    loopBar.style.width = d.loopAktiv ? Math.min(100, (d.uptimeMs - d.loopStartMs) / MOTOR_RUN_MS * 100) + '%' : '0%';

    roehre.textContent = txt(d.roehreAktiv);
    roehreR.textContent = d.roehreRichtung ? 'Richtung A' : 'Richtung B';
    roehreBar.style.width = d.roehreAktiv ? Math.min(100, (d.uptimeMs - d.roehreStartMs) / MOTOR_RUN_MS * 100) + '%' : '0%';

    servo.textContent = d.servoAuf ? 'AUF' : 'ZU';

    inputs.innerHTML =
      `<span class=pill>T0 ${txt(d.taster0)}</span><br>` +
      `<span class=pill>T1 ${txt(d.taster1)}</span><br>` +
      `<span class=pill>T2 ${txt(d.taster2)}</span><br>` +
      `<span class=pill>T3 ${txt(d.taster3)}</span><br>` +
      `<span class=pill>Schalter ${txt(d.schalterLinks)}</span>`;

    cntLoop.textContent = d.loopSchaltungen;
    cntRoehre.textContent = d.roehreSchaltungen;
    cntServo.textContent = d.servoSchaltungen;

    debug.textContent =
      'API OK\\n' +
      'IP: ' + d.ip + '\\n' +
      'Uptime: ' + d.uptimeMs + ' ms\\n' +
      'Letztes Update: ' + new Date().toLocaleTimeString();

  } catch(e) {
    debug.textContent = 'API nicht erreichbar: ' + e;
  }
}

setInterval(load, 1000);
load();
</script>
</body>
</html>
)rawliteral");
    }

    void handleCommand(const String& requestLine) {
        if (requestLine.indexOf("GET /cmd?do=loop") >= 0) {
            pendingCommand = WebVisu::CMD_LOOP_TOGGLE;
        } else if (requestLine.indexOf("GET /cmd?do=roehre") >= 0) {
            pendingCommand = WebVisu::CMD_ROEHRE_TOGGLE;
        } else if (requestLine.indexOf("GET /cmd?do=servo") >= 0) {
            pendingCommand = WebVisu::CMD_SERVO_TOGGLE;
        } else if (requestLine.indexOf("GET /cmd?do=lamp") >= 0) {
            pendingCommand = WebVisu::CMD_LAMP_TOGGLE;
        } else if (requestLine.indexOf("GET /cmd?do=stop") >= 0) {
            pendingCommand = WebVisu::CMD_ALL_STOP;
        }
    }

    void starteServerFallsNoetig() {
        if (!serverGestartet) {
            server.begin();
            serverGestartet = true;
            Serial.println("WebVisu Server gestartet auf Port 80");
        }
    }

    void reconnectFallsNoetig() {
        if (WiFi.status() == WL_CONNECTED) {
            starteServerFallsNoetig();
            return;
        }

        unsigned long jetzt = millis();

        if (jetzt - letzterReconnectVersuch < 10000) {
            return;
        }

        letzterReconnectVersuch = jetzt;

        Serial.println("WLAN getrennt. Neuer Verbindungsversuch...");
        WiFi.disconnect();
        delay(100);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
}

namespace WebVisu {

void begin(const char* ssid, const char* pass) {
    Serial.println();
    Serial.println("Starte WebVisu...");

    Serial.print("Verbinde WLAN: ");
    Serial.println(ssid);

    WiFi.begin(ssid, pass);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
        delay(250);
        Serial.print('.');
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WLAN verbunden.");
        starteServerFallsNoetig();
        druckeWifiDebug();
    } else {
        Serial.println("WLAN nicht verbunden. Sketch laeuft trotzdem weiter.");
        Serial.println("WebVisu versucht im Hintergrund weiter zu verbinden.");
    }
}

void update(const WebVisuState& state) {
    lastState = state;

    reconnectFallsNoetig();

    unsigned long jetzt = millis();

    if (jetzt - letzterIpDruck >= 5000) {
        letzterIpDruck = jetzt;
        druckeWifiDebug();
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    WiFiClient client = server.accept();

    if (!client) {
        return;
    }

    Serial.println("Browser/Client verbunden.");

    client.setTimeout(300);

    unsigned long start = millis();

    while (!client.available() && millis() - start < 1000) {
        delay(1);
    }

    if (!client.available()) {
        Serial.println("Client verbunden, aber keine Anfrage empfangen.");
        client.stop();
        return;
    }

    String requestLine = client.readStringUntil('\r');
    client.readStringUntil('\n');

    Serial.print("HTTP Request: ");
    Serial.println(requestLine);

    while (client.connected() && client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() <= 1) {
            break;
        }
    }

    if (requestLine.startsWith("GET /api/status")) {
        sendJson(client);
    } else if (requestLine.startsWith("GET /cmd")) {
        handleCommand(requestLine);
        sendJson(client);
    } else if (requestLine.startsWith("GET /ping")) {
        sendPlain(client, "pong");
    } else if (requestLine.startsWith("GET / ") || requestLine.startsWith("GET /index")) {
        sendIndex(client);
    } else if (requestLine.startsWith("GET /favicon.ico")) {
        sendNotFound(client);
    } else {
        sendNotFound(client);
    }

    delay(1);
    client.stop();

    Serial.println("HTTP Antwort gesendet.");
}

Command consumeCommand() {
    Command cmd = pendingCommand;
    pendingCommand = CMD_NONE;
    return cmd;
}

}