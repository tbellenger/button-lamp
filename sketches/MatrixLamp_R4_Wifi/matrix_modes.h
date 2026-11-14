// Effects for the 16x16 panel.
// y = 0 is the top of the picture, x = 0 is the left. panelIndex turns
// that into the seated panel. Speed is 0..100.
//
//   fire       the matrix fire (not drawn here)
//   rainbow    diagonal hue, drifting. Speed is the drift.
//   metaballs  blobs, 1 to 8. The page adds and removes them.
//   plasma     rotating field. Red zooms in, white zooms out.
//   sun        pulsing sun. Red grows it, white shrinks it.
//   spiral     two Archimedean arms, 180 degrees apart, one turn each.
//              Hue runs violet to cyan along each arm. Speed shifts that range.
//              Fixed spin, one turn every 9 s.
//   fluid      ink advected in a swirl. Speed is the flow.
//   heart      lub-dub pulse. Speed is the heart rate.
//   matrix     green rain. Light-green head, pure-green tail.
//              The whole field fades each step. Speed is the fall.
//   lightning  jagged bolt, logical picture space. Outline plus a
//              darker same-hue fill. A looping ramp scrolls along it.
//              Speed is that scroll. Flicker stays on.
//   off        black, except logical (0, 15): raw yellow (6, 6, 0).
//              Static. Speed is ignored. Brightness cannot round it off.
//   radar      live NEXRAD frames. North is up in this picture. Speed
//              is ignored. A mid-grey pixel marks home at (7, 7),
//              drawn last, on top of the rain.
//              The bottom row is a frame-progress bar.
//
// Removed with the old control panel: jumbo, classic, the four waves,
// secret, lava, music.

#pragma once

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "lamp_ids.h"
#include "lamp_core.h"
#include "radar_format.h"
#include "mode_ram.h"

struct ModeIn {
  uint8_t mode;
  uint8_t speed;
  uint8_t fader;
};

static const char* const kModeName[M_COUNT] = {
  "fire", "rainbow", "metaballs", "plasma", "sun", "spiral", "fluid", "heart",
  "matrix", "lightning", "off", "radar"
};

static const char* modesName(uint8_t mode) {
  if (mode >= M_COUNT) return kModeName[0];
  return kModeName[mode];
}

static int modesFromName(const char* s) {
  if (!s) return -1;
  if (!strcmp(s, "meatballs")) return M_METABALLS;
  for (uint8_t i = 0; i < M_COUNT; i++) {
    if (!strcmp(s, kModeName[i])) return (int)i;
  }
  return -1;
}

static int modeSpeed256(uint8_t s) {
  if (s <= 50) return 64 + (int)s * (256 - 64) / 50;
  return 256 + (int)(s - 50) * (1024 - 256) / 50;
}

static void hsvToRgb(uint8_t h, uint8_t s, uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b) {
  uint8_t region = (uint8_t)(h / 43);
  uint8_t rem = (uint8_t)((h - (uint8_t)(region * 43)) * 6);
  uint8_t p = (uint8_t)((uint16_t)v * (uint16_t)(255 - s) / 255);
  uint8_t q = (uint8_t)((uint16_t)v * (255 - ((uint16_t)s * rem / 255)) / 255);
  uint8_t t = (uint8_t)((uint16_t)v * (255 - ((uint16_t)s * (uint16_t)(255 - rem) / 255)) / 255);
  switch (region) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
  }
}

static uint32_t g_rainPhase = 0;

struct MetaBlob {
  float x;
  float y;
  float vx;
  float vy;
};
static MetaBlob g_metaB[META_MAX];
static uint8_t g_metaN = 3;
static uint32_t g_metaRng = 1u;

static float metaRand() {
  g_metaRng = g_metaRng * 1664525u + 1013904223u;
  return (float)((g_metaRng >> 8) & 0xffffffu) / 16777216.0f;
}

static void metaPlace(uint8_t i) {
  static const float kDir[8][2] = {
    {1.0f, 0.15f}, {0.7f, 0.7f}, {-0.2f, 1.0f}, {-0.75f, 0.55f},
    {-1.0f, -0.1f}, {-0.55f, -0.8f}, {0.15f, -1.0f}, {0.8f, -0.45f}
  };
  uint8_t d = (uint8_t)(metaRand() * 8.0f);
  if (d > 7) d = 7;
  float spd = 0.45f + metaRand() * 0.55f;
  g_metaB[i].x = 1.2f + metaRand() * 12.6f;
  g_metaB[i].y = 1.2f + metaRand() * 12.6f;
  g_metaB[i].vx = kDir[d][0] * spd;
  g_metaB[i].vy = kDir[d][1] * spd;
}

static uint8_t g_plasmaZoom = 2;
static float g_plasmaAngle = 0.0f;
static float g_plasmaTime = 0.0f;
static uint8_t g_sunSize = 2;
static float g_sunTime = 0.0f;

static void modesPlasmaZoom(int dir) {
  int z = (int)g_plasmaZoom + dir;
  if (z < 0) z = 0;
  if (z > 4) z = 4;
  g_plasmaZoom = (uint8_t)z;
}
static uint8_t modesPlasmaZoomLevel() { return g_plasmaZoom; }

static void modesSunSize(int dir) {
  int z = (int)g_sunSize + dir;
  if (z < 0) z = 0;
  if (z > 4) z = 4;
  g_sunSize = (uint8_t)z;
}
static uint8_t modesSunSizeLevel() { return g_sunSize; }

