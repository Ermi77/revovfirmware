#pragma once

// The sim control panel, served from flash. The raw
// literal keeps the UTF-8 glyph bytes (arrows,
// em dashes, box drawing) intact in PROGMEM; the
// page declares its charset so the browser renders
// them. Every log line is formatted on the server
// and appended verbatim here; the client only adds
// the timestamp.
//
// Brand system (SATEV Group / RevoV):
//   Deep Navy Blue  #0E1A40  strength, stability, trust
//   Neon Orange     #FF7101  energy, innovation, visibility
//   Metallic Silver #D9D9D9  precision, modern engineering
// The RevoV wordmark's final V extends upward like a
// cursor arrow — revolution, direction, progress.
//
// Brand imagery is served by the UI itself (/brand/*),
// so the design renders fully on devices joined to
// the sim AP, which has no internet access. The
// Google Fonts links from the design reference are
// deliberately omitted for the same reason: an
// external stylesheet is render-blocking and would
// delay first paint on the offline AP. The font
// stacks keep Space Grotesk / JetBrains Mono first
// so devices that carry them render with them.
//
// PagesGz.h is generated from this file by tools/gzip_page.py;
// regenerate it after every edit or the served page goes stale.
const char INDEX_PAGE[] PROGMEM = R"PAGE(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<title>SATEV Group — RevoV Control Panel</title>
<style>
/* ============================================================
   SATEV GROUP — DESIGN TOKENS
   ============================================================ */
:root{
  --navy:        #0E1A40;
  --navy-deep:   #070E22;
  --navy-panel:  #12204C;
  --orange:      #FF7101;
  --orange-soft: #FF8A2B;
  --silver:      #D9D9D9;
  --muted:       #8A95B8;
  --green:       #39D98A;
  --blue:        #7FD4FF;
  --red:         #FF5C5C;
  --amber:       #FFC53D;

  --radius:      14px;
  --radius-sm:   9px;

  color-scheme: dark;
}

*{ box-sizing: border-box; margin: 0; padding: 0; }

html, body{ height: 100%; }

body{
  font-family: 'Space Grotesk', 'Inter', 'Roboto', 'Segoe UI', system-ui, sans-serif;
  background:
    radial-gradient(1200px 700px at 12% -10%, rgba(255,113,1,.10), transparent 60%),
    radial-gradient(900px 600px at 95% 0%, rgba(127,212,255,.07), transparent 55%),
    linear-gradient(160deg, var(--navy) 0%, var(--navy-deep) 100%);
  background-attachment: fixed;
  color: var(--silver);
  min-height: 100vh;
  padding: 26px 22px 46px;
  -webkit-font-smoothing: antialiased;
}

/* Faint engineering grid overlay */
body::before{
  content:"";
  position: fixed; inset: 0;
  background-image:
    linear-gradient(rgba(217,217,217,.030) 1px, transparent 1px),
    linear-gradient(90deg, rgba(217,217,217,.030) 1px, transparent 1px);
  background-size: 46px 46px;
  pointer-events: none;
  z-index: 0;
}

.shell{ position: relative; z-index: 1; max-width: 1440px; margin: 0 auto; }

/* ============================================================
   TOP BAR
   ============================================================ */
.topbar{
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 18px;
  padding: 16px 20px;
  margin-bottom: 18px;
  background: linear-gradient(120deg, rgba(18,32,76,.85), rgba(7,14,34,.75));
  border: 1px solid rgba(217,217,217,.13);
  border-radius: var(--radius);
  backdrop-filter: blur(6px);
}

.brand{ display: flex; align-items: center; gap: 15px; }

.mark{ height: 46px; width: auto; flex-shrink: 0; border-radius: 8px; filter: drop-shadow(0 0 10px rgba(255,113,1,.35)); }

.brand h1{
  font-size: 21px;
  font-weight: 700;
  letter-spacing: .16em;
  color: #fff;
  line-height: 1.1;
}
.brand h1 .thin{ color: var(--orange); font-weight: 500; }

.brand p{
  font-size: 10.5px;
  letter-spacing: .22em;
  text-transform: uppercase;
  color: var(--muted);
  margin-top: 4px;
}

.topbar-right{ display: flex; align-items: center; gap: 14px; flex-wrap: wrap; }

.product-mark{ height: 30px; width: auto; border-radius: 6px; }

.revov-badge{
  font-size: 15px;
  font-weight: 700;
  letter-spacing: .12em;
  padding: 8px 16px;
  border-radius: 999px;
  border: 1px solid rgba(255,113,1,.45);
  background: rgba(255,113,1,.08);
  color: var(--silver);
}
.revov-badge span{ color: var(--orange); }

.status{
  display: flex; align-items: center; gap: 8px;
  font-size: 10.5px; letter-spacing: .2em; text-transform: uppercase;
  color: var(--muted);
  padding: 8px 14px;
  border: 1px solid rgba(217,217,217,.13);
  border-radius: 999px;
}
.status .dot{
  width: 8px; height: 8px; border-radius: 50%;
  background: var(--green);
  box-shadow: 0 0 10px var(--green);
  animation: blink 2.2s ease-in-out infinite;
}
@keyframes blink{ 0%,100%{opacity:1} 50%{opacity:.35} }

.btn-reset{ padding: 8px 14px; font-size: 10px; }

/* ============================================================
   HERO
   ============================================================ */
