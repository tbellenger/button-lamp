/* ========= MatrixLamp_R4_Wifi =========
   Uno R4 WiFi plus a BTF-LIGHTING WS2812B 16x16 panel (256 LEDs).

   Data is D6, through a 330 ohm resistor (220–470 is fine), NEO_GRB at
   800 kHz. The panel runs from its own 5 V supply. The Arduino and the
   panel must share ground. A 1000 µF capacitor across the panel 5 V and
   GND, at the panel, takes the switch-on surge.

   The picture is the 16x16 panel. The phone page has brightness, speed,
   and preset buttons. While Meatballs is showing, + Ball and - Ball
   change the count from 1 to 8.
   See matrix_modes.h for how each one sits on the grid.
   The board's 12x8 red matrix only scrolls the WiFi IP (or a waiting
   mark until DHCP has a real address). There is no frame upload and no
   animation buffer.

   Orientation, if the picture is mirrored or rotated, is the four
   PANEL_* settings below. They are the only thing to edit for that.

   Brightness starts at 20 of 255. BRIGHTNESS_CAP and POWER_BUDGET_MA
   keep a full-white frame inside a small supply. The page cannot raise
   brightness past that cap.

   WiFi is WiFiS3 plus secrets.h. Boot soft-resets the ESP32-S3 before
   the first WiFi.begin(). An AIRCR reboot does not reset that chip, and
   a leftover OK from the GitHub download makes the library's first
   SOFTRESETWIFI look done while the old port-80 socket stays open.
   The first begin() uses the library's 10 s timeout and runs before the
   matrix timer starts. After that, WiFi.setTimeout(0) and
   modem.timeout(800) keep the loop bounded.
   status() can report WL_CONNECTED as soon as the radio associates,
   while localIP() is still 0.0.0.0, so the lamp stays offline until the
   address is non-zero. A new begin() is not sent for 22 s.
   mDNS answers matrixlamp.local and matrix-lamp.local (UDP 5353).
   DHCP hostname is matrixlamp.

   GET /api/state and GET /api/set. balls=add and balls=remove change
   the Meatballs count. A number from 1 to 8 sets it.

   Host check for the fire:
     g++ -std=c++17 -DMATRIX_FIRE_TEST -x c++ matrix_fire.h -o /tmp/matrix_fire_test

   WiFi names and the OTA password live in data flash, not in the
   GitHub build. A USB build that has secrets.h copies them in when they
   differ. With neither, the board opens the setup AP MatrixLamp-Setup.
   POST /update (header X-OTA-Password) streams a .bin into ArduinoOTA
   InternalStorage. A bad or short image is not applied. The 16x16 shows
   an orange progress bar and the onboard matrix scan is stopped.

   Core 1.6.0, NeoPixel 1.15.5, ArduinoOTA 1.1.1, no secrets.h:
   114824 B flash (43% of 262144), 17032 B globals (51% of 32768),
   15736 B left for locals. The .bin is 114832 B. OTA allows at most
   122880 B. NeoPixel then mallocs 768 B for 256 LEDs.
*/

#include <Arduino.h>
#include <WiFiS3.h>
#define NO_OTA_NETWORK
#include <ArduinoOTA.h>
#include <Modem.h>
#include <Adafruit_NeoPixel.h>
#include <stdio.h>
#include <string.h>
#include "Arduino_LED_Matrix.h"
#include "matrix_fire.h"
#include "mdns_responder.h"

#if __has_include("fw_version.h")
#include "fw_version.h"
#endif
#ifndef FW_VERSION
#define FW_VERSION "local"
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#define LAMP_COMPILED_SECRETS 1
#else
#define LAMP_COMPILED_SECRETS 0
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

static const char FW_BUILT[] = __DATE__ " " __TIME__;
// Marker the upload scripts find in the .bin. Kept live by the boot print.
static const char FW_MARK[] = "MLFW:" FW_VERSION "|" __DATE__ " " __TIME__;

#include "lamp_creds.h"

// ---- Panel wiring. Edit these if the picture is the wrong way around. ----
// Data pin. Tom's panel is on D6 and answers to NEO_GRB + NEO_KHZ800.
static const uint8_t PANEL_PIN = 6;
static const int PANEL_W = 16;
static const int PANEL_H = 16;
static const int PANEL_N = PANEL_W * PANEL_H;

// Rows alternate direction, which is how these 16x16 panels are built.
// false: every row is left to right.
static const bool PANEL_SERPENTINE = true;
// Which rows run backwards when PANEL_SERPENTINE is true.
// true:  row 0 is left to right, row 1 is right to left (the usual one).
// false: row 0 is right to left, row 1 is left to right.
static const bool PANEL_SERPENTINE_ODD = true;
// Mirror after rotation, before the serpentine index.
static const bool PANEL_FLIP_X = false;
static const bool PANEL_FLIP_Y = false;
// Clockwise quarter-turns of the logical picture, applied before the flips
// and the serpentine index. 0, 1, 2, or 3. The panel is square, so a 90
// degree turn still maps every LED.
//
// The housing stands with +Y up. Rotate 1 put the ember row on the top
// edge of Tom's stand (flames fell from the ceiling). Another half turn,
// PANEL_ROTATE 3, puts that row on the desk. The same map is used for
// every effect, so the rainbow diagonal stays bottom-left to top-right.
static const uint8_t PANEL_ROTATE = 3;