static void modesMetaAdd() {
  if (g_metaN >= META_MAX) return;
  metaPlace(g_metaN);
  g_metaN++;
}
static void modesMetaRemove() {
  if (g_metaN <= META_MIN) return;
  g_metaN--;
}
static void modesMetaSet(uint8_t n) {
  if (n < META_MIN) n = META_MIN;
  if (n > META_MAX) n = META_MAX;
  while (g_metaN < n) {
    metaPlace(g_metaN);
    g_metaN++;
  }
  g_metaN = n;
}
static uint8_t modesMetaCount() { return g_metaN; }

static uint8_t (&g_dye)[256] = g_modeRam.fx.dye;
static uint8_t (&g_dyeH)[256] = g_modeRam.fx.dyeH;
static float g_fluidT = 0.0f;
static uint8_t g_fluidTick = 0;

static void fluidInject(int cx, int cy, uint8_t hue, uint8_t amt) {
  for (int y = cy - 2; y <= cy + 2; y++) {
    for (int x = cx - 2; x <= cx + 2; x++) {
      if (x < 0 || y < 0 || x > 15 || y > 15) continue;
      int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
      if (d > 5) continue;
      int add = amt - d * 28;
      if (add < 0) add = 0;
      int i = y * 16 + x;
      int v = (int)g_dye[i] + add;
      if (v > 255) v = 255;
      if (add > 40) g_dyeH[i] = hue;
      g_dye[i] = (uint8_t)v;
    }
  }
}

// Radius (1/16 px) and angle (0..255 = one turn) for each LED, in logical
// picture space. panelIndex applies PANEL_ROTATE when the pixel is sent.
static uint8_t (&g_polDist)[256] = g_modeRam.fx.polDist;
static uint8_t (&g_polAng)[256] = g_modeRam.fx.polAng;

static void geomBoot() {
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      float dx = (float)x - 7.5f;
      float dy = (float)y - 7.5f;
      float dist = sqrtf(dx * dx + dy * dy);
      float ang = atan2f(dy, dx);
      int di = (int)(dist * 16.0f);
      if (di > 255) di = 255;
      int ai = (int)((ang + 3.14159265f) * (256.0f / 6.2831853f));
      g_polDist[y * 16 + x] = (uint8_t)di;
      g_polAng[y * 16 + x] = (uint8_t)(ai & 255);
    }
  }
}

// Green rain, one drop per column. Logical y = 0 is the top of the
// picture and y grows toward the desk. panelIndex applies PANEL_ROTATE
// when the pixel is sent, so the head falls downward on the mounted panel.
//
// The head is saturated green (80, 255, 80). Pixel j behind it is pure
// green, 255 - j*35. Every 50 ms step the whole field is scaled by
// 185/256 (fade by 70), so old drops leave a decaying trail. The Speed
// slider scales that clock. At 50 it matches a 50 ms frame.
static const uint8_t RAIN_COLS = 16;
static const uint8_t RAIN_FRAME_MS = 50;
struct RainDrop {
  int8_t y;
  uint8_t len;
  uint8_t speed;
  uint8_t counter;
};
static RainDrop g_rain[RAIN_COLS];
static uint8_t (&g_rainRgb)[256 * 3] = g_modeRam.fx.rainRgb;
static uint32_t g_rainRng = 0x6D617472u;
static uint16_t g_rainAcc = 0;

static uint32_t rainRand() {
  g_rainRng = g_rainRng * 1664525u + 1013904223u;
  return g_rainRng;
}

static void rainSpawn(uint8_t x, uint8_t fresh) {
  uint32_t r = rainRand();
  // Length 4..9, speed 1..3. A new drop starts above the panel.
  // A fresh boot scatters them further up so they do not arrive together.
  g_rain[x].len = (uint8_t)(4u + (r % 6u));
  g_rain[x].speed = (uint8_t)(1u + ((r >> 4) % 3u));
  g_rain[x].counter = 0;
  if (fresh) g_rain[x].y = (int8_t)(-20 + (int)((r >> 8) % 20u));
  else g_rain[x].y = (int8_t)(-10 + (int)((r >> 8) % 11u));
}

static void rainPaintDrop(uint8_t x) {
  RainDrop* d = &g_rain[x];
  for (uint8_t j = 0; j < d->len; j++) {
    int cy = (int)d->y - (int)j;
    if (cy < 0 || cy > 15) continue;
    uint8_t* p = g_rainRgb + ((cy * 16 + x) * 3);
    if (j == 0) {
      p[0] = 80;
      p[1] = 255;
      p[2] = 80;
    } else {
      int g = 255 - (int)j * 35;
      if (g < 0) g = 0;
      p[0] = 0;
      p[1] = (uint8_t)g;
      p[2] = 0;
    }
  }
}

static void rainFrame() {
  for (int i = 0; i < 256 * 3; i++) {
    g_rainRgb[i] = (uint8_t)(((uint16_t)g_rainRgb[i] * 185u) >> 8);
  }
  for (uint8_t x = 0; x < RAIN_COLS; x++) {
    RainDrop* d = &g_rain[x];
    d->counter++;
    if (d->counter >= d->speed) {
      d->counter = 0;
      d->y++;
      if ((int)d->y - (int)d->len > 16) rainSpawn(x, 0);
    }
    rainPaintDrop(x);
  }
}

static void rainBoot() {
  g_rainRng ^= 0xA5A5u;
  g_rainAcc = 0;
  memset(g_rainRgb, 0, sizeof g_rainRgb);
  for (uint8_t x = 0; x < RAIN_COLS; x++) rainSpawn(x, 1);
}