.hero{
  display: flex;
  align-items: center;
  gap: 18px;
  padding: 16px 20px;
  margin-bottom: 18px;
  background: linear-gradient(155deg, rgba(18,32,76,.92), rgba(7,14,34,.92));
  border: 1px solid rgba(217,217,217,.12);
  border-radius: var(--radius);
  position: relative;
  overflow: hidden;
}
.hero::after{
  content:"";
  position: absolute; top: 0; left: 18px; right: 18px; height: 2px;
  background: linear-gradient(90deg, transparent, rgba(255,113,1,.55), transparent);
  opacity: .55;
}
.wordmark{
  margin: 0;
  display: flex;
  align-items: center;
  font-size: 34px;
  font-weight: 700;
  letter-spacing: .02em;
  color: var(--orange);
}
.v-arrow{ height: .92em; width: auto; }
.hero-sub{
  margin: 2px 0 0;
  font-size: 13px;
  font-weight: 600;
  letter-spacing: .2em;
  text-transform: uppercase;
  color: #fff;
}
.hero-tag{
  margin: 4px 0 0;
  font-size: 11px;
  letter-spacing: .14em;
  text-transform: uppercase;
  color: var(--muted);
}
.hero-img{
  height: 110px;
  width: auto;
  object-fit: contain;
  margin-left: auto;
  border-radius: 8px;
  background: rgba(5,10,26,.85);
}

/* ============================================================
   TELEMETRY STRIP
   ============================================================ */
.strip{
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  margin-bottom: 18px;
}
.chip{
  flex: 1 1 100px;
  display: flex;
  align-items: baseline;
  gap: 8px;
  background: linear-gradient(155deg, rgba(18,32,76,.92), rgba(7,14,34,.92));
  border: 1px solid rgba(217,217,217,.12);
  border-radius: var(--radius-sm);
  padding: 8px 12px;
}
.chip-name{ font-size: 9.5px; letter-spacing: .16em; text-transform: uppercase; color: var(--muted); }
.chip-value{ font-family: 'JetBrains Mono', ui-monospace, 'SF Mono', Menlo, Consolas, monospace; font-size: 15px; font-weight: 600; color: var(--orange); }
.chip-unit{ font-size: 9.5px; color: var(--muted); }
.chip-value.ok{ color: var(--green); }
.chip-value.warn{ color: var(--amber); }
.chip-value.error{ color: var(--red); }
.chip-value.pick{ color: var(--amber); }
.chip-value.place{ color: var(--blue); }
.chip-value.open{ color: var(--amber); }

/* ============================================================
   GRID
   ============================================================ */
.grid{
  display: grid;
  grid-template-columns: 1fr;
  gap: 18px;
}

.panel{
  background: linear-gradient(155deg, rgba(18,32,76,.92), rgba(7,14,34,.92));
  border: 1px solid rgba(217,217,217,.12);
  border-radius: var(--radius);
  padding: 18px 18px 20px;
  position: relative;
  overflow: hidden;
  transition: border-color .25s, box-shadow .25s, transform .25s;
}
.panel::after{
  content:"";
  position: absolute; top: 0; left: 18px; right: 18px; height: 2px;
  background: linear-gradient(90deg, transparent, rgba(255,113,1,.55), transparent);
  opacity: .55;
}
.panel:hover{
  border-color: rgba(255,113,1,.35);
  box-shadow: 0 12px 34px rgba(0,0,0,.42), 0 0 0 1px rgba(255,113,1,.08) inset;
}

.panel-head{
  display: flex;
  align-items: baseline;
  gap: 10px;
  margin-bottom: 16px;
  padding-bottom: 12px;
  border-bottom: 1px solid rgba(217,217,217,.09);
}
.panel-idx{
  font-family: 'JetBrains Mono', ui-monospace, 'SF Mono', Menlo, Consolas, monospace;
  font-size: 11px;
  color: var(--orange);
  letter-spacing: .1em;
}
.panel-head h2{
  font-size: 13px;
  font-weight: 600;
  letter-spacing: .17em;
  text-transform: uppercase;
  color: #fff;
}
.panel-tag{
  margin-left: auto;
  font-size: 9.5px;
  letter-spacing: .18em;
  text-transform: uppercase;
  color: var(--muted);
  border: 1px solid rgba(217,217,217,.15);
  border-radius: 999px;
  padding: 3px 10px;
  white-space: nowrap;
}

.panel-body{ display: flex; flex-direction: column; gap: 14px; }

/* Half-pair grid: panels 05 (LED Control) and 06
   (Temperature & Humidity) share the first half of
   the row — 05 first on the left, 06 next on the
   right, each container half the duo's track — and
   stack along the vertical Y-axis below 700px. The
   other half of the row belongs to panel 07 (Order
   & Instruction Handler, .panel-half). Grid stretch
   plus the min-height floor keep the containers
   uniform and consistent in dimension. */
.panel-duo{
  display: grid;
  grid-template-columns: 1fr;
  gap: 18px;
  grid-column: 1 / -1;
  align-items: stretch;
}
.panel-duo .panel{ min-height: 230px; }

/* ============================================================
   FIELDS
   ============================================================ */
.field-row{
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: 12px;
}

.field{ display: flex; flex-direction: column; gap: 6px; }

.field label{
  font-size: 10px;
  letter-spacing: .18em;
  text-transform: uppercase;
  color: var(--muted);
}

