/* ========= MatrixLamp_R4_Wifi =========
   Standalone Uno R4 WiFi sketch. Nothing is wired up: no NeoPixel strip,
   no buttons, no pots. The picture is the board's 12x8 red LED matrix.

   Loop, always, and it does not block the page:
     scroll the WiFi IP, then 10 seconds of fire,
     scroll the IP, then 10 seconds of fire, ...
   Until the radio has an address, that scroll slot is a waiting animation.
   The IP is printed on Serial at 115200 when it connects.

   A posted frame does not stop the loop. It replaces the fire:
     scroll the IP, then 10 seconds of that frame,
     scroll the IP, then 10 seconds of that frame, ...
   Each POST shows the new frame immediately and restarts the 10 seconds.
   POST /api/reset, DELETE /api/frame, or {"mode":"fire"} (alias "loop")
   puts the 10-second slot back on the fire. DELETE also clears the stored
   frame. Reset and mode fire keep it.

   Core 1.6.0 Arduino_LED_Matrix is 1-bit (loadFrame / renderBitmap). It has
   no setGrayscaleBits and no grayscale draw(). This sketch includes that
   header for the charlieplex map (turnLed) and runs its own timer: one LED
   at a time, Bresenham duty across levels 0..15.

   WiFi follows SpiralLamp_R4_FireTrail_WiFi_MIDI: WiFiS3, secrets.h,
   WiFi.setTimeout(0), status polled, one client, short reads and writes.
   No mDNS in this core. DHCP hostname is matrix-lamp.

   y = 0 is the top row, x = 0 is the left column (renderBitmap order).
   Brightness 0..15 is a matrix level. 16..255 maps to 0..15.

   Uno R4 WiFi (arduino:renesas_uno:unor4wifi), core 1.6.0, with secrets.h
   present: 80256 B flash (30% of 262144), 15552 B globals (47% of 32768),
   17216 B left for locals. An empty WIFI_SSID is 160 B smaller.
   The HTTP buffer is 6144 B of the globals.
*/

#include <Arduino.h>
#include <WiFiS3.h>
#include <Modem.h>
#include <stdio.h>
#include <string.h>
#include "Arduino_LED_Matrix.h"
#include "frame_json.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

static const int MATRIX_W = 12;
static const int MATRIX_H = 8;
static const int MATRIX_N = 96;
static const uint32_t SHOW_MS = 10000;
static const uint32_t LINK_MS = 4000;
static const uint32_t SCROLL_MS = 80;
static const uint32_t FIRE_MS = 70;
static const float MATRIX_TICK_HZ = 96000.0f;

static const uint32_t WIFI_ACCEPT_MS = 30;
static const uint32_t WIFI_STATUS_ONLINE_MS = 1000;
static const uint32_t WIFI_STATUS_JOIN_MS = 250;
static const uint32_t WIFI_RETRY_MS = 8000;
static const int HTTP_READ_MAX = 160;
static const int HTTP_WRITE_MAX = 192;
static const uint32_t HTTP_READ_TIMEOUT_MS = 1200;
static const uint32_t MODEM_CMD_BUDGET_MS = 800;
static const int REQ_CAP = 6144;

enum WifiPhase : uint8_t { WIFI_OFF = 0, WIFI_JOINING = 1, WIFI_ONLINE = 2 };
enum HttpPhase : uint8_t { PH_IDLE = 0, PH_READ = 1, PH_WRITE = 2 };
enum LampPhase : uint8_t { PHASE_SCROLL = 0, PHASE_SHOW = 1 };

// bit 0 = top row of a 5-row glyph. Drawn on matrix rows 1..5.
static const uint8_t DIGITS[10][3] = {
  {0x1f, 0x11, 0x1f},
  {0x12, 0x1f, 0x10},
  {0x1d, 0x15, 0x17},
  {0x15, 0x15, 0x1f},
  {0x07, 0x04, 0x1f},
  {0x17, 0x15, 0x1d},
  {0x1f, 0x15, 0x1d},
  {0x01, 0x01, 0x1f},
  {0x1f, 0x15, 0x1f},
  {0x17, 0x15, 0x1f},
};
static const uint8_t DOTCOL[1] = {0x10};

static volatile uint8_t g_disp[MATRIX_N];
static uint8_t g_acc[MATRIX_N];
static uint8_t g_heat[MATRIX_N];
static uint8_t g_custom[MATRIX_N];
static FspTimer g_matrixTimer;