static void rainStep(uint8_t speed, uint32_t dt, uint32_t period) {
  (void)period;
  int sp = modeSpeed256(speed);
  uint32_t add = (dt * (uint32_t)sp) / 256u;
  if (add > 400u) add = 400u;
  g_rainAcc = (uint16_t)(g_rainAcc + add);
  uint8_t n = 0;
  while (g_rainAcc >= RAIN_FRAME_MS && n < 8) {
    g_rainAcc = (uint16_t)(g_rainAcc - RAIN_FRAME_MS);
    rainFrame();
    n++;
  }
}

static void modesBoot() {
  geomBoot();
  g_metaRng ^= 0xA5A5u;
  g_metaN = 3;
  for (uint8_t i = 0; i < g_metaN; i++) metaPlace(i);
  memset(g_dye, 0, sizeof g_dye);
  memset(g_dyeH, 0, sizeof g_dyeH);
  fluidInject(4, 5, 200, 220);
  fluidInject(11, 10, 30, 200);
  fluidInject(8, 3, 140, 160);
  rainBoot();
}

static void sunWarm(float heat, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (heat < 0.18f) {
    *r = *g = *b = 0;
    return;
  }
  float t = (heat - 0.18f) / 2.05f;
  if (t > 1.0f) t = 1.0f;
  int h = (int)(t * 255.0f);
  if (h < 60) {
    *r = (uint8_t)(50 + h * 3);
    *g = 0;
    *b = 0;
  } else if (h < 130) {
    *r = 255;
    *g = (uint8_t)((h - 60) * 2);
    *b = 0;
  } else if (h < 200) {
    *r = 255;
    *g = (uint8_t)(140 + (h - 130));
    *b = (uint8_t)((h - 130) / 3);
  } else {
    *r = 255;
    *g = 255;
    int w = (h - 200) * 255 / 55;
    if (w > 255) w = 255;
    *b = (uint8_t)w;
  }
}

// Quarter-degree sine. 0..255 is one turn, value is sin * 127.
// libm sinf is hundreds of cycles; this is one load.
static const int8_t kSinQ[256] = {
     0,   3,   6,   9,  12,  16,  19,  22,  25,  28,  31,  34,  37,  40,  43,  46,
    49,  51,  54,  57,  60,  63,  65,  68,  71,  73,  76,  78,  81,  83,  85,  88,
    90,  92,  94,  96,  98, 100, 102, 104, 106, 107, 109, 111, 112, 113, 115, 116,
   117, 118, 120, 121, 122, 122, 123, 124, 125, 125, 126, 126, 126, 127, 127, 127,
   127, 127, 127, 127, 126, 126, 126, 125, 125, 124, 123, 122, 122, 121, 120, 118,
   117, 116, 115, 113, 112, 111, 109, 107, 106, 104, 102, 100,  98,  96,  94,  92,
    90,  88,  85,  83,  81,  78,  76,  73,  71,  68,  65,  63,  60,  57,  54,  51,
    49,  46,  43,  40,  37,  34,  31,  28,  25,  22,  19,  16,  12,   9,   6,   3,
     0,  -3,  -6,  -9, -12, -16, -19, -22, -25, -28, -31, -34, -37, -40, -43, -46,
   -49, -51, -54, -57, -60, -63, -65, -68, -71, -73, -76, -78, -81, -83, -85, -88,
   -90, -92, -94, -96, -98,-100,-102,-104,-106,-107,-109,-111,-112,-113,-115,-116,
  -117,-118,-120,-121,-122,-122,-123,-124,-125,-125,-126,-126,-126,-127,-127,-127,
  -127,-127,-127,-127,-126,-126,-126,-125,-125,-124,-123,-122,-122,-121,-120,-118,
  -117,-116,-115,-113,-112,-111,-109,-107,-106,-104,-102,-100, -98, -96, -94, -92,
   -90, -88, -85, -83, -81, -78, -76, -73, -71, -68, -65, -63, -60, -57, -54, -51,
   -49, -46, -43, -40, -37, -34, -31, -28, -25, -22, -19, -16, -12,  -9,  -6,  -3
};

static float sinQ(float rad) {
  int i = (int)(rad * 40.7436654f);
  return (float)kSinQ[i & 255] * (1.0f / 127.0f);
}

static float cosQ(float rad) {
  return sinQ(rad + 1.5707963f);
}

static void fluidVel(float x, float y, float t, float* vx, float* vy) {
  float fx = (x - 7.5f) * 0.42f;
  float fy = (y - 7.5f) * 0.42f;
  float wob = sinQ(t * 0.6f);
  *vx = -fy * (0.85f + 0.25f * wob) + sinQ(fx * 1.4f + t) * 0.55f;
  *vy = fx * (0.85f + 0.25f * wob) + cosQ(fy * 1.2f - t * 0.8f) * 0.50f;
}