// ---- Power. 20 of 255 is plenty on this panel. Do not ship it at 255. ----
// One WS2812B is about 60 mA at full white and brightness 255.
// 256 * 60 mA = 15360 mA at brightness 255.
// The cap is the lower of BRIGHTNESS_CAP and the budget:
//   budget brightness = POWER_BUDGET_MA * 255 / (PANEL_N * LED_MA_FULL)
// At 2500 mA that budget allows about 41, so BRIGHTNESS_CAP (40) wins.
// Full white at 40 is about 2.4 A. Full white at the default 20 is about 1.2 A.
// Fire is well under full white, so a USB port may still cope at 20, but a
// small 5 V supply is the intended source.
static const uint8_t BRIGHTNESS_DEFAULT = 20;
static const uint8_t BRIGHTNESS_CAP = 40;
static const uint16_t LED_MA_FULL = 60;
static const uint16_t POWER_BUDGET_MA = 2500;

static const int MATRIX_W = 12;
static const int MATRIX_H = 8;
static const int MATRIX_N = 96;
static const uint32_t SCROLL_MS = 80;
static const float MATRIX_TICK_HZ = 24000.0f;

static const uint32_t WIFI_ACCEPT_MS = 30;
static const uint32_t WIFI_STATUS_ONLINE_MS = 1000;
static const uint32_t WIFI_STATUS_JOIN_MS = 250;
static const uint32_t WIFI_RETRY_MS = 22000;
static const uint32_t WIFI_IP_POLL_MS = 3000;
static const int HTTP_READ_MAX = 160;
static const int HTTP_WRITE_MAX = 192;
static const uint32_t HTTP_READ_TIMEOUT_MS = 800;
static const uint32_t MODEM_CMD_BUDGET_MS = 800;

static const uint8_t WIFI_OFF = 0;
static const uint8_t WIFI_JOINING = 1;
static const uint8_t WIFI_ONLINE = 2;
static const uint8_t PH_IDLE = 0;
static const uint8_t PH_READ = 1;
static const uint8_t PH_WRITE = 2;
static const uint8_t PH_OTA = 3;

static_assert(PANEL_W == MATRIX_FIRE_W, "panel width must match the fire sim");
static_assert(PANEL_H == MATRIX_FIRE_H, "panel height must match the fire sim");
static_assert(PANEL_W == PANEL_H, "PANEL_ROTATE assumes a square panel");
static_assert(BRIGHTNESS_DEFAULT <= BRIGHTNESS_CAP, "default brightness is above the cap");

#include "matrix_modes.h"

// The matrix lamp has no knobs, faders, or buttons. The web page is the
// only control. Last preset, brightness and speed are kept in the data
// flash block after the WiFi credentials.

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

static Adafruit_NeoPixel g_panel(PANEL_N, PANEL_PIN, NEO_GRB + NEO_KHZ800);

static volatile uint8_t g_disp[MATRIX_N];
static uint8_t g_acc[MATRIX_N];
static volatile uint8_t g_scanLed = 0;
static volatile uint8_t g_lit = 255;
static uint8_t g_matrixOn = 0;
static FspTimer g_matrixTimer;

static uint8_t g_heat[MATRIX_FIRE_N];
static uint8_t g_mode = M_FIRE;
static uint8_t g_fader = 50;
static uint8_t g_speed = 50;
static uint8_t g_prefsDirty = 0;
static uint8_t g_dirty = 1;
static uint32_t g_lastFrame = 0;

static uint8_t g_wifi = WIFI_OFF;
static WiFiServer g_server(80);
static WiFiClient g_client;
static uint8_t g_http = PH_IDLE;
static char g_req[1024];
static uint16_t g_reqLen = 0;
static uint32_t g_clientMs = 0;
static char g_hdr[320];
static uint16_t g_hdrLen = 0;
static uint16_t g_hdrOff = 0;
static const char* g_body = 0;
static uint16_t g_bodyLen = 0;
static uint16_t g_bodyOff = 0;
static char g_json[1280];
static uint8_t g_fail = 0;
static uint16_t g_pageLen = 0;
static uint8_t g_otaActive = 0;
static uint8_t g_otaOpened = 0;
static uint8_t g_otaFail = 0;
static uint8_t g_otaApply = 0;
static uint8_t g_rebootAfter = 0;
static uint8_t g_ghJob = 0;
// GitHub fetch runs from loop, after the POST response has been flushed.
// httpStop clears g_ghJob, so the pending job lives here.
static uint8_t g_ghPend = 0;
static uint8_t g_wantCrc = 0;
static uint32_t g_crcExpect = 0;
static uint32_t g_crcGot = 0;
static uint32_t g_crcState = 0xffffffffu;
static uint32_t g_otaLen = 0;
static uint32_t g_otaGot = 0;
static uint8_t g_otaHead[8];
static uint8_t g_otaHeadN = 0;
static int g_otaPct = 0;
static int g_otaPaint = -1;
static const char* g_otaState = "idle";
static char g_otaMsg[40];
static char g_ghVer[24];
static char g_ghSha[48];
static char g_ghFile[40];
static uint32_t g_ghCrc = 0;
static uint8_t g_ghHave = 0;
static long g_ghSize = 0;
static char g_ghHost[64];
static char g_ghPath[1400];
static char g_manifest[480];
static int g_manifestN = 0;
static uint8_t g_ip[4] = {0, 0, 0, 0};
static int16_t g_rssi = 0;
static uint32_t g_lastStatus = 0;
static uint32_t g_lastAccept = 0;
static uint32_t g_lastRssi = 0;
static uint32_t g_lastIpPoll = 0;
static uint32_t g_joinSince = 0;
static uint8_t g_serverUp = 0;
static uint8_t g_serverMiss = 0;
static uint8_t g_joinTries = 0;