input[type="number"],
input[type="text"]{
  width: 100%;
  background: rgba(5,10,26,.85);
  border: 1px solid rgba(217,217,217,.22);
  border-radius: var(--radius-sm);
  color: var(--silver);
  font-family: 'JetBrains Mono', ui-monospace, 'SF Mono', Menlo, Consolas, monospace;
  font-size: 14px;
  padding: 11px 12px;
  transition: border-color .2s, box-shadow .2s, background .2s;
}
input::placeholder{ color: rgba(138,149,184,.55); }
input:hover{ border-color: rgba(217,217,217,.38); }
input:focus{
  outline: none;
  border-color: var(--orange);
  background: rgba(255,113,1,.06);
  box-shadow: 0 0 0 3px rgba(255,113,1,.16);
}

/* ============================================================
   BUTTONS
   ============================================================ */
.btn{
  font-family: inherit;
  font-size: 11.5px;
  font-weight: 600;
  letter-spacing: .14em;
  text-transform: uppercase;
  border-radius: var(--radius-sm);
  padding: 12px 14px;
  border: 1px solid transparent;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  transition: transform .15s, box-shadow .22s, background .22s, border-color .22s, color .22s;
  user-select: none;
}
.btn:active{ transform: translateY(1px) scale(.985); }

/* Primary — Neon Orange */
.btn-run{
  width: 100%;
  background: linear-gradient(180deg, var(--orange-soft), var(--orange));
  color: #0B1330;
  font-weight: 700;
  box-shadow: 0 4px 14px rgba(255,113,1,.22);
}
.btn-run:hover{
  box-shadow: 0 0 22px rgba(255,113,1,.62), 0 4px 16px rgba(255,113,1,.3);
  transform: translateY(-1px);
}

/* Ghost / secondary */
.btn-ghost{
  background: rgba(217,217,217,.05);
  border-color: rgba(217,217,217,.22);
  color: var(--silver);
}
.btn-ghost:hover{
  border-color: var(--orange);
  color: var(--orange);
  background: rgba(255,113,1,.08);
  box-shadow: 0 0 14px rgba(255,113,1,.22);
}

/* Segmented control */
.seg{
  display: flex;
  gap: 5px;
  padding: 5px;
  background: rgba(5,10,26,.85);
  border: 1px solid rgba(217,217,217,.16);
  border-radius: var(--radius-sm);
}
.seg-btn{
  flex: 1;
  font-family: inherit;
  font-size: 10.5px;
  font-weight: 600;
  letter-spacing: .12em;
  text-transform: uppercase;
  padding: 9px 6px;
  border-radius: 6px;
  border: 1px solid transparent;
  background: transparent;
  color: var(--muted);
  cursor: pointer;
  transition: all .2s;
}
.seg-btn:hover{ color: var(--silver); background: rgba(217,217,217,.06); }
.seg-btn.active{
  background: linear-gradient(180deg, rgba(255,113,1,.95), rgba(255,113,1,.78));
  color: #0B1330;
  font-weight: 700;
  box-shadow: 0 0 16px rgba(255,113,1,.42);
}