static void fluidSample(float x, float y, int* ink, int* hue) {
  if (x < 0.0f) x = 0.0f;
  if (y < 0.0f) y = 0.0f;
  if (x > 15.0f) x = 15.0f;
  if (y > 15.0f) y = 15.0f;
  int x0 = (int)x;
  int y0 = (int)y;
  if (x0 > 15) x0 = 15;
  if (y0 > 15) y0 = 15;
  int x1 = x0 < 15 ? x0 + 1 : x0;
  int y1 = y0 < 15 ? y0 + 1 : y0;
  float tx = x - (float)x0;
  float ty = y - (float)y0;
  int i00 = g_dye[y0 * 16 + x0];
  int i10 = g_dye[y0 * 16 + x1];
  int i01 = g_dye[y1 * 16 + x0];
  int i11 = g_dye[y1 * 16 + x1];
  float a = i00 + (i10 - i00) * tx;
  float b = i01 + (i11 - i01) * tx;
  *ink = (int)(a + (b - a) * ty);
  int best = i00;
  int bh = g_dyeH[y0 * 16 + x0];
  if (i10 > best) { best = i10; bh = g_dyeH[y0 * 16 + x1]; }
  if (i01 > best) { best = i01; bh = g_dyeH[y1 * 16 + x0]; }
  if (i11 > best) bh = g_dyeH[y1 * 16 + x1];
  *hue = bh;
}

static void fluidStep(uint8_t speed, float timeScale) {
  float scale = (0.35f + (float)speed * 0.013f) * timeScale;
  g_fluidT += 0.06f * scale;
  uint8_t nd[256];
  uint8_t nh[256];
  float adv = 0.85f * scale;
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      float vx, vy;
      fluidVel((float)x, (float)y, g_fluidT, &vx, &vy);
      int ink, hue;
      fluidSample((float)x - vx * adv, (float)y - vy * adv, &ink, &hue);
      ink = ink * 253 / 256;
      nd[y * 16 + x] = (uint8_t)ink;
      nh[y * 16 + x] = (uint8_t)hue;
    }
  }
  memcpy(g_dye, nd, sizeof g_dye);
  memcpy(g_dyeH, nh, sizeof g_dyeH);
  uint8_t every = (uint8_t)(18 - (int)speed / 10);
  if (every < 6) every = 6;
  if (++g_fluidTick >= every) {
    g_fluidTick = 0;
    int cx = 1 + (int)(metaRand() * 14.0f);
    int cy = 1 + (int)(metaRand() * 14.0f);
    fluidInject(cx, cy, (uint8_t)(metaRand() * 255.0f), 210);
  }
}

static float heartField(float x, float y) {
  float a = x * x + y * y - 1.0f;
  return a * a * a - x * x * y * y * y;
}

static float g_beat = 0.82f;

static float beatScale(uint32_t now, uint8_t speed) {
  uint32_t period = 2100u - (uint32_t)speed * 14u;
  if (period < 650u) period = 650u;
  float p = (float)(now % period) / (float)period;
  if (p < 0.10f) {
    float u = sinQ((p / 0.10f) * 3.14159265f);
    return 0.84f + 0.24f * u;
  }
  if (p < 0.16f) return 0.84f;
  if (p < 0.30f) {
    float u = sinQ(((p - 0.16f) / 0.14f) * 3.14159265f);
    return 0.84f + 0.13f * u;
  }
  return 0.82f;
}

static uint8_t g_modeWas = 255;

static void modesStep(const ModeIn* in, uint32_t now, uint32_t dt, uint32_t period) {
  int sp = modeSpeed256(in->speed);
  float timeScale = 1.0f;
  if (period > 0) timeScale = (float)dt / (float)period;
  if (timeScale < 0.25f) timeScale = 0.25f;
  if (timeScale > 3.0f) timeScale = 3.0f;
  if (in->mode != g_modeWas) g_modeWas = in->mode;
  if (in->mode == M_RAINBOW) {
    int add = (6 * sp) / 256;
    if (add < 1) add = 1;
    g_rainPhase += (uint32_t)((float)add * timeScale);
  }
  if (in->mode == M_METABALLS) {
    float scale = (float)sp / 256.0f;
    float step = 0.28f * scale * timeScale;
    for (uint8_t i = 0; i < g_metaN; i++) {
      g_metaB[i].x += g_metaB[i].vx * step;
      g_metaB[i].y += g_metaB[i].vy * step;
      if (g_metaB[i].x < 0.4f) { g_metaB[i].x = 0.4f; g_metaB[i].vx = -g_metaB[i].vx; }
      if (g_metaB[i].x > 14.6f) { g_metaB[i].x = 14.6f; g_metaB[i].vx = -g_metaB[i].vx; }
      if (g_metaB[i].y < 0.4f) { g_metaB[i].y = 0.4f; g_metaB[i].vy = -g_metaB[i].vy; }
      if (g_metaB[i].y > 14.6f) { g_metaB[i].y = 14.6f; g_metaB[i].vy = -g_metaB[i].vy; }
    }
  }
  if (in->mode == M_PLASMA) {
    float scale = (float)sp / 256.0f;
    g_plasmaAngle += 0.045f * scale * timeScale;
    if (g_plasmaAngle > 6.2831853f) g_plasmaAngle -= 6.2831853f;
    g_plasmaTime += 0.06f * scale * timeScale;
  }
  if (in->mode == M_SUN) {
    g_sunTime += 0.08f * ((float)sp / 256.0f) * timeScale;
  }
  if (in->mode == M_FLUID) fluidStep(in->speed, timeScale);
  if (in->mode == M_HEART) g_beat = beatScale(now, in->speed);
  if (in->mode == M_MATRIX) rainStep(in->speed, dt, period);
}

