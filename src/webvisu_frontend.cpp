#include <Arduino.h>

#include "webvisu_frontend.h"

namespace WebVisuFrontend {

void sendIndex(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Cache-Control: no-store, no-cache, must-revalidate");
  client.println("Pragma: no-cache");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();
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

<h2>Status</h2>
<div class="card">
  <p class="muted">Letzte API-Meldung der WebVisu:</p>
  <pre id="status">warte...</pre>
</div>

</div>

<script>
const MOTOR_RUN_MS = 600;
const $ = id => document.getElementById(id);

function txt(v){ return v ? 'AN' : 'AUS'; }
function cls(el,v){ el.className = v ? 'big ok' : 'big bad'; }

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
    $('status').textContent = 'Command OK: ' + c + '\nHTTP ' + r.status;
    await load();
  } catch(e) {
    $('status').textContent = 'Command Fehler: ' + e;
  }
}

async function load(){
  try {
    const r = await fetch('/api/status?t=' + Date.now());

    if (!r.ok) {
      $('status').textContent = 'API HTTP Fehler: ' + r.status;
      return;
    }

    const d = await r.json();

    $('wifi').textContent = d.wifi ? 'Verbunden' : 'Offline';
    cls($('wifi'), d.wifi);
    $('ip').textContent = d.ip;
    $('uptime').textContent = ms(d.uptimeMs);

    $('aufzug').textContent = txt(d.aufzugAktiv);
    cls($('aufzug'), d.aufzugAktiv);
    $('lampen').textContent = txt(d.lampenAn);
    cls($('lampen'), d.lampenAn);

    $('loop').textContent = txt(d.loopAktiv);
    $('loopR').textContent = d.loopRichtung ? 'Richtung A' : 'Richtung B';
    $('loopBar').style.width = d.loopAktiv ? Math.min(100, (d.uptimeMs - d.loopStartMs) / MOTOR_RUN_MS * 100) + '%' : '0%';

    $('roehre').textContent = txt(d.roehreAktiv);
    $('roehreR').textContent = d.roehreRichtung ? 'Richtung A' : 'Richtung B';
    $('roehreBar').style.width = d.roehreAktiv ? Math.min(100, (d.uptimeMs - d.roehreStartMs) / MOTOR_RUN_MS * 100) + '%' : '0%';

    $('servo').textContent = d.servoAuf ? 'AUF' : 'ZU';
    $('inputs').innerHTML =
      `<span class=pill>T0 ${txt(d.taster0)}</span><br>` +
      `<span class=pill>T1 ${txt(d.taster1)}</span><br>` +
      `<span class=pill>T2 ${txt(d.taster2)}</span><br>` +
      `<span class=pill>T3 ${txt(d.taster3)}</span><br>` +
      `<span class=pill>Schalter ${txt(d.schalterLinks)}</span>`;

    $('cntLoop').textContent = d.loopSchaltungen;
    $('cntRoehre').textContent = d.roehreSchaltungen;
    $('cntServo').textContent = d.servoSchaltungen;
    $('status').textContent = 'API OK\nIP: ' + d.ip + '\nUptime: ' + d.uptimeMs + ' ms\nUpdate: ' + new Date().toLocaleTimeString();
  } catch(e) {
    $('status').textContent = 'API nicht erreichbar: ' + e;
  }
}

setInterval(load, 1000);
load();
</script>
</body>
</html>
)rawliteral");
}

}