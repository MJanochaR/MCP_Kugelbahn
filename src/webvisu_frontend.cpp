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
<title>Kugelbahn MCP</title>
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
.winner{color:#16803c;font-weight:700;font-size:18px;margin:10px 0;padding:10px;background:#e6f9ec;border-radius:12px}
</style>
</head>
<body>
<div class="wrap">
<h1>Kugelbahn MCP</h1>

<div class="grid">
 <div class="card"><div class="muted">Uptime</div><div id="uptimeText" class="big">...</div></div>
 <div class="card"><div class="muted">Anlage</div><div id="anlage" class="big">...</div></div>
 <div class="card"><div class="muted">Rennenstatus</div><div id="rennen" class="big">...</div></div>
</div>

<h2>Manuelle Steuerung</h2>

<h3>Aktoren</h3>
<div class="grid">
 <div class="card">
   <h4>Motor Loop</h4>
   <p>Stellung: <span id="loopR" class="value"></span></p>
   <div class="bar"><span id="loopBar"></span></div>
   <button onclick="cmd('loop')">Loop schalten</button>
 </div>

 <div class="card">
   <h4>Motor Röhre</h4>
   <p>Stellung: <span id="roehreR" class="value"></span></p>
   <div class="bar"><span id="roehreBar"></span></div>
   <button onclick="cmd('roehre')">Röhre schalten</button>
 </div>

  <div class="card">
    <h4>Servo Röhre</h4>
    <p>Stellung: <span id="servo" class="value"></span></p>
    <button onclick="cmd('servo')">Servo umschalten</button>
    <hr style="border:0; border-top:1px solid #ccc; margin:5px 0;">
    <button onclick="cmd('test_release')">Auslass-Sequenz testen</button>
  </div>

 <div class="card">
   <h4>Aufzug</h4>
   <p>Status: <span id="aufzug" class="value"></span></p>
   <button onclick="cmd('aufzug')">Aufzug umschalten</button>
 </div>

</div>

<h3>Streckeneinstellung</h3>
<div class="grid">
 <div class="card">
   <h4>Startrichtung</h4>
   <p>Aktueller Modus: <span id="startMode" class="value"></span></p>
   <div style="display:flex; flex-direction:column; gap:8px; margin-top:10px;">
     <button onclick="setStartMode(0)">Alternierend</button>
     <button onclick="setStartMode(1)">Immer Rechts</button>
     <button onclick="setStartMode(2)">Immer Links</button>
   </div>
 </div>

 <div class="card">
   <h4>Strecken</h4>
   <p>Aktuelle Strecke: <span id="streckeMode" class="value"></span></p>
   <div style="display:flex; flex-direction:column; gap:8px; margin-top:10px;">
     <button onclick="setStrecke(1)">Aussortieren</button>
     <button onclick="setStrecke(2)">Rampe</button>
     <button onclick="setStrecke(3)">Looping</button>
     <button onclick="setStrecke(4)">Gerade</button>
     <hr style="border:0; border-top:1px solid #ccc; margin:5px 0;">
     <button onclick="setStrecke(5)">Zufall</button>
     <button onclick="setStrecke(6)">Gleichmäßig</button>
   </div>
 </div>
</div>

<h2>Rennen</h2>
<div class="grid">
 <div class="card" style="grid-column: span 2;">
    <div id="raceWinnerBox" style="display:none;" class="winner"></div>
    <button onclick="cmd('race_start')">Rennen starten</button>
    <button onclick="cmd('race_reset')">Rennen zurücksetzen</button>
    <div style="margin-top: 10px;">
      <p style="margin: 5px 0;"><b>Aussortieren-Logik: </b><span id="aussortierenState" class="value"></span></p>
      <button onclick="cmd('toggle_aussortieren')">Aussortieren Umschalten</button>
    </div>

    <div style="margin-top: 10px;">
      <p style="margin: 5px 0;"><b>Strecke im Rennen: </b><span id="raceStreckeState" class="value"></span></p>
      <button onclick="cmd('race_strecke_set&mode=2')">Rampe</button>
      <button onclick="cmd('race_strecke_set&mode=3')">Looping</button>
      <button onclick="cmd('race_strecke_set&mode=4')">Gerade</button>
      <button onclick="cmd('race_strecke_set&mode=5')">Zufall</button>
      <button onclick="cmd('race_strecke_set&mode=6')">Gleichmäßig</button>
    </div>
    
    <h3 style="margin-top: 20px;">Kugelzeiten</h3>
    <table style="width:100%;text-align:left;border-collapse:collapse;font-size:14px;margin-top:10px;">
     <tr style="border-bottom:1px solid #e5e7eb"><th>Kugel</th><th>Start</th><th>Ende</th><th>Laufzeit</th></tr>
     <tbody id="kugelnBody">
     </tbody>
   </table>
 </div>
</div>

<h2>Statistik</h2>
<div class="grid">
  <div class="card">
   <h3>Globale Werte</h3>
   <p>Kugeln gesamt (unten): <span id="kugelnCount" class="big"></span></p>
   <p>Aussortierte Kugeln: <span id="aussortiertCount" class="big"></span></p>
   <hr style="border:0; border-top:1px solid #ccc; margin:10px 0;">
   <p><b>Alltime Schnellste Zeit:</b><br><span id="alltimeFastest" class="value"></span></p>
   <button onclick="cmd('reset_stats')">Zähler & Rekorde zurücksetzen</button>
 </div>
 <div class="card" style="grid-column: span 2;">
   <h3>Strecken-Statistiken</h3>
   <table style="width:100%;text-align:left;border-collapse:collapse;font-size:14px;margin-top:10px;">
     <tr style="border-bottom:1px solid #e5e7eb"><th>Strecke</th><th>Durchläufe</th><th>Schnellste Zeit</th></tr>
     <tbody id="statsStreckenBody">
     </tbody>
   </table>
 </div>
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

function zeit(t){
  if (!t) return '-';
  return (t / 1000).toFixed(3) + ' s';
}

async function cmd(c){
  try {
    await fetch('/cmd?do=' + c + '&t=' + Date.now());
    await load();
  } catch(e) {
    console.error('Command Fehler:', e);
  }
}

async function setStartMode(m) {
  await cmd('startrichtung_set&mode=' + m);
}

async function setStrecke(m) {
  await cmd('strecke_set&mode=' + m);
}

async function load(){
  try {
    const r = await fetch('/api/status?t=' + Date.now());
    if (!r.ok) return;

    const d = await r.json();

    $('uptimeText').textContent = ms(d.uptimeMs);

    $('anlage').textContent = d.anlageScharf ? 'SCHARF' : 'AUS';
    cls($('anlage'), d.anlageScharf);

    $('rennen').textContent = d.raceState === 1 ? 'LÄUFT' : (d.raceState === 2 ? 'BEENDET' : 'BEREIT');
    cls($('rennen'), d.raceState === 1);

    $('aufzug').textContent = txt(d.aufzugAktiv);

    $('loopR').textContent = d.loopRichtung ? 'Looping' : 'Gerade';
    $('loopBar').style.width = d.loopAktiv ? Math.min(100, (d.uptimeMs - d.loopStartMs) / MOTOR_RUN_MS * 100) + '%' : '0%';

    $('roehreR').textContent = d.roehreRichtung ? 'Aussortieren' : 'Rampe';
    $('roehreBar').style.width = d.roehreAktiv ? Math.min(100, (d.uptimeMs - d.roehreStartMs) / MOTOR_RUN_MS * 100) + '%' : '0%';

    $('servo').textContent = d.servoAuf ? 'AUF' : 'ZU';

    $('aussortierenState').textContent = d.aussortierenAktiv ? 'Aktiviert' : 'Deaktiviert';

    // Gewinner anzeigen
    let allFinished = d.kugeln[0].abgeschlossen && d.kugeln[1].abgeschlossen && d.kugeln[2].abgeschlossen;
    if (allFinished) {
      let bestIdx = 0;
      let bestTime = d.kugeln[0].dauerMs;
      for (let i = 1; i < 3; i++) {
        if (d.kugeln[i].dauerMs < bestTime) {
          bestTime = d.kugeln[i].dauerMs;
          bestIdx = i;
        }
      }
      $('raceWinnerBox').style.display = 'block';
      $('raceWinnerBox').textContent = '🏆 Gewinner: Kugel ' + (bestIdx + 1) + ' (' + (bestTime / 1000).toFixed(3) + ' s)';
    } else {
      $('raceWinnerBox').style.display = 'none';
    }

    let raceModeStrs = ['?', 'Aussortieren', 'Rampe', 'Looping', 'Gerade', 'Zufall', 'Gleichmäßig'];
    $('raceStreckeState').textContent = d.raceStreckenMode !== undefined ? raceModeStrs[d.raceStreckenMode] : '?';
    
    let modeStrs = ['Alternierend', 'Immer Rechts', 'Immer Links'];
    $('startMode').textContent = d.startRichtungMode !== undefined ? modeStrs[d.startRichtungMode] : '?';
    
    let streckenStrs = ['Manuell', 'Aussortieren', 'Rampe', 'Looping', 'Gerade', 'Zufall', 'Gleichmäßig'];
    $('streckeMode').textContent = d.streckenMode !== undefined ? streckenStrs[d.streckenMode] : '?';
    
    let html = '';
    if (d.kugeln) {
      d.kugeln.forEach((k, idx) => {
        let dauer = k.aktiv ? zeit(d.uptimeMs - k.startMs) : (k.abgeschlossen ? zeit(k.dauerMs) : '-');
        let strInfo = (k.strecke > 0 && k.strecke <= 4) ? ` (${streckenStrs[k.strecke]})` : '';
        html += `<tr><td>Kugel ${idx + 1}${strInfo}</td><td>${k.startMs ? ms(k.startMs) : '-'}</td><td>${k.endMs ? ms(k.endMs) : '-'}</td><td>${dauer}</td></tr>`;
      });
    }
    $('kugelnBody').innerHTML = html;

    $('kugelnCount').textContent = d.kugelnSeitReset;
    $('aussortiertCount').textContent = d.aussortierteKugelnGesamt !== undefined ? d.aussortierteKugelnGesamt : 0;
    
    if (d.alltimeFastestMs !== undefined && d.alltimeFastestMs < 4000000000) {
      $('alltimeFastest').textContent = 'Zeit: ' + zeit(d.alltimeFastestMs) + ' : ' + streckenStrs[d.alltimeFastestStrecke];
    } else {
      $('alltimeFastest').textContent = '-';
    }

    let statsHtml = '';
    if (d.runsPerStrecke !== undefined && d.fastestMsPerStrecke !== undefined) {
      for(let i = 1; i <= 4; i++) {
        let ft = d.fastestMsPerStrecke[i] < 4000000000 ? zeit(d.fastestMsPerStrecke[i]) : '-';
        statsHtml += `<tr><td>${streckenStrs[i]}</td><td>${d.runsPerStrecke[i]}</td><td>${ft}</td></tr>`;
      }
    }
    const elStats = $('statsStreckenBody');
    if(elStats) elStats.innerHTML = statsHtml;

  } catch(e) {
    console.error('API nicht erreichbar:', e);
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