// Two Archimedean arms, the second rotated 180 degrees.
// r = 0.35 + 9.15 * (theta / thetaMax), thetaMax = 1.00 turn.
// Per-arm pitch is about 9.2 px (was 6 px for one arm) so the
// arm-to-arm gap stays about 1-2 px. Rebuilt once per frame.
// Angle is -spin, one turn every 9 s, the same direction as before.
static const uint8_t SP_N = 96;
static const float SP_TURNS = 1.00f;
static const float SP_R0 = 0.35f;
static const float SP_R1 = 9.50f;
// Gaussian table below is exp(-d^2 / (2 * 1.22^2)).
// Centreline in 1/16 px. The Gaussian of d^2 is a 64-step table.
static_assert(SP_N == 96, "spiral cache overlay is 96 points");
static int16_t (&g_spX)[SP_N] = g_modeRam.fx.spX;
static int16_t (&g_spY)[SP_N] = g_modeRam.fx.spY;
static uint8_t (&g_spU)[SP_N] = g_modeRam.fx.spU;
static uint32_t g_spNow = 0xffffffffu;

// Radar frames reuse this scratch. Coming back rebuilds the tables.
static void modesAfterRadar() {
  g_spNow = 0xffffffffu;
  geomBoot();
  memset(g_dye, 0, sizeof g_dye);
  memset(g_dyeH, 0, sizeof g_dyeH);
  fluidInject(4, 5, 200, 220);
  fluidInject(11, 10, 30, 200);
  fluidInject(8, 3, 140, 160);
  rainBoot();
}
static const uint8_t kGauss[64] = {
  255,239,230,220,211,202,194,186,178,171,164,157,151,145,139,133,
  128,122,117,112,108,103, 99, 95, 91, 87, 84, 80, 77, 74, 71, 68,
   65, 62, 60, 57, 55, 53, 51, 49, 47, 45, 43, 41, 39, 38, 36, 35,
   33, 32, 31, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 18
};

static void spiralPrep(uint32_t now) {
  if (g_spNow == now) return;
  g_spNow = now;
  float theta = SP_TURNS * 6.2831853f;
  float spin = (float)(now % 9000u) * (6.2831853f / 9000.0f);
  float dth = theta / (float)(SP_N - 1);
  float dr = (SP_R1 - SP_R0) / (float)(SP_N - 1);
  // At t = 0 the outer tip points down-left, matching Tom's photo.
  float ang = -spin + (2.35f - theta);
  float ca = cosf(ang);
  float sa = sinf(ang);
  float cd = cosf(dth);
  float sd = sinf(dth);
  float rad = SP_R0;
  for (uint8_t i = 0; i < SP_N; i++) {
    g_spX[i] = (int16_t)(rad * ca * 16.0f);
    g_spY[i] = (int16_t)(rad * sa * 16.0f);
    g_spU[i] = (uint8_t)((i * 255) / (SP_N - 1));
    float nca = ca * cd - sa * sd;
    float nsa = sa * cd + ca * sd;
    ca = nca;
    sa = nsa;
    rad += dr;
  }
}

static void paintSpiral(int x, int y, uint32_t now, uint8_t speed,
                        uint8_t* r, uint8_t* g, uint8_t* b) {
  spiralPrep(now);
  int ox = x * 16 - 120;
  int oy = y * 16 - 120;
  int best = 2000000000;
  int u = 0;
  for (uint8_t i = 0; i < SP_N; i++) {
    int px = g_spX[i];
    int py = g_spY[i];
    int ddx = ox - px;
    int ddy = oy - py;
    int d2 = ddx * ddx + ddy * ddy;
    // The other arm is this one turned 180 degrees.
    int ddx2 = ox + px;
    int ddy2 = oy + py;
    int d2b = ddx2 * ddx2 + ddy2 * ddy2;
    if (d2b < d2) d2 = d2b;
    if (d2 < best) {
      best = d2;
      u = g_spU[i];
    }
  }
  // d^2 is in (1/16 px)^2. Index 0 is the centreline, 63 is ~8 px out.
  // Sigma stays 1.22 px. The table replaces expf.
  int gi = best >> 5;
  if (gi > 63) gi = 63;
  int gauss = kGauss[gi];
  // 188 violet at the inner end, 130 cyan at the outer end.
  // Speed slides the whole range and keeps the spread.
  int hue = 188 + ((130 - 188) * u) / 255 + ((int)speed - 50);
  if (hue < 0) hue += 256;
  if (hue > 255) hue -= 256;
  int satArm = 155 + (100 * u) / 255;
  int peak = 255 - (30 * u) / 255;
  int val = 40 + ((peak - 40) * gauss) / 255;
  int w = gauss * 100 / 107;
  if (w > 100) w = 100;
  int sat = 200 + ((satArm - 200) * w) / 100;
  if (val > 255.0f) val = 255.0f;
  if (sat < 0.0f) sat = 0.0f;
  if (sat > 255.0f) sat = 255.0f;
  hsvToRgb((uint8_t)hue, (uint8_t)sat, (uint8_t)val, r, g, b);
}

static void paintFluid(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b) {
  int i = y * 16 + x;
  if (g_dye[i] < 8) {
    *r = *g = *b = 0;
    return;
  }
  hsvToRgb(g_dyeH[i], 230, g_dye[i], r, g, b);
}

// Point down, lobes up. Bit 15 is x = 0.
static const uint16_t kHeart[16] = {
  0x0000, 0x0000, 0x3c3c, 0x7e7e, 0xffff, 0xffff, 0xffff, 0x7ffe,
  0x3ffc, 0x1ff8, 0x0ff0, 0x07e0, 0x03c0, 0x0180, 0x0000, 0x0000
};

