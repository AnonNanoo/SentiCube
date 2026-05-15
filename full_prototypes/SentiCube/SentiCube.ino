/*
 * ============================================================
 *  SentiCube, Complete Firmware
 *  ESP32 · All Sensors · LoRa TX · SD Logging · Full Web UI
 * ============================================================
 *
 *  PIN LAYOUT
 *  ──────────
 *  I2C  SDA 21 / SCL 22   (MPU-6050, QMC5883P, AHT10, VL53L0X)
 *  I2S  SCK 32 / WS 25 / SD 33   (INMP441 microphone)
 *  SPI-SD   MOSI 23 / MISO 19 / SCK 18 / CS 13
 *  SPI-LoRa MOSI 27 / MISO 26 / SCK 5  / NSS 4 / RST 16 / DIO0 17
 *
 *  LIBRARIES REQUIRED
 *  ────────────────
 *  WiFiManager, Adafruit_AHTX0, Adafruit_VL53L0X,
 *  LoRa (Sandeep Mistry), SD, SPI, Wire, driver/i2s
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "driver/i2s.h"
#include <LoRa.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_VL53L0X.h>

// ─── I2C ────────────────────────────────────────────────────
#define SDA_PIN  21
#define SCL_PIN  22

// ─── I2S (INMP441) ──────────────────────────────────────────
#define I2S_WS     25
#define I2S_SD_PIN 33
#define I2S_SCK    32

// ─── SD Card (SPI bus A) ────────────────────────────────────
#define SD_MOSI   23
#define SD_MISO   19
#define SD_SCK    18
#define SD_CS     13

// ─── LoRa SX1278 (SPI bus B) ────────────────────────────────
#define LORA_SCK  5
#define LORA_MISO 26
#define LORA_MOSI 27
#define LORA_SS   4
#define LORA_RST  16
#define LORA_DIO0 17
#define LORA_FREQ 433E6

// ─── I2C Addresses ──────────────────────────────────────────
#define MPU_ADDR  0x68
#define QMC_ADDR  0x2C   // try 0x0D if this fails

// ─── Objects ────────────────────────────────────────────────
WebServer server(80);
Adafruit_AHTX0 aht;
Adafruit_VL53L0X lox;
SPIClass spiSD(VSPI);

// ─── Sensor Status ──────────────────────────────────────────
bool mpuOK = false, ahtOK = false, tofOK = false, qmcOK = false, sdOK = false, loraOK = false;

// ─── Sensor Data ────────────────────────────────────────────
float temp = 0, hum = 0, distance = 0;
float ax = 0, ay = 0, az = 0;
float gx = 0, gy = 0, gz = 0;
float ax0 = 0, ay0 = 0, az0 = 0;
float gx0 = 0, gy0 = 0, gz0 = 0;
float heading = -1;
String rmsValue = "0";
String peakValue = "0";

// ─── ToF baseline / event tracking ──────────────────────────
float tofBaseline = -1;
float prevDistance = -1;
bool prevMotionAlert = false;
bool prevDistAlert = false;
bool prevSoundAlert = false;
float tofDelta = 0;
float tofRate = 0;

// ─── Timing ─────────────────────────────────────────────────
unsigned long lastSensorRead = 0;
unsigned long lastLoRaSend = 0;
unsigned long lastSDLog = 0;

// ─────────────────────────────────────────────────────────────
//  EMBEDDED WEB UI
// ─────────────────────────────────────────────────────────────
const char index_html[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SentiCube</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{
  --bg:#080c12;
  --surface:#0f1520;
  --surface2:#151d2e;
  --border:#1e2d45;
  --accent:#3b82f6;
  --accent-dim:rgba(59,130,246,.15);
  --warn:#f59e0b;
  --warn-dim:rgba(245,158,11,.15);
  --danger:#ef4444;
  --danger-dim:rgba(239,68,68,.15);
  --ok:#22c55e;
  --ok-dim:rgba(34,197,94,.15);
  --text:#e2e8f0;
  --muted:#64748b;
  --radius:14px;
  --font:'IBM Plex Mono',monospace;
}
@import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@300;400;500;600&family=Space+Grotesk:wght@400;500;700&display=swap');
html,body{min-height:100vh;background:var(--bg);color:var(--text);font-family:var(--font)}
body::before{
  content:'';position:fixed;inset:0;
  background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,.08) 2px,rgba(0,0,0,.08) 4px);
  pointer-events:none;z-index:1000
}
.wrapper{max-width:1200px;margin:0 auto;padding:24px 16px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:32px;flex-wrap:wrap;gap:12px}
.logo{display:flex;align-items:center;gap:12px}
.logo-icon{width:36px;height:36px;border:2px solid var(--accent);border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:18px}
.logo h1{font-family:'Space Grotesk',sans-serif;font-size:22px;font-weight:700;letter-spacing:.04em;color:#fff}
.logo p{font-size:11px;color:var(--muted);letter-spacing:.08em;margin-top:2px}
.header-right{display:flex;align-items:center;gap:10px}
.status-banner{border-radius:var(--radius);padding:14px 20px;display:flex;align-items:center;gap:12px;margin-bottom:24px;border:1px solid transparent;transition:all .4s}
.status-banner.ok{background:var(--ok-dim);border-color:rgba(34,197,94,.3)}
.status-banner.warn{background:var(--warn-dim);border-color:rgba(245,158,11,.3)}
.status-banner.danger{background:var(--danger-dim);border-color:rgba(239,68,68,.3)}
.status-dot{width:10px;height:10px;border-radius:50%;flex-shrink:0;animation:pulse 2s infinite}
.status-banner.ok .status-dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.status-banner.warn .status-dot{background:var(--warn);box-shadow:0 0 8px var(--warn)}
.status-banner.danger .status-dot{background:var(--danger);box-shadow:0 0 8px var(--danger)}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.4}}
.status-banner span{font-size:13px;font-weight:500}
.grid-3{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}
.grid-2{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.grid-sensor{display:grid;grid-template-columns:repeat(2,1fr);gap:10px}
@media(max-width:900px){.grid-3{grid-template-columns:1fr 1fr}.main-layout{flex-direction:column!important}}
@media(max-width:600px){.grid-3,.grid-2{grid-template-columns:1fr}}
.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);padding:16px}
.card-title{font-size:10px;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin-bottom:10px;display:flex;align-items:center;gap:6px}
.vault-status{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.vault-dot{width:8px;height:8px;border-radius:50%}
.vault-dot.secured{background:var(--ok);box-shadow:0 0 6px var(--ok)}
.vault-dot.tampered{background:var(--danger);box-shadow:0 0 6px var(--danger);animation:pulse 1s infinite}
.vault-name{font-family:'Space Grotesk',sans-serif;font-size:15px;font-weight:600;color:#fff}
.vault-label{font-size:11px;color:var(--muted);margin-top:4px}
.vault-loc{font-size:10px;color:var(--muted);margin-top:6px;opacity:.7}
.section-heading{font-family:'Space Grotesk',sans-serif;font-size:13px;font-weight:600;color:var(--text);margin-bottom:12px;letter-spacing:.02em}
.main-layout{display:flex;gap:20px;margin-top:24px}
.col-left{flex:1;display:flex;flex-direction:column;gap:20px}
.col-right{width:360px;flex-shrink:0;display:flex;flex-direction:column;gap:20px}
.timeline{display:flex;flex-direction:column;gap:0}
.tl-item{display:flex;gap:12px;padding:10px 0;border-bottom:1px solid var(--border);align-items:flex-start}
.tl-item:last-child{border-bottom:none}
.tl-icon{width:28px;height:28px;border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:12px;flex-shrink:0;margin-top:2px}
.tl-icon.normal{background:var(--accent-dim);color:var(--accent)}
.tl-icon.warning{background:var(--warn-dim);color:var(--warn)}
.tl-icon.danger{background:var(--danger-dim);color:var(--danger)}
.tl-msg{font-size:12px;color:var(--text);line-height:1.4}
.tl-time{font-size:10px;color:var(--muted);margin-top:2px}
.mode-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:4px}
.mode-btn{border:1px solid var(--border);background:var(--surface2);border-radius:10px;padding:12px 8px;text-align:center;cursor:pointer;transition:all .2s;font-family:var(--font);font-size:11px;color:var(--muted);letter-spacing:.06em}
.mode-btn:hover{border-color:var(--accent);color:var(--text)}
.mode-btn.active-locked{border-color:var(--danger);background:var(--danger-dim);color:var(--danger)}
.mode-btn.active-maintenance{border-color:var(--warn);background:var(--warn-dim);color:var(--warn)}
.mode-btn.active-armed{border-color:var(--ok);background:var(--ok-dim);color:var(--ok)}
.mode-icon{font-size:20px;margin-bottom:4px}
.conn-status{display:flex;align-items:center;gap:8px}
.conn-dot{width:8px;height:8px;border-radius:50%}
.conn-online .conn-dot{background:var(--ok);box-shadow:0 0 6px var(--ok);animation:pulse 2s infinite}
.conn-offline .conn-dot{background:var(--danger)}
.conn-label{font-size:13px;color:var(--text)}
.conn-sub{font-size:10px;color:var(--muted);margin-top:4px}
.lora-badge{display:inline-flex;align-items:center;gap:5px;background:var(--accent-dim);border:1px solid rgba(59,130,246,.3);border-radius:20px;padding:3px 10px;font-size:10px;color:var(--accent);margin-top:8px}
.insight-item{display:flex;align-items:center;gap:10px;padding:10px;background:var(--surface2);border-radius:10px;border:1px solid var(--border)}
.insight-icon{font-size:18px;width:32px;text-align:center}
.insight-label{font-size:10px;color:var(--muted);letter-spacing:.06em;text-transform:uppercase}
.insight-value{font-size:13px;color:var(--text);margin-top:2px;font-weight:500}
.sensor-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;margin-top:8px}
.s-card{background:var(--surface2);border:1px solid var(--border);border-radius:10px;padding:12px}
.s-label{font-size:9px;letter-spacing:.1em;text-transform:uppercase;color:var(--muted);margin-bottom:4px}
.s-value{font-size:14px;color:#fff;font-weight:500;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.s-value.ok{color:var(--ok)}
.s-value.warn{color:var(--warn)}
.refresh-pill{display:flex;align-items:center;gap:6px;font-size:10px;color:var(--muted);padding:4px 10px;border:1px solid var(--border);border-radius:20px}
.spin{animation:spin 1s linear infinite;display:inline-block}
@keyframes spin{to{transform:rotate(360deg)}}
.hid{display:none}
.baseline-row{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}
.baseline-btn{border:1px solid var(--border);background:var(--surface2);color:var(--text);border-radius:10px;padding:10px 12px;cursor:pointer;font-family:var(--font);font-size:11px}
.baseline-btn:hover{border-color:var(--accent)}
.small-note{font-size:10px;color:var(--muted);margin-top:8px;line-height:1.4}
</style>
</head>
<body>
<div class="wrapper">
  <header>
    <div class="logo">
      <div class="logo-icon"></div>
      <div>
        <h1>SentiCube</h1>
        <p>VAULT &amp; SAFE PROTECTION SYSTEM</p>
      </div>
    </div>
    <div class="header-right">
      <div class="refresh-pill">
        <span id="spin-icon" class="spin hid">↻</span>
        <span id="last-update">–</span>
      </div>
    </div>
  </header>

  <div class="status-banner warn" id="status-banner">
    <div class="status-dot"></div>
    <span id="status-text">Loading sensor data…</span>
  </div>

  <div class="section-heading">Monitored Zones</div>
  <div class="grid-3">
    <div class="card">
      <div class="vault-status">
        <div class="vault-dot secured" id="zone-motion-dot"></div>
        <span class="vault-name">Motion</span>
      </div>
      <div class="vault-label" id="zone-motion-status">Checking…</div>
      <div class="vault-loc">MPU-6050 · Accel + Gyro</div>
    </div>
    <div class="card">
      <div class="vault-status">
        <div class="vault-dot secured" id="zone-dist-dot"></div>
        <span class="vault-name">Proximity</span>
      </div>
      <div class="vault-label" id="zone-dist-status">Checking…</div>
      <div class="vault-loc">VL53L0X · Time-of-Flight</div>
      <div class="baseline-row">
        <button class="baseline-btn" onclick="resetBaseline()">Reset baseline</button>
      </div>
      <div class="small-note" id="baseline-note">Baseline not set yet.</div>
    </div>
    <div class="card">
      <div class="vault-status">
        <div class="vault-dot secured" id="zone-sound-dot"></div>
        <span class="vault-name">Sound</span>
      </div>
      <div class="vault-label" id="zone-sound-status">Checking…</div>
      <div class="vault-loc">INMP441 · I2S Microphone</div>
    </div>
  </div>

  <div class="main-layout">
    <div class="col-left">
      <div class="card">
        <div class="card-title">⚡ Live Sensor Readings</div>
        <div class="sensor-grid">
          <div class="s-card"><div class="s-label">Temperature</div><div class="s-value" id="s-temp">–</div></div>
          <div class="s-card"><div class="s-label">Humidity</div><div class="s-value" id="s-hum">–</div></div>
          <div class="s-card"><div class="s-label">Distance (ToF)</div><div class="s-value" id="s-dist">–</div></div>
          <div class="s-card"><div class="s-label">Heading (Mag)</div><div class="s-value" id="s-heading">–</div></div>
          <div class="s-card"><div class="s-label">Accel X / Y / Z</div><div class="s-value" id="s-accel">–</div></div>
          <div class="s-card"><div class="s-label">Gyro X / Y / Z</div><div class="s-value" id="s-gyro">–</div></div>
          <div class="s-card"><div class="s-label">Sound RMS</div><div class="s-value" id="s-rms">–</div></div>
          <div class="s-card"><div class="s-label">Sound Peak</div><div class="s-value" id="s-peak">–</div></div>
        </div>
      </div>

      <div class="card">
        <div class="card-title">📋 Activity Timeline</div>
        <div class="timeline" id="timeline">
          <div class="tl-item">
            <div class="tl-icon normal">🔑</div>
            <div><div class="tl-msg">System online — awaiting sensor data</div><div class="tl-time">Just now</div></div>
          </div>
        </div>
      </div>
    </div>

    <div class="col-right">
      <div class="card">
        <div class="card-title">🔐 Security Mode</div>
        <div class="mode-grid">
          <div class="mode-btn active-locked" id="mode-locked" onclick="setMode('locked')">
            <div class="mode-icon">🔒</div>LOCKED
          </div>
          <div class="mode-btn" id="mode-maintenance" onclick="setMode('maintenance')">
            <div class="mode-icon">🔧</div>MAINT
          </div>
          <div class="mode-btn" id="mode-armed" onclick="setMode('armed')">
            <div class="mode-icon">🛡️</div>ARMED
          </div>
        </div>
      </div>

      <div class="card">
        <div class="card-title">📡 Connection</div>
        <div style="display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px">
          <div>
            <div class="conn-status conn-online" id="conn-status">
              <div class="conn-dot"></div>
              <span class="conn-label" id="conn-label">Online</span>
            </div>
            <div class="conn-sub" id="conn-ip">–</div>
          </div>
          <div class="lora-badge" id="lora-badge">📻 LoRa TX –</div>
        </div>
      </div>

      <div class="card">
        <div class="card-title">🔍 Sensor Insights</div>
        <div style="display:flex;flex-direction:column;gap:8px;margin-top:4px">
          <div class="insight-item">
            <div class="insight-icon">🌡️</div>
            <div><div class="insight-label">Environment</div><div class="insight-value" id="ins-env">Optimal conditions</div></div>
          </div>
          <div class="insight-item">
            <div class="insight-icon">📳</div>
            <div><div class="insight-label">Vibration / Motion</div><div class="insight-value" id="ins-vib">Stable environment</div></div>
          </div>
          <div class="insight-item">
            <div class="insight-icon">🚪</div>
            <div><div class="insight-label">Proximity</div><div class="insight-value" id="ins-dist">All vaults sealed</div></div>
          </div>
          <div class="insight-item">
            <div class="insight-icon">🔊</div>
            <div><div class="insight-label">Sound Level</div><div class="insight-value" id="ins-sound">Quiet</div></div>
          </div>
          <div class="insight-item">
            <div class="insight-icon">🧭</div>
            <div><div class="insight-label">Orientation</div><div class="insight-value" id="ins-mag">Stable heading</div></div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>

<script>
let currentMode = 'locked';
let loraCount = 0;
let lastData = null;
let prevMotion = false, prevDistAlert = false, prevSoundAlert = false;
const log = [
  { icon:'🔑', msg:'System started — monitoring active', time:fmtTime(new Date()), sev:'normal' }
];

function fmtTime(d){ return d.toLocaleTimeString(); }

function setMode(m) {
  currentMode = m;
  ['locked','maintenance','armed'].forEach(k => {
    const el = document.getElementById('mode-'+k);
    el.className = 'mode-btn';
    if(k === m) el.classList.add('active-'+k);
  });
  pushLog(m==='locked'?'🔒':m==='maintenance'?'🔧':'🛡️','Mode changed to '+m.toUpperCase(),'normal');
}

function pushLog(icon, msg, sev) {
  log.unshift({ icon, msg, time: fmtTime(new Date()), sev });
  if(log.length > 20) log.pop();
  renderLog();
}

function renderLog() {
  const tl = document.getElementById('timeline');
  tl.innerHTML = log.slice(0,10).map(e => `
    <div class="tl-item">
      <div class="tl-icon ${e.sev}">${e.icon}</div>
      <div><div class="tl-msg">${e.msg}</div><div class="tl-time">${e.time}</div></div>
    </div>`).join('');
}

function setZone(id, ok, msg) {
  document.getElementById('zone-'+id+'-dot').className = 'vault-dot ' + (ok ? 'secured' : 'tampered');
  document.getElementById('zone-'+id+'-status').textContent = msg;
}

function setStatus(type, msg) {
  document.getElementById('status-banner').className = 'status-banner ' + type;
  document.getElementById('status-text').textContent = msg;
}

function setText(id, txt) { document.getElementById(id).textContent = txt; }

async function resetBaseline() {
  try {
    const r = await fetch('/reset-baseline', { method:'POST' });
    const t = await r.text();
    pushLog('📏', t || 'Baseline reset', 'normal');
    document.getElementById('baseline-note').textContent = 'Baseline captured from current distance.';
  } catch(e) {
    pushLog('⚠️', 'Baseline reset failed', 'warning');
  }
}

function headingLabel(h) {
  return ['N','NE','E','SE','S','SW','W','NW'][Math.round(h/45)%8];
}

async function update() {
  document.getElementById('spin-icon').classList.remove('hid');
  try {
    const r = await fetch('/data');
    const text = await r.text();

    const d = {};
    text.trim().split('\n').forEach(line => {
      const eq = line.indexOf('=');
      if(eq > 0) d[line.slice(0,eq)] = line.slice(eq+1);
    });

    const ok = k => d[k] === '1';
    const num = k => parseFloat(d[k] || '0');

    lastData = d;

    if(ok('ahtOK')){
      const tC = num('temperature').toFixed(1);
      const tF = (num('temperature')*9/5+32).toFixed(0);
      setText('s-temp', tC+'°C / '+tF+'°F');
      setText('s-hum', num('humidity').toFixed(1)+'%');
    } else {
      setText('s-temp', 'Sensor offline');
      setText('s-hum', 'Sensor offline');
    }

    if(ok('tofOK')){
      const dist = num('distance');
      setText('s-dist', dist > 0 ? dist+' mm' : 'Out of range');
      const baseline = num('tofBaseline');
      document.getElementById('baseline-note').textContent =
        baseline > 0 ? ('Baseline: ' + baseline.toFixed(0) + ' mm') : 'Baseline not set yet.';
    } else {
      setText('s-dist', 'Sensor offline');
    }

    if(ok('qmcOK')){
      const hdg = num('heading');
      setText('s-heading', hdg >= 0 ? hdg.toFixed(1)+'°  '+headingLabel(hdg) : 'No fix');
    } else {
      setText('s-heading', 'Sensor offline');
    }

    if(ok('mpuOK')){
      setText('s-accel', num('ax').toFixed(2)+' / '+num('ay').toFixed(2)+' / '+num('az').toFixed(2)+' g');
      setText('s-gyro', num('gx').toFixed(1)+' / '+num('gy').toFixed(1)+' / '+num('gz').toFixed(1)+' °/s');
    } else {
      setText('s-accel', 'Sensor offline');
      setText('s-gyro', 'Sensor offline');
    }

    const rms = num('rms');
    const peak = num('peak');
    setText('s-rms', rms.toFixed(0));
    setText('s-peak', peak.toFixed(0));

    setText('conn-ip', 'IP: '+window.location.hostname);
    loraCount++;
    document.getElementById('lora-badge').textContent =
      ok('loraOK') ? '📻 LoRa TX #'+loraCount : '📻 LoRa offline';

    if(ok('ahtOK')){
      const t = num('temperature');
      const tC = t.toFixed(1), hPct = num('humidity').toFixed(0);
      setText('ins-env',
        t < 18 ? '❄️ Cold — '+tC+'°C  '+hPct+'% RH' :
        t > 32 ? '🔥 Hot — '+tC+'°C  '+hPct+'% RH' :
                '✅ Optimal — '+tC+'°C  '+hPct+'% RH'
      );
    } else { setText('ins-env','⚪ AHT sensor offline'); }

    const motion = ok('mpuOK')
      ? Math.abs(num('ax')) + Math.abs(num('ay')) + Math.abs(num('az'))
      : 0;
    const highMotion = ok('mpuOK') && motion > 0.15;
    if(ok('mpuOK')){
      setText('ins-vib', highMotion ? '⚠️ Motion detected ('+motion.toFixed(2)+' g)' : '✅ Stable — '+motion.toFixed(3)+' g');
    } else { setText('ins-vib','⚪ MPU sensor offline'); }

    const tofBaseline = num('tofBaseline');
    const dist = ok('tofOK') ? num('distance') : -1;
    const distDelta = ok('tofOK') && tofBaseline > 0 && dist > 0 ? Math.abs(dist - tofBaseline) : 0;
    const distAlert = ok('tofOK') && tofBaseline > 0 && (distDelta > 40);

    if(ok('tofOK')){
      setText('ins-dist',
        tofBaseline <= 0 ? '⚪ Baseline not set' :
        dist <= 0 ? '⚪ Out of range' :
        distDelta > 80 ? '🚨 Far from baseline — ' + distDelta.toFixed(0) + ' mm' :
        distDelta > 40 ? '⚠️ Deviation — ' + distDelta.toFixed(0) + ' mm' :
                         '✅ Near baseline — ' + distDelta.toFixed(0) + ' mm'
      );
    } else { setText('ins-dist','⚪ ToF sensor offline'); }

    const rmsLvl = rms;
    const soundAlert = rmsLvl > 10000;
    setText('ins-sound',
      rmsLvl > 50000 ? '🔴 Loud noise — RMS '+rmsLvl.toFixed(0) :
      rmsLvl > 10000 ? '🟡 Moderate — RMS '+rmsLvl.toFixed(0) :
                       '✅ Quiet — RMS '+rmsLvl.toFixed(0)
    );

    if(ok('qmcOK')){
      const hdg = num('heading');
      setText('ins-mag', hdg >= 0 ? '🧭 '+hdg.toFixed(0)+'° '+headingLabel(hdg) : '⚪ No fix');
    } else { setText('ins-mag','⚪ Magnetometer offline'); }

    if(ok('mpuOK')){
      setZone('motion', !highMotion, highMotion ? '⚠ Motion — '+motion.toFixed(2)+' g' : 'Still — '+motion.toFixed(3)+' g');
    } else { setZone('motion', true, 'Sensor offline'); }

    if(ok('tofOK')){
      setZone('dist', !distAlert, tofBaseline <= 0 ? 'Baseline not set' : (dist <= 0 ? 'Out of range' : distDelta.toFixed(0)+' mm from baseline'));
    } else { setZone('dist', true, 'Sensor offline'); }

    const soundOk = !soundAlert;
    setZone('sound', soundOk,
      rmsLvl > 50000 ? '🔴 Loud — '+rmsLvl.toFixed(0) :
      rmsLvl > 10000 ? '⚠ Moderate — '+rmsLvl.toFixed(0) :
                       'Quiet — '+rmsLvl.toFixed(0)
    );

    if(highMotion !== prevMotion){
      if(highMotion) pushLog('📳','Motion detected by MPU-6050 ('+motion.toFixed(2)+' g)','warning');
      else pushLog('✅','Motion stopped — environment stable','normal');
      prevMotion = highMotion;
    }
    if(distAlert !== prevDistAlert){
      if(distAlert) pushLog('🚪','Object deviated from ToF baseline by '+distDelta.toFixed(0)+' mm','warning');
      else pushLog('✅','ToF distance returned near baseline','normal');
      prevDistAlert = distAlert;
    }
    if(soundAlert !== prevSoundAlert){
      if(soundAlert) pushLog('🔊','Sound spike — RMS '+rmsLvl.toFixed(0),'warning');
      else pushLog('✅','Sound level returned to normal','normal');
      prevSoundAlert = soundAlert;
    }

    const anyAlert = highMotion || distAlert || soundAlert;
    setStatus(anyAlert ? 'danger' : 'ok',
      anyAlert ? '🚨  ALERT — Sensor event active' : '✅  All systems normal — monitoring active');

    document.getElementById('last-update').textContent = 'Updated '+fmtTime(new Date());
    document.getElementById('conn-status').className = 'conn-status conn-online';
    document.getElementById('conn-label').textContent = 'Online';
  } catch(e) {
    setStatus('warn','⚠️  Connection lost — retrying…');
    document.getElementById('conn-status').className = 'conn-status conn-offline';
    document.getElementById('conn-label').textContent = 'Offline';
  }
  document.getElementById('spin-icon').classList.add('hid');
}

setInterval(update, 500);
update();
</script>
</body>
</html>
)HTMLEOF";

// ─────────────────────────────────────────────────────────────
//  I2S INIT
// ─────────────────────────────────────────────────────────────
void initI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD_PIN
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// ─────────────────────────────────────────────────────────────
//  QMC5883P INIT
// ─────────────────────────────────────────────────────────────
bool initQMC() {
  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x0B); Wire.write(0x01);
  if (Wire.endTransmission() != 0) return false;
  delay(20);

  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x09); Wire.write(0x1D);
  if (Wire.endTransmission() != 0) return false;

  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x0A); Wire.write(0x00);
  Wire.endTransmission();
  return true;
}

// ─────────────────────────────────────────────────────────────
//  MPU-6050 CALIBRATION
// ─────────────────────────────────────────────────────────────
void calibrateMPU() {
  Serial.println("[MPU] Calibrating (200 samples)...");
  long axS=0, ayS=0, azS=0, gxS=0, gyS=0, gzS=0;
  uint8_t d[14];

  for (int i = 0; i < 200; i++) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 14);
    for (int j = 0; j < 14; j++) d[j] = Wire.read();

    axS += (int16_t)(d[0]<<8|d[1]);
    ayS += (int16_t)(d[2]<<8|d[3]);
    azS += (int16_t)(d[4]<<8|d[5]);
    gxS += (int16_t)(d[8]<<8|d[9]);
    gyS += (int16_t)(d[10]<<8|d[11]);
    gzS += (int16_t)(d[12]<<8|d[13]);
    delay(5);
  }

  ax0 = axS / 200.0 / 16384.0;
  ay0 = ayS / 200.0 / 16384.0;
  az0 = azS / 200.0 / 16384.0;
  gx0 = gxS / 200.0 / 131.0;
  gy0 = gyS / 200.0 / 131.0;
  gz0 = gzS / 200.0 / 131.0;
  Serial.println("[MPU] Calibration done");
}

// ─────────────────────────────────────────────────────────────
//  SENSOR READS
// ─────────────────────────────────────────────────────────────
void readMPU() {
  if (!mpuOK) return;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14);

  uint8_t d[14];
  for (int i = 0; i < 14; i++) d[i] = Wire.read();

  ax = (int16_t)(d[0]<<8|d[1]) / 16384.0 - ax0;
  ay = (int16_t)(d[2]<<8|d[3]) / 16384.0 - ay0;
  az = (int16_t)(d[4]<<8|d[5]) / 16384.0 - az0;
  gx = (int16_t)(d[8]<<8|d[9])  / 131.0   - gx0;
  gy = (int16_t)(d[10]<<8|d[11])/ 131.0   - gy0;
  gz = (int16_t)(d[12]<<8|d[13])/ 131.0   - gz0;
}

void readMAG() {
  if (!qmcOK) return;

  Wire.beginTransmission(QMC_ADDR);
  Wire.write(0x00);
  Wire.endTransmission(false);

  if (Wire.requestFrom(QMC_ADDR, 6) != 6) { heading = -1; return; }

  int16_t x = Wire.read() | (Wire.read() << 8);
  int16_t y = Wire.read() | (Wire.read() << 8);
  Wire.read(); Wire.read();

  if (x == 0 && y == 0) { heading = -1; return; }
  float h = atan2((float)y, (float)x) * 180.0 / PI;
  if (h < 0) h += 360;
  heading = h;
}

void readAHT() {
  if (!ahtOK) return;
  sensors_event_t h, t;
  aht.getEvent(&h, &t);
  temp = t.temperature;
  hum  = h.relative_humidity;
}

void readTOF() {
  if (!tofOK) return;
  VL53L0X_RangingMeasurementData_t m;
  lox.rangingTest(&m, false);

  if (m.RangeStatus != 4) {
    distance = m.RangeMilliMeter;

    if (prevDistance > 0) {
      tofRate = fabs(distance - prevDistance);
    } else {
      tofRate = 0;
    }

    prevDistance = distance;

    if (tofBaseline > 0) {
      tofDelta = fabs(distance - tofBaseline);
    } else {
      tofDelta = 0;
    }
  }
}

void readMIC() {
  const int N = 128;
  int32_t b[N];
  size_t r;

  i2s_read(I2S_NUM_0, b, sizeof(b), &r, portMAX_DELAY);
  if (r == 0) return;

  int c = r / 4;
  long long sum = 0;
  int32_t peak = 0;

  for (int i = 0; i < c; i++) {
    int32_t s = b[i] >> 8;
    if (s < 0) s = -s;
    if (s > peak) peak = s;
    sum += (long long)s * s;
  }

  float rms = sqrt((float)sum / c);
  rmsValue  = String(rms, 1);
  peakValue = String(peak);
}

// ─────────────────────────────────────────────────────────────
//  SD LOGGING
// ─────────────────────────────────────────────────────────────
void logSD() {
  if (!sdOK) return;

  File f = SD.open("/senticube_log.txt", FILE_APPEND);
  if (!f) return;

  f.print("ms=");    f.print(millis());
  f.print(" t=");    f.print(temp, 2);
  f.print(" h=");    f.print(hum, 2);
  f.print(" d=");    f.print(distance, 0);
  f.print(" base="); f.print(tofBaseline, 0);
  f.print(" delta=");f.print(tofDelta, 0);
  f.print(" ax=");   f.print(ax, 3);
  f.print(" ay=");   f.print(ay, 3);
  f.print(" az=");   f.print(az, 3);
  f.print(" gx=");   f.print(gx, 3);
  f.print(" gy=");   f.print(gy, 3);
  f.print(" gz=");   f.print(gz, 3);
  f.print(" hdg=");  f.print(heading, 1);
  f.print(" rms=");  f.print(rmsValue);
  f.print(" peak="); f.println(peakValue);
  f.close();
}

// ─────────────────────────────────────────────────────────────
//  LORA TX — PACKET TYPE 1: Raw sensor data (every 2 s)
// ─────────────────────────────────────────────────────────────
void sendLoRaData() {
  if (!loraOK) return;

  String p = "{\"type\":\"data\"";
  p += ",\"ms\":"; p += millis();
  if (ahtOK) { p += ",\"t\":"; p += String(temp, 2); p += ",\"h\":"; p += String(hum, 2); }
  if (tofOK) { p += ",\"d\":"; p += String(distance, 0); }
  if (mpuOK) {
    p += ",\"ax\":"; p += String(ax, 3);
    p += ",\"ay\":"; p += String(ay, 3);
    p += ",\"az\":"; p += String(az, 3);
    p += ",\"gx\":"; p += String(gx, 3);
    p += ",\"gy\":"; p += String(gy, 3);
    p += ",\"gz\":"; p += String(gz, 3);
  }
  if (qmcOK) { p += ",\"hdg\":"; p += String(heading, 1); }
  p += ",\"rms\":"; p += rmsValue;
  p += ",\"pk\":"; p += peakValue;
  p += ",\"base\":"; p += String(tofBaseline, 0);
  p += ",\"delta\":"; p += String(tofDelta, 0);
  p += "}";

  LoRa.beginPacket();
  LoRa.print(p);
  LoRa.endPacket();

  Serial.print("[LoRa DATA] ");
  Serial.println(p);
}

// ─────────────────────────────────────────────────────────────
//  LORA TX — PACKET TYPE 2: Event
// ─────────────────────────────────────────────────────────────
void sendLoRaEvent(const char* code, const char* desc) {
  if (!loraOK) return;

  String p = "{\"type\":\"event\"";
  p += ",\"ms\":"; p += millis();
  p += ",\"code\":\""; p += code; p += "\"";
  p += ",\"desc\":\""; p += desc; p += "\"";
  if (mpuOK) {
    p += ",\"ax\":"; p += String(ax, 3);
    p += ",\"ay\":"; p += String(ay, 3);
    p += ",\"az\":"; p += String(az, 3);
  }
  if (tofOK) {
    p += ",\"d\":"; p += String(distance, 0);
    p += ",\"base\":"; p += String(tofBaseline, 0);
    p += ",\"delta\":"; p += String(tofDelta, 0);
  }
  p += ",\"rms\":"; p += rmsValue;
  p += "}";

  LoRa.beginPacket();
  LoRa.print(p);
  LoRa.endPacket();

  Serial.print("[LoRa EVENT] ");
  Serial.println(p);
}

// ─────────────────────────────────────────────────────────────
//  EVENT DETECTOR
// ─────────────────────────────────────────────────────────────
void detectAndSendEvents() {
  float motionMag = abs(ax) + abs(ay) + abs(az);
  bool motionAlert = mpuOK && (motionMag > 0.15);

  bool distAlert = false;
  if (tofOK && tofBaseline > 0 && distance > 0) {
    distAlert = (tofDelta > 40.0) || (tofRate > 25.0);
  }

  float rmsF = rmsValue.toFloat();
  bool soundAlert = (rmsF > 10000);

  if (motionAlert != prevMotionAlert) {
    if (motionAlert) sendLoRaEvent("MOTION_ON", "Motion detected");
    else sendLoRaEvent("MOTION_OFF", "Motion stopped");
    prevMotionAlert = motionAlert;
  }

  if (distAlert != prevDistAlert) {
    if (distAlert) sendLoRaEvent("PROX_ON", "Distance deviated from baseline");
    else sendLoRaEvent("PROX_OFF", "Distance returned near baseline");
    prevDistAlert = distAlert;
  }

  if (soundAlert != prevSoundAlert) {
    if (soundAlert) sendLoRaEvent("SOUND_ON", "Sound spike detected");
    else sendLoRaEvent("SOUND_OFF", "Sound level normal");
    prevSoundAlert = soundAlert;
  }
}

// ─────────────────────────────────────────────────────────────
//  WEB HANDLERS
// ─────────────────────────────────────────────────────────────
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleData() {
  String t = "";
  t += "temperature=" + String(temp, 2) + "\n";
  t += "humidity=" + String(hum, 2) + "\n";
  t += "distance=" + String(distance, 0) + "\n";
  t += "tofBaseline=" + String(tofBaseline, 0) + "\n";
  t += "tofDelta=" + String(tofDelta, 0) + "\n";
  t += "tofRate=" + String(tofRate, 0) + "\n";
  t += "ax=" + String(ax, 4) + "\n";
  t += "ay=" + String(ay, 4) + "\n";
  t += "az=" + String(az, 4) + "\n";
  t += "gx=" + String(gx, 4) + "\n";
  t += "gy=" + String(gy, 4) + "\n";
  t += "gz=" + String(gz, 4) + "\n";
  t += "heading=" + String(heading, 2) + "\n";
  t += "rms=" + rmsValue + "\n";
  t += "peak=" + peakValue + "\n";
  t += "mpuOK=";  t += (mpuOK ? "1" : "0"); t += "\n";
  t += "ahtOK=";  t += (ahtOK ? "1" : "0"); t += "\n";
  t += "tofOK=";  t += (tofOK ? "1" : "0"); t += "\n";
  t += "qmcOK=";  t += (qmcOK ? "1" : "0"); t += "\n";
  t += "loraOK="; t += (loraOK ? "1" : "0"); t += "\n";
  t += "sdOK=";   t += (sdOK ? "1" : "0"); t += "\n";
  server.send(200, "text/plain", t);
}

void handleResetBaseline() {
  if (!tofOK || distance <= 0) {
    server.send(400, "text/plain", "no valid ToF reading");
    return;
  }

  tofBaseline = distance;
  tofDelta = 0;
  tofRate = 0;
  prevDistAlert = false;

  String msg = "baseline set to " + String(tofBaseline, 0) + " mm";
  Serial.print("[ToF] ");
  Serial.println(msg);

  if (loraOK) {
    sendLoRaEvent("BASESET", msg.c_str());
  }

  server.send(200, "text/plain", msg);
}

// ─────────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[SentiCube] Booting...");

  Wire.begin(SDA_PIN, SCL_PIN);

  WiFiManager wm;
  wm.autoConnect("SentiCube");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdOK = SD.begin(SD_CS, spiSD);
  Serial.println(sdOK ? "[SD] OK" : "[SD] FAILED");

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  loraOK = LoRa.begin(LORA_FREQ);
  if (loraOK) {
    LoRa.setSyncWord(0xF3);
    Serial.println("[LoRa] OK — 433 MHz, sync 0xF3");
  } else {
    Serial.println("[LoRa] FAILED — continuing without LoRa");
  }

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  mpuOK = (Wire.endTransmission() == 0);
  Serial.println(mpuOK ? "[MPU] OK" : "[MPU] FAILED");

  ahtOK = aht.begin(&Wire);
  Serial.println(ahtOK ? "[AHT] OK" : "[AHT] FAILED");

  tofOK = lox.begin();
  Serial.println(tofOK ? "[ToF] OK" : "[ToF] FAILED");

  qmcOK = initQMC();
  Serial.println(qmcOK ? "[QMC] OK" : "[QMC] FAILED — try 0x0D");

  initI2S();
  Serial.println("[I2S] INMP441 ready");

  if (mpuOK) calibrateMPU();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/reset-baseline", HTTP_POST, handleResetBaseline);
  server.begin();

  Serial.println("[HTTP] Server started");
  Serial.println("[SentiCube] Ready!");
}

// ─────────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  if (millis() - lastSensorRead > 200) {
    lastSensorRead = millis();
    readMPU();
    readAHT();
    readTOF();
    readMAG();
    readMIC();
    if (loraOK) detectAndSendEvents();
  }

  if (loraOK && (millis() - lastLoRaSend > 2000)) {
    lastLoRaSend = millis();
    sendLoRaData();
  }

  if (sdOK && (millis() - lastSDLog > 5000)) {
    lastSDLog = millis();
    logSD();
  }
}