/* Z-Axis action buttons */
.axis-actions{ display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
.btn-action{
  padding: 20px 10px;
  flex-direction: column;
  gap: 8px;
  font-size: 11px;
  border-radius: var(--radius-sm);
  background: rgba(217,217,217,.04);
  border: 1px solid rgba(217,217,217,.2);
  color: var(--silver);
}
.btn-action .ico{ font-size: 19px; line-height: 1; color: var(--orange); }
.btn-action:hover{
  border-color: var(--orange);
  background: rgba(255,113,1,.1);
  box-shadow: 0 0 20px rgba(255,113,1,.3);
  transform: translateY(-2px);
}
.btn-action.place .ico{ color: var(--blue); }
.btn-action.place:hover{
  border-color: var(--blue);
  background: rgba(127,212,255,.1);
  box-shadow: 0 0 20px rgba(127,212,255,.28);
}

/* Door buttons */
.door-actions{ display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
.door-btn{ padding: 15px 10px; }
.door-btn.active{
  border-color: var(--orange);
  background: rgba(255,113,1,.14);
  color: var(--orange);
  box-shadow: 0 0 18px rgba(255,113,1,.32);
}

/* ============================================================
   LED PANEL
   ============================================================ */
.led-row{
  display: flex;
  align-items: center;
  gap: 18px;
  flex-wrap: wrap;
}
.led-indicator{
  --led-color: transparent;
  width: 56px; height: 56px;
  border-radius: 50%;
  flex-shrink: 0;
  background: rgba(217,217,217,.05);
  border: 2px solid rgba(217,217,217,.2);
  transition: all .35s ease;
}
.led-indicator.on{
  background: radial-gradient(circle at 35% 30%, rgba(255,255,255,.85), var(--led-color) 60%);
  border-color: var(--led-color);
  box-shadow: 0 0 26px var(--led-color), 0 0 60px rgba(255,113,1,.2);
}
.led-info{ flex: 1; min-width: 130px; }
.led-state{
  font-size: 20px;
  font-weight: 700;
  letter-spacing: .16em;
  color: var(--muted);
  transition: color .3s;
}
.led-state.on{ color: var(--orange); text-shadow: 0 0 16px rgba(255,113,1,.65); }
.led-info .hint{ font-size: 10px; letter-spacing: .14em; text-transform: uppercase; color: var(--muted); margin-top: 3px; }

.switch{
  width: 72px; height: 38px;
  border-radius: 999px;
  background: rgba(5,10,26,.9);
  border: 1px solid rgba(217,217,217,.25);
  position: relative;
  cursor: pointer;
  padding: 0;
  flex-shrink: 0;
  transition: all .3s;
}
.switch .knob{
  position: absolute;
  top: 4px; left: 4px;
  width: 28px; height: 28px;
  border-radius: 50%;
  background: var(--muted);
  transition: all .3s cubic-bezier(.4,1.4,.5,1);
}
.switch.on{
  background: rgba(255,113,1,.16);
  border-color: var(--orange);
  box-shadow: 0 0 22px rgba(255,113,1,.45);
}
.switch.on .knob{
  left: 39px;
  background: var(--orange);
  box-shadow: 0 0 16px rgba(255,113,1,.95);
}

/* ============================================================
   SENSOR PANEL
   ============================================================ */
.sensor-row{
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
}
.sensor-readout{
  flex: 1 1 150px;
  display: flex;
  flex-direction: column;
  gap: 5px;
  background: rgba(5,10,26,.85);
  border: 1px solid rgba(217,217,217,.14);
  border-radius: var(--radius-sm);
  padding: 14px 16px;
  transition: border-color .25s, box-shadow .25s;
}
.sensor-readout:hover{
  border-color: rgba(255,113,1,.35);
  box-shadow: 0 0 16px rgba(255,113,1,.14);
}
.sensor-label{
  display: flex;
  align-items: center;
  gap: 7px;
  font-size: 10px;
  letter-spacing: .18em;
  text-transform: uppercase;
  color: var(--muted);
}
.sensor-ico{ font-size: 13px; line-height: 1; }
.sensor-value{
  display: flex;
  align-items: baseline;
  gap: 6px;
  font-family: 'JetBrains Mono', ui-monospace, 'SF Mono', Menlo, Consolas, monospace;
  font-size: 34px;
  font-weight: 700;
  line-height: 1.15;
  color: var(--muted);
  transition: color .3s;
}
.sensor-value .sensor-unit{
  font-size: 14px;
  font-weight: 600;
  letter-spacing: .06em;
}
.sensor-value.live{
  color: var(--orange);
  animation: sensorPulse 2s ease-in-out infinite;
}
.sensor-value.live .sensor-unit{ color: var(--orange-soft); }
@keyframes sensorPulse{
  0%,100%{ text-shadow: 0 0 12px rgba(255,113,1,.40); }
  50%{ text-shadow: 0 0 24px rgba(255,113,1,.80); }
}
.sensor-status{
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 10px;
  letter-spacing: .18em;
  text-transform: uppercase;
  color: var(--muted);
}
.sensor-dot{
  width: 8px; height: 8px;
  border-radius: 50%;
  flex-shrink: 0;
  background: var(--red);
  box-shadow: 0 0 10px var(--red);
}
.sensor-dot.live{
  background: var(--green);
  box-shadow: 0 0 10px var(--green);
  animation: blink 2.2s ease-in-out infinite;
}

/* ============================================================
   ORDER PANEL
   ============================================================ */
.order-actions{ display: grid; grid-template-columns: 1.25fr 1fr; gap: 12px; }

.btn-ready{
  background: transparent;
  border: 1.5px solid var(--green);
  color: var(--green);
  font-weight: 700;
  animation: readyPulse 2.6s ease-out infinite;
}
.btn-ready:hover{
  background: rgba(57,217,138,.12);
  box-shadow: 0 0 22px rgba(57,217,138,.5);
}
@keyframes readyPulse{
  0%   { box-shadow: 0 0 0 0 rgba(57,217,138,.45); }
  70%  { box-shadow: 0 0 0 11px rgba(57,217,138,0); }
  100% { box-shadow: 0 0 0 0 rgba(57,217,138,0); }
}

/* ============================================================
   LOG PANEL
   ============================================================ */
.log-tools{ margin-left: auto; display: flex; gap: 8px; }
.btn-clear{
  font-size: 9.5px;
  padding: 5px 11px;
  letter-spacing: .14em;
  border-radius: 999px;
  background: transparent;
  border: 1px solid rgba(217,217,217,.2);
  color: var(--muted);
  font-family: inherit;
  cursor: pointer;
  text-transform: uppercase;
  font-weight: 600;
  transition: all .2s;
}
.btn-clear:hover{ border-color: var(--orange); color: var(--orange); }

.log-body{
  height: 262px;
  overflow-y: auto;
  background: rgba(4,8,20,.92);
  border: 1px solid rgba(217,217,217,.12);
  border-radius: 10px;
  padding: 13px 14px;
  font-family: 'JetBrains Mono', ui-monospace, 'SF Mono', Menlo, Consolas, monospace;
  font-size: 12.3px;
  line-height: 1.75;
  scroll-behavior: smooth;
}
.log-body::-webkit-scrollbar{ width: 9px; }
.log-body::-webkit-scrollbar-track{ background: rgba(217,217,217,.04); border-radius: 6px; }
.log-body::-webkit-scrollbar-thumb{ background: rgba(255,113,1,.38); border-radius: 6px; }
.log-body::-webkit-scrollbar-thumb:hover{ background: rgba(255,113,1,.65); }

.log-row{ display: flex; gap: 11px; align-items: flex-start; }
.log-ts{ color: var(--orange); opacity: .8; flex-shrink: 0; }
.log-msg{ color: var(--silver); word-break: break-word; }

.log-row.esp   .log-msg{ color: var(--blue); }
.log-row.ready .log-msg{ color: var(--green); }
.log-row.cmd   .log-msg{ color: var(--orange); }
.log-row.error .log-msg{ color: var(--red); }
.log-row.sys   .log-msg{ color: var(--muted); }

/* ============================================================
   RESPONSIVE
   ============================================================ */
@media (min-width: 700px){
  .grid{ grid-template-columns: repeat(2, 1fr); }
  .panel-duo{ grid-template-columns: repeat(2, 1fr); grid-column: span 1; }
  .panel-half{ grid-column: span 1; }
  .panel-wide{ grid-column: span 2; }
  .panel-log{ grid-column: 1 / -1; }
}

@media (min-width: 1180px){
  .grid{ grid-template-columns: repeat(4, 1fr); }
  .panel-duo{ grid-column: span 2; }
  .panel-half{ grid-column: span 2; }
  .panel-wide{ grid-column: span 2; }
  .panel-log{ grid-column: 1 / -1; }
  .log-body{ height: 300px; }
}

@media (max-width: 480px){
  body{ padding: 16px 13px 34px; }
  .topbar{ padding: 14px; }
  .brand h1{ font-size: 17px; letter-spacing: .12em; }
  .order-actions{ grid-template-columns: 1fr; }
  .sensor-row{ flex-direction: column; }
  .hero{ flex-direction: column; align-items: flex-start; }
  .hero-img{ margin-left: 0; height: 90px; }
}
</style>
</head>
<body>
<div class="shell">

  <!-- ══════════════ TOP BAR ══════════════ -->
  <header class="topbar">
    <div class="brand">
      <img class="mark" src="/brand/company.jpg" alt="SATEV Group">
      <div>
        <h1>SATEV <span class="thin">GROUP</span></h1>
        <p>The Ultimate Engineering · Valley → Air → Space</p>
      </div>
    </div>

    <div class="topbar-right">
      <img class="product-mark" src="/brand/product.jpg" alt="RevoV">
      <button class="btn btn-ghost btn-reset" id="resetBtn" type="button">Reset</button>
      <div class="status"><span class="dot" id="statusDot"></span><span id="statusText">Connecting</span></div>
    </div>
  </header>

  <!-- ══════════════ HERO ══════════════ -->
  <section class="hero">
    <div class="hero-text">
      <h1 class="wordmark">Revo<svg class="v-arrow" viewBox="0 0 40 32" role="img" aria-label="V"><path d="M3 3 L16 29 L23 13 L37 2" fill="none" stroke="currentColor" stroke-width="4.5" stroke-linejoin="miter"/><path d="M37 2 L27 3 L34 11" fill="none" stroke="currentColor" stroke-width="4.5" stroke-linejoin="miter"/></svg></h1>
      <p class="hero-sub">Vending Machine</p>
      <p class="hero-tag">From the East African Valley to Space</p>
    </div>
    <img class="hero-img" src="/brand/machine.jpg" alt="RevoV vending machine">
  </section>

  <!-- ══════════════ TELEMETRY ══════════════ -->
  <section class="strip" id="strip">
    <div class="chip"><span class="chip-name">X Axis</span><span class="chip-value" id="valX">0</span><span class="chip-unit">slot</span></div>
    <div class="chip"><span class="chip-name">Y Axis</span><span class="chip-value" id="valY">0</span><span class="chip-unit">shelf</span></div>
    <div class="chip"><span class="chip-name">Z Axis</span><span class="chip-value" id="valZ">up</span></div>
    <div class="chip"><span class="chip-name">Door</span><span class="chip-value" id="valDoor">CLOSED</span></div>
    <div class="chip"><span class="chip-name">Machine</span><span class="chip-value" id="valMachine">IDLE</span></div>
    <div class="chip"><span class="chip-name">LED</span><span class="chip-value" id="valLed">green</span></div>
  </section>

  <!-- ══════════════ CONTROL GRID ══════════════ -->
  <main class="grid">

    <!-- ── PANEL 1 : X-AXIS ── -->
    <section class="panel">
      <div class="panel-head">
        <span class="panel-idx">01</span>
        <h2>X-Axis</h2>
        <span class="panel-tag">Horizontal</span>
      </div>
      <div class="panel-body">
        <div class="field">
          <label for="xRow">Row</label>
          <input type="number" id="xRow" min="0" max="10" value="0" placeholder="0">
        </div>

        <div class="seg" id="xDirSeg">
          <button class="seg-btn active" data-val="forward" type="button">▲ Forward</button>
          <button class="seg-btn" data-val="backward" type="button">▼ Backward</button>
        </div>

        <button class="btn btn-run" id="xRun" type="button">Run X-Axis</button>
      </div>
    </section>

    <!-- ── PANEL 2 : Y-AXIS ── -->
    <section class="panel">
      <div class="panel-head">
        <span class="panel-idx">02</span>
        <h2>Y-Axis</h2>
        <span class="panel-tag">Vertical</span>
      </div>
      <div class="panel-body">
        <div class="field">
            <label for="yPos">Position / Level</label>
            <input type="number" id="yPos" min="1" max="5" value="1" placeholder="1">
        </div>

        <div class="seg" id="yDirSeg">
          <button class="seg-btn active" data-val="up" type="button">▲ Up</button>
          <button class="seg-btn" data-val="down" type="button">▼ Down</button>
        </div>

        <button class="btn btn-run" id="yRun" type="button">Run Y-Axis</button>
      </div>
    </section>

    <!-- ── PANEL 3 : Z-AXIS ── -->
    <section class="panel">
      <div class="panel-head">
        <span class="panel-idx">03</span>
        <h2>Z-Axis</h2>
        <span class="panel-tag">Pick &amp; Place</span>
      </div>
      <div class="panel-body">
        <div class="axis-actions">
          <button class="btn btn-action" id="zPick" type="button">
            <span class="ico">◈</span> Pick
          </button>
          <button class="btn btn-action place" id="zPlace" type="button">
            <span class="ico">◉</span> Place
          </button>
        </div>
      </div>
    </section>

    <!-- ── PANEL 4 : DOORS ── -->
    <section class="panel">
      <div class="panel-head">
        <span class="panel-idx">04</span>
        <h2>Doors</h2>
        <span class="panel-tag">Dispenser</span>
      </div>
      <div class="panel-body">
        <div class="door-actions">
          <button class="btn btn-ghost door-btn" id="doorOpen" type="button">Open Door</button>
          <button class="btn btn-ghost door-btn active" id="doorClose" type="button">Close Door</button>
        </div>
      </div>
    </section>

    <!-- ── HALF-PAIR GRID : PANELS 5 & 6 ── -->
    <div class="panel-duo">

      <!-- ── PANEL 5 : LED ── -->
      <section class="panel">
        <div class="panel-head">
          <span class="panel-idx">05</span>
          <h2>LED Control</h2>
          <span class="panel-tag">Illumination</span>
        </div>
        <div class="panel-body">
          <div class="led-row">
            <div class="led-indicator" id="ledIndicator"></div>
            <div class="led-info">
              <p class="led-state" id="ledState">OFF</p>
              <p class="hint">Machine interior illumination</p>
            </div>
            <button class="switch" id="ledSwitch" role="switch" aria-checked="false" aria-label="Toggle LED" type="button">
              <span class="knob"></span>
            </button>
          </div>
        </div>
      </section>

      <!-- ── PANEL 6 : TEMPERATURE & HUMIDITY ── -->
      <section class="panel">
        <div class="panel-head">
          <span class="panel-idx">06</span>
          <h2>Temperature &amp; Humidity</h2>
          <span class="panel-tag">ESP Sensor</span>
        </div>
        <div class="panel-body">
          <div class="sensor-row" aria-live="polite">
            <div class="sensor-readout">
              <div class="sensor-label"><span class="sensor-ico">🌡️</span>Temperature</div>
              <div class="sensor-value" id="valTemp"><span id="valTempNum">--</span><span class="sensor-unit">°C</span></div>
            </div>
            <div class="sensor-readout">
              <div class="sensor-label"><span class="sensor-ico">💧</span>Humidity</div>
              <div class="sensor-value" id="valHum"><span id="valHumNum">--</span><span class="sensor-unit">%</span></div>
            </div>
          </div>
          <div class="sensor-status">
            <span class="sensor-dot" id="sensorDot"></span>
            <span id="sensorStatus">Sensor: No Data</span>
          </div>
        </div>
      </section>

    </div>

    <!-- ── PANEL 7 : ORDER & INSTRUCTION HANDLER ── -->
    <section class="panel panel-half">
      <div class="panel-head">
        <span class="panel-idx">07</span>
        <h2>Order &amp; Instruction Handler</h2>
        <span class="panel-tag">ESP ⇄ Mega</span>
      </div>
      <div class="panel-body">
        <div class="field-row">
          <div class="field">
            <label for="shelfId">Shelf ID</label>
            <input type="number" id="shelfId" min="1" max="5" value="1" placeholder="1">
          </div>
          <div class="field">
            <label for="qty">Quantity</label>
            <input type="number" id="qty" min="1" max="255" value="1" placeholder="1">
          </div>
          <div class="field">
            <label for="slotStart">Slot Start</label>
            <input type="number" id="slotStart" min="1" max="10" value="1" placeholder="1">
          </div>
        </div>

        <div class="order-actions">
          <button class="btn btn-run" id="orderRun" type="button">Run Order</button>
          <button class="btn btn-ready" id="orderReady" type="button">Ready</button>
        </div>
      </div>
    </section>

    <!-- ── PANEL 8 : SYSTEM LOG ── -->
    <section class="panel panel-log">
      <div class="panel-head">
        <span class="panel-idx">08</span>
        <h2>System Log</h2>
        <div class="log-tools">
          <button class="btn-clear" id="logClear" type="button">Clear</button>
        </div>
      </div>
      <div class="log-body" id="logBody" role="log" aria-live="polite"></div>
    </section>

  </main>
</div>

<script>
/* ============================================================
   SATEV / RevoV — CONTROL PANEL LOGIC
   Every command rides the WebSocket to the ESP32 brain;
   the machine's own trace, acks, and ESP instructions
   stream back into the log box.
   ============================================================ */
(function () {
  'use strict';

  const LOG_CAP = 250;
  const RECONNECT_MS = 2000;
  const LED_COLORS = { green: '#39D98A', yellow: '#FFC53D', red: '#FF5C5C' };

  const logBody = document.getElementById('logBody');
  const statusDot = document.getElementById('statusDot');
  const statusText = document.getElementById('statusText');
  const valX = document.getElementById('valX');
  const valY = document.getElementById('valY');
  const valZ = document.getElementById('valZ');
  const valDoor = document.getElementById('valDoor');
  const valMachine = document.getElementById('valMachine');
  const valLed = document.getElementById('valLed');
  const doorOpenBtn = document.getElementById('doorOpen');
  const doorCloseBtn = document.getElementById('doorClose');
  const ledSwitch = document.getElementById('ledSwitch');
  const ledIndicator = document.getElementById('ledIndicator');
  const ledState = document.getElementById('ledState');
  const valTemp = document.getElementById('valTemp');
  const valTempNum = document.getElementById('valTempNum');
  const valHum = document.getElementById('valHum');
  const valHumNum = document.getElementById('valHumNum');
  const sensorDot = document.getElementById('sensorDot');
  const sensorStatus = document.getElementById('sensorStatus');

  const state = { xDir: 'forward', yDir: 'up' };
  let ws = null;
  let reconnectTimer = null;
  let follow = true;

  /* ---------- LOGGING ENGINE ---------- */
  function timestamp() {
    const d = new Date();
    const p = (n, w) => String(n).padStart(w, '0');
    return `${p(d.getHours(), 2)}:${p(d.getMinutes(), 2)}:${p(d.getSeconds(), 2)}.${p(d.getMilliseconds(), 3)}`;
  }

  function log(message, type = 'sys') {
    const row = document.createElement('div');
    row.className = `log-row ${type}`;

    const ts = document.createElement('span');
    ts.className = 'log-ts';
    ts.textContent = `[${timestamp()}]`;

    const msg = document.createElement('span');
    msg.className = 'log-msg';
    msg.textContent = message;

    row.appendChild(ts);
    row.appendChild(msg);
    logBody.appendChild(row);

    // Trim to keep the DOM light
    while (logBody.children.length > LOG_CAP) {
      logBody.removeChild(logBody.firstChild);
    }

    // Auto-scroll to the newest entry (unless the
    // operator scrolled up to read history)
    if (follow) logBody.scrollTop = logBody.scrollHeight;
  }

  logBody.addEventListener('scroll', () => {
    follow = logBody.scrollTop + logBody.clientHeight >= logBody.scrollHeight - 24;
  });

  document.getElementById('logClear').addEventListener('click', () => {
    logBody.innerHTML = '';
    log('Log buffer cleared by operator.', 'sys');
  });

  /* ---------- CONNECTION ---------- */
  function setConnected(connected) {
    statusDot.style.background = connected ? 'var(--green)' : 'var(--orange)';
    statusDot.style.boxShadow = `0 0 10px ${connected ? 'var(--green)' : 'var(--orange)'}`;
    statusText.textContent = connected ? 'System Online' : 'Reconnecting';
  }

  function scheduleReconnect() {
    if (reconnectTimer === null) {
      reconnectTimer = setTimeout(connect, RECONNECT_MS);
    }
  }

  function connect() {
    reconnectTimer = null;
    try {
      ws = new WebSocket(`ws://${location.hostname}:81/`);
    } catch (error) {
      ws = null;
      setConnected(false);
      scheduleReconnect();
      return;
    }
    ws.onopen = () => {
      setConnected(true);
      log('LINK ▸ WebSocket established — port 81.', 'esp');
    };
    ws.onmessage = (event) => {
      let msg = null;
      try { msg = JSON.parse(event.data); } catch (error) { return; }
      handleMessage(msg);
    };
    ws.onclose = () => {
      ws = null;
      setConnected(false);
      log('LINK ▸ connection lost — reconnecting…', 'error');
      scheduleReconnect();
    };
    ws.onerror = () => { /* onclose follows */ };
  }

  /* ---------- LIVE STATE ---------- */
  function applyState(msg) {
    valX.textContent = msg.x;
    valY.textContent = msg.y;

    const zAction = msg.zAction || '';
    valZ.textContent = zAction !== '' ? zAction : msg.z;
    valZ.className = 'chip-value' +
      (zAction === 'PICK' ? ' pick' : zAction === 'PLACE' ? ' place' : '');

    const doorOpen = msg.door === 'open';
    valDoor.textContent = doorOpen ? 'OPEN' : 'CLOSED';
    valDoor.className = 'chip-value' + (doorOpen ? ' open' : '');

    valMachine.textContent = msg.machine;
    valMachine.className = 'chip-value' +
      (msg.machine === 'HALTED' || msg.machine === 'ERROR' ? ' error' :
       msg.machine === 'IDLE' ? ' ok' : ' warn');

    valLed.textContent = msg.led;

    doorOpenBtn.classList.toggle('active', doorOpen);
    doorCloseBtn.classList.toggle('active', !doorOpen);

    const ledOn = msg.ledMode === 'on';
    ledSwitch.classList.toggle('on', ledOn);
    ledSwitch.setAttribute('aria-checked', String(ledOn));
    ledState.classList.toggle('on', ledOn);
    ledState.textContent = ledOn ? 'ON' : 'OFF';

    // The indicator mirrors the machine's own LED
    // color: green idle, amber dispatching, red fault.
    const color = LED_COLORS[msg.led];
    ledIndicator.classList.toggle('on', color !== undefined);
    if (color) ledIndicator.style.setProperty('--led-color', color);
  }

  function handleMessage(msg) {
    if (!msg || !msg.type) return;
    if (msg.type === 'state') {
      applyState(msg);
    } else if (msg.type === 'ack' || msg.type === 'error') {
      log(msg.message, msg.type === 'ack' ? 'ready' : 'error');
    } else if (msg.line !== undefined) {
      let cls = 'sys';
      if (msg.type === 'warn' || msg.type === 'error') {
        cls = 'error';
      } else if (msg.type === 'log') {
        if (msg.line.indexOf('Instruction from ESP') === 0) cls = 'esp';
        else if (msg.line.indexOf('Mega Ready Signal') === 0) cls = 'ready';
      }
      log(msg.line, cls);
    }
  }

  /* ---------- COMMANDS ---------- */
  function sendCommand(command) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(command));
    } else {
      log(`LINK ▸ offline — command dropped: ${JSON.stringify(command)}`, 'error');
    }
  }

  /* ---------- SEGMENTED CONTROLS ---------- */
  function bindSegment(segId, key) {
    const seg = document.getElementById(segId);
    seg.addEventListener('click', (e) => {
      const btn = e.target.closest('.seg-btn');
      if (!btn) return;
      seg.querySelectorAll('.seg-btn').forEach((b) => b.classList.remove('active'));
      btn.classList.add('active');
      state[key] = btn.dataset.val;
    });
  }
  bindSegment('xDirSeg', 'xDir');
  bindSegment('yDirSeg', 'yDir');

  /* ---------- PANEL 1 : X-AXIS ---------- */
  document.getElementById('xRun').addEventListener('click', () => {
    sendCommand({
      type: 'axis', axis: 'x',
      dir: state.xDir === 'forward' ? 1 : -1,
      row: Number(document.getElementById('xRow').value)
    });
  });

  /* ---------- PANEL 2 : Y-AXIS ---------- */
  document.getElementById('yRun').addEventListener('click', () => {
    sendCommand({
      type: 'axis', axis: 'y',
      dir: state.yDir === 'up' ? 1 : -1,
      position: Number(document.getElementById('yPos').value)
    });
  });

  /* ---------- PANEL 3 : Z-AXIS ---------- */
  document.getElementById('zPick').addEventListener('click', () => {
    sendCommand({ type: 'z', action: 'pick' });
  });
  document.getElementById('zPlace').addEventListener('click', () => {
    sendCommand({ type: 'z', action: 'place' });
  });

  /* ---------- PANEL 4 : DOORS ---------- */
  doorOpenBtn.addEventListener('click', () => sendCommand({ type: 'door', open: true }));
  doorCloseBtn.addEventListener('click', () => sendCommand({ type: 'door', open: false }));

  /* ---------- PANEL 5 : LED ---------- */
  ledSwitch.addEventListener('click', () => {
    sendCommand({ type: 'led', on: !ledSwitch.classList.contains('on') });
  });

  /* ---------- PANEL 6 : TEMPERATURE & HUMIDITY ---------- */
  // Placeholder data source. The contract is a promise
  // resolving to { temperature: number|null,
  // humidity: number|null }: swap this function's body
  // for a real ESP link — a WebSocket event, an SSE
  // stream, or an HTTP poll — and the display logic
  // below stays untouched.
  function fetchSensorData() {
    return new Promise((resolve) => {
      // Simulated DHT22 on the ESP: most polls carry a
      // reading; roughly one in five returns no data to
      // exercise the "--" state.
      if (Math.random() < 0.2) {
        resolve({ temperature: null, humidity: null });
        return;
      }
      resolve({
        temperature: 20 + Math.random() * 10,
        humidity: 40 + Math.random() * 30
      });
    });
  }

  // Renders one reading. A reading is live only when
  // both values are present and finite, so a missing,
  // malformed, or failed response can never render as
  // 0, NaN, undefined, or null — only as "--".
  function updateSensorDisplay(data) {
    const hasReading = !!data &&
      data.temperature !== null && data.temperature !== undefined &&
      data.humidity !== null && data.humidity !== undefined;
    const temperature = hasReading ? Number(data.temperature) : NaN;
    const humidity = hasReading ? Number(data.humidity) : NaN;
    const live = hasReading && isFinite(temperature) && isFinite(humidity);

    if (live) {
      valTempNum.textContent = temperature.toFixed(1);
      valHumNum.textContent = String(Math.round(humidity));
      valTemp.classList.add('live');
      valHum.classList.add('live');
      sensorDot.classList.add('live');
      sensorStatus.textContent = 'Sensor: Live';
    } else {
      valTempNum.textContent = '--';
      valHumNum.textContent = '--';
      valTemp.classList.remove('live');
      valHum.classList.remove('live');
      sensorDot.classList.remove('live');
      sensorStatus.textContent = 'Sensor: No Data';
    }
  }

  function pollSensor() {
    fetchSensorData()
      .then(updateSensorDisplay)
      .catch(() => {
        updateSensorDisplay({ temperature: null, humidity: null });
      });
  }

  const SENSOR_MS = 2000;
  pollSensor();
  setInterval(pollSensor, SENSOR_MS);

  /* ---------- PANEL 7 : ORDER & INSTRUCTION HANDLER ---------- */
  document.getElementById('orderRun').addEventListener('click', () => {
    sendCommand({
      type: 'order',
      shelfId: Number(document.getElementById('shelfId').value),
      quantity: Number(document.getElementById('qty').value),
      startSlot: Number(document.getElementById('slotStart').value)
    });
  });
  document.getElementById('orderReady').addEventListener('click', () => {
    sendCommand({ type: 'ready' });
  });

  /* ---------- RESET ---------- */
  document.getElementById('resetBtn').addEventListener('click', () => {
    sendCommand({ type: 'reset' });
  });

  /* ---------- BOOT ---------- */
  log('SATEV GROUP ▸ RevoV Control Panel initializing…', 'sys');
  log('The Ultimate Engineering — Valley → Air → Space.', 'sys');
  connect();
})();
</script>
</body>
</html>
)PAGE";