static int heartOn(int x, int y) {
  if (x < 0 || y < 0 || x > 15 || y > 15) return 0;
  return (kHeart[y] & (uint16_t)(1u << (15 - x))) != 0;
}

static void paintHeart(int x, int y, uint32_t now, uint8_t speed,
                       uint8_t* r, uint8_t* g, uint8_t* b) {
  (void)now;
  (void)speed;
  float sc = g_beat;
  int on = heartOn(x, y);
  int halo = 0;
  if (!on) {
    halo = heartOn(x - 1, y) || heartOn(x + 1, y) || heartOn(x, y - 1) || heartOn(x, y + 1);
  }
  *r = *g = *b = 0;
  if (!on && !halo) return;
  float beat = (sc - 0.82f) / 0.26f;
  if (beat < 0.0f) beat = 0.0f;
  if (beat > 1.0f) beat = 1.0f;
  if (on) {
    int rr = 170 + (int)(beat * 85.0f);
    int gg = 16 + (int)(beat * 70.0f);
    int bb = 24 + (int)(beat * 50.0f);
    *r = (uint8_t)rr;
    *g = (uint8_t)gg;
    *b = (uint8_t)bb;
    return;
  }
  int glow = (int)(28.0f + beat * 70.0f);
  *r = (uint8_t)glow;
  *g = (uint8_t)(glow / 5);
  *b = (uint8_t)(glow / 4);
}

static void paintMatrix(int x, int y, uint8_t* r, uint8_t* g, uint8_t* b) {
  if ((unsigned)x >= RAIN_COLS || (unsigned)y >= 16u) return;
  const uint8_t* p = g_rainRgb + ((y * 16 + x) * 3);
  *r = p[0];
  *g = p[1];
  *b = p[2];
}

// Jagged bolt in logical picture space (x = 0 left, y = 0 top).
// 0 = off, 1 = interior fill, 2 = outline. panelIndex applies
// PANEL_ROTATE when the pixel is sent; this table is not pre-rotated.
// The end caps clip the grid corners on purpose.
static const uint8_t kBoltRole[16][16] = {
  { 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 2, 2, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 2, 1, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 2, 1, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 2, 1, 0, 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 3, 2, 1, 0, 0, 1, 2, 3, 3, 3, 3, 0, 0, 0, 0, 0 },
  { 0, 3, 2, 1, 0, 0, 1, 2, 2, 2, 2, 3, 0, 0, 0, 0 },
  { 0, 0, 3, 2, 1, 0, 0, 1, 1, 1, 1, 2, 3, 0, 0, 0 },
  { 0, 0, 0, 3, 2, 1, 1, 1, 1, 0, 0, 1, 2, 3, 0, 0 },
  { 0, 0, 0, 0, 3, 2, 2, 2, 2, 1, 0, 0, 1, 2, 3, 0 },
  { 0, 0, 0, 0, 0, 3, 3, 3, 3, 2, 1, 0, 0, 1, 2, 3 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 1, 0, 0, 1, 2 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 1, 0, 1, 2 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 1, 1, 2 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 2, 2, 2 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3 },
};

// 0 = unlit. Otherwise along = value / 65535 (0 = lower-left tip,
// 1 = upper-right tip).
static const uint16_t kBoltAlongQ16[16][16] = {
  { 65535, 65535, 64904,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0 },
  { 65535, 65535, 64904, 62378,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0 },
  { 65535, 64904, 62378, 59853, 57328,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0 },
  { 64904, 62378,     0, 57327, 54802, 52276,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0 },
  { 62378, 59853,     0,     0, 52276, 49751, 47226,     0,     0,     0,     0,     0,     0,     0,     0,     0 },
  { 59852, 57327, 54802,     0,     0, 47225, 44700, 39626, 30982, 27410, 26517,     0,     0,     0,     0,     0 },
  {     0, 54802, 52276, 49751,     0,     0, 42175, 34553, 30982, 27410, 26517, 24938,     0,     0,     0,     0 },
  {     0,     0, 49750, 47225, 44700,     0,     0, 34553, 30982, 27410, 25886, 23360, 20835,     0,     0,     0 },
  {     0,     0,     0, 44700, 42175, 39649, 38125, 34553, 30982,     0,     0, 20835, 18310, 15784,     0,     0 },
  {     0,     0,     0,     0, 40596, 39018, 38125, 34553, 30982, 23360,     0,     0, 15784, 13259, 10734,     0 },
  {     0,     0,     0,     0,     0, 39018, 38125, 34553, 25908, 20835, 18310,     0,     0, 10733,  8208,  5682 },
  {     0,     0,     0,     0,     0,     0,     0,     0,     0, 18310, 15784, 13259,     0,     0,  5682,  3157 },
  {     0,     0,     0,     0,     0,     0,     0,     0,     0,     0, 13258, 10733,  8208,     0,  3157,   631 },
  {     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  8208,  5682,  3157,   631,     1 },
  {     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,  3156,   631,     1,     1 },
  {     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,   631,     1,     1 },
};

static const uint8_t kRamp[][3] = {
  {255,  20, 170}, {255,  25,  90}, {255,  55,  30},
  {255, 140,  20}, {255, 210,  45}, {170, 220,  50},
  { 30, 210, 150}, { 15, 200, 230}, { 40, 100, 255},
  {130,  40, 255}, {210,  20, 230},
};
static const int kRampN = 11;

