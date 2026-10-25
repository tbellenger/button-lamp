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
   A posted animation does the same with frames[0..N-1], stepping every
   interval milliseconds and wrapping for the whole 10-second slot.
   Each POST shows the new picture immediately and restarts the 10 seconds.
   POST /api/reset, DELETE /api/frame, DELETE /api/animation, or
   {"mode":"fire"} (alias "loop") puts the 10-second slot back on the fire.
   DELETE also clears the stored picture. Reset and mode fire keep it.

   Core 1.6.0 Arduino_LED_Matrix is 1-bit (loadFrame / renderBitmap) and its
   own timer runs at 10 kHz. It has no grayscale draw(). This sketch includes
   that header for the charlieplex map (turnLed) and runs its own timer at
   24 kHz: one LED at a time, Bresenham duty across levels 0..15. That is a
   250 Hz scan of the 96 LEDs. 96 kHz left too little time for the modem UART
   (Serial2 at 115200) to finish a byte between ticks.

   WiFi is WiFiS3 plus secrets.h. The first WiFi.begin() in setup uses the
   library defaults (10 s connect wait, 10 s modem command timeout) and runs
   before the matrix timer starts. After that, WiFi.setTimeout(0) and
   modem.timeout(800) keep the loop bounded. status() can report WL_CONNECTED
   as soon as the radio associates, while localIP() is still 0.0.0.0, so the
   lamp stays in the joining animation until the address is non-zero. A new
   begin() is not sent for 22 s, so DHCP is not reset mid-handshake.
   mDNS answers matrixlamp.local and matrix-lamp.local (UDP 5353).
   DHCP hostname is matrixlamp, so http://matrixlamp/ works on routers
   that publish the DHCP name. matrix-lamp is mDNS only.

   y = 0 is the top row, x = 0 is the left column (renderBitmap order).
   Brightness 0..15 is a matrix level. 16..255 maps to 0..15.

   Uno R4 WiFi (arduino:renesas_uno:unor4wifi), core 1.6.0. Empty WIFI_SSID
   (no secrets.h): 94416 B flash (36% of 262144), 23048 B globals (70% of
   32768), 9720 B left for locals. secrets.h with a short SSID and password
   is 94624 B flash; globals stay 23048 B. The request buffer and the state
   JSON are each 6144 B. The mDNS packet buffer is 512 B. The core's 8 KB
   heap and 1 KB main stack still fit above the globals. The mDNS socket
   (WiFiUDP, about 1.5 KB) is malloc'd while WiFi is up, next to the
   client's 1024 B FIFO.
*/

#include <Arduino.h>
#include <WiFiS3.h>
#include <Modem.h>
#include <stdio.h>
#include <string.h>
#include "Arduino_LED_Matrix.h"
#include "frame_json.h"
#include "matrix_fire.h"
#include "mdns_responder.h"

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
// 24000/96 = 250 Hz scan. Dim levels span several scans; 96 kHz starved the modem.
static const float MATRIX_TICK_HZ = 24000.0f;

