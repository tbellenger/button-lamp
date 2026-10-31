/* ========= MatrixLamp_R4_Wifi =========
   Uno R4 WiFi plus a BTF-LIGHTING WS2812B 16x16 panel (256 LEDs).

   Data is D6, through a 330 ohm resistor (220–470 is fine), NEO_GRB at
   800 kHz. The panel runs from its own 5 V supply. The Arduino and the
   panel must share ground. A 1000 µF capacitor across the panel 5 V and
   GND, at the panel, takes the switch-on surge.

   The picture is the 16x16 panel: fire, a solid colour, or a rainbow.
   The board's 12x8 red matrix only scrolls the WiFi IP (or a waiting
   mark until DHCP has a real address). There is no frame upload and no
   animation buffer.

   Orientation, if the picture is mirrored or rotated, is the four
   PANEL_* settings below. They are the only thing to edit for that.

   Brightness starts at 20 of 255. BRIGHTNESS_CAP and POWER_BUDGET_MA
   keep a full-white frame inside a small supply. The page cannot raise
   brightness past that cap.

   WiFi is WiFiS3 plus secrets.h. The first WiFi.begin() in setup uses
   the library defaults and runs before the matrix timer starts. After
   that, WiFi.setTimeout(0) and modem.timeout(800) keep the loop bounded.
   status() can report WL_CONNECTED as soon as the radio associates,
   while localIP() is still 0.0.0.0, so the lamp stays offline until the
   address is non-zero. A new begin() is not sent for 22 s.
   mDNS answers matrixlamp.local and matrix-lamp.local (UDP 5353).
   DHCP hostname is matrixlamp.

   The page is the same shape as the spiral lamp's
   (sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI/): mode, brightness,
   speed, intensity, colour buttons. GET /api/state and GET /api/set.

   Host check for the fire:
     g++ -std=c++17 -DMATRIX_FIRE_TEST -x c++ matrix_fire.h -o /tmp/matrix_fire_test

   Core 1.6.0, Adafruit NeoPixel 1.15.5, empty WIFI_SSID (no secrets.h):
   85992 B flash (32% of 262144), 10900 B globals (33% of 32768),
   21868 B left for locals. NeoPixel then mallocs 768 B for 256 LEDs.
*/

#include <Arduino.h>
#include <WiFiS3.h>
#include <Modem.h>
#include <Adafruit_NeoPixel.h>
#include <stdio.h>
#include <string.h>
#include "Arduino_LED_Matrix.h"
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
// Clockwise quarter-turns of the logical picture. 0, 1, 2, or 3.
// The panel is square, so a 90 degree turn still maps every LED.
static const uint8_t PANEL_ROTATE = 0;

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

static const uint8_t MODE_OFF = 0;
static const uint8_t MODE_FIRE = 1;
static const uint8_t MODE_SOLID = 2;
static const uint8_t MODE_RAINBOW = 3;

static const uint8_t COL_RED = 0;
static const uint8_t COL_ORANGE = 1;
static const uint8_t COL_YELLOW = 2;
static const uint8_t COL_WHITE = 3;
static const uint8_t COL_GREEN = 4;
static const uint8_t COL_BLUE = 5;
static const uint8_t COL_PURPLE = 6;
static const uint8_t COL_COUNT = 7;

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

static_assert(PANEL_W == MATRIX_FIRE_W, "panel width must match the fire sim");
static_assert(PANEL_H == MATRIX_FIRE_H, "panel height must match the fire sim");
static_assert(PANEL_W == PANEL_H, "PANEL_ROTATE assumes a square panel");
static_assert(BRIGHTNESS_DEFAULT <= BRIGHTNESS_CAP, "default brightness is above the cap");

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
static uint8_t g_mode = MODE_FIRE;
static uint8_t g_resume = MODE_FIRE;
static uint8_t g_brightness = BRIGHTNESS_DEFAULT;
static uint8_t g_speed = 50;
static uint8_t g_intensity = 50;
static uint8_t g_color = COL_ORANGE;
static uint8_t g_hue = 0;
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
static char g_json[640];
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