static void rampColor(float t, uint8_t* r, uint8_t* g, uint8_t* b) {
  // floorf already folds a negative phase into [0, 1). A per-cycle
  // add would spin for the whole uptime on every lit pixel.
  t = t - floorf(t);
  if (t < 0.0f) t += 1.0f;
  float x = t * (float)kRampN;
  int i = (int)x; if (i >= kRampN) i = kRampN - 1;
  int j = i + 1; if (j >= kRampN) j = 0;
  float f = x - (float)i; f = f * f * (3.0f - 2.0f * f);
  *r = (uint8_t)((1.0f - f) * kRamp[i][0] + f * kRamp[j][0] + 0.5f);
  *g = (uint8_t)((1.0f - f) * kRamp[i][1] + f * kRamp[j][1] + 0.5f);
  *b = (uint8_t)((1.0f - f) * kRamp[i][2] + f * kRamp[j][2] + 0.5f);
}

// Night light. Logical (0, 15) is the bottom row, left column of the
// picture. FX_ROTATE 3 (panelIndex) seats that cell; this is not
// pre-rotated. On the lamp it is the bottom-left LED.
//
// The colour is raw yellow (6, 6, 0). There is no gamma table. NeoPixel
// show() does (channel * (applied + 1)) >> 8, and applied is
// fader * cap / 100 with cap 40, so the multiplier is at most 41.
// 6 * 41 = 246, which shifts to 0, so a literal 6 is fully off at every
// legal brightness. Each non-zero channel is raised to the smallest
// value that show() still keeps (at least 1). Fader 0..2 makes applied
// 0; the multiplier is then 1 and no 8-bit channel survives, so the
// pixel goes dark with the rest of the panel. Speed is not read.
static uint8_t offChannel(uint8_t raw, uint8_t applied) {
  if (raw == 0) return 0;
  if (lamp_fx_scale(raw, applied) != 0) return raw;
  uint16_t mul = (uint16_t)applied + 1u;
  uint16_t need = (uint16_t)((256u + mul - 1u) / mul);
  if (need > 255u) return raw;
  return (uint8_t)need;
}

// Level 0 is no echo. 1..15 step by 5 dBZ from 5 through 75+.
static const uint8_t kRadarRgb[16][3] = {
  {  0,   0,   0},
  {130, 230, 140},
  { 40, 200,  50},
  {  0, 150,   0},
  {220, 220,  40},
  {255, 200,   0},
  {255, 140,   0},
  {255,  90,   0},
  {230,  40,   0},
  {190,   0,   0},
  {150,   0,   0},
  {140,   0,  50},
  {180,   0, 120},
  {210,   0, 170},
  {240,  40, 210},
  {255, 150, 240},
};

static const uint8_t* g_radarBits = 0;
static uint8_t g_radarN = 0;

static void modesRadarSet(const uint8_t* bits, uint8_t n) {
  if (n > RADAR_FRAMES_MAX) n = RADAR_FRAMES_MAX;
  g_radarBits = (n && bits) ? bits : 0;
  g_radarN = g_radarBits ? n : 0;
}

static void paintDimWhite(uint8_t fader, uint8_t* r, uint8_t* g, uint8_t* b) {
  uint8_t applied = lamp_fx_applied(fader);
  uint8_t w = offChannel(40, applied);
  *r = *g = *b = w;
}

// Home is neutral mid-grey in the frame buffer, before NeoPixel brightness.
// offChannel is the same floor paintOff uses, so a low fader cannot round
// a non-zero channel to black. Drawn after the rain and the progress bar.
static void paintRadarHome(uint8_t fader, uint8_t* r, uint8_t* g, uint8_t* b) {
  uint8_t applied = lamp_fx_applied(fader);
  uint8_t w = offChannel(128, applied);
  *r = *g = *b = w;
}

static void paintRadar(int x, int y, uint32_t now, uint8_t fader,
                       uint8_t* r, uint8_t* g, uint8_t* b) {
  *r = *g = *b = 0;
  if ((unsigned)x >= 16u || (unsigned)y >= 16u) return;
  uint8_t n = g_radarN;
  uint8_t idx = 0;
  uint8_t level = 0;
  if (!n || !g_radarBits) {
    // Waiting on the fetch. A dim checker replaces the previous preset
    // so a frozen old frame is not what the panel keeps showing.
    if (((x + y) & 3) == 0) *b = 36;
    else *b = 8;
  } else {
    idx = radarFrameIndex(now, n);
    if (idx >= n) idx = (uint8_t)(n - 1u);
    const uint8_t* frame = g_radarBits + (uint16_t)idx * RADAR_FRAME_BYTES;
    level = radarLevelAt(frame, x, y);
    if (level > 15) level = 15;
    uint8_t applied = lamp_fx_applied(fader);
    *r = offChannel(kRadarRgb[level][0], applied);
    *g = offChannel(kRadarRgb[level][1], applied);
    *b = offChannel(kRadarRgb[level][2], applied);
  }
  uint8_t bar = radarBarLength(n ? idx : 0, n);
  if (y == 15 && (unsigned)x < bar) paintDimWhite(fader, r, g, b);
  if (x == 7 && y == 7) paintRadarHome(fader, r, g, b);
}

static void paintOff(int x, int y, uint8_t fader, uint8_t* r, uint8_t* g, uint8_t* b) {
  *r = *g = *b = 0;
  if (x != 0 || y != 15) return;
  uint8_t applied = lamp_fx_applied(fader);
  *r = offChannel(6, applied);
  *g = offChannel(6, applied);
}

