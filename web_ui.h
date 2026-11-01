#pragma once
#include <Arduino.h>
// Web control panel served at "/". Plain HTML/CSS/JS, no external resources
// (works in setup-AP mode without internet).

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dot Matrix</title>
<style>
:root{--bg:#0c0d10;--card:#15171c;--line:#262a32;--text:#e8e9ec;--muted:#8b909b;--led:#ff3b30;--led-dim:#3a1614;--ok:#34c759}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:0 16px 40px}
.wrap{max-width:760px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;padding:16px 0 8px}
header h1{font-size:18px;margin:0;letter-spacing:.5px;flex:1}
header h1 b{color:var(--led);text-shadow:0 0 8px var(--led)}
.pill{font-size:12px;color:var(--muted);background:var(--card);border:1px solid var(--line);border-radius:99px;padding:3px 10px;white-space:nowrap}
.pill.ok::before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--ok);margin-right:6px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin:12px 0}
.card h2{font-size:13px;text-transform:uppercase;letter-spacing:1px;color:var(--muted);margin:0 0 10px;font-weight:600}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
.row.end{justify-content:flex-end}
.grow{flex:1;min-width:0}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
label{display:flex;flex-direction:column;gap:4px;font-size:12px;color:var(--muted)}
label.inline{flex-direction:row;align-items:center;justify-content:space-between;font-size:14px;color:var(--text);padding:6px 0}
input,select,button,textarea{font:inherit;color:var(--text)}
input[type=text],input[type=password],input[type=number],select,textarea{background:#0f1115;border:1px solid var(--line);border-radius:9px;padding:9px 10px;width:100%}
input:focus,select:focus,textarea:focus{outline:none;border-color:var(--led)}
input[type=range]{width:100%;accent-color:var(--led)}
button{background:#232730;border:1px solid var(--line);border-radius:9px;padding:9px 14px;cursor:pointer}
button:hover{border-color:#3a404b}
button.primary{background:var(--led);border-color:var(--led);color:#fff;font-weight:600}
button.ghost{background:transparent}
button.danger{color:#ff6b61}
button.small{padding:5px 10px;font-size:13px}
.seg{display:flex;background:#0f1115;border:1px solid var(--line);border-radius:10px;padding:3px;gap:3px}
.seg button{flex:1;border:0;background:transparent;color:var(--muted);padding:8px}
.seg button.on{background:var(--led);color:#fff;font-weight:600}
.tabs{display:flex;gap:4px;overflow-x:auto;border-bottom:1px solid var(--line);margin-top:8px}
.tabs button{background:transparent;border:0;border-bottom:2px solid transparent;border-radius:0;color:var(--muted);padding:10px 12px;white-space:nowrap}
.tabs button.on{color:var(--text);border-color:var(--led)}
.tab[hidden]{display:none}
.sw{position:relative;display:inline-block;width:42px;height:24px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sw span{position:absolute;inset:0;background:#2a2e36;border-radius:24px;transition:.15s}
.sw span::before{content:"";position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.15s}
.sw input:checked+span{background:var(--led)}
.sw input:checked+span::before{transform:translateX(18px)}
.msg{border:1px solid var(--line);border-radius:12px;padding:10px;margin-bottom:10px;background:#111318}
.msg.off{opacity:.5}
.msg details{margin:8px 0 4px}
.msg summary{cursor:pointer;color:var(--muted);font-size:13px}
.msg .grid{margin-top:8px}
.chips{display:flex;gap:6px;flex-wrap:wrap;margin:8px 0}
.chip{font-size:12px;padding:4px 9px;border-radius:99px;background:#1d2027;border:1px solid var(--line);cursor:pointer;font-family:ui-monospace,monospace}
.val{color:var(--text);font-variant-numeric:tabular-nums}
.hint{font-size:12px;color:var(--muted);margin:6px 0 0}
.matrix-wrap{overflow-x:auto;padding:8px;background:#000;border-radius:10px}
canvas{display:block;touch-action:none;cursor:crosshair;margin:0 auto}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%) translateY(80px);background:#22252c;border:1px solid var(--line);padding:9px 16px;border-radius:10px;transition:.2s;z-index:9}
#toast.show{transform:translateX(-50%) translateY(0)}
.dirty{box-shadow:0 0 0 2px var(--led)}
code{font-family:ui-monospace,monospace;font-size:12px;background:#0f1115;padding:2px 5px;border-radius:5px;word-break:break-all}
.progress{height:6px;background:#0f1115;border-radius:6px;overflow:hidden;margin-top:8px}
.progress div{height:100%;width:0;background:var(--led);transition:width .1s}
</style></head>
<body><div class="wrap">

<header>
  <h1><b>●</b> DOT MATRIX</h1>
  <span class="pill" id="clockPill" style="cursor:pointer" title="Tap to sync time from this device" onclick="syncTime(false)">--:--</span>
  <span class="pill" id="netPill">…</span>
</header>

<div class="card">
  <div class="row">
    <div class="seg grow" id="modeSeg">
      <button data-mode="0">Messages</button><button data-mode="1">Clock</button><button data-mode="2">Drawing</button>
    </div>
    <label class="sw" title="Display power"><input type="checkbox" data-key="power"><span></span></label>
  </div>
  <label style="margin-top:12px">Brightness <span class="val" data-show="brightness"></span>
    <input type="range" min="0" max="15" data-key="brightness"></label>
  <div class="row" style="margin-top:10px">
    <input type="text" id="sayText" class="grow" placeholder="Send an instant message…" maxlength="200">
    <button class="primary" id="sayBtn">Send</button>
  </div>
</div>

<div class="tabs" id="tabs">
  <button data-tab="messages" class="on">Messages</button>
  <button data-tab="clock">Clock</button>
  <button data-tab="draw">Draw</button>
  <button data-tab="display">Display</button>
  <button data-tab="settings">Settings</button>
</div>

<!-- ===================== MESSAGES ===================== -->
<section class="tab" id="tab-messages">
  <div class="card">
    <h2>Playlist</h2>
    <div id="playlist"></div>
    <div class="chips" id="tokens" title="Tap to insert into the last selected message">
      <span class="chip">{TIME}</span><span class="chip">{TIMES}</span><span class="chip">{DATE}</span>
      <span class="chip">{DAY}</span><span class="chip">{AMPM}</span><span class="chip">{IP}</span>
    </div>
    <div class="row">
      <button id="addMsg">+ Add message</button>
      <span class="grow"></span>
      <button class="primary" id="savePlaylist">Save playlist</button>
    </div>
  </div>

  <div class="card">
    <h2>Default animation</h2>
    <div class="grid">
      <label>Effect in<select data-key="fxIn" class="fxsel"></select></label>
      <label>Effect out<select data-key="fxOut" class="fxsel"></select></label>
      <label>Alignment<select data-key="align"><option value="0">Left</option><option value="1">Center</option><option value="2">Right</option></select></label>
    </div>
    <label style="margin-top:10px">Speed <span class="val" data-show="speed"></span>
      <input type="range" min="5" max="200" data-key="speed" style="direction:rtl"></label>
    <label>Pause between in/out <span class="val" data-show="pause"></span>
      <input type="range" min="0" max="15000" step="250" data-key="pause"></label>
    <label class="inline">Auto-scroll text that is too long to fit
      <span class="sw"><input type="checkbox" data-key="autoScroll"><span></span></span></label>
  </div>
</section>

<!-- ===================== CLOCK ===================== -->
<section class="tab" id="tab-clock" hidden>
  <div class="card">
    <h2>Time</h2>
    <div class="row">
      <div class="grow">
        <div id="devTime" style="font-size:30px;font-weight:600;font-variant-numeric:tabular-nums;color:var(--led)">--:--:--</div>
        <div class="hint" id="timeInfo" style="margin:2px 0 0"></div>
      </div>
      <button class="primary" id="syncTimeBtn" onclick="syncTime(false)">Sync from this device</button>
    </div>
    <p class="hint">On home WiFi the display gets time from the internet. Without internet (e.g. setup hotspot) it has no clock battery, so this page sets the time from your phone/computer automatically whenever it's missing.</p>
  </div>
  <div class="card">
    <h2>Clock</h2>
    <label class="inline">24-hour format<span class="sw"><input type="checkbox" data-key="clock24"><span></span></span></label>
    <label class="inline">Show seconds<span class="sw"><input type="checkbox" data-key="clockSec"><span></span></span></label>
    <label class="inline">Blinking colon<span class="sw"><input type="checkbox" data-key="clockBlink"><span></span></span></label>
    <p class="hint">Seconds need ~6 modules to fit; on 4 modules try character spacing 0.</p>
    <div class="row end" style="margin-top:8px"><button class="primary" onclick="push({mode:1})">Show clock now</button></div>
  </div>
  <div class="card">
    <h2>Time zone &amp; date</h2>
    <div class="grid">
      <label>Time zone<select id="tzSel"></select></label>
      <label>POSIX TZ string<input type="text" data-key="tz" data-str id="tzInput"></label>
      <label>Date format ({DATE})<select id="dateSel"></select></label>
      <label>Custom (strftime)<input type="text" data-key="dateFmt" data-str id="dateInput"></label>
    </div>
    <p class="hint">"Sync from this device" also sets the time zone to match your phone/computer.</p>
  </div>
</section>

<!-- ===================== DRAW ===================== -->
<section class="tab" id="tab-draw" hidden>
  <div class="card">
    <h2>Pixel editor</h2>
    <div class="matrix-wrap"><canvas id="cv"></canvas></div>
    <div class="row" style="margin-top:10px">
      <div class="seg" id="toolSeg"><button data-tool="pen" class="on">Pen</button><button data-tool="erase">Eraser</button><button data-tool="toggle">Toggle</button></div>
      <label class="inline" style="gap:8px">Live<span class="sw"><input type="checkbox" id="live" checked><span></span></span></label>
    </div>
    <div class="row" style="margin-top:10px">
      <button class="small" data-op="clear">Clear</button>
      <button class="small" data-op="fill">Fill</button>
      <button class="small" data-op="invert">Invert</button>
      <button class="small" data-op="left">◀</button>
      <button class="small" data-op="right">▶</button>
      <button class="small" data-op="up">▲</button>
      <button class="small" data-op="down">▼</button>
      <button class="small" data-op="flip">Mirror</button>
    </div>
    <div class="row" style="margin-top:10px">
      <input type="text" id="stampText" class="grow" placeholder="Text to pixels (rough, 8px)" maxlength="40">
      <button class="small" id="stampBtn">Render</button>
    </div>
    <div class="row end" style="margin-top:10px"><button class="primary" id="showDraw">Show on display</button></div>
  </div>
</section>

<!-- ===================== DISPLAY ===================== -->
<section class="tab" id="tab-display" hidden>
  <div class="card">
    <h2>Appearance</h2>
    <label class="inline">Invert (dark text on lit background)<span class="sw"><input type="checkbox" data-key="invert"><span></span></span></label>
    <label class="inline">Upside down<span class="sw"><input type="checkbox" data-key="flipV"><span></span></span></label>
    <label class="inline">Mirror left/right<span class="sw"><input type="checkbox" data-key="flipH"><span></span></span></label>
    <label>Character spacing <span class="val" data-show="spacing"></span>
      <input type="range" min="0" max="4" data-key="spacing"></label>
    <p class="hint">Mirror doesn't combine with scroll left/right or slice effects (library limitation).</p>
  </div>
  <div class="card">
    <h2>Night mode</h2>
    <label class="inline">Enable night mode<span class="sw"><input type="checkbox" data-key="nightOn"><span></span></span></label>
    <div class="grid">
      <label>Starts at<select data-key="nightStart" class="hours"></select></label>
      <label>Ends at<select data-key="nightEnd" class="hours"></select></label>
    </div>
    <label style="margin-top:10px">Night brightness <span class="val" data-show="nightBright"></span>
      <input type="range" min="0" max="15" data-key="nightBright"></label>
    <label class="inline">Turn display off at night instead<span class="sw"><input type="checkbox" data-key="nightBlank"><span></span></span></label>
  </div>
</section>

<!-- ===================== SETTINGS ===================== -->
<section class="tab" id="tab-settings" hidden>
  <div class="card">
    <h2>WiFi</h2>
    <div class="grid">
      <label>Network<input type="text" id="wSsid" list="ssids" autocomplete="off"><datalist id="ssids"></datalist></label>
      <label>Password<input type="password" id="wPass" placeholder="unchanged"></label>
    </div>
    <div class="row" style="margin-top:10px">
      <button id="scanBtn">Scan</button><span class="grow hint" id="scanInfo"></span>
      <button class="primary" id="wifiSave">Save &amp; reboot</button>
    </div>
  </div>
  <div class="card">
    <h2>Hardware</h2>
    <div class="grid">
      <label>Module type<select data-key="hwType">
        <option value="1">FC-16 (most 4-in-1)</option><option value="0">Generic</option>
        <option value="2">Parola</option><option value="3">ICStation</option></select></label>
      <label>Number of 8x8 modules<input type="number" min="1" max="16" data-key="modules"></label>
    </div>
    <p class="hint">Text garbled, mirrored or split into blocks? Try another module type. Changing these reboots the board.</p>
  </div>
  <div class="card">
    <h2>Access</h2>
    <div class="grid">
      <label>Hostname (.local)<input type="text" data-key="host" data-str maxlength="32"></label>
      <label>Web password (user: admin)<input type="password" id="webPass" placeholder="none"></label>
    </div>
    <div class="row end" style="margin-top:10px"><button id="webPassSave">Set password</button></div>
  </div>
  <div class="card">
    <h2>Firmware update</h2>
    <p class="hint" style="margin-top:0">Arduino IDE: Sketch &gt; Export Compiled Binary, then upload the <code>.ino.bin</code> file.</p>
    <div class="row" style="margin-top:8px"><input type="file" id="fwFile" accept=".bin" class="grow"><button id="fwBtn">Upload</button></div>
    <div class="progress"><div id="fwBar"></div></div>
  </div>
  <div class="card">
    <h2>System</h2>
    <div id="sysInfo" class="hint" style="margin:0 0 10px"></div>
    <div class="row"><button id="rebootBtn">Reboot</button><button class="danger" id="resetBtn">Factory reset</button></div>
  </div>
  <div class="card">
    <h2>HTTP API</h2>
    <p class="hint" style="margin-top:0">
      <code>GET /api/say?text=Hello&amp;repeat=2</code><br><br>
      <code>POST /api/config {"brightness":8,"mode":1}</code><br><br>
      <code>GET /api/state</code>
    </p>
  </div>
</section>

</div>
<div id="toast"></div>

<script>
const $ = s => document.querySelector(s), $$ = s => [...document.querySelectorAll(s)];
let S = null, pending = {}, pushTimer = null, playlistDirty = false, lastTxt = null;

function toast(m) { const t = $('#toast'); t.textContent = m; t.classList.add('show'); clearTimeout(t._h); t._h = setTimeout(() => t.classList.remove('show'), 1800); }

async function post(url, body) {
  const r = await fetch(url, {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body)});
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || r.status);
  if (j.reboot) { toast('Rebooting…'); setTimeout(() => location.reload(), 6000); }
  return j;
}

function push(obj, quiet) {
  Object.assign(S, obj);
  if ('mode' in obj) renderMode();
  return post('/api/config', obj).then(() => { if (!quiet) toast('Saved'); }).catch(e => toast('Error: ' + e.message));
}

function queuePush(k, v) {
  pending[k] = v;
  clearTimeout(pushTimer);
  pushTimer = setTimeout(() => { const p = pending; pending = {}; push(p, true); }, 250);
}

// ---------- generic bound controls
function fmtVal(k, v) {
  if (k === 'pause') return (v / 1000).toFixed(v % 1000 ? 2 : 0) + ' s';
  if (k === 'speed') return v + ' ms/frame';
  return v;
}
function refreshLabels() { $$('[data-show]').forEach(e => e.textContent = fmtVal(e.dataset.show, S[e.dataset.show])); }

function fillBound() {
  $$('[data-key]').forEach(el => {
    const v = S[el.dataset.key];
    if (el.type === 'checkbox') el.checked = !!v; else el.value = v;
  });
  refreshLabels();
}

function bindControls() {
  $$('[data-key]').forEach(el => {
    const k = el.dataset.key, live = el.type === 'range';
    el.addEventListener(live ? 'input' : 'change', () => {
      let v = el.type === 'checkbox' ? el.checked : ('str' in el.dataset ? el.value : parseInt(el.value, 10));
      if (typeof v === 'number' && isNaN(v)) return;
      S[k] = v; refreshLabels();
      if (k === 'tz') syncTzSel();
      if (k === 'dateFmt') syncDateSel();
      queuePush(k, v);
    });
  });
}

// ---------- mode + tabs
function renderMode() { $$('#modeSeg button').forEach(b => b.classList.toggle('on', +b.dataset.mode === S.mode)); }
$$('#modeSeg button').forEach(b => b.onclick = () => push({mode: +b.dataset.mode}));
$$('#tabs button').forEach(b => b.onclick = () => {
  $$('#tabs button').forEach(x => x.classList.toggle('on', x === b));
  $$('.tab').forEach(t => t.hidden = t.id !== 'tab-' + b.dataset.tab);
  if (b.dataset.tab === 'draw') sizeCanvas();
});

// ---------- instant message
function say(text, fxIn, fxOut) {
  const p = new URLSearchParams({text});
  if (fxIn != null) p.set('fxIn', fxIn);
  if (fxOut != null) p.set('fxOut', fxOut);
  return fetch('/api/say', {method: 'POST', body: p}).then(() => toast('Sent'));
}
$('#sayBtn').onclick = () => { const t = $('#sayText').value.trim(); if (t) say(t); };
$('#sayText').onkeydown = e => { if (e.key === 'Enter') $('#sayBtn').click(); };

// ---------- playlist
function fxOptions(withDefault) {
  let h = withDefault ? '<option value="-1">Default</option>' : '';
  h += '<option value="255">Random</option>';
  S.effects.forEach((n, i) => h += `<option value="${i}">${n}</option>`);
  return h;
}
function setDirty(d) { playlistDirty = d; $('#savePlaylist').classList.toggle('dirty', d); }

function renderPlaylist() {
  const box = $('#playlist');
  box.innerHTML = '';
  S.playlist.forEach((m, i) => {
    const d = document.createElement('div');
    d.className = 'msg' + (m.en === false ? ' off' : '');
    d.innerHTML = `
      <div class="row"><input type="text" class="grow txt" maxlength="200" placeholder="Message text">
        <label class="sw"><input type="checkbox" class="en"><span></span></label></div>
      <details><summary>Animation for this message</summary><div class="grid">
        <label>Effect in<select class="fi">${fxOptions(true)}</select></label>
        <label>Effect out<select class="fo">${fxOptions(true)}</select></label>
        <label>Alignment<select class="al"><option value="-1">Default</option><option value="0">Left</option><option value="1">Center</option><option value="2">Right</option></select></label>
        <label>Speed (ms/frame)<input type="number" class="sp" min="0" max="1000" placeholder="default"></label>
        <label>Pause (ms)<input type="number" class="pa" min="0" max="60000" step="250" placeholder="default"></label>
      </div></details>
      <div class="row end">
        <button class="small ghost" data-a="play">Preview</button>
        <button class="small ghost" data-a="up">↑</button>
        <button class="small ghost" data-a="down">↓</button>
        <button class="small ghost danger" data-a="del">Delete</button>
      </div>`;
    const q = s => d.querySelector(s);
    q('.txt').value = m.t || '';
    q('.en').checked = m.en !== false;
    q('.fi').value = m.fi ?? -1; q('.fo').value = m.fo ?? -1; q('.al').value = m.al ?? -1;
    q('.sp').value = m.sp >= 0 ? m.sp : ''; q('.pa').value = m.pa >= 0 ? m.pa : '';
    q('.txt').onfocus = e => lastTxt = e.target;
    q('.txt').oninput = e => { m.t = e.target.value; setDirty(true); };
    q('.en').onchange = e => { m.en = e.target.checked; d.classList.toggle('off', !m.en); setDirty(true); };
    ['fi', 'fo', 'al'].forEach(k => q('.' + k).onchange = e => { m[k] = +e.target.value; setDirty(true); });
    ['sp', 'pa'].forEach(k => q('.' + k).oninput = e => { const v = parseInt(e.target.value, 10); m[k] = isNaN(v) ? -1 : v; setDirty(true); });
    d.querySelectorAll('[data-a]').forEach(b => b.onclick = () => {
      const a = b.dataset.a, L = S.playlist;
      if (a === 'play') return say(m.t, m.fi >= 0 ? m.fi : S.fxIn, m.fo >= 0 ? m.fo : S.fxOut);
      if (a === 'del') L.splice(i, 1);
      if (a === 'up' && i > 0) [L[i - 1], L[i]] = [L[i], L[i - 1]];
      if (a === 'down' && i < L.length - 1) [L[i + 1], L[i]] = [L[i], L[i + 1]];
      setDirty(true); renderPlaylist();
    });
    box.appendChild(d);
  });
  $('#addMsg').disabled = S.playlist.length >= 20;
}
$('#addMsg').onclick = () => { S.playlist.push({t: '', en: true, fi: -1, fo: -1, al: -1, sp: -1, pa: -1}); setDirty(true); renderPlaylist(); $$('#playlist .txt').pop().focus(); };
$('#savePlaylist').onclick = () => push({playlist: S.playlist}).then(() => setDirty(false));
$$('#tokens .chip').forEach(c => c.onclick = () => {
  const el = lastTxt || $$('#playlist .txt').pop();
  if (!el) return;
  const p = el.selectionStart ?? el.value.length;
  el.value = el.value.slice(0, p) + c.textContent + el.value.slice(el.selectionEnd ?? p);
  el.dispatchEvent(new Event('input')); el.focus();
});

// ---------- clock
// [label, POSIX TZ, browser (IANA) zones that map to it]
const TZ = [
  ['UTC', 'UTC0', 'UTC Etc/UTC Etc/GMT'],
  ['London, Dublin, Lisbon', 'GMT0BST,M3.5.0/1,M10.5.0', 'Europe/London Europe/Dublin Europe/Lisbon'],
  ['Central Europe (Berlin, Paris)', 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Berlin Europe/Paris Europe/Madrid Europe/Rome Europe/Amsterdam Europe/Brussels Europe/Vienna Europe/Zurich Europe/Stockholm Europe/Oslo Europe/Copenhagen Europe/Prague Europe/Warsaw Europe/Budapest Europe/Belgrade'],
  ['Eastern Europe (Athens, Kyiv)', 'EET-2EEST,M3.5.0/3,M10.5.0/4', 'Europe/Athens Europe/Kiev Europe/Kyiv Europe/Bucharest Europe/Helsinki Europe/Sofia Europe/Riga Europe/Vilnius Europe/Tallinn'],
  ['Moscow, Istanbul', 'MSK-3', 'Europe/Moscow Europe/Istanbul'],
  ['Dubai', '<+04>-4', 'Asia/Dubai Asia/Muscat'],
  ['Pakistan', 'PKT-5', 'Asia/Karachi'],
  ['India, Sri Lanka', 'IST-5:30', 'Asia/Kolkata Asia/Calcutta Asia/Colombo'],
  ['Nepal', '<+0545>-5:45', 'Asia/Kathmandu Asia/Katmandu'],
  ['Bangladesh', '<+06>-6', 'Asia/Dhaka'],
  ['Bangkok, Jakarta', '<+07>-7', 'Asia/Bangkok Asia/Jakarta Asia/Ho_Chi_Minh Asia/Saigon'],
  ['China, Singapore, Philippines', 'CST-8', 'Asia/Shanghai Asia/Singapore Asia/Manila Asia/Hong_Kong Asia/Taipei Asia/Kuala_Lumpur Australia/Perth'],
  ['Japan, Korea', 'JST-9', 'Asia/Tokyo Asia/Seoul'],
  ['Sydney, Melbourne', 'AEST-10AEDT,M10.1.0,M4.1.0/3', 'Australia/Sydney Australia/Melbourne Australia/Canberra Australia/Hobart'],
  ['New Zealand', 'NZST-12NZDT,M9.5.0,M4.1.0/3', 'Pacific/Auckland'],
  ['US Eastern', 'EST5EDT,M3.2.0,M11.1.0', 'America/New_York America/Toronto America/Detroit'],
  ['US Central', 'CST6CDT,M3.2.0,M11.1.0', 'America/Chicago America/Winnipeg'],
  ['US Mountain', 'MST7MDT,M3.2.0,M11.1.0', 'America/Denver America/Edmonton'],
  ['Arizona', 'MST7', 'America/Phoenix'],
  ['US Pacific', 'PST8PDT,M3.2.0,M11.1.0', 'America/Los_Angeles America/Vancouver'],
  ['Brazil (São Paulo)', '<-03>3', 'America/Sao_Paulo'],
  ['South Africa', 'SAST-2', 'Africa/Johannesburg'],
  ['West Africa (Lagos)', 'WAT-1', 'Africa/Lagos'],
  ['Custom…', '', '']
];
const DATES = [['11 Sep 2026', '%d %b %Y'], ['11/09/2026', '%d/%m/%Y'], ['09/11/2026', '%m/%d/%Y'], ['2026-09-11', '%Y-%m-%d'], ['Fri 11 Sep', '%a %d %b'], ['September 11', '%B %d'], ['Custom…', '']];
$('#tzSel').innerHTML = TZ.map(([n, v]) => `<option value="${v}">${n}</option>`).join('');
$('#dateSel').innerHTML = DATES.map(([n, v]) => `<option value="${v}">${n}</option>`).join('');
function syncTzSel() { $('#tzSel').value = TZ.some(t => t[1] === S.tz) ? S.tz : ''; }
function syncDateSel() { $('#dateSel').value = DATES.some(t => t[1] === S.dateFmt) ? S.dateFmt : ''; }
$('#tzSel').onchange = e => { if (e.target.value) { $('#tzInput').value = e.target.value; push({tz: e.target.value}); } else $('#tzInput').focus(); };
$('#dateSel').onchange = e => { if (e.target.value) { $('#dateInput').value = e.target.value; push({dateFmt: e.target.value}); } else $('#dateInput').focus(); };
$$('.hours').forEach(s => s.innerHTML = [...Array(24).keys()].map(h => `<option value="${h}">${String(h).padStart(2, '0')}:00</option>`).join(''));

// ---------- pixel editor
const cv = $('#cv'), ctx = cv.getContext('2d');
let bmp = [], cell = 14, tool = 'pen', painting = false, paintVal = 1, sendTimer = null;

function loadBitmap() {
  bmp = [];
  for (let x = 0; x < S.cols; x++) bmp.push(parseInt((S.bitmap || '').substr(x * 2, 2), 16) || 0);
}
const bmpHex = () => bmp.map(b => b.toString(16).padStart(2, '0')).join('');
const getPx = (x, y) => (bmp[x] >> y) & 1;
const setPx = (x, y, v) => { bmp[x] = v ? bmp[x] | (1 << y) : bmp[x] & ~(1 << y); };

function sizeCanvas() {
  const avail = cv.parentElement.clientWidth - 16;
  cell = Math.max(8, Math.min(28, Math.floor(avail / S.cols)));
  const dpr = window.devicePixelRatio || 1;
  cv.width = S.cols * cell * dpr; cv.height = 8 * cell * dpr;
  cv.style.width = S.cols * cell + 'px'; cv.style.height = 8 * cell + 'px';
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  drawCanvas();
}
function drawCanvas() {
  ctx.fillStyle = '#000'; ctx.fillRect(0, 0, S.cols * cell, 8 * cell);
  const r = cell * 0.38;
  for (let x = 0; x < S.cols; x++) for (let y = 0; y < 8; y++) {
    ctx.beginPath(); ctx.arc(x * cell + cell / 2, y * cell + cell / 2, r, 0, 7);
    ctx.fillStyle = getPx(x, y) ? '#ff3b30' : '#2a1110'; ctx.fill();
  }
  ctx.strokeStyle = '#333'; ctx.lineWidth = 1;
  for (let m = 8; m < S.cols; m += 8) { ctx.beginPath(); ctx.moveTo(m * cell + .5, 0); ctx.lineTo(m * cell + .5, 8 * cell); ctx.stroke(); }
}
function sendBitmap(show) {
  S.bitmap = bmpHex();
  return post('/api/draw', {bitmap: S.bitmap, show: !!show}).catch(e => toast('Error: ' + e.message));
}
function scheduleSend() {
  if (!$('#live').checked || sendTimer) return;
  sendTimer = setTimeout(() => { sendTimer = null; sendBitmap(false); }, 120);
}
function cellAt(e) {
  const b = cv.getBoundingClientRect();
  return [Math.floor((e.clientX - b.left) / cell), Math.floor((e.clientY - b.top) / cell)];
}
function paintAt(e) {
  const [x, y] = cellAt(e);
  if (x < 0 || y < 0 || x >= S.cols || y >= 8) return;
  if (getPx(x, y) === paintVal) return;
  setPx(x, y, paintVal); drawCanvas(); scheduleSend();
}
cv.onpointerdown = e => {
  const [x, y] = cellAt(e);
  if (x < 0 || y < 0 || x >= S.cols || y >= 8) return;
  painting = true; cv.setPointerCapture(e.pointerId);
  paintVal = tool === 'pen' ? 1 : tool === 'erase' ? 0 : 1 - getPx(x, y);
  paintAt(e);
};
cv.onpointermove = e => { if (painting) paintAt(e); };
cv.onpointerup = cv.onpointercancel = () => { painting = false; };
$$('#toolSeg button').forEach(b => b.onclick = () => { tool = b.dataset.tool; $$('#toolSeg button').forEach(x => x.classList.toggle('on', x === b)); });
$$('[data-op]').forEach(b => b.onclick = () => {
  const op = b.dataset.op, n = S.cols;
  if (op === 'clear') bmp.fill(0);
  if (op === 'fill') bmp.fill(255);
  if (op === 'invert') bmp = bmp.map(v => ~v & 255);
  if (op === 'left') bmp = bmp.slice(1).concat(bmp[0]);
  if (op === 'right') bmp = [bmp[n - 1]].concat(bmp.slice(0, n - 1));
  if (op === 'up') bmp = bmp.map(v => ((v >> 1) | (v << 7)) & 255);
  if (op === 'down') bmp = bmp.map(v => ((v << 1) | (v >> 7)) & 255);
  if (op === 'flip') bmp.reverse();
  drawCanvas(); scheduleSend();
});
$('#stampBtn').onclick = () => {
  const t = $('#stampText').value;
  const c = document.createElement('canvas'); c.width = S.cols; c.height = 8;
  const g = c.getContext('2d');
  g.fillStyle = '#000'; g.fillRect(0, 0, S.cols, 8);
  g.fillStyle = '#fff'; g.font = '8px monospace'; g.textBaseline = 'top';
  const w = g.measureText(t).width;
  g.fillText(t, Math.max(0, (S.cols - w) / 2), 0);
  const px = g.getImageData(0, 0, S.cols, 8).data;
  for (let x = 0; x < S.cols; x++) for (let y = 0; y < 8; y++) setPx(x, y, px[(y * S.cols + x) * 4] > 100);
  drawCanvas(); scheduleSend();
};
$('#showDraw').onclick = () => { sendBitmap(true).then(() => { S.mode = 2; renderMode(); toast('Showing drawing'); }); };
window.addEventListener('resize', () => { if (!$('#tab-draw').hidden) sizeCanvas(); });

// ---------- settings
$('#scanBtn').onclick = async () => {
  $('#scanInfo').textContent = 'Scanning…';
  try {
    const list = await (await fetch('/api/scan')).json();
    $('#ssids').innerHTML = list.map(n => `<option value="${n.ssid.replace(/"/g, '&quot;')}">${n.rssi} dBm${n.open ? ' · open' : ''}</option>`).join('');
    $('#scanInfo').textContent = list.length + ' networks found';
  } catch { $('#scanInfo').textContent = 'Scan failed'; }
};
$('#wifiSave').onclick = () => {
  const ssid = $('#wSsid').value.trim();
  if (!ssid) return toast('Enter a network name');
  const body = {ssid};
  if ($('#wPass').value) body.pass = $('#wPass').value;
  post('/api/wifi', body).then(() => toast('Saved. Reconnect to your WiFi and open http://' + S.host + '.local'));
};
$('#webPassSave').onclick = () => {
  const p = $('#webPass').value;
  if (!p && !confirm('Remove the web password?')) return;
  push({webPass: p}).then(() => toast(p ? 'Password set (user: admin)' : 'Password removed'));
};
$('#rebootBtn').onclick = () => confirm('Reboot the display?') && post('/api/reboot', {});
$('#resetBtn').onclick = () => confirm('Erase ALL settings including WiFi?') && post('/api/reset', {});
$('#fwBtn').onclick = () => {
  const f = $('#fwFile').files[0];
  if (!f) return toast('Choose a .bin file');
  const x = new XMLHttpRequest(), fd = new FormData();
  fd.append('firmware', f);
  x.upload.onprogress = e => $('#fwBar').style.width = (e.loaded / e.total * 100) + '%';
  x.onload = () => { if (x.status === 200) { toast('Updated, rebooting…'); setTimeout(() => location.reload(), 8000); } else toast('Update failed'); };
  x.onerror = () => toast('Upload error');
  x.open('POST', '/update'); x.send(fd);
};

// ---------- time sync
let lastStatus = null, statusAt = 0, lastAutoSync = 0;
const pad2 = n => String(n).padStart(2, '0');
const tzAutoDone = () => { try { return localStorage.getItem('tzAuto') === '1'; } catch { return false; } };
const markTzAuto = () => { try { localStorage.setItem('tzAuto', '1'); } catch {} };

// Time zone of this browser as POSIX TZ: a known zone keeps its DST rules, otherwise a fixed offset.
function browserTz(st) {
  const iana = Intl.DateTimeFormat().resolvedOptions().timeZone || '';
  const hit = TZ.find(t => t[2] && t[2].split(' ').includes(iana));
  if (hit) return hit[1] !== S.tz ? hit[1] : null;
  const off = -new Date().getTimezoneOffset(), a = Math.abs(off), h = Math.floor(a / 60), m = a % 60;
  if (st && st.utcOffset === off) return null;
  if (!off) return 'UTC0';
  return `<${off > 0 ? '+' : '-'}${pad2(h)}${m ? pad2(m) : ''}>${off > 0 ? '-' : ''}${h}${m ? ':' + pad2(m) : ''}`;
}

async function syncTime(auto) {
  const body = {ms: Date.now()};
  if (!auto || (S.tz === 'UTC0' && !tzAutoDone())) {
    const tz = browserTz(lastStatus);
    if (tz) body.tz = tz;
    if (auto) markTzAuto();
  }
  try {
    await post('/api/time', body);
    if (body.tz) { S.tz = body.tz; $('#tzInput').value = body.tz; syncTzSel(); }
    toast(auto ? 'Clock set from this device' : 'Time synced from this device');
    pollStatus();
  } catch (e) { toast('Time sync failed: ' + e.message); }
}

function maybeAutoSync(st) {
  const drift = Math.abs(st.epoch - Date.now() / 1000);
  const needed = !st.synced || (st.timeSrc !== 'internet' && drift > 10) ||
                 (S.tz === 'UTC0' && !tzAutoDone() && browserTz(st));
  if (needed && Date.now() - lastAutoSync > 30000) { lastAutoSync = Date.now(); syncTime(true); }
}

function tickTime() {
  const st = lastStatus;
  if (!st) return;
  let txt = '--:--:--';
  if (st.synced) {
    const d = new Date((st.epoch + (Date.now() - statusAt) / 1000 + st.utcOffset * 60) * 1000);
    let h = d.getUTCHours();
    const suffix = S.clock24 ? '' : (h < 12 ? ' AM' : ' PM');
    if (!S.clock24) h = h % 12 || 12;
    txt = `${S.clock24 ? pad2(h) : h}:${pad2(d.getUTCMinutes())}:${pad2(d.getUTCSeconds())}${suffix}`;
  }
  $('#devTime').textContent = txt;
  $('#clockPill').textContent = st.synced ? txt : 'no time · tap to sync';
}

// ---------- status
function renderStatus(st) {
  lastStatus = st; statusAt = Date.now();
  tickTime();
  const src = {internet: 'internet', device: 'set from a phone/computer', none: 'not set'}[st.timeSrc] || st.timeSrc;
  const oa = Math.abs(st.utcOffset);
  let info = `Source: ${src} · UTC${st.utcOffset < 0 ? '-' : '+'}${Math.floor(oa / 60)}${oa % 60 ? ':' + pad2(oa % 60) : ''}`;
  if (st.synced) {
    const drift = Math.round(st.epoch - Date.now() / 1000);
    if (Math.abs(drift) > 5) info += ` · ${Math.abs(drift)}s ${drift > 0 ? 'ahead of' : 'behind'} this device`;
    if (st.utcOffset !== -new Date().getTimezoneOffset()) info += ' · time zone differs from this device';
  }
  $('#timeInfo').textContent = info;
  const np = $('#netPill');
  np.textContent = st.ap ? 'Setup AP' : `${st.ip} · ${st.rssi} dBm`;
  np.classList.toggle('ok', !st.ap);
  const up = st.uptime, h = Math.floor(up / 3600), m = Math.floor(up / 60) % 60;
  $('#sysInfo').innerHTML = `Firmware ${st.fw} · IP ${st.ip} · Free heap ${Math.round(st.heap / 1024)} KB · Uptime ${h}h ${m}m${st.night ? ' · Night mode active' : ''}`;
}
async function pollStatus() {
  try { const st = await (await fetch('/api/status')).json(); renderStatus(st); maybeAutoSync(st); } catch {}
}

async function init() {
  S = await (await fetch('/api/state')).json();
  $$('.fxsel').forEach(s => s.innerHTML = fxOptions(false));
  fillBound(); bindControls(); renderMode(); renderPlaylist();
  syncTzSel(); syncDateSel();
  $('#wSsid').value = S.ssid || '';
  if (S.webPassSet) $('#webPass').placeholder = 'set (type to change)';
  loadBitmap(); renderStatus(S.status); maybeAutoSync(S.status);
  setInterval(pollStatus, 5000);
  setInterval(tickTime, 1000);
}
window.addEventListener('beforeunload', e => { if (playlistDirty) { e.preventDefault(); e.returnValue = ''; } });
init().catch(e => toast('Failed to load: ' + e.message));
</script>
</body></html>
)rawliteral";