static bool g_scrollTrack = false;
static int g_scrollX = -MATRIX_W;
static uint32_t g_scrollLast = 0;
static char g_ipText[16] = "";
static int g_ipCols = 0;

static const char PAGE[] = R"PAGE(<!DOCTYPE html>
<html lang=en><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Matrix Lamp</title>
<style>
body{margin:0;background:#14120f;color:#f3efe6;font:18px/1.35 system-ui,sans-serif}
main{max-width:28rem;margin:0 auto;padding:1rem 1rem 2.5rem}
h1{font-size:1.45rem;margin:0 0 .2rem}
label{display:block;margin:.85rem 0 .25rem;font-size:.85rem;color:#c8bba6}
select,button{font:inherit;background:#2a241c;color:#f3efe6;border:1px solid #5c4d38;border-radius:.6rem;padding:.7rem;width:100%;box-sizing:border-box}
button{margin:.3rem 0;min-height:2.9rem}
.row{display:flex;gap:.5rem}
.row button{flex:1;width:auto}
input[type=range]{width:100%;height:2rem;accent-color:#e07030}
pre{background:#0c0b09;border-radius:.6rem;padding:.7rem;white-space:pre-wrap;font:13px/1.45 ui-monospace,monospace}
.note{color:#b5a892;font-size:.85rem}
</style></head><body><main>
<h1>Matrix Lamp</h1>
<p class=note id=link>connecting</p>
<label for=mode>Mode</label>
<select id=mode>
<option value=off>Off</option>
<option value=fire>Fire</option>
<option value=solid>Solid colour</option>
<option value=rainbow>Rainbow</option>
</select>
<button type=button id=power>Turn off</button>
<label for=br>Brightness <span id=brv>20</span></label>
<input id=br type=range min=0 max=40 value=20>
<label for=sp>Speed <span id=spv>50</span></label>
<input id=sp type=range min=0 max=100 value=50>
<label for=fi>Fire intensity <span id=fiv>50</span></label>
<input id=fi type=range min=0 max=100 value=50>
<label>Solid colour</label>
<div class=row>
<button type=button data-c=red>Red</button>
<button type=button data-c=orange>Orange</button>
<button type=button data-c=yellow>Yellow</button>
<button type=button data-c=white>White</button>
</div>
<div class=row>
<button type=button data-c=green>Green</button>
<button type=button data-c=blue>Blue</button>
<button type=button data-c=purple>Purple</button>
</div>
<pre id=st>loading</pre>
<p class=note>Brightness is out of 255 and starts at 20, which is plenty on this panel. The slider stops at the cap (40, or lower if the power budget is tighter) so a USB port or a small 5 V supply is not asked for a full-white frame. Speed is the fire and the rainbow. Intensity is how tall and hot the fire is: 50 is the tuned flame, higher reaches further up the panel. The colour buttons switch to that solid colour. The board's 12×8 matrix scrolls the IP address. Serpentine, flip, and rotate are settings in the sketch, not on this page.</p>
<script>
var mode=document.getElementById('mode'),br=document.getElementById('br'),sp=document.getElementById('sp');
var fi=document.getElementById('fi'),st=document.getElementById('st'),brv=document.getElementById('brv');
var spv=document.getElementById('spv'),fiv=document.getElementById('fiv'),link=document.getElementById('link');
var power=document.getElementById('power');
var pending='',busy=0;
function send(q){pending=q;if(!busy)pump();}
function pump(){
  if(!pending)return;
  var q=pending;pending='';busy=1;
  fetch('/api/set?'+q).then(function(r){return r.json();}).then(show).catch(function(){}).then(function(){busy=0;pump();});
}
function show(s){
  if(!s||!s.mode)return;
  var ae=document.activeElement;
  if(ae!==mode)mode.value=s.mode;
  if(s.brightnessCap)br.max=s.brightnessCap;
  if(ae!==br)br.value=s.brightness;
  if(ae!==sp)sp.value=s.speed;
  if(ae!==fi)fi.value=s.intensity;
  brv.textContent=br.value;
  spv.textContent=sp.value;
  fiv.textContent=fi.value;
  power.textContent=(s.mode==='off')?'Turn on':'Turn off';
  link.textContent=s.wifi+'  '+s.ip+'   '+s.ledCount+' LEDs';
  st.textContent=
    'mode '+s.mode+'\n'+
    'brightness '+s.brightness+' / '+s.brightnessCap+'\n'+
    'speed '+s.speed+'   intensity '+s.intensity+'\n'+
    'colour '+s.color+'\n'+
    'wifi '+s.wifi+'   rssi '+s.rssi+'\n'+
    'panel '+s.width+'x'+s.height+'   serpentine '+(s.serpentine?'on':'off')+'\n'+
    'flipX '+(s.flipX?'on':'off')+'   flipY '+(s.flipY?'on':'off')+'   rotate '+s.rotate+'\n'+
    'cap '+s.brightnessCap+'   budget '+s.powerBudgetMa+' mA';
}
mode.onchange=function(){send('mode='+encodeURIComponent(mode.value));};
power.onclick=function(){send('power=toggle');};
function slide(el,key,label){
  var t=0,last=0;
  el.oninput=function(){
    label.textContent=el.value;
    var now=Date.now();
    if(now-last>200){last=now;send(key+'='+el.value);}
    clearTimeout(t);
    t=setTimeout(function(){last=Date.now();send(key+'='+el.value);},220);
  };
  el.onchange=function(){clearTimeout(t);send(key+'='+el.value);};
}
slide(br,'brightness',brv);
slide(sp,'speed',spv);
slide(fi,'intensity',fiv);
var colours=document.querySelectorAll('button[data-c]');
for(var ci=0;ci<colours.length;ci++){
  colours[ci].onclick=function(){send('color='+this.getAttribute('data-c')+'&mode=solid');};
}
function poll(){
  if(busy||pending)return;
  fetch('/api/state').then(function(r){return r.json();}).then(show).catch(function(){});
}
setInterval(poll,1000);
poll();
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

static void wheel(uint8_t pos, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (pos < 85) {
    *r = (uint8_t)(255 - pos * 3);
    *g = (uint8_t)(pos * 3);
    *b = 0;
  } else if (pos < 170) {
    pos = (uint8_t)(pos - 85);
    *r = 0;
    *g = (uint8_t)(255 - pos * 3);
    *b = (uint8_t)(pos * 3);
  } else {
    pos = (uint8_t)(pos - 170);
    *r = (uint8_t)(pos * 3);
    *g = 0;
    *b = (uint8_t)(255 - pos * 3);
  }
}

static void solidRgb(uint8_t which, uint8_t* r, uint8_t* g, uint8_t* b) {
  *r = 0;
  *g = 0;
  *b = 0;
  switch (which) {
    case COL_RED: *r = 255; break;
    case COL_ORANGE: *r = 255; *g = 80; break;
    case COL_YELLOW: *r = 255; *g = 180; break;
    case COL_WHITE: *r = 255; *g = 255; *b = 255; break;
    case COL_GREEN: *g = 255; break;
    case COL_BLUE: *b = 255; break;
    default: *r = 180; *b = 255; break;
  }
}

static const char* colorName(uint8_t which) {
  static const char* const N[] = {
    "red", "orange", "yellow", "white", "green", "blue", "purple"
  };
  if (which >= COL_COUNT) return "orange";
  return N[which];
}

static const char* modeName(uint8_t mode) {
  if (mode == MODE_FIRE) return "fire";
  if (mode == MODE_SOLID) return "solid";
  if (mode == MODE_RAINBOW) return "rainbow";
  return "off";
}

static void paintFire() {
  for (int y = 0; y < PANEL_H; y++) {
    for (int x = 0; x < PANEL_W; x++) {
      FireRgb c = firePalette(g_heat[y * PANEL_W + x]);
      g_panel.setPixelColor(panelIndex(x, y), c.r, c.g, c.b);
    }
  }
}

static void paintSolid() {
  uint8_t r, g, b;
  solidRgb(g_color, &r, &g, &b);
  for (int y = 0; y < PANEL_H; y++) {
    for (int x = 0; x < PANEL_W; x++) {
      g_panel.setPixelColor(panelIndex(x, y), r, g, b);
    }
  }
}

static void paintRainbow() {
  uint8_t step = (uint8_t)(3 + g_speed / 8);
  g_hue = (uint8_t)(g_hue + step);
  for (int y = 0; y < PANEL_H; y++) {
    for (int x = 0; x < PANEL_W; x++) {
      uint8_t r, g, b;
      wheel((uint8_t)(g_hue + (uint8_t)(x * 10) + (uint8_t)(y * 6)), &r, &g, &b);
      g_panel.setPixelColor(panelIndex(x, y), r, g, b);
    }
  }
}

static uint32_t stepPeriodMs() {
  // speed 0 -> 150 ms, speed 50 -> 85 ms, speed 100 -> 20 ms.
  return 20u + (uint32_t)(100 - g_speed) * 130u / 100u;
}

static void applyBrightness() {
  uint8_t cap = brightnessLimit();
  if (g_brightness > cap) g_brightness = cap;
  g_panel.setBrightness(g_brightness);
}

static void tickPanel(uint32_t now) {
  if (g_mode == MODE_OFF) {
    if (!g_dirty) return;
    applyBrightness();
    g_panel.clear();
    panelShow();
    g_dirty = 0;
    return;
  }
  if (g_mode == MODE_SOLID) {
    if (!g_dirty) return;
    applyBrightness();
    paintSolid();
    panelShow();
    g_dirty = 0;
    return;
  }
  uint32_t period = stepPeriodMs();
  if (!g_dirty && (uint32_t)(now - g_lastFrame) < period) return;
  g_lastFrame = now;
  applyBrightness();
  if (g_mode == MODE_FIRE) {
    matrixFireSetIntensity(g_intensity);
    matrixFireStep(g_heat);
    paintFire();
  } else {
    paintRainbow();
  }
  panelShow();
  g_dirty = 0;
}

static void setMode(uint8_t mode) {
  if (mode > MODE_RAINBOW) mode = MODE_OFF;
  if (mode == g_mode) return;
  if (mode != MODE_OFF) g_resume = mode;
  g_mode = mode;
  g_dirty = 1;
}

static bool modeFromName(const char* s, uint8_t* out) {
  if (!strcmp(s, "off")) { *out = MODE_OFF; return true; }
  if (!strcmp(s, "fire")) { *out = MODE_FIRE; return true; }
  if (!strcmp(s, "solid")) { *out = MODE_SOLID; return true; }
  if (!strcmp(s, "rainbow")) { *out = MODE_RAINBOW; return true; }
  return false;
}

static bool colorFromName(const char* s, uint8_t* out) {
  for (uint8_t i = 0; i < COL_COUNT; i++) {
    if (!strcmp(s, colorName(i))) { *out = i; return true; }
  }
  return false;
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

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool applyOne(const char* key, const char* val, const char** err) {
  if (!strcmp(key, "mode")) {
    uint8_t m;
    if (!modeFromName(val, &m)) { *err = "mode"; return false; }
    setMode(m);
    return true;
  }
  if (!strcmp(key, "brightness")) {
    int n;
    if (!parseU255(val, &n)) { *err = "brightness"; return false; }
    uint8_t cap = brightnessLimit();
    if (n > (int)cap) n = (int)cap;
    g_brightness = (uint8_t)n;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "speed")) {
    int n;
    if (!parseU100(val, &n)) { *err = "speed"; return false; }
    g_speed = (uint8_t)n;
    return true;
  }
  if (!strcmp(key, "intensity")) {
    int n;
    if (!parseU100(val, &n)) { *err = "intensity"; return false; }
    g_intensity = (uint8_t)n;
    matrixFireSetIntensity(g_intensity);
    return true;
  }
  if (!strcmp(key, "color") || !strcmp(key, "colour")) {
    if (!strcmp(val, "next")) {
      g_color = (uint8_t)((g_color + 1) % COL_COUNT);
    } else {
      uint8_t c;
      if (!colorFromName(val, &c)) { *err = "color"; return false; }
      g_color = c;
    }
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "power")) {
    int v;
    if (!strcmp(val, "toggle")) v = (g_mode == MODE_OFF) ? 1 : 0;
    else if (!strcmp(val, "0") || !strcmp(val, "off")) v = 0;
    else if (!strcmp(val, "1") || !strcmp(val, "on")) v = 1;
    else { *err = "power"; return false; }
    if (v) setMode(g_resume == MODE_OFF ? MODE_FIRE : g_resume);
    else setMode(MODE_OFF);
    return true;
  }
  *err = "key";
  return false;
}

static void applyQuery(const char* q, const char** err) {
  while (*q) {
    char key[16];
    char val[20];
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

static int buildState(char* b, size_t n, bool ok, const char* err) {
  const char* wifi = (g_wifi == WIFI_ONLINE) ? "connected" :
                     (g_wifi == WIFI_JOINING) ? "joining" : "off";
  uint8_t cap = brightnessLimit();
  uint8_t br = g_brightness;
  if (br > cap) br = cap;
  return snprintf(b, n,
    "{\"ok\":%s,\"error\":\"%s\",\"mode\":\"%s\",\"power\":%s,"
    "\"brightness\":%u,\"brightnessCap\":%u,"
    "\"speed\":%u,\"intensity\":%u,\"color\":\"%s\","
    "\"wifi\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,"
    "\"ledCount\":%u,\"width\":%u,\"height\":%u,"
    "\"serpentine\":%s,\"flipX\":%s,\"flipY\":%s,\"rotate\":%u,"
    "\"powerBudgetMa\":%u}",
    ok ? "true" : "false",
    err ? err : "",
    modeName(g_mode),
    (g_mode == MODE_OFF) ? "false" : "true",
    (unsigned)br,
    (unsigned)cap,
    (unsigned)g_speed,
    (unsigned)g_intensity,
    colorName(g_color),
    wifi,
    (unsigned)g_ip[0], (unsigned)g_ip[1], (unsigned)g_ip[2], (unsigned)g_ip[3],
    (int)g_rssi,
    (unsigned)PANEL_N,
    (unsigned)PANEL_W,
    (unsigned)PANEL_H,
    PANEL_SERPENTINE ? "true" : "false",
    PANEL_FLIP_X ? "true" : "false",
    PANEL_FLIP_Y ? "true" : "false",
    (unsigned)(PANEL_ROTATE & 3),
    (unsigned)POWER_BUDGET_MA);
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

static void httpStop() {
  g_client.stop();
  g_http = PH_IDLE;
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
    httpBegin(200, "OK", "text/html; charset=utf-8", PAGE, g_pageLen);
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
      }
    }
  }
  bool full = g_reqLen >= (sizeof g_req) - 1;
  bool hdr = g_reqLen > 0 && strstr(g_req, "\r\n\r\n") != 0;
  bool timed = (uint32_t)(now - g_clientMs) >= HTTP_READ_TIMEOUT_MS;
  if (hdr || full || timed) {
    if (g_reqLen == 0) {
      httpStop();
      return;
    }
    httpHandle();
  }
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
  httpStop();
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
  randomSeed((uint32_t)micros() ^ (uint32_t)analogRead(A0));
  matrixFireReset();
  matrixFireSetIntensity(g_intensity);
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
  tickScroll(now);
  tickPanel(now);
}
