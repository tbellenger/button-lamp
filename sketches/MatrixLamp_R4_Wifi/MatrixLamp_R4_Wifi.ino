/* ========= MatrixLamp_R4_Wifi =========
   Uno R4 WiFi plus a BTF-LIGHTING WS2812B 16x16 panel (256 LEDs).

   Data is D6, through a 330 ohm resistor (220–470 is fine), NEO_GRB at
   800 kHz. The panel runs from its own 5 V supply. The Arduino and the
   panel must share ground. A 1000 µF capacitor across the panel 5 V and
   GND, at the panel, takes the switch-on surge.

   The picture is the 16x16 panel. The page is the spiral lamp's physical
   panel (rocker, toggles, arcade buttons, key, mushroom, fader, knob).
   Switches at rest play the matrix fire. The other positions play the
   spiral modes, plus Metaballs (rrllrrll), Plasma (llrrllrr), and Sun
   (rrrllrrr) while idle.
   See matrix_modes.h for how each one sits on the grid.
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
   110596 B flash (42% of 262144), 11544 B globals (35% of 32768),
   21224 B left for locals. NeoPixel then mallocs 768 B for 256 LEDs.
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

static const uint8_t MODE_FIRE = 0;
static const uint8_t MODE_JUMBO = 1;
static const uint8_t MODE_CLASSIC = 2;
static const uint8_t MODE_RAINBOW = 3;
static const uint8_t MODE_WAVE = 4;
static const uint8_t MODE_WAVE_MOD = 5;
static const uint8_t MODE_WAVE_RAINBOW = 6;
static const uint8_t MODE_WAVE_EPIC = 7;
static const uint8_t MODE_SECRET = 8;
static const uint8_t MODE_LAVA = 9;
static const uint8_t MODE_MUSIC = 10;
static const uint8_t MODE_METABALLS = 11;
static const uint8_t MODE_PLASMA = 12;
static const uint8_t MODE_SUN = 13;

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

#include "matrix_modes.h"

static_assert(MODE_METABALLS == M_METABALLS, "metaballs mode id");
static_assert(MODE_PLASMA == M_PLASMA, "plasma mode id");
static_assert(MODE_SUN == M_SUN, "sun mode id");

// Arcade buttons, same pins as the spiral lamp. Active low, internal pullup.
// Unconnected pins stay high, so only a real press is seen.
static const uint8_t PIN_RED = 8;
static const uint8_t PIN_WHITE = 4;
static const uint32_t ARCADE_DB_MS = 20;

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
static uint8_t g_rocker = 0;
static uint8_t g_toggle = 0;
static uint8_t g_keySw = 0;
static uint8_t g_wave = 0;
static uint8_t g_chaos = 0;
static uint8_t g_classic = 0;
static uint8_t g_top = 0;
static uint8_t g_bot = 0;
static uint8_t g_fireTrail = 1;
static uint8_t g_fader = 50;
static uint8_t g_speed = 50;
static char g_seq[8];
static uint8_t g_seqN = 0;
static char g_metaSeq[8];
static uint8_t g_metaSeqN = 0;
static uint8_t g_meta = 0;
static uint8_t g_plasma = 0;
static uint8_t g_sun = 0;
struct ArcadeBtn {
  uint8_t pin;
  uint8_t stable;
  uint8_t raw;
  uint32_t since;
};
static ArcadeBtn g_btnRed = {PIN_RED, HIGH, HIGH, 0};
static ArcadeBtn g_btnWhite = {PIN_WHITE, HIGH, HIGH, 0};
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
static char g_json[1024];
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

static const char PAGE[] = R"PAGE(
<!DOCTYPE html>
<html lang=en><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Matrix Lamp</title>
<style>
body{margin:0;background:#241910;color:#f3efe6;font:16px/1.35 system-ui,sans-serif}
main{max-width:72rem;margin:0 auto;padding:1rem 1rem 2rem}
.stage{display:flex;flex-direction:column;gap:.9rem;align-items:stretch}
.panelcol{min-width:0}
.presets{min-width:0}
@media (min-width:760px){
.stage{flex-direction:row;align-items:flex-start}
.presets{order:-1;flex:0 0 320px;width:320px;max-height:calc(100vh - 6.5rem);overflow:auto}
.panelcol{flex:1 1 auto}
}
h1{font-size:1.15rem;font-weight:650;letter-spacing:.04em;margin:0 0 .15rem}
.note{color:#cbbba6;font-size:.82rem;margin:.35rem 0 .7rem}
pre{background:#1a120c;border-radius:.5rem;padding:.6rem .7rem;white-space:pre-wrap;font:12px/1.4 ui-monospace,monospace;margin:.8rem 0}
.wood{position:relative;width:100%;aspect-ratio:184.15/133.35;border-radius:10px;
 background:
  repeating-linear-gradient(90deg,rgba(90,55,25,.08) 0 2px,transparent 2px 7px),
  linear-gradient(180deg,#e0c48a,#c4965a 40%,#b8894c 70%,#a8753d);
 box-shadow:inset 0 0 0 3px #8a5a32,inset 0 0 40px rgba(80,40,10,.25),0 14px 30px rgba(0,0,0,.45);
 touch-action:none;user-select:none}
.screw{position:absolute;width:2.2%;aspect-ratio:1;border-radius:50%;transform:translate(-50%,-50%);
 background:radial-gradient(circle at 35% 30%,#ddd,#888 45%,#444);box-shadow:0 1px 2px rgba(0,0,0,.5)}
.screw:after{content:"";position:absolute;left:18%;right:18%;top:46%;height:8%;background:#333;transform:rotate(35deg)}
button{font:inherit;color:inherit;border:0;padding:0;background:transparent;cursor:pointer}
.ctrl{position:absolute;transform:translate(-50%,-50%)}
.tag{position:absolute;left:50%;top:108%;transform:translateX(-50%);white-space:nowrap;
 font-size:clamp(8px,1.7vw,11px);letter-spacing:.08em;text-transform:uppercase;color:rgba(55,32,12,.8);
 text-shadow:0 1px 0 rgba(255,230,190,.35);pointer-events:none}
.arcade{width:15.2%;aspect-ratio:1}
.arcade b{display:block;width:100%;height:100%;border-radius:50%}
.arcade.white b{background:radial-gradient(circle at 34% 28%,#fff,#e6e6e6 42%,#b9b9b9 70%,#7d7d7d);
 box-shadow:0 .45em 0 #6a6a6a,0 .7em 1em rgba(0,0,0,.35),inset 0 -.4em .6em rgba(0,0,0,.12)}
.arcade.red b{background:radial-gradient(circle at 34% 28%,#ff8a80,#e53935 40%,#b71c1c 72%,#6d1010);
 box-shadow:0 .45em 0 #5c1212,0 .7em 1em rgba(0,0,0,.35),inset 0 -.35em .5em rgba(0,0,0,.2)}
.arcade.down b{transform:translateY(.28em);box-shadow:0 .12em 0 #444,0 .2em .4em rgba(0,0,0,.3)}
.key{width:12.1%;aspect-ratio:1;border-radius:50%;
 background:radial-gradient(circle at 35% 30%,#f2f3f5,#b7bcc2 42%,#6e747a 75%,#3e4348);
 box-shadow:0 .35em 0 #3a3a3a,0 .55em .8em rgba(0,0,0,.35),inset 0 0 0 .35em #8d9399}
.key i{position:absolute;left:46%;top:28%;width:8%;height:44%;background:#161616;border-radius:2px;transition:transform .16s}
.key.on i{transform:rotate(90deg)}
.mush{width:16%;aspect-ratio:1}
.mush .col{position:absolute;left:28%;right:28%;bottom:0;height:28%;background:#1b1b1b;border-radius:3px}
.mush b{position:absolute;left:0;right:0;top:0;bottom:18%;border-radius:50%;
 background:radial-gradient(circle at 32% 28%,#ff7b7b,#d32f2f 46%,#8f1515 78%,#5c0d0d);
 box-shadow:inset 0 -.4em .5em rgba(0,0,0,.25),0 .4em .6em rgba(0,0,0,.35);transition:transform .12s}
.mush.on b{transform:translateY(12%) scaleY(.82)}
.tog{width:8.5%;aspect-ratio:1;border-radius:50%;
 background:radial-gradient(circle at 40% 35%,#e4e7ea,#9aa0a6 55%,#5e646a);
 box-shadow:0 .25em 0 #3a3a3a,inset 0 0 0 .18em #6c7278}
.tog i{position:absolute;left:42%;top:-55%;width:16%;height:130%;background:linear-gradient(#f5f5f5,#9aa0a6);border-radius:2px;
 transform-origin:50% 78%;transform:rotate(32deg);transition:transform .12s;box-shadow:1px 1px 0 #333}
.tog.on i{transform:rotate(-32deg)}
.rock{width:12%;height:30%;border-radius:5px;background:#141414;
 box-shadow:inset 0 0 0 3px #0a0a0a,0 3px 8px rgba(0,0,0,.4)}
.rock i{position:absolute;left:12%;right:12%;height:46%;border-radius:3px;
 background:linear-gradient(#4a4a4a,#161616);top:50%;transition:top .12s;box-shadow:0 2px 0 #000}
.rock.on i{top:5%}
.rock .led{position:absolute;left:42%;top:8%;width:16%;aspect-ratio:1;border-radius:50%;background:#3a1a1a}
.rock.on .led{background:#ff2a2a;box-shadow:0 0 6px #ff2a2a}
.cov{width:14%;height:26%}
.cov .lid{position:absolute;left:0;right:0;top:0;bottom:0;border-radius:3px 3px 6px 6px;
 background:linear-gradient(#e24b4b,#9d1c1c);box-shadow:0 4px 0 #5c1010,0 6px 8px rgba(0,0,0,.3);
 transform-origin:50% 0;transition:transform .18s;z-index:2}
.cov.open .lid{transform:rotateX(68deg)}
.cov .tog,.cov .lid{pointer-events:none}
.cov .tog{position:absolute;left:50%;top:58%;width:46%}
.knob{width:12.5%;aspect-ratio:1;border-radius:50%;
 background:radial-gradient(circle at 35% 30%,#3c3c3c,#141414 60%,#000);
 box-shadow:0 .55em 0 #1a120c,0 .8em 1em rgba(0,0,0,.4)}
.knob i{position:absolute;left:46%;top:10%;width:8%;height:34%;background:#f2f2f2;border-radius:2px;transform-origin:50% 120%}
.fade{width:62%;height:16%}
.fade .slot{position:absolute;left:0;right:0;top:46%;height:16%;background:#2b2118;border-radius:99px;box-shadow:inset 0 2px 3px rgba(0,0,0,.6)}
.fade .screw{width:3.6%;top:54%}
.fade .thumb{position:absolute;top:18%;width:12%;height:64%;margin-left:-6%;border-radius:3px;
 background:linear-gradient(#3a3a3a,#111);box-shadow:0 3px 0 #000,0 4px 8px rgba(0,0,0,.45)}
.fade .thumb i{display:block;height:2px;margin:46% 15% 0;background:#eee}
details{background:#1a120c;border:1px solid #5c4630;border-radius:.55rem;margin:.35rem 0}
summary{padding:.7rem .85rem;cursor:pointer;font-weight:650}
details ol{margin:.15rem .9rem .75rem;padding-left:1.15rem;color:#e7d8c6;font-size:.92rem}
details li{margin:.28rem 0}
.w,.r{display:inline-block;padding:.02rem .42rem;border-radius:99px;font-weight:700;font-size:.84em;line-height:1.45}
.w{background:#f4f4f4;color:#222}
.r{background:#c62828;color:#fff}
</style></head><body><main>
<h1>Matrix Lamp</h1>
<p class=note id=link>connecting</p>
<div class=stage>
<div class=panelcol>
<div class=wood id=face>
<i class=screw style="left:3.4%;top:4.8%"></i>
<i class=screw style="left:96.6%;top:4.8%"></i>
<i class=screw style="left:3.4%;top:95.2%"></i>
<i class=screw style="left:96.6%;top:95.2%"></i>
<button type=button class="ctrl arcade white" id=white style="left:20.7%;top:21.4%"><b></b><span class=tag>white</span></button>
<button type=button class="ctrl key" id=key style="left:50%;top:19%"><i></i><span class=tag>key</span></button>
<button type=button class="ctrl mush" id=chaos style="left:79.3%;top:21.4%"><i class=col></i><b></b><span class=tag>chaos</span></button>
<button type=button class="ctrl arcade red" id=red style="left:13.8%;top:59.5%"><b></b><span class=tag>red</span></button>
<button type=button class="ctrl tog" id=toggle style="left:32.8%;top:59.5%"><i></i><span class=tag>toggle</span></button>
<button type=button class="ctrl rock" id=rocker style="left:50%;top:59.5%"><i class=led></i><i></i><span class=tag>rocker</span></button>
<div class="ctrl cov" id=cov style="left:67.2%;top:59.5%">
<button type=button class="tog" id=wave><i></i></button>
<button type=button class=lid id=lid></button>
<span class=tag>wave</span>
</div>
<button type=button class="ctrl knob" id=knob style="left:86.2%;top:59.5%"><i></i><span class=tag>speed</span></button>
<div class="ctrl fade" id=fade style="left:50%;top:89.9%">
<i class=slot></i>
<i class=screw style="left:0"></i>
<i class=screw style="left:100%"></i>
<b class=thumb id=thumb><i></i></b>
<span class=tag>brightness</span>
</div>
</div>
<pre id=st>loading</pre>
</div>
<div class=presets>
<p class=note>Presets. Tap a name. Fader halfway is brightness 20 of 255 (cap 40). Red is D8, white is D4, both active low. The mushroom overrides every other switch. The 12×8 matrix scrolls the IP.</p>
<details><summary>Fire</summary><ol>
<li>Turn the rocker, toggle, key, wave cover, and mushroom off.</li>
<li>If Metaballs, Plasma, or Sun is still showing, turn any one of those switches on, then off again. That clears the code.</li>
<li>The panel plays the matrix fire. The knob is the flame speed. The fader is brightness.</li>
</ol></details>
<details><summary>Jumbo</summary><ol>
<li>Mushroom off, key off, wave cover off.</li>
<li>Rocker on. If the picture is one flat colour, Classic is still armed: flip the toggle on once to clear it, then Jumbo is the rocker.</li>
<li>Toggle on cycles white, red, purple, blue, green, yellow, orange. Toggle off holds the colour.</li>
<li>The knob is the bottom-up wipe speed. The fader is brightness.</li>
</ol></details>
<details><summary>Classic</summary><ol>
<li>Mushroom off, key off, wave cover off, toggle off, rocker on.</li>
<li>Press <span class=w>white</span> <span class=r>red</span> <span class=w>white</span> <span class=w>white</span> <span class=r>red</span> <span class=w>white</span> <span class=r>red</span> <span class=r>red</span>.</li>
<li>The whole panel is one colour from the knob: left orange, middle yellow, right white. The fader is brightness.</li>
<li>Flip the toggle on to leave. That returns to Jumbo.</li>
</ol></details>
<details><summary>Rainbow</summary><ol>
<li>Mushroom off and wave cover off. The key wins over the rocker.</li>
<li>Turn the key on. The toggle reverses the rainbow.</li>
<li>The knob is speed. The fader is brightness.</li>
</ol></details>
<details><summary>Wave</summary><ol>
<li>Mushroom off, key off, toggle off. Leave the rocker off if you also want to avoid Secret.</li>
<li>Tap the red wave cover. It opens and the switch goes on.</li>
<li>A bright band ripples up the rows. The knob is speed. The fader is brightness.</li>
</ol></details>
<details><summary>Wave modifier</summary><ol>
<li>Mushroom off, key off, rocker off.</li>
<li>Toggle on, then tap the red wave cover so Wave is on.</li>
<li>The band crossfades a colour pair. The knob is speed. The fader is brightness.</li>
</ol></details>
<details><summary>Wave rainbow</summary><ol>
<li>Mushroom off, toggle off, rocker off.</li>
<li>Key on, and the red wave cover on.</li>
<li>Each row is a hue and the band still travels. The knob is speed. The fader is brightness.</li>
</ol></details>
<details><summary>Wave epic</summary><ol>
<li>Mushroom off and rocker off. Rocker on with these switches is Secret instead.</li>
<li>Key on, toggle on, and the red wave cover on.</li>
<li>One row flashes through the epic colours. The knob is speed. The fader is brightness.</li>
</ol></details>
<details><summary>Secret</summary><ol>
<li>Mushroom off.</li>
<li>Rocker on, toggle on, key on, and the red wave cover on. All four.</li>
<li>The knob fills the panel from the bottom. Low on the fader mixes in the violet-to-red gradient. The fader is also brightness.</li>
</ol></details>
<details><summary>Lava</summary><ol>
<li>Press the red mushroom. It overrides every other switch.</li>
<li><span class=w>White</span> cycles the top ball. <span class=r>Red</span> cycles the bottom ball: red, purple, blue, green, yellow, orange, off.</li>
<li>The fire trail is on at boot. There is no switch for it.</li>
<li>The knob is speed. The fader is brightness. Press the mushroom again to leave.</li>
</ol></details>
<details><summary>Music</summary><ol>
<li>Mushroom off, rocker off, key off, wave cover off.</li>
<li>Toggle on.</li>
<li>A dim pulse and a short built-in phrase. No MIDI on this lamp. The knob is speed. The fader is brightness.</li>
</ol></details>
<details><summary>Metaballs</summary><ol>
<li>Every switch off, so the fire is showing. These presses are not the Classic code.</li>
<li>Press <span class=w>white</span> <span class=w>white</span> <span class=r>red</span> <span class=r>red</span> <span class=w>white</span> <span class=w>white</span> <span class=r>red</span> <span class=r>red</span>.</li>
<li>After that, <span class=r>red</span> adds a blob, up to 8, and <span class=w>white</span> removes one, down to 1. Those presses are not another code.</li>
<li>The knob is blob speed. The fader is brightness. Turn any other switch on to leave.</li>
</ol></details>
<details><summary>Plasma</summary><ol>
<li>Every switch off.</li>
<li>Press <span class=r>red</span> <span class=r>red</span> <span class=w>white</span> <span class=w>white</span> <span class=r>red</span> <span class=r>red</span> <span class=w>white</span> <span class=w>white</span>.</li>
<li>After that, <span class=r>red</span> zooms in and <span class=w>white</span> zooms out, five steps, held at the ends. Not another code.</li>
<li>The knob is the rotate and zoom speed. The fader is brightness. Turn any other switch on to leave.</li>
</ol></details>
<details><summary>Sun</summary><ol>
<li>Every switch off.</li>
<li>Press <span class=w>white</span> <span class=w>white</span> <span class=w>white</span> <span class=r>red</span> <span class=r>red</span> <span class=w>white</span> <span class=w>white</span> <span class=w>white</span>.</li>
<li>After that, <span class=r>red</span> grows the sun and <span class=w>white</span> shrinks it, five steps, held at the ends. Not another code.</li>
<li>The knob is the pulse and ray speed. The fader is brightness. Turn any other switch on to leave.</li>
</ol></details>
</div>
</div>
<script>
var presets=document.querySelectorAll('details');
for(var pi=0;pi<presets.length;pi++){
  presets[pi].ontoggle=function(){
    if(!this.open)return;
    for(var pj=0;pj<presets.length;pj++) if(presets[pj]!==this) presets[pj].open=false;
  };
}
var S={rocker:0,toggle:0,key:0,wave:0,chaos:0,fader:50,speed:50,cover:0};
var link=document.getElementById('link'),st=document.getElementById('st');
var pending='',busy=0,drag=0;
function send(q){pending=q;if(!busy)pump();}
function pump(){
  if(!pending)return;
  var q=pending;pending='';busy=1;
  fetch('/api/set?'+q).then(function(r){return r.json();}).then(show).catch(function(){}).then(function(){busy=0;pump();});
}
function paint(){
  document.getElementById('rocker').className='ctrl rock'+(S.rocker?' on':'');
  document.getElementById('toggle').className='ctrl tog'+(S.toggle?' on':'');
  document.getElementById('key').className='ctrl key'+(S.key?' on':'');
  document.getElementById('chaos').className='ctrl mush'+(S.chaos?' on':'');
  document.getElementById('wave').className='tog'+(S.wave?' on':'');
  document.getElementById('cov').className='ctrl cov'+(S.cover?' open':'');
  var deg=-135+S.speed*270/100;
  document.querySelector('#knob i').style.transform='rotate('+deg+'deg)';
  document.getElementById('thumb').style.left=S.fader+'%';
}
function show(s){
  if(!s||!s.mode)return;
  if(!drag){
    S.rocker=s.rocker?1:0; S.toggle=s.toggle?1:0; S.key=s.key?1:0;
    S.wave=s.wave?1:0; S.chaos=s.chaos?1:0;
    S.cover=S.wave?1:0;
    S.fader=s.fader; S.speed=s.speed;
    paint();
  }
  link.textContent=s.wifi+'  '+s.ip+'   '+s.ledCount+' LEDs';
  st.textContent=
    'mode '+s.mode+(s.classic?' (classic armed)':'')+'\n'+
    'rocker '+(s.rocker?'on':'off')+'  toggle '+(s.toggle?'on':'off')+'\n'+
    'key '+(s.key?'on':'off')+'  wave '+(s.wave?'on':'off')+'  chaos '+(s.chaos?'on':'off')+'\n'+
    'brightness '+s.brightness+' / '+s.brightnessCap+'   fader '+s.fader+'%\n'+
    'speed '+s.speed+'   fire trail '+(s.fireTrail?'on':'off')+'\n'+
    (s.mode==='metaballs'?('balls '+s.balls+'  (red adds, white removes)\n'):'')+
    (s.mode==='plasma'?('zoom '+s.zoom+' / 4  (red zooms in, white zooms out)\n'):'')+
    (s.mode==='sun'?('sun '+s.sun+' / 4  (red grows, white shrinks)\n'):'')+
    'top '+s.topName+'   bottom '+s.bottomName+'\n'+
    'wifi '+s.wifi+'   rssi '+s.rssi;
}
function flip(name){
  S[name]=S[name]?0:1;
  if(name=='toggle'&&S.toggle){}
  paint();
  send(name+'='+(S[name]?'1':'0'));
}
document.getElementById('rocker').onclick=function(){flip('rocker');};
document.getElementById('toggle').onclick=function(){flip('toggle');};
document.getElementById('key').onclick=function(){flip('key');};
document.getElementById('chaos').onclick=function(){flip('chaos');};
document.getElementById('cov').onclick=function(){
  S.wave=S.wave?0:1;
  S.cover=S.wave?1:0;
  paint();
  send('wave='+(S.wave?'1':'0'));
};
function press(id,key){
  var el=document.getElementById(id);
  el.onpointerdown=function(){el.classList.add('down');};
  el.onpointerup=function(){el.classList.remove('down');send(key+'=press');};
  el.onpointercancel=function(){el.classList.remove('down');};
  el.onpointerleave=function(){el.classList.remove('down');};
}
press('white','white');
press('red','red');
function dragPointer(el,move,key){
  var down=0;
  el.onpointerdown=function(ev){
    down=1; drag=1;
    if(el.setPointerCapture)el.setPointerCapture(ev.pointerId);
    move(ev); paint();
  };
  el.onpointermove=function(ev){if(!down)return; move(ev); paint();};
  function end(){
    if(!down)return;
    down=0; drag=0;
    send(key+'='+(key=='speed'?S.speed:S.fader));
  }
  el.onpointerup=end; el.onpointercancel=end;
}
dragPointer(document.getElementById('knob'),function(ev){
  var r=document.getElementById('knob').getBoundingClientRect();
  var x=ev.clientX-(r.left+r.width/2), y=ev.clientY-(r.top+r.height/2);
  var a=Math.atan2(y,x)+Math.PI/2;
  if(a>Math.PI)a-=Math.PI*2;
  var deg=a*180/Math.PI;
  if(deg<-135)deg=-135; if(deg>135)deg=135;
  S.speed=Math.round((deg+135)/270*100);
},'speed');
dragPointer(document.getElementById('fade'),function(ev){
  var r=document.getElementById('fade').getBoundingClientRect();
  var t=(ev.clientX-r.left)/r.width;
  if(t<0)t=0; if(t>1)t=1;
  S.fader=Math.round(t*100);
},'fader');
function poll(){
  if(busy||pending||drag)return;
  fetch('/api/state').then(function(r){return r.json();}).then(show).catch(function(){});
}
paint();
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

static uint8_t derivedMode() {
  if (g_chaos) return MODE_LAVA;
  if (g_wave && g_toggle && g_rocker && g_keySw) return MODE_SECRET;
  if (g_wave) {
    if (g_keySw && g_toggle) return MODE_WAVE_EPIC;
    if (g_keySw) return MODE_WAVE_RAINBOW;
    if (g_toggle) return MODE_WAVE_MOD;
    return MODE_WAVE;
  }
  if (g_keySw) return MODE_RAINBOW;
  if (!g_rocker && g_toggle) return MODE_MUSIC;
  if (g_rocker && g_classic) return MODE_CLASSIC;
  if (g_rocker) return MODE_JUMBO;
  if (g_meta) return MODE_METABALLS;
  if (g_plasma) return MODE_PLASMA;
  if (g_sun) return MODE_SUN;
  return MODE_FIRE;
}

static const char* lavaName(uint8_t i) {
  static const char* const N[] = {
    "red", "purple", "blue", "green", "yellow", "orange", "off"
  };
  return N[i % 7];
}

static const char* modeName(uint8_t mode) {
  static const char* const N[] = {
    "fire", "jumbo", "classic", "rainbow", "wave", "wave-mod",
    "wave-rainbow", "wave-epic", "secret", "lava", "music", "metaballs",
    "plasma", "sun"
  };
  if (mode > MODE_SUN) return "fire";
  return N[mode];
}

static void pushSeq(char c) {
  if (g_chaos || g_keySw || g_wave || !g_rocker) return;
  if (g_seqN < 8) g_seq[g_seqN++] = c;
  else {
    memmove(g_seq, g_seq + 1, 7);
    g_seq[7] = c;
  }
  if (g_seqN == 8 && memcmp(g_seq, "rlrrlrll", 8) == 0) {
    if (!g_toggle) g_classic = 1;
    g_seqN = 0;
  }
}

// Idle-only codes. Classic owns the buttons while the rocker is on, so this
// buffer is separate. It is ignored once Metaballs or Plasma is showing:
// those presses zoom or add balls instead of spelling another code.
static void pushMeta(char c) {
  if (g_rocker || g_toggle || g_keySw || g_wave || g_chaos || g_meta || g_plasma || g_sun) return;
  if (g_metaSeqN < 8) g_metaSeq[g_metaSeqN++] = c;
  else {
    memmove(g_metaSeq, g_metaSeq + 1, 7);
    g_metaSeq[7] = c;
  }
  if (g_metaSeqN == 8 && memcmp(g_metaSeq, "rrllrrll", 8) == 0) {
    g_meta = 1;
    g_plasma = 0;
    g_sun = 0;
    g_metaSeqN = 0;
    Serial.print(F("Metaballs "));
    Serial.println(modesMetaCount());
  } else if (g_metaSeqN == 8 && memcmp(g_metaSeq, "llrrllrr", 8) == 0) {
    g_plasma = 1;
    g_meta = 0;
    g_sun = 0;
    g_metaSeqN = 0;
    Serial.print(F("Plasma zoom "));
    Serial.println(modesPlasmaZoomLevel());
  } else if (g_metaSeqN == 8 && memcmp(g_metaSeq, "rrrllrrr", 8) == 0) {
    g_sun = 1;
    g_meta = 0;
    g_plasma = 0;
    g_metaSeqN = 0;
    Serial.print(F("Sun size "));
    Serial.println(modesSunSizeLevel());
  }
}

static void leaveMeta() {
  g_meta = 0;
  g_plasma = 0;
  g_sun = 0;
  g_metaSeqN = 0;
}

static void onArcade(char c) {
  if (derivedMode() == MODE_METABALLS) {
    if (c == 'l') modesMetaAdd();
    else modesMetaRemove();
    Serial.print(F("Metaballs "));
    Serial.println(modesMetaCount());
    g_dirty = 1;
    return;
  }
  if (derivedMode() == MODE_PLASMA) {
    if (c == 'l') modesPlasmaZoom(1);
    else modesPlasmaZoom(-1);
    Serial.print(F("Plasma zoom "));
    Serial.println(modesPlasmaZoomLevel());
    g_dirty = 1;
    return;
  }
  if (derivedMode() == MODE_SUN) {
    if (c == 'l') modesSunSize(1);
    else modesSunSize(-1);
    Serial.print(F("Sun size "));
    Serial.println(modesSunSizeLevel());
    g_dirty = 1;
    return;
  }
  if (g_chaos) {
    if (c == 'r') g_top = (uint8_t)((g_top + 1) % 7);
    else g_bot = (uint8_t)((g_bot + 1) % 7);
  }
  pushSeq(c);
  pushMeta(c);
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
  if (mode == MODE_FIRE) {
    matrixFireSetIntensity(50);
    matrixFireStep(g_heat);
    paintFire();
  } else {
    ModeIn in;
    in.mode = mode;
    in.speed = g_speed;
    in.fader = g_fader;
    in.top = g_top;
    in.bottom = g_bot;
    in.fireTrail = g_fireTrail;
    in.toggle = g_toggle;
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
  g_fader = (uint8_t)pct;
  g_dirty = 1;
}

static bool applyOne(const char* key, const char* val, const char** err) {
  if (!strcmp(key, "rocker")) {
    uint8_t v = g_rocker;
    if (!parseSwitch(val, &v)) { *err = "rocker"; return false; }
    if (v && !g_rocker) leaveMeta();
    g_rocker = v;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "toggle")) {
    uint8_t v = g_toggle;
    if (!parseSwitch(val, &v)) { *err = "toggle"; return false; }
    if (v && !g_toggle) { g_classic = 0; leaveMeta(); }
    g_toggle = v;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "key")) {
    uint8_t v = g_keySw;
    if (!parseSwitch(val, &v)) { *err = "key"; return false; }
    if (v && !g_keySw) leaveMeta();
    g_keySw = v;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "wave")) {
    uint8_t v = g_wave;
    if (!parseSwitch(val, &v)) { *err = "wave"; return false; }
    if (v && !g_wave) leaveMeta();
    g_wave = v;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "chaos")) {
    uint8_t v = g_chaos;
    if (!parseSwitch(val, &v)) { *err = "chaos"; return false; }
    if (v && !g_chaos) leaveMeta();
    g_chaos = v;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "fader")) {
    int n;
    if (!parseU100(val, &n)) { *err = "fader"; return false; }
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
    g_speed = (uint8_t)n;
    return true;
  }
  if (!strcmp(key, "white") || !strcmp(key, "top")) {
    if (!strcmp(val, "1") || !strcmp(val, "next") || !strcmp(val, "press")) {
      onArcade('r');
      return true;
    }
    int n;
    if (!parseU100(val, &n) || n > 6) { *err = "top"; return false; }
    g_top = (uint8_t)n;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "red") || !strcmp(key, "bottom")) {
    if (!strcmp(val, "1") || !strcmp(val, "next") || !strcmp(val, "press")) {
      onArcade('l');
      return true;
    }
    int n;
    if (!parseU100(val, &n) || n > 6) { *err = "bottom"; return false; }
    g_bot = (uint8_t)n;
    g_dirty = 1;
    return true;
  }
  if (!strcmp(key, "fire")) {
    if (!strcmp(val, "toggle")) g_fireTrail = g_fireTrail ? 0 : 1;
    else if (!strcmp(val, "0")) g_fireTrail = 0;
    else if (!strcmp(val, "1")) g_fireTrail = 1;
    else { *err = "fire"; return false; }
    g_dirty = 1;
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
  uint8_t mode = derivedMode();
  uint8_t cap = brightnessLimit();
  uint8_t level = appliedBrightness();
  return snprintf(b, n,
    "{\"ok\":%s,\"error\":\"%s\",\"mode\":\"%s\","
    "\"rocker\":%s,\"toggle\":%s,\"key\":%s,\"wave\":%s,\"chaos\":%s,"
    "\"classic\":%s,\"fireTrail\":%s,"
    "\"fader\":%u,\"brightness\":%u,\"brightnessCap\":%u,\"speed\":%u,"
    "\"top\":%u,\"topName\":\"%s\",\"bottom\":%u,\"bottomName\":\"%s\","
    "\"balls\":%u,\"zoom\":%u,\"sun\":%u,"
    "\"wifi\":\"%s\",\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,"
    "\"ledCount\":%u,\"width\":%u,\"height\":%u,"
    "\"serpentine\":%s,\"flipX\":%s,\"flipY\":%s,\"rotate\":%u,"
    "\"powerBudgetMa\":%u}",
    ok ? "true" : "false",
    err ? err : "",
    modeName(mode),
    g_rocker ? "true" : "false",
    g_toggle ? "true" : "false",
    g_keySw ? "true" : "false",
    g_wave ? "true" : "false",
    g_chaos ? "true" : "false",
    g_classic ? "true" : "false",
    g_fireTrail ? "true" : "false",
    (unsigned)g_fader,
    (unsigned)level,
    (unsigned)cap,
    (unsigned)g_speed,
    (unsigned)g_top, lavaName(g_top),
    (unsigned)g_bot, lavaName(g_bot),
    (unsigned)modesMetaCount(),
    (unsigned)modesPlasmaZoomLevel(),
    (unsigned)modesSunSizeLevel(),
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

static uint8_t arcadeFell(ArcadeBtn* b, uint32_t now) {
  uint8_t raw = (uint8_t)digitalRead(b->pin);
  if (raw != b->raw) {
    b->raw = raw;
    b->since = now;
  }
  if (raw != b->stable && (uint32_t)(now - b->since) >= ARCADE_DB_MS) {
    b->stable = raw;
    if (raw == LOW) return 1;
  }
  return 0;
}

static void pollArcade(uint32_t now) {
  if (arcadeFell(&g_btnRed, now)) onArcade('l');
  if (arcadeFell(&g_btnWhite, now)) onArcade('r');
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
  pinMode(PIN_RED, INPUT_PULLUP);
  pinMode(PIN_WHITE, INPUT_PULLUP);
  g_btnRed.stable = g_btnRed.raw = (uint8_t)digitalRead(PIN_RED);
  g_btnWhite.stable = g_btnWhite.raw = (uint8_t)digitalRead(PIN_WHITE);
  g_btnRed.since = g_btnWhite.since = millis();
  modesMetaBoot();
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
  pollArcade(now);
  webPoll(now);
  tickScroll(now);
  tickPanel(now);
}