static void paintLightning(int x, int y, uint32_t now, uint8_t speed,
                           uint8_t* r, uint8_t* g, uint8_t* b) {
  *r = *g = *b = 0;
  if ((unsigned)x >= 16u || (unsigned)y >= 16u) return;
  uint8_t role = kBoltRole[y][x];
  if (role == 0) return;
  float along = (float)kBoltAlongQ16[y][x] / 65535.0f;
  float cps = 0.12f + (float)speed * 0.0044f;
  float phase = ((float)now / 1000.0f) * cps;
  uint32_t rng = now * 1664525u + 1013904223u;
  float wobble = 0.5f + 0.5f * sinf((float)now * 0.031f);
  float crackle = (float)((rng >> 8) & 255) / 255.0f;
  float flick = 0.96f + 0.03f * wobble + 0.01f * crackle;
  if (flick > 1.0f) flick = 1.0f;
  rampColor(along - phase, r, g, b);
  // role 2 = outline (full). roles 1 and 3 = dim rings (50%).
  float scale = (role == 2) ? flick : (0.50f * flick);
  *r = (uint8_t)((float)*r * scale + 0.5f);
  *g = (uint8_t)((float)*g * scale + 0.5f);
  *b = (uint8_t)((float)*b * scale + 0.5f);
}

static void modesColor(const ModeIn* in, int x, int y, uint32_t now,
                       uint8_t* r, uint8_t* g, uint8_t* b) {
  *r = *g = *b = 0;
  int sp = modeSpeed256(in->speed);
  if (in->mode == M_RAINBOW) {
    int along = x + ((PANEL_H - 1) - y);
    int t = (int)((g_rainPhase + (uint32_t)along * 8u) & 255u);
    hsvToRgb((uint8_t)t, 255, 255, r, g, b);
    return;
  }
  if (in->mode == M_METABALLS) {
    float field = 0.0f;
    for (uint8_t i = 0; i < g_metaN; i++) {
      float dx = (float)x - g_metaB[i].x;
      float dy = (float)y - g_metaB[i].y;
      float d2 = dx * dx + dy * dy + 0.30f;
      field += 1.9f / d2;
    }
    if (field < 0.58f) return;
    float u = (field - 0.58f) * 0.42f;
    if (u > 1.0f) u = 1.0f;
    uint8_t hue = (uint8_t)((now / 28u) + (uint32_t)(field * 26.0f));
    uint8_t sat = (uint8_t)(255 - (int)(u * 80.0f));
    uint8_t val = (uint8_t)(u * 255.0f);
    hsvToRgb(hue, sat, val, r, g, b);
    return;
  }
  if (in->mode == M_PLASMA) {
    float breathe = 0.82f + 0.38f * sinQ(g_plasmaTime * 0.37f);
    float zoom = breathe * (0.50f + (float)g_plasmaZoom * 0.32f);
    float ca = cosQ(g_plasmaAngle);
    float sa = sinQ(g_plasmaAngle);
    float dx = ((float)x - 7.5f) * zoom;
    float dy = ((float)y - 7.5f) * zoom;
    float rx = dx * ca - dy * sa;
    float ry = dx * sa + dy * ca;
    float v = sinQ(rx * 0.90f + g_plasmaTime)
            + sinQ(ry * 0.70f - g_plasmaTime * 0.85f)
            + sinQ((rx + ry) * 0.48f + g_plasmaTime * 0.40f);
    float n = (v + 3.0f) * (1.0f / 6.0f);
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    uint8_t hue = (uint8_t)(n * 210.0f + g_plasmaTime * 14.0f);
    uint8_t val = (uint8_t)(70.0f + n * 185.0f);
    hsvToRgb(hue, 235, val, r, g, b);
    return;
  }
  if (in->mode == M_SUN) {
    float dist = (float)g_polDist[y * 16 + x] * (1.0f / 16.0f);
    float ang = ((float)g_polAng[y * 16 + x] * (6.2831853f / 256.0f)) - 3.14159265f;
    float pulse = 0.86f + 0.14f * sinQ(g_sunTime * 1.7f);
    float coreR = (1.45f + (float)g_sunSize * 0.62f) * pulse;
    float core = coreR / (dist + 0.42f);
    float ray = sinQ(ang * (5.0f + (float)g_sunSize) + g_sunTime * 1.8f);
    ray = 0.55f + 0.45f * ray;
    float grit = 0.62f + 0.38f * sinQ(ang * 13.0f - g_sunTime * 2.6f + dist * 0.9f);
    if (grit < 0.0f) grit = 0.0f;
    float fall = 1.0f / (1.0f + dist * 0.20f);
    float rayGain = 0.42f + (float)g_sunSize * 0.14f;
    float heat = core * 1.05f + ray * grit * fall * rayGain;
    sunWarm(heat, r, g, b);
    (void)sp;
    return;
  }
  if (in->mode == M_SPIRAL) {
    paintSpiral(x, y, now, in->speed, r, g, b);
    return;
  }
  if (in->mode == M_FLUID) {
    paintFluid(x, y, r, g, b);
    return;
  }
  if (in->mode == M_MATRIX) {
    paintMatrix(x, y, r, g, b);
    return;
  }
  if (in->mode == M_HEART) {
    paintHeart(x, y, now, in->speed, r, g, b);
    return;
  }
  if (in->mode == M_LIGHTNING) paintLightning(x, y, now, in->speed, r, g, b);
  if (in->mode == M_OFF) paintOff(x, y, in->fader, r, g, b);
  if (in->mode == M_RADAR) paintRadar(x, y, now, in->fader, r, g, b);
}