static bool g_scrollTrack = false;
static int g_scrollX = -MATRIX_W;
static uint32_t g_scrollLast = 0;
static char g_ipText[16] = "";
static int g_ipCols = 0;

static const char PAGE[] = R"PAGE(
<!DOCTYPE html>
<html lang=en><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Matrix Lamp</title>
<style>
body{margin:0;background:#16110d;color:#f4efe6;font:16px/1.35 system-ui,sans-serif}
main{max-width:26rem;margin:0 auto;padding:1rem 1rem 2rem}
h1{font-size:1.25rem;margin:0}
.sub{color:#cbbba6;font-size:.85rem;margin:.2rem 0 1rem}
label{display:block;margin:.85rem 0 .25rem;font-size:.75rem;letter-spacing:.08em;text-transform:uppercase;color:#cbbba6}
input[type=range]{width:100%;accent-color:#e07040}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:.45rem;margin:1rem 0}
button{font:inherit;color:inherit;background:#2a2118;border:1px solid #5c4630;border-radius:.65rem;padding:.7rem .35rem}
button.on{background:#c4552a;border-color:#f0c2a4}
details{margin-top:1rem;background:#120e0a;border-radius:.65rem;padding:.7rem .85rem}
summary{cursor:pointer;font-weight:650}
pre{white-space:pre-wrap;font:12px/1.4 ui-monospace,monospace;margin:.6rem 0 0}
input[type=password],input[type=file]{display:block;width:100%;margin:.4rem 0;box-sizing:border-box}
.row{display:flex;gap:.4rem;flex-wrap:wrap;margin-top:.4rem}
.row button{flex:1}
#balls{align-items:center;margin:.2rem 0 0}
#balls[hidden]{display:none}
#bcnt{flex:0 0 4.6rem;text-align:center}
button:disabled{opacity:.35}
</style></head><body><main>
<h1>Matrix Lamp</h1>
<p class=sub id=ver>version</p>
<label for=bri>Brightness</label>
<input id=bri type=range min=0 max=100 value=50>
<label for=spd>Speed</label>
<input id=spd type=range min=0 max=100 value=50>
<p class=sub id=spdnote></p>
<div class=grid id=grid></div>
<div class=row id=balls hidden>
<button type=button id=bmin>- Ball</button>
<span id=bcnt>3 of 8</span>
<button type=button id=badd>+ Ball</button>
</div>
<details><summary>Update</summary>
<p class=sub>On Spiral, Speed shifts the colour and the arm keeps a fixed spin. Everywhere else, Speed is how fast the preset moves.</p>
<input id=otap type=password placeholder="OTA password" autocomplete=current-password>
<input id=bin type=file accept=.bin>
<div class=row>
<button type=button id=chk>Check GitHub</button>
<button type=button id=gh>Update</button>
<button type=button id=up>Upload .bin</button>
</div>
<pre id=otast></pre>
</details>
<pre id=st></pre>
<script>
var NAMES=['fire','rainbow','metaballs','plasma','sun','spiral','fluid','heart'];
var LABELS={fire:'Fire',rainbow:'Rainbow',metaballs:'Meatballs',plasma:'Plasma',sun:'Sun',spiral:'Spiral',fluid:'Fluid',heart:'Heart'};
var S={mode:'fire',fader:50,speed:50,balls:3};
var linkBusy=0,drag=0;
var ver=document.getElementById('ver'),st=document.getElementById('st'),otast=document.getElementById('otast');
var grid=document.getElementById('grid');
var note=document.getElementById('spdnote');
function say(t){otast.textContent=t;}
function noteFor(m){
  note.textContent = m=='spiral' ? 'Speed shifts the spiral colour. Spin stays fixed.' : 'Speed is how fast this preset moves.';
}
function paintBtns(){
  var bs=grid.querySelectorAll('button');
  for(var i=0;i<bs.length;i++) bs[i].className = bs[i].dataset.m==S.mode ? 'on' : '';
}
function ballRow(){
  var box=document.getElementById('balls');
  if(S.mode=='metaballs') box.removeAttribute('hidden');
  else box.setAttribute('hidden','');
  document.getElementById('bcnt').textContent=S.balls+' of 8';
  document.getElementById('badd').disabled=S.balls>=8;
  document.getElementById('bmin').disabled=S.balls<=1;
}
NAMES.forEach(function(n){
  var b=document.createElement('button');
  b.type='button'; b.textContent=LABELS[n]; b.dataset.m=n;
  b.onclick=function(){S.mode=n; paintBtns(); noteFor(n); ballRow(); send('mode='+n);};
  grid.appendChild(b);
});
document.getElementById('badd').onclick=function(){send('balls=add');};
document.getElementById('bmin').onclick=function(){send('balls=remove');};
function send(q){
  fetch('/api/set?'+q).then(function(r){return r.json();}).then(show).catch(function(){});
}
function show(s){
  if(!s||!s.mode)return;
  if(!drag){
    S.mode=s.mode; S.fader=s.fader; S.speed=s.speed;
    if(s.balls) S.balls=s.balls;
    document.getElementById('bri').value=S.fader;
    document.getElementById('spd').value=S.speed;
    paintBtns(); noteFor(S.mode); ballRow();
  }
  ver.textContent='firmware '+s.fw+(s.built?('  '+s.built):'')+(s.ghVersion?('   release '+s.ghVersion+(s.ghSha?(' '+s.ghSha.slice(0,7)):'')):'');
  st.textContent=s.mode+'  bri '+s.brightness+'/'+s.brightnessCap+'  speed '+s.speed+'\n'+s.wifi+' '+s.ip;
  if(s.ota&&s.ota!='idle') say(s.ota+' '+(s.otaPct||0)+'% '+(s.otaMsg||'')+(s.ghVersion?('  '+s.ghVersion):''));
}
document.getElementById('bri').oninput=function(){drag=1; S.fader=+this.value;};
document.getElementById('bri').onchange=function(){drag=0; send('fader='+S.fader);};
document.getElementById('spd').oninput=function(){drag=1; S.speed=+this.value; noteFor(S.mode);};
document.getElementById('spd').onchange=function(){drag=0; send('speed='+S.speed);};
var otap=document.getElementById('otap');
try{otap.value=sessionStorage.getItem('otap')||'';}catch(e){}
otap.onchange=function(){try{sessionStorage.setItem('otap',otap.value);}catch(e){}};
function otaPost(u){fetch(u,{method:'POST',headers:{'X-OTA-Password':otap.value}}).catch(function(){say('no reply');});}
function watch(done,n){
  var i=0,t=setInterval(function(){
    fetch('/api/state').then(function(r){return r.json();}).then(function(s){show(s); if(done(s))clearInterval(t);}).catch(function(){});
    if(++i>n)clearInterval(t);
  },1000);
}
document.getElementById('chk').onclick=function(){
  say('checking'); otaPost('/api/ota/check');
  watch(function(s){return s.ota=='checked'||s.ota=='error';},40);
};
document.getElementById('gh').onclick=function(){
  say('fetching'); otaPost('/api/ota/apply');
  var stamp=ver.textContent;
  watch(function(s){
    if(s.ota=='error')return 1;
    if(ver.textContent!=stamp && s.fw){say('updated '+s.fw); setTimeout(function(){location.reload();},800); return 1;}
    return 0;
  },90);
};
document.getElementById('up').onclick=function(){
  var f=document.getElementById('bin').files[0]; if(!f){say('Choose a .bin');return;}
  var x=new XMLHttpRequest(); x.open('POST','/update');
  x.setRequestHeader('X-OTA-Password',otap.value);
  x.upload.onprogress=function(e){if(e.lengthComputable)say('send '+Math.round(100*e.loaded/e.total)+'%');};
  x.onload=function(){say(x.status+' '+(x.responseText||'')); if(x.status==200)setTimeout(function(){location.reload();},4000);};
  x.onerror=function(){say('closed before reply');};
  x.send(f);
};
function poll(){if(drag)return; fetch('/api/state').then(function(r){return r.json();}).then(show).catch(function(){});}
setInterval(poll,1000); poll();
</script>
</main></body></html>
)PAGE";

static uint8_t brightnessLimit() {
  uint32_t denom = (uint32_t)PANEL_N * (uint32_t)LED_MA_FULL;
  uint32_t fromBudget = 255;
  if (denom > 0) {
    fromBudget = ((uint32_t)POWER_BUDGET_MA * 255ul) / denom;
    if (fromBudget < 1) fromBudget = 1;
  }
  uint32_t cap = BRIGHTNESS_CAP;
  if (fromBudget < cap) cap = fromBudget;
  if (cap > 255) cap = 255;
  return (uint8_t)cap;
}

static uint16_t panelIndex(int x, int y) {
  uint8_t rot = (uint8_t)(PANEL_ROTATE & 3);
  if (rot == 1) {
    int nx = (PANEL_W - 1) - y;
    y = x;
    x = nx;
  } else if (rot == 2) {
    x = (PANEL_W - 1) - x;
    y = (PANEL_H - 1) - y;
  } else if (rot == 3) {
    int ny = (PANEL_H - 1) - x;
    x = y;
    y = ny;
  }
  if (PANEL_FLIP_X) x = (PANEL_W - 1) - x;
  if (PANEL_FLIP_Y) y = (PANEL_H - 1) - y;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= PANEL_W) x = PANEL_W - 1;
  if (y >= PANEL_H) y = PANEL_H - 1;
  if (PANEL_SERPENTINE) {
    bool odd = (y & 1) != 0;
    if (odd == PANEL_SERPENTINE_ODD) x = (PANEL_W - 1) - x;
  }
  return (uint16_t)(y * PANEL_W + x);
}

static void matrixIsr(timer_callback_args_t*) {
  uint8_t led = g_scanLed;
  uint8_t level = g_disp[led];
  uint8_t a = (uint8_t)(g_acc[led] + level);
  bool on = false;
  if (a >= 15) {
    a = (uint8_t)(a - 15);
    on = level > 0;
  }
  g_acc[led] = a;
  turnLed((int)led, on);
  g_lit = on ? led : (uint8_t)255;
  if (++led >= MATRIX_N) led = 0;
  g_scanLed = led;
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
  g_matrixOn = ok ? 1 : 0;
  if (!ok) Serial.println(F("Matrix timer failed"));
}

static void matrixPause() {
  if (!g_matrixOn) return;
  g_matrixTimer.stop();
  if (g_lit != 255) {
    turnLed((int)g_lit, false);
    g_lit = 255;
  }
}

static void matrixResume() {
  if (!g_matrixOn) return;
  g_matrixTimer.start();
}

static void panelShow() {
  matrixPause();
  g_panel.show();
  matrixResume();
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

static void tickScroll(uint32_t now) {
  if (g_wifi == WIFI_ONLINE && g_ip[0] != 0) {
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
      if (g_scrollX > g_ipCols) g_scrollTrack = false;
    }
    return;
  }
  g_scrollTrack = false;
  renderLink(now);
}

static uint8_t derivedMode() { return g_mode; }

static void setMode(uint8_t m) {
  if (m >= M_COUNT) m = M_FIRE;
  if (g_mode != m) g_prefsDirty = 1;
  g_mode = m;
  g_dirty = 1;
}

static uint8_t appliedBrightness() {
  uint8_t cap = brightnessLimit();
  return (uint8_t)((uint16_t)g_fader * cap / 100u);
}

static void applyBrightness() {
  g_panel.setBrightness(appliedBrightness());
}

static void fillMode(const ModeIn* in, uint32_t now) {
  for (int y = 0; y < PANEL_H; y++) {
    for (int x = 0; x < PANEL_W; x++) {
      uint8_t r, g, b;
      modesColor(in, x, y, now, &r, &g, &b);
      g_panel.setPixelColor(panelIndex(x, y), r, g, b);
    }
  }
}

static void paintFire() {
  for (int y = 0; y < PANEL_H; y++) {
    for (int x = 0; x < PANEL_W; x++) {
      FireRgb c = firePalette(g_heat[y * PANEL_W + x]);
      g_panel.setPixelColor(panelIndex(x, y), c.r, c.g, c.b);
    }
  }
}

static uint32_t stepPeriodMs() {
  return 20u + (uint32_t)(100 - g_speed) * 130u / 100u;
}

static void tickPanel(uint32_t now) {
  uint8_t mode = derivedMode();
  uint32_t period = stepPeriodMs();
  if (!g_dirty && (uint32_t)(now - g_lastFrame) < period) return;
  g_lastFrame = now;
  applyBrightness();
  if (mode == M_FIRE) {
    matrixFireSetIntensity(50);
    matrixFireStep(g_heat);
    paintFire();
  } else {
    ModeIn in;
    in.mode = mode;
    in.speed = g_speed;
    in.fader = g_fader;
    modesStep(&in, now);
    fillMode(&in, now);
  }
  panelShow();
  g_dirty = 0;
}

static bool parseU100(const char* v, int* out) {
  if (!v[0]) return false;
  int n = 0;
  for (const char* p = v; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    n = n * 10 + (*p - '0');
    if (n > 100) return false;
  }
  *out = n;
  return true;
}

static bool parseU255(const char* v, int* out) {
  if (!v[0]) return false;
  int n = 0;
  for (const char* p = v; *p; p++) {
    if (*p < '0' || *p > '9') return false;
    n = n * 10 + (*p - '0');
    if (n > 255) return false;
  }
  *out = n;
  return true;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool parseSwitch(const char* v, uint8_t* cur) {
  if (!strcmp(v, "1") || !strcmp(v, "on")) { *cur = 1; return true; }
  if (!strcmp(v, "0") || !strcmp(v, "off")) { *cur = 0; return true; }
  if (!strcmp(v, "toggle")) { *cur = *cur ? 0 : 1; return true; }
  return false;
}

static void setFaderFromLevel(int level) {
  uint8_t cap = brightnessLimit();
  if (cap < 1) cap = 1;
  if (level < 0) level = 0;
  if (level > (int)cap) level = (int)cap;
  int pct = (level * 100 + cap / 2) / (int)cap;
  if (pct > 100) pct = 100;
  if (g_fader != (uint8_t)pct) g_prefsDirty = 1;
  g_fader = (uint8_t)pct;
  g_dirty = 1;
}

static bool applyOne(const char* key, const char* val, const char** err) {
  if (!strcmp(key, "fader")) {
    int n;
    if (!parseU100(val, &n)) { *err = "fader"; return false; }
    if (g_fader != (uint8_t)n) g_prefsDirty = 1;
    g_fader = (uint8_t)n;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "brightness")) {
    int n;
    if (!parseU255(val, &n)) { *err = "brightness"; return false; }
    setFaderFromLevel(n);
    return true;
  }
  if (!strcmp(key, "speed")) {
    int n;
    if (!parseU100(val, &n)) { *err = "speed"; return false; }
    if (g_speed != (uint8_t)n) g_prefsDirty = 1;
    g_speed = (uint8_t)n;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "mode") || !strcmp(key, "preset")) {
    int m = modesFromName(val);
    if (m < 0) { *err = "mode"; return false; }
    setMode((uint8_t)m);
    return true;
  }
  if (!strcmp(key, "balls")) {
    uint8_t before = modesMetaCount();
    if (!strcmp(val, "add") || !strcmp(val, "+")) modesMetaAdd();
    else if (!strcmp(val, "remove") || !strcmp(val, "-")) modesMetaRemove();
    else {
      int n;
      if (!parseU100(val, &n) || n < (int)META_MIN || n > (int)META_MAX) {
        *err = "balls";
        return false;
      }
      modesMetaSet((uint8_t)n);
    }
    if (modesMetaCount() != before) g_prefsDirty = 1;
    g_dirty = 1;
    return true;
  }
  *err = "key";
  return false;
}

static int buildState(char* b, size_t n, bool ok, const char* err) {
  const char* wifi = (g_wifi == WIFI_ONLINE) ? "connected" :
                     (g_wifi == WIFI_JOINING) ? "joining" : "off";
  uint8_t cap = brightnessLimit();
  uint8_t level = appliedBrightness();
  return snprintf(b, n,
    "{\"ok\":%s,\"error\":\"%s\",\"mode\":\"%s\","
    "\"fader\":%u,\"brightness\":%u,\"brightnessCap\":%u,\"speed\":%u,"
    "\"balls\":%u,\"zoom\":%u,\"sun\":%u,"
    "\"wifi\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,"
    "\"ledCount\":%u,\"width\":%u,\"height\":%u,\"rotate\":%u,"
    "\"fw\":\"%s\",\"built\":\"%s\",\"ota\":\"%s\",\"otaPct\":%d,"
    "\"otaMsg\":\"%s\",\"ghVersion\":\"%s\",\"ghSha\":\"%s\",\"ghSize\":%ld}",
    ok ? "true" : "false",
    err ? err : "",
    modesName(g_mode),
    (unsigned)g_fader,
    (unsigned)level,
    (unsigned)cap,
    (unsigned)g_speed,
    (unsigned)modesMetaCount(),
    (unsigned)modesPlasmaZoomLevel(),
    (unsigned)modesSunSizeLevel(),
    wifi,
    (unsigned)g_ip[0], (unsigned)g_ip[1], (unsigned)g_ip[2], (unsigned)g_ip[3],
    (int)g_rssi,
    (unsigned)PANEL_N,
    (unsigned)PANEL_W,
    (unsigned)PANEL_H,
    (unsigned)(PANEL_ROTATE & 3),
    FW_VERSION, FW_BUILT, g_otaState, g_otaPct,
    g_otaMsg, g_ghVer, g_ghSha, g_ghSize);
}

static void applyQuery(const char* q, const char** err) {
  while (*q) {
    char key[16];
    char val[24];
    uint8_t ki = 0, vi = 0;
    bool trunc = false;
    while (*q && *q != '=' && *q != '&') {
      if (ki + 1 < sizeof key) key[ki++] = *q;
      else trunc = true;
      q++;
    }
    key[ki] = 0;
    if (*q == '=') q++;
    while (*q && *q != '&') {
      char c = *q++;
      if (c == '+') c = ' ';
      else if (c == '%' && q[0] && q[1]) {
        int hi = hexVal(q[0]);
        int lo = hexVal(q[1]);
        if (hi >= 0 && lo >= 0) {
          c = (char)((hi << 4) | lo);
          q += 2;
        }
      }
      if (vi + 1 < sizeof val) val[vi++] = c;
      else trunc = true;
    }
    val[vi] = 0;
    if (*q == '&') q++;
    if (!key[0]) continue;
    if (trunc) { *err = "key"; return; }
    if (!applyOne(key, val, err)) return;
  }
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

static void httpJson(bool ok, const char* err) {
  int n = buildState(g_json, sizeof g_json, ok, err);
  if (n < 0 || (size_t)n >= sizeof g_json) {
    n = snprintf(g_json, sizeof g_json, "{\"ok\":false,\"error\":\"full\"}");
    ok = false;
  }
  if (n < 0) n = 0;
  httpBegin(ok ? 200 : 400, ok ? "OK" : "Bad Request",
            "application/json", g_json, (uint16_t)n);
}

static void rememberIp(const IPAddress& ip);

#include "lamp_ota.h"

static void httpStop() {
  g_client.stop();
  g_http = PH_IDLE;
  g_ghJob = 0;
  g_otaApply = 0;
  g_rebootAfter = 0;
}

static void httpHandle() {
  if (strncmp(g_req, "GET ", 4) != 0) {
    httpJson(false, "method");
    return;
  }
  char path[48];
  char query[160];
  const char* p = g_req + 4;
  const char* end = strpbrk(p, " \r\n");
  if (!end) end = p + strlen(p);
  const char* qmark = 0;
  for (const char* s = p; s < end; s++) {
    if (*s == '?') { qmark = s; break; }
  }
  size_t plen = (size_t)((qmark ? qmark : end) - p);
  if (plen >= sizeof path) plen = sizeof path - 1;
  memcpy(path, p, plen);
  path[plen] = 0;
  query[0] = 0;
  if (qmark) {
    size_t qlen = (size_t)(end - (qmark + 1));
    if (qlen >= sizeof query) qlen = sizeof query - 1;
    memcpy(query, qmark + 1, qlen);
    query[qlen] = 0;
  }
  if (!strcmp(path, "/") || !strcmp(path, "/index.html")) {
    if (g_setupAp) {
      httpBegin(200, "OK", "text/html; charset=utf-8", SETUP_PAGE, (uint16_t)strlen(SETUP_PAGE));
    } else {
      httpBegin(200, "OK", "text/html; charset=utf-8", PAGE, g_pageLen);
    }
    return;
  }
  if (!strcmp(path, "/favicon.ico")) {
    httpBegin(204, "No Content", "text/plain", "", 0);
    return;
  }
  if (!strcmp(path, "/api/state")) {
    httpJson(true, 0);
    return;
  }
  if (!strcmp(path, "/api/set")) {
    const char* err = 0;
    applyQuery(query, &err);
    httpJson(err == 0, err);
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
      int n = g_client.read((uint8_t*)g_req + g_reqLen, (size_t)avail);
      if (n > 0) {
        g_reqLen = (uint16_t)(g_reqLen + n);
        g_req[g_reqLen] = 0;
        g_clientMs = now;
      }
    }
  }
  bool post = strncmp(g_req, "POST ", 5) == 0;
  bool hdr = g_reqLen > 0 && strstr(g_req, "\r\n\r\n") != 0;
  bool full = g_reqLen >= (sizeof g_req) - 1;
  bool timed = (uint32_t)(now - g_clientMs) >= HTTP_READ_TIMEOUT_MS;
  if (!hdr) {
    if (g_reqLen == 0 && timed) {
      httpStop();
      return;
    }
    if (post && full) {
      otaReply(413, "Payload Too Large", "headers\n");
      return;
    }
    if (post && timed) {
      otaReply(408, "Request Timeout", "timeout\n");
      return;
    }
    if (!post && (full || timed) && g_reqLen > 0) httpHandle();
    return;
  }
  if (post) {
    otaDispatchPost(now);
    return;
  }
  httpHandle();
}

static void httpWrite() {
  if (g_hdrOff < g_hdrLen) {
    uint16_t n = (uint16_t)(g_hdrLen - g_hdrOff);
    if (n > HTTP_WRITE_MAX) n = HTTP_WRITE_MAX;
    size_t w = g_client.write((const uint8_t*)g_hdr + g_hdrOff, n);
    if (w == 0) {
      if (++g_fail > 8) httpStop();
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
      if (++g_fail > 8) httpStop();
      return;
    }
    g_fail = 0;
    g_bodyOff = (uint16_t)(g_bodyOff + w);
    return;
  }
  uint8_t job = g_ghJob;
  uint8_t apply = g_otaApply;
  uint8_t reboot = g_rebootAfter;
  g_ghJob = 0;
  g_otaApply = 0;
  g_rebootAfter = 0;
  if (job || apply || reboot) {
    // The WiFi stack buffers the reply. Closing before flush makes the
    // phone see a dead socket and the page used to call that a reboot.
    modem.timeout(400);
    g_client.flush();
    delay(200);
    modem.timeout(MODEM_CMD_BUDGET_MS);
  }
  httpStop();
  if (apply) {
    if (!otaCommit()) {
      otaSet("error", "apply failed");
      otaSolid(180, 0, 0);
      delay(1000);
      otaResumeLamp();
    }
    return;
  }
  if (reboot) NVIC_SystemReset();
  if (job) g_ghPend = job;
}

static void wifiBegin() {
  if (g_creds.pass[0] == 0) WiFi.begin(g_creds.ssid);
  else WiFi.begin(g_creds.ssid, g_creds.pass);
  g_joinSince = millis();
}

// The ESP32-S3 stays up across an AIRCR reset of the RA4M1. modem.begin()
// sends one SOFTRESETWIFI and treats the first OK as success. After a
// GitHub download that OK is often still sitting in the UART, so the
// reset never happens and the old listen socket keeps port 80.
// Two further resets, then a firmware-version round trip, make the
// first OK be a real answer.
static bool wifiModemAnswers() {
  const char* v = WiFi.firmwareVersion();
  if (!v || v[0] == 0) return false;
  return strcmp(v, "99.99.99") != 0;
}

static void wifiResetModem() {
  Serial.println(F("WiFi modem reset"));
  for (uint8_t n = 0; n < 3; n++) {
    modem.beginned = false;
    modem.timeout(1000);
    modem.begin(115200, 1);
    delay(150);
    WiFi.end();
    delay(200);
    WiFi.end();
    delay(400);
    modem.timeout(1500);
    if (wifiModemAnswers()) {
      Serial.println(F("WiFi modem ready"));
      modem.timeout(MODEM_CMD_BUDGET_MS);
      return;
    }
    Serial.println(F("WiFi modem retry"));
    delay(200);
  }
  modem.timeout(MODEM_CMD_BUDGET_MS);
  Serial.println(F("WiFi modem silent"));
}

static void ensureServer() {
  if (g_serverUp) return;
  g_server.begin();
  g_serverUp = g_server ? 1 : 0;
  if (g_serverUp) g_serverMiss = 0;
  else if (g_serverMiss < 250) g_serverMiss++;
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
    g_joinTries = 0;
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
  ensureServer();
}

static void wifiDrop(uint32_t now) {
  Serial.println(F("WiFi dropped"));
  mdnsServiceStop();
  g_ip[0] = g_ip[1] = g_ip[2] = g_ip[3] = 0;
  g_rssi = 0;
  g_wifi = WIFI_JOINING;
  g_server.end();
  g_serverUp = 0;
  if (g_http != PH_IDLE) httpStop();
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
      ensureServer();
      // Port 80 still held by a socket from before the reset: begin()
      // keeps failing. Reset the modem and join again.
      if (!g_serverUp && g_serverMiss >= 3) {
        g_serverMiss = 0;
        wifiResetModem();
        wifiDrop(now);
      }
      return;
    }
  } else if (g_wifi == WIFI_ONLINE) {
    wifiDrop(now);
    return;
  }
  // Still joining, or associated with no address yet. Do not call begin()
  // again until WIFI_RETRY_MS, or the DHCP handshake is aborted.
  // Every second miss also resets the modem, in case the UART is wedged.
  if ((uint32_t)(now - g_joinSince) >= WIFI_RETRY_MS) {
    g_joinTries++;
    if ((g_joinTries & 1u) == 0) wifiResetModem();
    wifiBegin();
  }
}

static void webSetup() {
  size_t n = strlen(PAGE);
  g_pageLen = (n > 65535u) ? 65535u : (uint16_t)n;
  credsBoot();
  if (g_creds.ssid[0] == 0 && !g_setupAp) {
    g_wifi = WIFI_OFF;
    Serial.println(F("WiFi skipped (no credentials in EEPROM or secrets.h)"));
    return;
  }
  // Before the matrix timer, so the modem round trips can block.
  wifiResetModem();
  if (g_setupAp) {
    startSetupAp();
    WiFi.setTimeout(0);
    modem.timeout(MODEM_CMD_BUDGET_MS);
    return;
  }
  g_wifi = WIFI_JOINING;
  Serial.println(F("WiFi joining"));
  // WiFi.begin() waits while _timeout is the library default 10 s, and
  // only for WL_CONNECTED — not for an address. The matrix timer is not
  // running yet, so the wait does not fight the LED ISR.
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
  if (g_http == PH_OTA) { otaHttpRead(now); return; }
  if (g_http == PH_READ) { httpRead(now); return; }
  if (g_http == PH_WRITE) { httpWrite(); return; }
  if (g_setupAp) {
    if (g_serverUp && (uint32_t)(now - g_lastAccept) >= WIFI_ACCEPT_MS) {
      g_lastAccept = now;
      WiFiClient incoming = g_server.available();
      if (incoming) {
        g_client = incoming;
        g_reqLen = 0;
        g_req[0] = 0;
        g_clientMs = now;
        g_http = PH_READ;
      }
    }
    return;
  }
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

static void loadPrefs() {
  uint8_t mode = M_FIRE, fader = 50, speed = 50, balls = 0;
  if (!prefsLoad(&mode, &fader, &speed, &balls)) return;
  if (mode < M_COUNT) g_mode = mode;
  if (fader <= 100) g_fader = fader;
  if (speed <= 100) g_speed = speed;
  if (balls >= META_MIN && balls <= META_MAX) modesMetaSet(balls);
  Serial.print(F("Prefs "));
  Serial.print(modesName(g_mode));
  Serial.print(F(" bri "));
  Serial.print(g_fader);
  Serial.print(F(" speed "));
  Serial.print(g_speed);
  Serial.print(F(" balls "));
  Serial.println(modesMetaCount());
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Matrix Lamp R4 WiFi starting"));
  Serial.println(FW_MARK);
  Serial.print(F("OTA max bytes "));
  Serial.println(InternalStorage.maxSize());
  Serial.print(F("Panel "));
  Serial.print(PANEL_W);
  Serial.print(F("x"));
  Serial.print(PANEL_H);
  Serial.print(F(" on D"));
  Serial.print(PANEL_PIN);
  Serial.print(F(", brightness "));
  Serial.print(BRIGHTNESS_DEFAULT);
  Serial.print(F(" cap "));
  Serial.println(brightnessLimit());
  randomSeed((uint32_t)micros());
  modesBoot();
  loadPrefs();
  matrixFireReset();
  matrixFireSetIntensity(50);
  memset(g_heat, 0, sizeof g_heat);
  for (uint8_t i = 0; i < 40; i++) matrixFireStep(g_heat);
  webSetup();
  g_panel.begin();
  applyBrightness();
  g_panel.clear();
  g_panel.show();
  matrixBegin();
  g_scrollTrack = false;
  g_lastFrame = 0;
  g_dirty = 1;
}

void loop() {
  uint32_t now = millis();
  webPoll(now);
  if (g_prefsDirty && g_http == PH_IDLE && !g_otaActive && !g_ghPend) {
    g_prefsDirty = 0;
    if (!prefsSave(g_mode, g_fader, g_speed, modesMetaCount())) {
      Serial.println(F("Prefs save failed"));
    }
  }
  if (g_ghPend && g_http == PH_IDLE && !g_otaActive) {
    uint8_t job = g_ghPend;
    g_ghPend = 0;
    githubRun(job);
  }
  if (!g_otaActive) {
    tickScroll(now);
    tickPanel(now);
  }
}