static WifiPhase g_wifi = WIFI_OFF;
static WiFiServer g_server(80);
static WiFiClient g_client;
static HttpPhase g_http = PH_IDLE;
static char g_req[REQ_CAP];
static uint16_t g_reqLen = 0;
static uint32_t g_clientMs = 0;
static char g_hdr[360];
static uint16_t g_hdrLen = 0;
static uint16_t g_hdrOff = 0;
static const char* g_body = 0;
static uint16_t g_bodyLen = 0;
static uint16_t g_bodyOff = 0;
static char g_json[768];
static uint8_t g_fail = 0;
static uint16_t g_pageLen = 0;
static uint8_t g_ip[4] = {0, 0, 0, 0};
static int16_t g_rssi = 0;
static uint32_t g_lastStatus = 0;
static uint32_t g_lastAccept = 0;
static uint32_t g_lastRssi = 0;
static uint32_t g_joinSince = 0;
static uint8_t g_serverUp = 0;

static LampPhase g_phase = PHASE_SCROLL;
static uint8_t g_kind = FRAME_FIRE;
static uint32_t g_phaseStart = 0;
static uint32_t g_lastFire = 0;
static bool g_scrollTrack = false;
static int g_scrollX = -MATRIX_W;
static uint32_t g_scrollLast = 0;
static char g_ipText[16] = "0.0.0.0";
static int g_ipCols = 0;