static const uint32_t WIFI_ACCEPT_MS = 30;
static const uint32_t WIFI_STATUS_ONLINE_MS = 1000;
static const uint32_t WIFI_STATUS_JOIN_MS = 250;
static const uint32_t WIFI_RETRY_MS = 22000;
static const uint32_t WIFI_IP_POLL_MS = 3000;
static const int HTTP_READ_MAX = 160;
static const int HTTP_WRITE_MAX = 192;
static const uint32_t HTTP_READ_TIMEOUT_MS = 1200;
static const uint32_t MODEM_CMD_BUDGET_MS = 800;
static const int REQ_CAP = 6144;
static const int JSON_CAP = 6144;

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
static uint8_t g_anim[FRAME_ANIM_MAX * MATRIX_N];
static uint16_t g_animDur[FRAME_ANIM_MAX];
static uint8_t g_animCount = 0;
static uint8_t g_animIndex = 0;
static uint16_t g_interval = FRAME_INTERVAL_DEFAULT;
static uint32_t g_animLast = 0;
static uint8_t g_stored = FRAME_FIRE;
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
static char g_json[JSON_CAP];
static uint8_t g_fail = 0;
static uint16_t g_pageLen = 0;
static uint8_t g_ip[4] = {0, 0, 0, 0};
static int16_t g_rssi = 0;
static uint32_t g_lastStatus = 0;
static uint32_t g_lastAccept = 0;
static uint32_t g_lastRssi = 0;
static uint32_t g_lastIpPoll = 0;
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
button.on{background:#5a241c;border-color:#ffb4a8}
.strip{display:flex;gap:.35rem;overflow-x:auto;padding:.2rem 0 .45rem;-webkit-overflow-scrolling:touch}
.strip button{flex:0 0 auto;width:auto;min-height:0;padding:.28rem .32rem;margin:0}
.mini{display:grid;grid-template-columns:repeat(12,3px);grid-auto-rows:3px;gap:1px;margin-top:.22rem}
.mini i{display:block;width:3px;height:3px;background:#2a1210}
input[type=number]{font:inherit;display:block;width:6.2rem;margin-top:.45rem;background:#2c1410;color:#f7eee9;border:1px solid #7d4036;border-radius:.55rem;padding:.45rem .55rem}
.frameLabel{margin:.85rem 0 .15rem;font-size:.85rem;color:#e4c4b8}
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
<p class=frameLabel id=framelabel>Frame 1 of 1</p>
<div class=strip id=strip></div>
<div class=row>
<button type=button id=prev>Previous</button>
<button type=button id=next>Next</button>
</div>
<div class=row>
<button type=button id=add>Add frame</button>
<button type=button id=dup>Duplicate</button>
<button type=button id=del>Delete</button>
</div>
<div class=row>
<button type=button id=play>Play</button>
</div>
<label for=iv>Interval <span id=ivv>100</span> ms</label>
<input id=iv type=range min=20 max=5000 step=10 value=100>
<input id=ivn type=number min=20 max=5000 step=10 value=100>
<button type=button class=primary id=send>Send animation</button>
<button type=button class=primary id=fire>Resume fire</button>
<p class=note>Click or drag to paint this frame. The brush is 0–15; click a lit pixel again to turn it off. Painting posts that one frame, which replaces the fire for 10 seconds, then the IP scrolls, then the frame shows again. Add frame, duplicate, and delete build a strip. Play previews it here. Send animation loops those frames, at the interval, through the same 10-second slot. Resume fire puts that slot back on the fire. The IP scroll keeps running.</p>
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
var MAXF=16, cur=0, playing=0, playTimer=0;
function blank(){
  var f=[];
  for(var yb=0;yb<8;yb++){
    f[yb]=[];
    for(var xb=0;xb<12;xb++) f[yb][xb]=0;
  }
  return f;
}
function copyFrame(src){
  var f=blank();
  for(var yc=0;yc<8;yc++) for(var xc=0;xc<12;xc++){
    var lv=src&&src[yc]?(src[yc][xc]|0):0;
    if(lv<0) lv=0;
    if(lv>15) lv=15;
    f[yc][xc]=lv;
  }
  return f;
}
var frames=[blank()];
var iv=document.getElementById('iv');
var ivn=document.getElementById('ivn');
var ivv=document.getElementById('ivv');
function clampIv(n){
  n=Math.round(+n);
  if(!(n>=20)) n=20;
  if(n>5000) n=5000;
  return n;
}
function setIv(n){
  n=clampIv(n);
  iv.value=n; ivn.value=n; ivv.textContent=String(n);
  if(playing) schedulePlay();
}
iv.oninput=function(){setIv(iv.value);};
ivn.onchange=function(){setIv(ivn.value);};
function dot(lv){
  if(!lv) return '#2a1210';
  return 'rgba(255,48,32,'+(0.25+0.75*lv/15).toFixed(2)+')';
}
function renderStrip(){
  var strip=document.getElementById('strip');
  strip.innerHTML='';
  for(var i=0;i<frames.length;i++){
    var b=document.createElement('button');
    b.type='button';
    b.className=i===cur?'on':'';
    b.dataset.i=i;
    b.appendChild(document.createTextNode(String(i+1)));
    var mini=document.createElement('span');
    mini.className='mini';
    for(var y=0;y<8;y++) for(var x=0;x<12;x++){
      var px=document.createElement('i');
      px.style.background=dot(frames[i][y][x]);
      mini.appendChild(px);
    }
    b.appendChild(mini);
    strip.appendChild(b);
  }
  document.getElementById('framelabel').textContent='Frame '+(cur+1)+' of '+frames.length;
}
function storeFrame(){
  for(var y=0;y<8;y++) for(var x=0;x<12;x++) frames[cur][y][x]=levels[y][x];
}
function paintAll(){
  for(var y=0;y<8;y++) for(var x=0;x<12;x++) look(cell(x,y), levels[y][x]);
}
function loadFrame(i){
  if(i<0) i=frames.length-1;
  if(i>=frames.length) i=0;
  cur=i;
  for(var y=0;y<8;y++) for(var x=0;x<12;x++) levels[y][x]=frames[cur][y][x];
  paintAll();
  renderStrip();
}
function pause(){
  playing=0;
  clearTimeout(playTimer);
  var b=document.getElementById('play');
  b.textContent='Play';
  b.className='';
}
function schedulePlay(){
  clearTimeout(playTimer);
  if(!playing) return;
  playTimer=setTimeout(function(){
    if(!playing) return;
    storeFrame();
    loadFrame(cur+1);
    schedulePlay();
  }, clampIv(iv.value));
}
function show(s){
  if(!s||!s.mode) return;
  if(!seeded && !dirty){
    seeded=1;
    if(s.frames && s.frames.length) adopt(s.frames, s.interval, s.frame);
    else if(s.leds && s.leds.length===8) adopt([s.leds], s.interval||100, 0);
  }
  var what=s.phase==='show'
    ? (s.mode==='animation' ? ('animation '+((s.frame|0)+1)+'/'+(s.frameCount||1))
       : (s.mode==='custom'?'custom frame':'fire'))
    : (s.wifi==='connected'?'scrolling IP':'waiting for WiFi');
  var left=s.phase==='show' ? ('  '+(s.remainingMs/1000).toFixed(1)+'s left') : '';
  var extra=(s.frameCount?('\n'+s.frameCount+' frames @ '+(s.interval||0)+' ms'):'');
  link.textContent=(s.ip||'0.0.0.0')+'   '+(s.wifi||'');
  st.textContent=what+left+'\n10s slot '+s.mode+extra+'\nrssi '+s.rssi;
}
function adopt(list, interval, index){
  var next=[];
  for(var i=0;i<list.length && next.length<MAXF;i++) next.push(copyFrame(list[i]));
  if(!next.length) return;
  frames=next;
  if(interval) setIv(interval);
  var fi=index|0;
  if(fi<0||fi>=frames.length) fi=0;
  cur=fi;
  for(var y=0;y<8;y++) for(var x=0;x<12;x++) levels[y][x]=frames[cur][y][x];
  paintAll();
  renderStrip();
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
  pause();
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
  storeFrame();
  renderStrip();
  if(gestured) flushNow();
}
grid.addEventListener('pointerup',endGesture);
grid.addEventListener('pointercancel',endGesture);
function setAll(lv){
  dirty=1;
  pause();
  for(var y3=0;y3<8;y3++) for(var x3=0;x3<12;x3++){
    levels[y3][x3]=lv;
    look(cell(x3,y3),lv);
  }
  storeFrame();
  renderStrip();
  flushNow();
}
document.getElementById('clear').onclick=function(){setAll(0);};
document.getElementById('fill').onclick=function(){setAll(+br.value);};
document.getElementById('strip').onclick=function(ev){
  var b=ev.target.closest?ev.target.closest('button'):null;
  if(!b||b.dataset.i==null) return;
  pause();
  storeFrame();
  loadFrame(+b.dataset.i);
};
document.getElementById('prev').onclick=function(){pause(); storeFrame(); loadFrame(cur-1);};
document.getElementById('next').onclick=function(){pause(); storeFrame(); loadFrame(cur+1);};
document.getElementById('add').onclick=function(){
  pause(); storeFrame();
  if(frames.length>=MAXF){ link.textContent='16 frames max'; return; }
  frames.splice(cur+1,0,blank());
  loadFrame(cur+1);
};
document.getElementById('dup').onclick=function(){
  pause(); storeFrame();
  if(frames.length>=MAXF){ link.textContent='16 frames max'; return; }
  frames.splice(cur+1,0,copyFrame(frames[cur]));
  loadFrame(cur+1);
};
document.getElementById('del').onclick=function(){
  pause();
  if(frames.length===1){ frames[0]=blank(); loadFrame(0); return; }
  frames.splice(cur,1);
  if(cur>=frames.length) cur=frames.length-1;
  loadFrame(cur);
};
document.getElementById('play').onclick=function(){
  if(playing){ pause(); return; }
  storeFrame();
  playing=1;
  this.textContent='Pause';
  this.className='on';
  schedulePlay();
};
document.getElementById('send').onclick=function(){
  pause();
  storeFrame();
  clearTimeout(hold);
  queue('POST','/api/animation', JSON.stringify({interval:clampIv(iv.value), frames:frames}));
};
document.getElementById('fire').onclick=function(){
  pause();
  clearTimeout(hold);
  queue('POST','/api/reset',null);
};
renderStrip();
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

static const uint8_t* animAt(uint8_t i) {
  if (g_animCount == 0) return g_custom;
  if (i >= g_animCount) i = 0;
  return g_anim + (size_t)i * MATRIX_N;
}

static void enterShow(uint32_t now) {
  g_phase = PHASE_SHOW;
  g_phaseStart = now;
  if (g_kind == FRAME_FIRE) {
    matrixFireStep(g_heat);
    blit(g_heat);
    g_lastFire = now;
  } else if (g_kind == FRAME_ANIMATION && g_animCount > 0) {
    g_animIndex = 0;
    g_animLast = now;
    blit(animAt(0));
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
    matrixFireStep(g_heat);
    blit(g_heat);
  } else if (g_kind == FRAME_ANIMATION && g_animCount > 0) {
    uint8_t guard = 0;
    while (guard < 8) {
      uint16_t dur = g_animDur[g_animIndex];
      if (dur < FRAME_INTERVAL_MIN) dur = g_interval;
      if ((uint32_t)(now - g_animLast) < dur) break;
      g_animLast += dur;
      g_animIndex = (uint8_t)((g_animIndex + 1) % g_animCount);
      guard++;
    }
    if (guard) {
      uint16_t dur = g_animDur[g_animIndex];
      if (dur < FRAME_INTERVAL_MIN) dur = g_interval;
      if ((uint32_t)(now - g_animLast) >= dur) g_animLast = now;
      blit(animAt(g_animIndex));
    }
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

static void showAnimationNow(uint32_t now) {
  g_kind = FRAME_ANIMATION;
  enterShow(now);
}

static void clearPicture() {
  memset(g_custom, 0, sizeof g_custom);
  g_animCount = 0;
  g_animIndex = 0;
  g_interval = FRAME_INTERVAL_DEFAULT;
  g_stored = FRAME_FIRE;
}

static void storeSingleFrame() {
  memcpy(g_anim, g_custom, MATRIX_N);
  g_animCount = 1;
  g_animDur[0] = g_interval;
  g_animIndex = 0;
  g_stored = FRAME_CUSTOM;
}

static bool applyKind(uint8_t kind, uint32_t now) {
  if (kind == FRAME_CUSTOM) {
    showCustomNow(now);
    return true;
  }
  if (kind == FRAME_ANIMATION) {
    if (g_stored != FRAME_ANIMATION || g_animCount < 1) return false;
    showAnimationNow(now);
    return true;
  }
  showFireNow(now);
  return true;
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

static bool jadd(char* b, size_t n, size_t* i, const char* s) {
  size_t k = strlen(s);
  if (*i + k >= n) return false;
  memcpy(b + *i, s, k);
  *i += k;
  b[*i] = 0;
  return true;
}

static bool jnum(char* b, size_t n, size_t* i, unsigned v) {
  char tmp[8];
  int k = snprintf(tmp, sizeof tmp, "%u", v);
  if (k < 0 || (size_t)k >= sizeof tmp) return false;
  return jadd(b, n, i, tmp);
}

static bool jframe(char* b, size_t n, size_t* i, const uint8_t* leds) {
  if (!jadd(b, n, i, "[")) return false;
  for (int y = 0; y < MATRIX_H; y++) {
    if (y && !jadd(b, n, i, ",")) return false;
    if (!jadd(b, n, i, "[")) return false;
    for (int x = 0; x < MATRIX_W; x++) {
      if (x && !jadd(b, n, i, ",")) return false;
      if (!jnum(b, n, i, leds[y * MATRIX_W + x])) return false;
    }
    if (!jadd(b, n, i, "]")) return false;
  }
  return jadd(b, n, i, "]");
}

static int buildState(char* b, size_t n, bool ok, const char* err, uint32_t now) {
  const char* mode = "fire";
  if (g_kind == FRAME_CUSTOM) mode = "custom";
  else if (g_kind == FRAME_ANIMATION) mode = "animation";
  const char* phase = (g_phase == PHASE_SHOW) ? "show" : "scroll";
  const char* wifi = (g_wifi == WIFI_ONLINE) ? "connected" :
                     (g_wifi == WIFI_JOINING) ? "joining" : "off";
  unsigned remaining = 0;
  if (g_phase == PHASE_SHOW) {
    uint32_t elapsed = now - g_phaseStart;
    remaining = (elapsed >= SHOW_MS) ? 0 : (unsigned)(SHOW_MS - elapsed);
  }
  unsigned frameIndex = (g_kind == FRAME_ANIMATION) ? g_animIndex : 0;
  const uint8_t* shown = g_custom;
  if (g_kind == FRAME_ANIMATION && g_animCount > 0) shown = animAt(g_animIndex);
  int w = snprintf(b, n,
    "{\"ok\":%s,\"error\":\"%s\",\"mode\":\"%s\",\"phase\":\"%s\","
    "\"wifi\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,\"remainingMs\":%u,"
    "\"frameCount\":%u,\"interval\":%u,\"frame\":%u,\"leds\":",
    ok ? "true" : "false",
    err ? err : "",
    mode, phase, wifi,
    (unsigned)g_ip[0], (unsigned)g_ip[1], (unsigned)g_ip[2], (unsigned)g_ip[3],
    (int)g_rssi, remaining,
    (unsigned)g_animCount, (unsigned)g_interval, frameIndex);
  if (w < 0 || (size_t)w >= n) return -1;
  size_t i = (size_t)w;
  if (!jframe(b, n, &i, shown)) return -1;
  if (!jadd(b, n, &i, ",\"frames\":[")) return -1;
  for (uint8_t f = 0; f < g_animCount; f++) {
    if (f && !jadd(b, n, &i, ",")) return -1;
    if (!jframe(b, n, &i, animAt(f))) return -1;
  }
  if (!jadd(b, n, &i, "],\"durations\":[")) return -1;
  for (uint8_t f = 0; f < g_animCount; f++) {
    if (f && !jadd(b, n, &i, ",")) return -1;
    if (!jnum(b, n, &i, g_animDur[f])) return -1;
  }
  if (!jadd(b, n, &i, "]}")) return -1;
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

static void handlePicture(const char* body, uint32_t now) {
  body = body ? body : "";
  if (frameFindTopKey(body, "frames")) {
    uint16_t durs[FRAME_ANIM_MAX];
    uint8_t count = 0;
    uint16_t interval = FRAME_INTERVAL_DEFAULT;
    const char* err = 0;
    // g_json is free until the response is built. Parser leaves it unchanged on failure.
    if (!frameParseAnimation(body, (uint8_t*)g_json, durs, &count, &interval, &err) || count < 1) {
      httpJson(false, err ? err : "frames", now);
      return;
    }
    memcpy(g_anim, g_json, (size_t)count * MATRIX_N);
    memcpy(g_animDur, durs, (size_t)count * sizeof(uint16_t));
    g_animCount = count;
    g_interval = interval;
    g_stored = FRAME_ANIMATION;
    memcpy(g_custom, g_anim, MATRIX_N);
    showAnimationNow(now);
    httpJson(true, 0, now);
    return;
  }
  bool pix = false;
  bool hasMode = false;
  uint8_t kind = FRAME_FIRE;
  const char* err = 0;
  uint8_t temp[MATRIX_N];
  memcpy(temp, g_custom, sizeof temp);
  if (!frameParseBody(body, temp, &pix, &hasMode, &kind, &err)) {
    httpJson(false, err ? err : "json", now);
    return;
  }
  if (pix) {
    memcpy(g_custom, temp, sizeof g_custom);
    storeSingleFrame();
    showCustomNow(now);
  } else if (hasMode) {
    if (!applyKind(kind, now)) {
      httpJson(false, "frames", now);
      return;
    }
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
  if ((!strcmp(path, "/api/frame") || !strcmp(path, "/api/animation")) &&
      !strcmp(method, "DELETE")) {
    clearPicture();
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
    if (!applyKind(kind, now)) {
      httpJson(false, "frames", now);
      return;
    }
    httpJson(true, 0, now);
    return;
  }
  if (!strcmp(path, "/api/frame") || !strcmp(path, "/api/leds") || !strcmp(path, "/api/animation")) {
    if (strcmp(method, "POST")) { httpJson(false, "method", now); return; }
    handlePicture(body, now);
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

// Core 1.6.0: status() is WL_CONNECTED once the radio associates. localIP()
// stays 0.0.0.0 until DHCP finishes, and a failed modem read looks the same.
static bool ipReady(const IPAddress& ip) {
  if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) return false;
  if (ip[0] == 0) return false;
  return true;
}

static void rememberIp(const IPAddress& ip) {
  g_ip[0] = ip[0];
  g_ip[1] = ip[1];
  g_ip[2] = ip[2];
  g_ip[3] = ip[3];
  buildIpText();
}

static void noteConnected() {
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
  Serial.println(F("http://matrixlamp.local/  and  http://matrix-lamp.local/"));
  Serial.println(F("DHCP name matrixlamp (http://matrixlamp/ if the router publishes it)."));
}

static void adoptIp(const IPAddress& ip) {
  bool changed = ip[0] != g_ip[0] || ip[1] != g_ip[1] || ip[2] != g_ip[2] || ip[3] != g_ip[3];
  if (changed) rememberIp(ip);
  if (g_wifi != WIFI_ONLINE) {
    g_wifi = WIFI_ONLINE;
    noteConnected();
    mdnsServiceArm(millis());
  } else if (changed) {
    Serial.print(F("WiFi IP "));
    Serial.print(g_ip[0]); Serial.print('.');
    Serial.print(g_ip[1]); Serial.print('.');
    Serial.print(g_ip[2]); Serial.print('.');
    Serial.println(g_ip[3]);
    mdnsServiceArm(millis());
  }
  if (!g_serverUp) {
    g_server.begin();
    g_serverUp = g_server ? 1 : 0;
  }
}

static void wifiDrop(uint32_t now) {
  Serial.println(F("WiFi dropped"));
  mdnsServiceStop();
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
    bool refresh = (g_wifi != WIFI_ONLINE) || g_ip[0] == 0 ||
                   (uint32_t)(now - g_lastIpPoll) >= WIFI_IP_POLL_MS;
    if (refresh) {
      g_lastIpPoll = now;
      IPAddress ip = WiFi.localIP();
      // Ignore a 0.0.0.0 read once we already have an address: localIP()
      // returns that on a modem timeout as well as before DHCP.
      if (ipReady(ip)) adoptIp(ip);
    }
    if (g_wifi == WIFI_ONLINE && g_ip[0] != 0) {
      if ((uint32_t)(now - g_lastRssi) >= 5000) {
        g_rssi = (int16_t)WiFi.RSSI();
        g_lastRssi = now;
      }
      if (!g_serverUp) {
        g_server.begin();
        g_serverUp = g_server ? 1 : 0;
      }
      return;
    }
  } else if (g_wifi == WIFI_ONLINE) {
    wifiDrop(now);
    return;
  }
  // Still joining, or associated with no address yet. Do not call begin()
  // again until WIFI_RETRY_MS, or the DHCP handshake is aborted.
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
  Serial.println(F("WiFi joining"));
  // WiFi.begin() waits while _timeout is the default 10 s, and only for
  // WL_CONNECTED — not for an address. modem.begin() restores a 10 s command
  // timeout; leave that in place for this first attempt. The matrix timer
  // is not running yet, so the wait does not fight the LED ISR.
  WiFi.status();
  WiFi.setHostname("matrixlamp");
  wifiBegin();
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    if (ipReady(ip)) adoptIp(ip);
  }
  WiFi.setTimeout(0);
  modem.timeout(MODEM_CMD_BUDGET_MS);
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
  if (g_wifi == WIFI_ONLINE && g_ip[0] != 0) {
    static const char* const kMdnsNames[] = {
      "matrixlamp.local",
      "matrix-lamp.local"
    };
    mdnsServicePoll(now, g_ip, kMdnsNames, 2);
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
  for (uint8_t i = 0; i < 40; i++) matrixFireStep(g_heat);
  // Radio first, then the 24 kHz matrix timer. The first WiFi.begin() can
  // block; later polls must not.
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
