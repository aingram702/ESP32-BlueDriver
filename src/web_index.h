// ============================================================================
//  web_index.h  -  Single-page control panel, served from flash (PROGMEM).
//  Pure inline HTML/CSS/JS: no external CDN, so it works on the offline AP.
// ============================================================================
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTMLDOC(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-BlueDriver</title>
<style>
:root{--bg:#0b0f17;--panel:#141b2b;--panel2:#1b2438;--line:#26324d;
--txt:#e6edf7;--mut:#8b9ac0;--acc:#3da9fc;--ok:#37d67a;--warn:#ffb020;--bad:#ff5470}
*{box-sizing:border-box}
body{margin:0;font:14px/1.4 system-ui,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--txt)}
header{position:sticky;top:0;z-index:5;background:linear-gradient(180deg,#101725,#0b0f17);
border-bottom:1px solid var(--line);padding:10px 14px}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:18px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--bad);box-shadow:0 0 8px var(--bad)}
.dot.on{background:var(--ok);box-shadow:0 0 10px var(--ok)}
.stats{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}
.chip{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:5px 9px;font-size:12px;color:var(--mut)}
.chip b{color:var(--txt)}
nav{display:flex;gap:6px;margin-top:10px;flex-wrap:wrap}
nav button{background:var(--panel);color:var(--mut);border:1px solid var(--line);
border-radius:8px;padding:7px 12px;cursor:pointer;font-size:13px}
nav button.active{background:var(--acc);color:#04121f;border-color:var(--acc);font-weight:600}
main{padding:12px 14px 60px}
.bar{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:10px}
input,select,textarea{background:var(--panel2);color:var(--txt);border:1px solid var(--line);
border-radius:8px;padding:8px 10px;font-size:13px}
input:focus,select:focus,textarea:focus{outline:none;border-color:var(--acc)}
.btn{background:var(--acc);color:#04121f;border:none;border-radius:8px;padding:8px 13px;
font-weight:600;cursor:pointer;font-size:13px}
.btn.sec{background:var(--panel2);color:var(--txt);border:1px solid var(--line)}
.btn.bad{background:var(--bad);color:#fff}
.btn.ok{background:var(--ok);color:#04121f}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{padding:8px 9px;border-bottom:1px solid var(--line);text-align:left;white-space:nowrap}
th{color:var(--mut);font-weight:600;cursor:pointer;position:sticky;top:0;background:var(--panel)}
tbody tr:hover{background:var(--panel)}
td.mac{font-family:ui-monospace,Menlo,monospace}
.rssi{font-family:ui-monospace,monospace}
.tag{display:inline-block;padding:1px 6px;border-radius:6px;font-size:11px;background:var(--panel2);border:1px solid var(--line)}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:12px}
.card h3{margin:0 0 10px;font-size:15px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
label{display:block;font-size:12px;color:var(--mut);margin:8px 0 3px}
.muted{color:var(--mut)}
.kv{display:grid;grid-template-columns:130px 1fr;gap:4px 10px;font-size:13px}
.kv div:nth-child(odd){color:var(--mut)}
pre{background:#0a0e16;border:1px solid var(--line);border-radius:8px;padding:10px;overflow:auto;
font-size:12px;max-height:50vh}
.modal{position:fixed;inset:0;background:rgba(0,0,0,.6);display:none;align-items:center;
justify-content:center;z-index:20;padding:14px}
.modal.show{display:flex}
.modal .card{max-width:680px;width:100%;max-height:88vh;overflow:auto;margin:0}
.right{margin-left:auto}
.signal{height:6px;border-radius:3px;background:linear-gradient(90deg,var(--bad),var(--warn),var(--ok))}
small{color:var(--mut)}
.hide{display:none}
@media(max-width:560px){.grid{grid-template-columns:1fr}.hcol{display:none}}
</style>
</head>
<body>
<header>
  <div class="brand"><span id="led" class="dot"></span>ESP32-BlueDriver
    <small style="font-weight:400;color:var(--mut)">BLE Wardriver</small></div>
  <div class="stats">
    <span class="chip">Devices <b id="s-dev">0</b></span>
    <span class="chip">New this run <b id="s-new">0</b></span>
    <span class="chip">Scan <b id="s-scan">--</b></span>
    <span class="chip">GPS <b id="s-gps">--</b></span>
    <span class="chip">Uptime <b id="s-up">0s</b></span>
    <span class="chip">Heap <b id="s-heap">0</b>k</span>
    <span class="chip">PSRAM <b id="s-ps">0</b>k</span>
  </div>
  <nav>
    <button data-tab="devices" class="active">Devices</button>
    <button data-tab="tools">Pentest Tools</button>
    <button data-tab="settings">Settings</button>
  </nav>
</header>
<main>

<!-- DEVICES ------------------------------------------------------------- -->
<section id="tab-devices">
  <div class="bar">
    <button id="btnScan" class="btn">Toggle Scan</button>
    <input id="q" placeholder="Search name / MAC / vendor" style="flex:1;min-width:160px">
    <select id="sort">
      <option value="last">Sort: Last seen</option>
      <option value="rssi">Sort: Signal</option>
      <option value="count">Sort: Hits</option>
      <option value="name">Sort: Name</option>
      <option value="first">Sort: First seen</option>
    </select>
    <a class="btn sec" href="/export.csv">Export CSV</a>
    <a class="btn sec" href="/export.json">JSON</a>
    <button id="btnClear" class="btn bad">Clear</button>
  </div>
  <div style="overflow:auto;border:1px solid var(--line);border-radius:12px">
  <table>
    <thead><tr>
      <th>Name</th><th>MAC</th><th>Vendor</th><th>RSSI</th>
      <th class="hcol">Hits</th><th class="hcol">Type</th><th class="hcol">GPS</th><th class="hcol">Last</th>
    </tr></thead>
    <tbody id="rows"></tbody>
  </table>
  </div>
  <p class="muted" id="tableInfo" style="margin-top:8px"></p>
</section>

<!-- TOOLS --------------------------------------------------------------- -->
<section id="tab-tools" class="hide">
  <div class="card">
    <h3>GATT Enumeration</h3>
    <p class="muted">Connect to a device you are authorized to test and map its
      services, characteristics and readable values.</p>
    <div class="bar">
      <input id="gattMac" placeholder="AA:BB:CC:DD:EE:FF" style="flex:1;min-width:180px">
      <select id="gattType"><option value="0">Public addr</option><option value="1">Random addr</option></select>
      <button id="btnGatt" class="btn">Enumerate</button>
    </div>
    <pre id="gattOut" class="hide"></pre>
  </div>

  <div class="card">
    <h3>Beacon Transmitter</h3>
    <p class="muted">Broadcast a test beacon to see how nearby apps/scanners
      react, or to validate proximity systems you control.</p>
    <label>Type</label>
    <select id="bType">
      <option value="ibeacon">iBeacon</option>
      <option value="eddystone">Eddystone-URL</option>
      <option value="custom">Custom (raw mfg hex)</option>
    </select>
    <div class="grid">
      <div><label>Adv name (optional)</label><input id="bName" placeholder="BlueDriver-Test"></div>
      <div id="wrap1"><label id="lbl1">UUID (32 hex)</label><input id="bArg1"></div>
      <div id="wrap2"><label id="lbl2">major,minor</label><input id="bArg2" placeholder="1,2"></div>
    </div>
    <div class="bar" style="margin-top:10px">
      <button id="btnBeaconStart" class="btn ok">Start broadcasting</button>
      <button id="btnBeaconStop" class="btn bad">Stop</button>
      <span id="beaconState" class="muted right"></span>
    </div>
  </div>
</section>

<!-- SETTINGS ------------------------------------------------------------ -->
<section id="tab-settings" class="hide">
  <div class="card">
    <h3>Access Point</h3>
    <div class="grid">
      <div><label>SSID</label><input id="cSsid"></div>
      <div><label>Password (blank = open, else 8+ chars)</label><input id="cPass"></div>
    </div>
    <label style="margin-top:10px"><input type="checkbox" id="cActive"> Active scan (request names — more data, slightly noisier)</label>
    <div class="bar" style="margin-top:12px">
      <button id="btnSaveCfg" class="btn">Save &amp; reboot</button>
      <span class="muted">AP changes apply after reboot.</span>
    </div>
  </div>
  <div class="card">
    <h3>About</h3>
    <div class="kv" id="about"></div>
    <p class="muted" style="margin-top:10px">ESP32-S3 has a Bluetooth LE radio only
      (no Bluetooth Classic), so this tool wardrives BLE advertisements.</p>
  </div>
</section>
</main>

<!-- DETAIL MODAL -->
<div class="modal" id="modal"><div class="card">
  <div class="bar"><h3 id="mTitle" style="margin:0">Device</h3>
    <button class="btn sec right" onclick="closeModal()">Close</button></div>
  <div class="kv" id="mBody"></div>
  <div class="bar" style="margin-top:12px">
    <button class="btn" id="mGatt">Enumerate GATT</button>
  </div>
  <pre id="mGattOut" class="hide"></pre>
</div></div>

<script>
const $=s=>document.querySelector(s), $$=s=>document.querySelectorAll(s);
let lastRows=[];
function tab(name){
  $$('nav button').forEach(b=>b.classList.toggle('active',b.dataset.tab===name));
  ['devices','tools','settings'].forEach(t=>$('#tab-'+t).classList.toggle('hide',t!==name));
}
$$('nav button').forEach(b=>b.onclick=()=>tab(b.dataset.tab));

function fmtAge(s){if(s<60)return s+'s';if(s<3600)return Math.floor(s/60)+'m';return Math.floor(s/3600)+'h';}
function esc(s){return (s||'').replace(/[<>&]/g,c=>({'<':'&lt;','>':'&gt;','&':'&amp;'}[c]));}

async function poll(){
  try{
    const s=await (await fetch('/api/status')).json();
    $('#s-dev').textContent=s.devices;
    $('#s-new').textContent=s.newDevices;
    $('#s-scan').textContent=s.scanning?'ON':'off';
    $('#led').classList.toggle('on',s.scanning);
    $('#s-gps').textContent=s.gps.valid?(s.gps.sats+' sats'):(s.gps.present?'no fix':'none');
    $('#s-up').textContent=fmtAge(s.uptime);
    $('#s-heap').textContent=Math.round(s.heap/1024);
    $('#s-ps').textContent=Math.round(s.psram/1024);
    $('#beaconState').textContent=s.beacon.active?('Broadcasting: '+s.beacon.info):'idle';
    $('#about').innerHTML=`<div>Firmware</div><div>${s.fw} v${s.ver}</div>
      <div>Free heap</div><div>${s.heap} B</div><div>Free PSRAM</div><div>${s.psram} B</div>`;
  }catch(e){}
}

async function loadDevices(){
  const q=encodeURIComponent($('#q').value), sort=$('#sort').value;
  try{
    const d=await (await fetch(`/api/devices?limit=300&q=${q}&sort=${sort}`)).json();
    lastRows=d.rows;
    $('#rows').innerHTML=d.rows.map((r,i)=>{
      const sig=Math.max(0,Math.min(100,(r.rssi+100)*1.6));
      return `<tr onclick="showDev(${i})">
        <td>${esc(r.name)||'<span class=muted>(no name)</span>'}</td>
        <td class=mac>${r.mac}</td>
        <td>${esc(r.mfg)||'<span class=muted>—</span>'}</td>
        <td class=rssi>${r.rssi}<div class=signal style="width:${sig}%"></div></td>
        <td class=hcol>${r.count}</td>
        <td class=hcol><span class=tag>${r.conn?'conn':'adv'}</span></td>
        <td class=hcol>${r.gps?'📍':''}</td>
        <td class=hcol>${fmtAge(r.lastAge)}</td></tr>`;
    }).join('');
    $('#tableInfo').textContent=`Showing ${d.shown} of ${d.total} unique devices`;
  }catch(e){}
}

function showDev(i){
  const r=lastRows[i];
  $('#mTitle').textContent=r.name||r.mac;
  $('#mBody').innerHTML=`
    <div>MAC</div><div class=mac>${r.mac}</div>
    <div>Name</div><div>${esc(r.name)||'—'}</div>
    <div>Vendor</div><div>${esc(r.mfg)||'—'}</div>
    <div>Addr type</div><div>${r.addrType==1?'random':'public'}</div>
    <div>RSSI now / best</div><div>${r.rssi} / ${r.rssiBest} dBm</div>
    <div>TX power</div><div>${r.tx||'—'}</div>
    <div>Connectable</div><div>${r.conn?'yes':'no'}</div>
    <div>Service UUID</div><div class=mac>${esc(r.svc)||'—'}</div>
    <div>Appearance</div><div>${r.appearance||'—'}</div>
    <div>Hits</div><div>${r.count}</div>
    <div>First / Last</div><div>${fmtAge(r.firstAge)} ago / ${fmtAge(r.lastAge)} ago</div>
    <div>Location</div><div>${r.gps?(r.lat.toFixed(5)+', '+r.lon.toFixed(5)):'no GPS fix'}</div>`;
  $('#mGattOut').classList.add('hide');
  $('#mGatt').disabled=!r.conn;
  $('#mGatt').onclick=()=>runGatt(r.mac,r.addrType,'#mGattOut');
  $('#modal').classList.add('show');
}
function closeModal(){$('#modal').classList.remove('show');}

async function runGatt(mac,type,outSel){
  const out=$(outSel);out.classList.remove('hide');out.textContent='Connecting to '+mac+' …';
  try{
    const r=await (await fetch(`/api/pentest/gatt?mac=${mac}&type=${type}`,{method:'POST'})).json();
    out.textContent=JSON.stringify(r,null,2);
  }catch(e){out.textContent='Error: '+e;}
}

$('#btnScan').onclick=async()=>{await fetch('/api/scan?toggle=1',{method:'POST'});poll();};
$('#btnClear').onclick=async()=>{if(confirm('Clear all discovered devices?')){await fetch('/api/clear',{method:'POST'});loadDevices();poll();}};
$('#q').oninput=loadDevices; $('#sort').onchange=loadDevices;
$('#btnGatt').onclick=()=>runGatt($('#gattMac').value.trim(),$('#gattType').value,'#gattOut');

function beaconLabels(){
  const t=$('#bType').value;
  if(t==='ibeacon'){$('#lbl1').textContent='UUID (32 hex)';$('#lbl2').textContent='major,minor';$('#wrap2').classList.remove('hide');}
  else if(t==='eddystone'){$('#lbl1').textContent='URL (https://…)';$('#wrap2').classList.add('hide');}
  else{$('#lbl1').textContent='Raw mfg data (hex)';$('#wrap2').classList.add('hide');}
}
$('#bType').onchange=beaconLabels; beaconLabels();
$('#btnBeaconStart').onclick=async()=>{
  const p=new URLSearchParams({action:'start',type:$('#bType').value,name:$('#bName').value,
    arg1:$('#bArg1').value,arg2:$('#bArg2').value});
  const r=await (await fetch('/api/pentest/beacon?'+p,{method:'POST'})).json();
  if(r.error)alert(r.error);poll();
};
$('#btnBeaconStop').onclick=async()=>{await fetch('/api/pentest/beacon?action=stop',{method:'POST'});poll();};

async function loadCfg(){
  const c=await (await fetch('/api/config')).json();
  $('#cSsid').value=c.ssid;$('#cPass').value=c.pass;$('#cActive').checked=c.active;
}
$('#btnSaveCfg').onclick=async()=>{
  const p=new URLSearchParams({ssid:$('#cSsid').value,pass:$('#cPass').value,active:$('#cActive').checked?1:0});
  await fetch('/api/config?'+p,{method:'POST'});
  alert('Saved. Rebooting…');
};

tab('devices');loadCfg();poll();loadDevices();
setInterval(poll,1500);setInterval(loadDevices,2000);
</script>
</body></html>)HTMLDOC";