static const char PAGE[] = R"PAGE(<!DOCTYPE html>
<html lang=en><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Matrix Lamp</title>
<style>
:root{color-scheme:dark}
body{margin:0;background:#140806;color:#f7eee9;font:16px/1.35 system-ui,sans-serif}
main{max-width:32rem;margin:0 auto;padding:1rem 1rem 2rem}
h1{font-size:1.4rem;margin:0}
.note{color:#d7b2a6;font-size:.88rem;margin:.35rem 0 .8rem}
.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:4px;touch-action:none;user-select:none}
.grid b{display:block;aspect-ratio:1;border-radius:4px;background:#2a1210;box-shadow:inset 0 0 0 1px #5c2e28}
label{display:flex;justify-content:space-between;margin:.95rem 0 .2rem;font-size:.85rem;color:#e4c4b8}
input[type=range]{width:100%;height:2rem;accent-color:#ff3a2a}
.row{display:flex;gap:.5rem;margin-top:.7rem}
button{font:inherit;flex:1;background:#2c1410;color:#f7eee9;border:1px solid #7d4036;border-radius:.65rem;padding:.75rem .4rem;min-height:2.9rem}
button.primary{background:#9a2a1c;border-color:#e15a48;margin-top:.55rem;width:100%}
pre{background:#1b0c09;border-radius:.65rem;padding:.7rem;white-space:pre-wrap;font:12px/1.45 ui-monospace,monospace;margin:1rem 0 0}
</style></head><body><main>
<h1>Matrix Lamp</h1>
<p class=note id=link>connecting</p>
<div class=grid id=grid></div>
<label for=br>Brush <span id=brv>15</span></label>
<input id=br type=range min=0 max=15 value=15>
<div class=row>
<button type=button id=clear>Clear</button>
<button type=button id=fill>Fill</button>
</div>
<button type=button class=primary id=fire>Resume fire</button>
<p class=note>Click or drag. The brush is 0–15; click a lit pixel again to turn it off. A posted frame replaces the fire for 10 seconds, then the IP scrolls, then that frame shows again. Resume fire puts the 10-second slot back on the fire. The IP scroll keeps running.</p>
<pre id=st>loading</pre>
<script>
var grid=document.getElementById('grid');
var br=document.getElementById('br');
var brv=document.getElementById('brv');
var st=document.getElementById('st');
var link=document.getElementById('link');
var levels=[],cells=[];
for(var y=0;y<8;y++){
  levels[y]=[];
  for(var x=0;x<12;x++){
    levels[y][x]=0;
    var el=document.createElement('b');
    el.dataset.x=x;
    el.dataset.y=y;
    grid.appendChild(el);
    cells.push(el);
  }
}
function cell(x,y){return cells[y*12+x];}
function look(el,lv){
  if(!lv){
    el.style.background='#2a1210';
    el.style.boxShadow='inset 0 0 0 1px #5c2e28';
  }else{
    var a=(0.3+0.7*lv/15).toFixed(3);
    el.style.background='rgba(255,48,32,'+a+')';
    el.style.boxShadow='0 0 '+(2+Math.round(lv*0.7))+'px rgba(255,60,36,.95)';
  }
}
for(var i=0;i<cells.length;i++) look(cells[i],0);
br.oninput=function(){brv.textContent=br.value;};
var dirty=0,seeded=0,painting=null,gestured=0,busy=0,wait=0,pend=null,hold=0;
function show(s){
  if(!s||!s.mode) return;
  if(!seeded && !dirty && s.leds && s.leds.length===8){
    seeded=1;
    for(var y2=0;y2<8;y2++){
      if(!s.leds[y2]||s.leds[y2].length!==12) continue;
      for(var x2=0;x2<12;x2++){
        var lv=s.leds[y2][x2]|0;
        if(lv<0) lv=0;
        if(lv>15) lv=15;
        levels[y2][x2]=lv;
        look(cell(x2,y2),lv);
      }
    }
  }
  var what=s.phase==='show'
    ? (s.mode==='custom'?'custom frame':'fire')
    : (s.wifi==='connected'?'scrolling IP':'waiting for WiFi');
  var left=s.phase==='show' ? ('  '+(s.remainingMs/1000).toFixed(1)+'s left') : '';
  link.textContent=(s.ip||'0.0.0.0')+'   '+(s.wifi||'');
  st.textContent=what+left+'\n10s phase '+s.mode+'\nrssi '+s.rssi;
}
function queue(method,path,body){
  pend={m:method,p:path,b:body};
  wait=1;
  if(!busy) pump();
}
function pump(){
  if(!wait||!pend) return;
  var req=pend; wait=0; busy=1;
  fetch(req.p,{method:req.m,headers:req.b?{'Content-Type':'application/json'}:{},body:req.b||undefined})
    .then(function(r){return r.json();})
    .then(show)
    .catch(function(){link.textContent='no response';})
    .then(function(){busy=0; if(wait) pump();});
}
function schedule(){
  clearTimeout(hold);
  hold=setTimeout(function(){queue('POST','/api/frame',JSON.stringify({leds:levels}));},45);
}
function flushNow(){
  clearTimeout(hold);
  queue('POST','/api/frame',JSON.stringify({leds:levels}));
}
function apply(el){
  var x=+el.dataset.x, y=+el.dataset.y;
  if(levels[y][x]===painting) return;
  levels[y][x]=painting;
  look(el,painting);
  dirty=1; gestured=1;
  schedule();
}
function atPoint(ev){
  var el=document.elementFromPoint(ev.clientX,ev.clientY);
  if(el&&el.dataset&&el.dataset.x!=null) return el;
  return null;
}
grid.addEventListener('pointerdown',function(ev){
  var el=ev.target.closest?ev.target.closest('b'):null;
  if(!el||el.dataset.x==null) return;
  ev.preventDefault();
  var brush=+br.value;
  var lv=levels[+el.dataset.y][+el.dataset.x];
  painting=(brush===0||lv===brush)?0:brush;
  gestured=0;
  apply(el);
  if(grid.setPointerCapture) grid.setPointerCapture(ev.pointerId);
});
grid.addEventListener('pointermove',function(ev){
  if(painting===null) return;
  var el=atPoint(ev);
  if(el) apply(el);
});
function endGesture(){
  if(painting===null) return;
  painting=null;
  if(gestured) flushNow();
}
grid.addEventListener('pointerup',endGesture);
grid.addEventListener('pointercancel',endGesture);
function setAll(lv){
  dirty=1;
  for(var y3=0;y3<8;y3++) for(var x3=0;x3<12;x3++){
    levels[y3][x3]=lv;
    look(cell(x3,y3),lv);
  }
  flushNow();
}
document.getElementById('clear').onclick=function(){setAll(0);};
document.getElementById('fill').onclick=function(){setAll(+br.value);};
document.getElementById('fire').onclick=function(){
  clearTimeout(hold);
  queue('POST','/api/reset',null);
};
function poll(){
  if(busy||wait) return;
  fetch('/api/state').then(function(r){return r.json();}).then(show).catch(function(){});
}
setInterval(poll,1000);
poll();
</script>
</main></body></html>
)PAGE";

static void matrixIsr(timer_callback_args_t*) {
  static uint8_t led = 0;
  uint8_t a = (uint8_t)(g_acc[led] + g_disp[led]);
  bool on = false;
  if (a >= 15) {
    a = (uint8_t)(a - 15);
    on = true;
  }
  g_acc[led] = a;
  turnLed((int)led, on);
  if (++led >= MATRIX_N) led = 0;
}

static void matrixBegin() {
  for (uint8_t i = 0; i < MATRIX_N; i++) g_acc[i] = (uint8_t)((i * 7) % 15);
  uint8_t type = 0;
  int8_t ch = FspTimer::get_available_timer(type);
  if (ch < 0) {
    Serial.println(F("Matrix timer unavailable"));
    return;
  }
  bool ok = g_matrixTimer.begin(TIMER_MODE_PERIODIC, type, (uint8_t)ch,
                                 MATRIX_TICK_HZ, 50.0f, matrixIsr, nullptr);
  ok = ok && g_matrixTimer.setup_overflow_irq();
  ok = ok && g_matrixTimer.open();
  ok = ok && g_matrixTimer.start();
  if (!ok) Serial.println(F("Matrix timer failed"));
}

static void blit(const uint8_t* src) {
  for (int i = 0; i < MATRIX_N; i++) g_disp[i] = src[i];
}

static void stepFire() {
  static int8_t wind = 0;
  static uint8_t windTick = 0;
  if (++windTick >= 10) {
    windTick = 0;
    int next = (int)wind + (int)random(0, 3) - 1;
    if (next < -1) next = -1;
    if (next > 1) next = 1;
    wind = (int8_t)next;
  }
  uint8_t row[MATRIX_W];
  for (int y = 0; y < MATRIX_H - 1; y++) {
    for (int x = 0; x < MATRIX_W; x++) {
      int sx = x + (int)wind;
      if (sx < 0) sx = 0;
      if (sx > MATRIX_W - 1) sx = MATRIX_W - 1;
      int xm = (x == 0) ? 0 : x - 1;
      int xp = (x == MATRIX_W - 1) ? MATRIX_W - 1 : x + 1;
      int below = g_heat[(y + 1) * MATRIX_W + sx];
      int left = g_heat[(y + 1) * MATRIX_W + xm];
      int right = g_heat[(y + 1) * MATRIX_W + xp];
      int v = (below * 2 + left + right) / 4;
      v -= (int)random(0, 2);
      if (y < 3) v -= (int)random(0, 2);
      if (v < 0) v = 0;
      if (v > 15) v = 15;
      row[x] = (uint8_t)v;
    }
    memcpy(g_heat + y * MATRIX_W, row, MATRIX_W);
  }
  for (int x = 0; x < MATRIX_W; x++) {
    int v = 11 + (int)random(0, 5);
    if (random(0, 8) == 0) v = (int)random(0, 6);
    g_heat[(MATRIX_H - 1) * MATRIX_W + x] = (uint8_t)v;
  }
}

static int textWidth(const char* s) {
  int at = 0;
  for (; *s; ++s) {
    if (*s >= '0' && *s <= '9') at += 4;
    else if (*s == '.') at += 2;
    else at += 2;
  }
  return at;
}

static uint8_t columnBits(const char* s, int col) {
  int at = 0;
  for (const char* p = s; *p; ++p) {
    const uint8_t* g = 0;
    int n = 0;
    int span = 2;
    if (*p >= '0' && *p <= '9') {
      g = DIGITS[*p - '0'];
      n = 3;
      span = 4;
    } else if (*p == '.') {
      g = DOTCOL;
      n = 1;
      span = 2;
    }
    if (col < at + span) {
      int local = col - at;
      if (local < 0 || local >= n || !g) return 0;
      return g[local];
    }
    at += span;
  }
  return 0;
}

static void renderIp() {
  for (int i = 0; i < MATRIX_N; i++) g_disp[i] = 0;
  for (int x = 0; x < MATRIX_W; x++) {
    int src = g_scrollX + x;
    if (src < 0) continue;
    uint8_t bits = columnBits(g_ipText, src);
    for (int b = 0; b < 5; b++) {
      if (bits & (1 << b)) g_disp[(1 + b) * MATRIX_W + x] = 15;
    }
  }
}

static void renderLink(uint32_t now) {
  for (int i = 0; i < MATRIX_N; i++) g_disp[i] = 0;
  static const uint8_t trail[5] = {15, 9, 5, 2, 1};
  int head = (int)((now / 60) % MATRIX_W);
  for (int t = 0; t < 5; t++) {
    int x = head - t;
    if (x < 0) x += MATRIX_W;
    g_disp[3 * MATRIX_W + x] = trail[t];
    g_disp[4 * MATRIX_W + x] = trail[t];
    if (t == 0) {
      g_disp[2 * MATRIX_W + x] = 5;
      g_disp[5 * MATRIX_W + x] = 5;
    }
  }
}

static void buildIpText() {
  snprintf(g_ipText, sizeof g_ipText, "%u.%u.%u.%u",
           (unsigned)g_ip[0], (unsigned)g_ip[1],
           (unsigned)g_ip[2], (unsigned)g_ip[3]);
  g_ipCols = textWidth(g_ipText);
}

static void enterShow(uint32_t now);
static void tickScroll(uint32_t now);

static void enterScroll(uint32_t now) {
  g_phase = PHASE_SCROLL;
  g_phaseStart = now;
  g_scrollTrack = false;
  g_scrollX = -MATRIX_W;
  g_scrollLast = now;
  tickScroll(now);
}

static void enterShow(uint32_t now) {
  g_phase = PHASE_SHOW;
  g_phaseStart = now;
  if (g_kind == FRAME_FIRE) {
    stepFire();
    blit(g_heat);
    g_lastFire = now;
  } else {
    blit(g_custom);
  }
}

static void tickScroll(uint32_t now) {
  if (g_wifi == WIFI_ONLINE) {
    if (!g_scrollTrack) {
      buildIpText();
      g_scrollX = -MATRIX_W;
      g_scrollLast = now;
      g_scrollTrack = true;
      renderIp();
      return;
    }
    if ((uint32_t)(now - g_scrollLast) >= SCROLL_MS) {
      uint32_t steps = (now - g_scrollLast) / SCROLL_MS;
      if (steps > 2) steps = 2;
      g_scrollLast += steps * SCROLL_MS;
      g_scrollX += (int)steps;
      renderIp();
      if (g_scrollX > g_ipCols) enterShow(now);
    }
    return;
  }
  if (g_scrollTrack) {
    g_scrollTrack = false;
    g_phaseStart = now;
  }
  renderLink(now);
  if ((uint32_t)(now - g_phaseStart) >= LINK_MS) enterShow(now);
}

static void tickShow(uint32_t now) {
  if (g_kind == FRAME_FIRE && (uint32_t)(now - g_lastFire) >= FIRE_MS) {
    g_lastFire = now;
    stepFire();
    blit(g_heat);
  }
  if ((uint32_t)(now - g_phaseStart) >= SHOW_MS) enterScroll(now);
}

static void showFireNow(uint32_t now) {
  g_kind = FRAME_FIRE;
  enterShow(now);
}

static void showCustomNow(uint32_t now) {
  g_kind = FRAME_CUSTOM;
  enterShow(now);
}

static bool matchHdr(const char* p, const char* key) {
  while (*key) {
    char c = *p++;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c != *key++) return false;
  }
  return true;
}

static int contentLength(const char* req) {
  const char* end = strstr(req, "\r\n\r\n");
  if (!end) return 0;
  const char* p = req;
  while (p < end) {
    if (matchHdr(p, "content-length:")) {
      p += 15;
      while (*p == ' ' || *p == '\t') p++;
      if (*p < '0' || *p > '9') return -1;
      int n = 0;
      while (*p >= '0' && *p <= '9') {
        n = n * 10 + (*p - '0');
        if (n > 1000000) return -1;
        p++;
      }
      return n;
    }
    while (p < end && !(*p == '\r' && (p + 1) < end && p[1] == '\n')) p++;
    if (p < end && *p == '\r') p += 2;
  }
  return 0;
}

static int buildState(char* b, size_t n, bool ok, const char* err, uint32_t now) {
  const char* mode = (g_kind == FRAME_CUSTOM) ? "custom" : "fire";
  const char* phase = (g_phase == PHASE_SHOW) ? "show" : "scroll";
  const char* wifi = (g_wifi == WIFI_ONLINE) ? "connected" :
                     (g_wifi == WIFI_JOINING) ? "joining" : "off";
  unsigned remaining = 0;
  if (g_phase == PHASE_SHOW) {
    uint32_t elapsed = now - g_phaseStart;
    remaining = (elapsed >= SHOW_MS) ? 0 : (unsigned)(SHOW_MS - elapsed);
  }
  int w = snprintf(b, n,
    "{\"ok\":%s,\"error\":\"%s\",\"mode\":\"%s\",\"phase\":\"%s\","
    "\"wifi\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,\"remainingMs\":%u,\"leds\":[",
    ok ? "true" : "false",
    err ? err : "",
    mode, phase, wifi,
    (unsigned)g_ip[0], (unsigned)g_ip[1], (unsigned)g_ip[2], (unsigned)g_ip[3],
    (int)g_rssi, remaining);
  if (w < 0 || (size_t)w >= n) return -1;
  size_t i = (size_t)w;
  for (int y = 0; y < MATRIX_H; y++) {
    if (i + 1 >= n) return -1;
    b[i++] = '[';
    for (int x = 0; x < MATRIX_W; x++) {
      char num[8];
      int k = snprintf(num, sizeof num, "%u%s",
                       (unsigned)g_custom[y * MATRIX_W + x],
                       (x < MATRIX_W - 1) ? "," : "");
      if (k < 0 || i + (size_t)k >= n) return -1;
      memcpy(b + i, num, (size_t)k);
      i += (size_t)k;
    }
    if (i + 2 >= n) return -1;
    b[i++] = ']';
    if (y < MATRIX_H - 1) b[i++] = ',';
  }
  if (i + 2 >= n) return -1;
  b[i++] = ']';
  b[i++] = '}';
  b[i] = 0;
  return (int)i;
}

static void httpBegin(int code, const char* reason, const char* type,
                      const char* body, uint16_t len) {
  int n = snprintf(g_hdr, sizeof g_hdr,
    "HTTP/1.1 %d %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: %u\r\n"
    "Connection: close\r\n"
    "Cache-Control: no-store\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "\r\n",
    code, reason, type, (unsigned)len);
  if (n < 0 || (size_t)n >= sizeof g_hdr) {
    g_hdrLen = 0;
    g_body = 0;
    g_bodyLen = 0;
  } else {
    g_hdrLen = (uint16_t)n;
    g_body = body;
    g_bodyLen = len;
  }
  g_hdrOff = 0;
  g_bodyOff = 0;
  g_fail = 0;
  g_http = PH_WRITE;
}

static void httpJson(bool ok, const char* err, uint32_t now) {
  int n = buildState(g_json, sizeof g_json, ok, err, now);
  if (n < 0) {
    n = snprintf(g_json, sizeof g_json, "{\"ok\":false,\"error\":\"full\"}");
    ok = false;
  }
  if (n < 0) n = 0;
  httpBegin(ok ? 200 : 400, ok ? "OK" : "Bad Request",
            "application/json", g_json, (uint16_t)n);
}

static bool modeFromRequest(const char* query, const char* body, uint8_t* kind) {
  const char* q = query;
  while (q && *q) {
    if (strncmp(q, "mode=", 5) == 0) return frameParseModeToken(q + 5, kind);
    while (*q && *q != '&') q++;
    if (*q == '&') q++;
  }
  const char* m = frameFindKey(body ? body : "", "mode");
  if (!m) return false;
  m = frameSkipWs(m);
  if (*m != '"') return false;
  return frameParseModeToken(m + 1, kind);
}

static void handleFrame(const char* body, uint32_t now) {
  bool pix = false;
  bool hasMode = false;
  uint8_t kind = FRAME_FIRE;
  const char* err = 0;
  uint8_t temp[MATRIX_N];
  memcpy(temp, g_custom, sizeof temp);
  if (!frameParseBody(body ? body : "", temp, &pix, &hasMode, &kind, &err)) {
    httpJson(false, err ? err : "json", now);
    return;
  }
  if (pix) {
    memcpy(g_custom, temp, sizeof g_custom);
    showCustomNow(now);
  } else if (hasMode) {
    if (kind == FRAME_CUSTOM) showCustomNow(now);
    else showFireNow(now);
  } else {
    httpJson(false, "json", now);
    return;
  }
  httpJson(true, 0, now);
}

static void readToken(const char*& p, char* dst, size_t n) {
  size_t i = 0;
  while (*p && *p != ' ' && *p != '\r' && *p != '\n' && i + 1 < n) dst[i++] = *p++;
  dst[i] = 0;
  while (*p && *p != ' ' && *p != '\r' && *p != '\n') p++;
}

static void httpHandle(uint32_t now, const char* body) {
  const char* p = g_req;
  char method[8];
  readToken(p, method, sizeof method);
  if (*p == ' ') p++;
  char raw[72];
  readToken(p, raw, sizeof raw);
  char path[48];
  char query[40];
  path[0] = 0;
  query[0] = 0;
  char* qmark = strchr(raw, '?');
  if (qmark) {
    *qmark = 0;
    strncpy(query, qmark + 1, sizeof query - 1);
    query[sizeof query - 1] = 0;
  }
  strncpy(path, raw, sizeof path - 1);
  path[sizeof path - 1] = 0;

  if (!strcmp(method, "OPTIONS")) {
    httpBegin(204, "No Content", "text/plain", "", 0);
    return;
  }
  if (!strcmp(path, "/") || !strcmp(path, "/index.html")) {
    if (strcmp(method, "GET")) { httpJson(false, "method", now); return; }
    httpBegin(200, "OK", "text/html; charset=utf-8", PAGE, g_pageLen);
    return;
  }
  if (!strcmp(path, "/favicon.ico")) {
    httpBegin(204, "No Content", "text/plain", "", 0);
    return;
  }
  if (!strcmp(path, "/api/state")) {
    if (strcmp(method, "GET")) { httpJson(false, "method", now); return; }
    httpJson(true, 0, now);
    return;
  }
  if (!strcmp(path, "/api/reset")) {
    if (strcmp(method, "POST")) { httpJson(false, "method", now); return; }
    showFireNow(now);
    httpJson(true, 0, now);
    return;
  }
  if (!strcmp(path, "/api/frame") && !strcmp(method, "DELETE")) {
    memset(g_custom, 0, sizeof g_custom);
    showFireNow(now);
    httpJson(true, 0, now);
    return;
  }
  if (!strcmp(path, "/api/mode")) {
    if (strcmp(method, "POST")) { httpJson(false, "method", now); return; }
    uint8_t kind = FRAME_FIRE;
    if (!modeFromRequest(query, body, &kind)) {
      httpJson(false, "mode", now);
      return;
    }
    if (kind == FRAME_CUSTOM) showCustomNow(now);
    else showFireNow(now);
    httpJson(true, 0, now);
    return;
  }
  if (!strcmp(path, "/api/frame") || !strcmp(path, "/api/leds")) {
    if (strcmp(method, "POST")) { httpJson(false, "method", now); return; }
    handleFrame(body, now);
    return;
  }
  static const char MISS[] = "not found\n";
  httpBegin(404, "Not Found", "text/plain; charset=utf-8", MISS, (uint16_t)(sizeof MISS - 1));
}

static void httpRead(uint32_t now) {
  int avail = g_client.available();
  if (avail > 0) {
    if (avail > HTTP_READ_MAX) avail = HTTP_READ_MAX;
    int room = (int)sizeof g_req - 1 - (int)g_reqLen;
    if (avail > room) avail = room;
    if (avail > 0) {
      uint8_t tmp[HTTP_READ_MAX];
      int n = g_client.read(tmp, (size_t)avail);
      if (n > 0) {
        memcpy(g_req + g_reqLen, tmp, (size_t)n);
        g_reqLen = (uint16_t)(g_reqLen + n);
        g_req[g_reqLen] = 0;
      }
    }
  }
  char* hdrEnd = strstr(g_req, "\r\n\r\n");
  bool timed = (uint32_t)(now - g_clientMs) >= HTTP_READ_TIMEOUT_MS;
  if (!hdrEnd) {
    if (timed || g_reqLen >= (sizeof g_req) - 1) {
      g_client.stop();
      g_http = PH_IDLE;
    }
    return;
  }
  int headerBytes = (int)(hdrEnd - g_req) + 4;
  int cl = contentLength(g_req);
  if (cl < 0) {
    httpJson(false, "json", now);
    return;
  }
  if (headerBytes + cl >= (int)sizeof g_req) {
    static const char FULL[] = "{\"ok\":false,\"error\":\"full\"}";
    httpBegin(413, "Payload Too Large", "application/json", FULL, (uint16_t)(sizeof FULL - 1));
    return;
  }
  if ((int)g_reqLen < headerBytes + cl) {
    if (timed) {
      g_client.stop();
      g_http = PH_IDLE;
    }
    return;
  }
  g_req[headerBytes + cl] = 0;
  httpHandle(now, g_req + headerBytes);
}

static void httpWrite() {
  if (g_hdrOff < g_hdrLen) {
    uint16_t n = (uint16_t)(g_hdrLen - g_hdrOff);
    if (n > HTTP_WRITE_MAX) n = HTTP_WRITE_MAX;
    size_t w = g_client.write((const uint8_t*)g_hdr + g_hdrOff, n);
    if (w == 0) {
      if (++g_fail > 8) { g_client.stop(); g_http = PH_IDLE; }
      return;
    }
    g_fail = 0;
    g_hdrOff = (uint16_t)(g_hdrOff + w);
    return;
  }
  if (g_body && g_bodyOff < g_bodyLen) {
    uint16_t n = (uint16_t)(g_bodyLen - g_bodyOff);
    if (n > HTTP_WRITE_MAX) n = HTTP_WRITE_MAX;
    size_t w = g_client.write((const uint8_t*)g_body + g_bodyOff, n);
    if (w == 0) {
      if (++g_fail > 8) { g_client.stop(); g_http = PH_IDLE; }
      return;
    }
    g_fail = 0;
    g_bodyOff = (uint16_t)(g_bodyOff + w);
    return;
  }
  g_client.stop();
  g_http = PH_IDLE;
}

static void wifiBegin() {
  if (WIFI_PASS[0] == 0) WiFi.begin(WIFI_SSID);
  else WiFi.begin(WIFI_SSID, WIFI_PASS);
  g_joinSince = millis();
}

static void noteConnected() {
  IPAddress ip = WiFi.localIP();
  g_ip[0] = ip[0];
  g_ip[1] = ip[1];
  g_ip[2] = ip[2];
  g_ip[3] = ip[3];
  g_rssi = (int16_t)WiFi.RSSI();
  g_lastRssi = millis();
  Serial.print(F("WiFi "));
  Serial.print(g_ip[0]); Serial.print('.');
  Serial.print(g_ip[1]); Serial.print('.');
  Serial.print(g_ip[2]); Serial.print('.');
  Serial.println(g_ip[3]);
  Serial.print(F("Open http://"));
  Serial.print(g_ip[0]); Serial.print('.');
  Serial.print(g_ip[1]); Serial.print('.');
  Serial.print(g_ip[2]); Serial.print('.');
  Serial.print(g_ip[3]);
  Serial.println(F("/"));
  Serial.println(F("mDNS off: matrix-lamp.local will not resolve. DHCP name is matrix-lamp."));
}

static void wifiDrop(uint32_t now) {
  Serial.println(F("WiFi dropped"));
  g_ip[0] = g_ip[1] = g_ip[2] = g_ip[3] = 0;
  g_rssi = 0;
  g_wifi = WIFI_JOINING;
  g_server.end();
  g_serverUp = 0;
  if (g_http != PH_IDLE) {
    g_client.stop();
    g_http = PH_IDLE;
  }
  g_joinSince = now - WIFI_RETRY_MS;
}

static void wifiPollStatus(uint32_t now) {
  uint8_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    if (g_wifi != WIFI_ONLINE) {
      g_wifi = WIFI_ONLINE;
      noteConnected();
    } else if ((uint32_t)(now - g_lastRssi) >= 5000) {
      g_rssi = (int16_t)WiFi.RSSI();
      g_lastRssi = now;
    }
    if (!g_serverUp) {
      g_server.begin();
      g_serverUp = g_server ? 1 : 0;
    }
    return;
  }
  if (g_wifi == WIFI_ONLINE) {
    wifiDrop(now);
    return;
  }
  if ((uint32_t)(now - g_joinSince) >= WIFI_RETRY_MS) wifiBegin();
}

static void webSetup() {
  size_t n = strlen(PAGE);
  g_pageLen = (n > 65535u) ? 65535u : (uint16_t)n;
  if (WIFI_SSID[0] == 0) {
    g_wifi = WIFI_OFF;
    Serial.println(F("WiFi skipped (no WIFI_SSID in secrets.h)"));
    return;
  }
  g_wifi = WIFI_JOINING;
  WiFi.setTimeout(0);
  WiFi.status();
  modem.timeout(MODEM_CMD_BUDGET_MS);
  WiFi.setHostname("matrix-lamp");
  wifiBegin();
  Serial.println(F("WiFi joining"));
}

static void webPoll(uint32_t now) {
  if (g_wifi == WIFI_OFF) return;
  if (g_http == PH_READ) { httpRead(now); return; }
  if (g_http == PH_WRITE) { httpWrite(); return; }
  if (g_wifi == WIFI_ONLINE && g_serverUp && (uint32_t)(now - g_lastAccept) >= WIFI_ACCEPT_MS) {
    g_lastAccept = now;
    WiFiClient incoming = g_server.available();
    if (incoming) {
      g_client = incoming;
      g_reqLen = 0;
      g_req[0] = 0;
      g_clientMs = now;
      g_http = PH_READ;
      return;
    }
  }
  uint32_t period = (g_wifi == WIFI_ONLINE) ? WIFI_STATUS_ONLINE_MS : WIFI_STATUS_JOIN_MS;
  if ((uint32_t)(now - g_lastStatus) >= period) {
    g_lastStatus = now;
    wifiPollStatus(now);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Matrix Lamp R4 WiFi starting"));
  randomSeed((uint32_t)micros() ^ (uint32_t)analogRead(A0));
  for (uint8_t i = 0; i < 40; i++) stepFire();
  // Bring the radio up before the matrix ISR, so the modem's first
  // commands are not sharing the CPU with a 96 kHz tick.
  webSetup();
  matrixBegin();
  enterScroll(millis());
}

void loop() {
  uint32_t now = millis();
  webPoll(now);
  if (g_phase == PHASE_SCROLL) tickScroll(now);
  else tickShow(now);
}
