// Effects for the 16x16 panel.
// y = 0 is the top of the picture, x = 0 is the left. panelIndex turns
// that into the seated panel. Speed is 0..100.
//
//   fire       the matrix fire (not drawn here)
//   rainbow    diagonal hue, drifting. Speed is the drift.
//   metaballs  blobs, 1 to 8. The page adds and removes them.
//   plasma     rotating field. Red zooms in, white zooms out.
//   sun        pulsing sun. Red grows it, white shrinks it.
//   spiral     one Archimedean arm, about 1.52 turns, Gaussian width.
//              Hue runs violet to cyan along the arm. Speed shifts that range.
//              Fixed spin, one turn every 9 s.
//   fluid      ink advected in a swirl. Speed is the flow.
//   heart      lub-dub pulse. Speed is the heart rate.
//
// Removed with the old control panel: jumbo, classic, the four waves,
// secret, lava, music. Nothing here plays a stored frame list.

#pragma once

#include <stdint.h>
#include <string.h>
#include <math.h>

static const uint8_t M_FIRE = 0;
static const uint8_t M_RAINBOW = 1;
static const uint8_t M_METABALLS = 2;
static const uint8_t M_PLASMA = 3;
static const uint8_t M_SUN = 4;
static const uint8_t M_SPIRAL = 5;
static const uint8_t M_FLUID = 6;
static const uint8_t M_HEART = 7;
static const uint8_t M_COUNT = 8;

struct ModeIn {
  uint8_t mode;
  uint8_t speed;
  uint8_t fader;
};

static const char* const kModeName[M_COUNT] = {
  "fire", "rainbow", "metaballs", "plasma", "sun", "spiral", "fluid", "heart"
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

static const uint8_t META_MIN = 1;
static const uint8_t META_MAX = 8;
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

static uint8_t g_dye[256];
static uint8_t g_dyeH[256];
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

static void modesBoot() {
  g_metaRng ^= 0xA5A5u;
  g_metaN = 3;
  for (uint8_t i = 0; i < g_metaN; i++) metaPlace(i);
  memset(g_dye, 0, sizeof g_dye);
  memset(g_dyeH, 0, sizeof g_dyeH);
  fluidInject(4, 5, 200, 220);
  fluidInject(11, 10, 30, 200);
  fluidInject(8, 3, 140, 160);
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

static void fluidVel(float x, float y, float t, float* vx, float* vy) {
  float fx = (x - 7.5f) * 0.42f;
  float fy = (y - 7.5f) * 0.42f;
  float wob = sinf(t * 0.6f);
  *vx = -fy * (0.85f + 0.25f * wob) + sinf(fx * 1.4f + t) * 0.55f;
  *vy = fx * (0.85f + 0.25f * wob) + cosf(fy * 1.2f - t * 0.8f) * 0.50f;
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

static void fluidStep(uint8_t speed) {
  float scale = 0.35f + (float)speed * 0.013f;
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

static float beatScale(uint32_t now, uint8_t speed) {
  uint32_t period = 2100u - (uint32_t)speed * 14u;
  if (period < 650u) period = 650u;
  float p = (float)(now % period) / (float)period;
  if (p < 0.10f) {
    float u = sinf((p / 0.10f) * 3.14159265f);
    return 0.84f + 0.24f * u;
  }
  if (p < 0.16f) return 0.84f;
  if (p < 0.30f) {
    float u = sinf(((p - 0.16f) / 0.14f) * 3.14159265f);
    return 0.84f + 0.13f * u;
  }
  return 0.82f;
}

static uint8_t g_modeWas = 255;

static void modesStep(const ModeIn* in, uint32_t now) {
  (void)now;
  int sp = modeSpeed256(in->speed);
  if (in->mode != g_modeWas) g_modeWas = in->mode;
  if (in->mode == M_RAINBOW) {
    int add = (6 * sp) / 256;
    if (add < 1) add = 1;
    g_rainPhase += (uint32_t)add;
  }
  if (in->mode == M_METABALLS) {
    float scale = (float)sp / 256.0f;
    float step = 0.28f * scale;
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
    g_plasmaAngle += 0.045f * scale;
    if (g_plasmaAngle > 6.2831853f) g_plasmaAngle -= 6.2831853f;
    g_plasmaTime += 0.06f * scale;
  }
  if (in->mode == M_SUN) {
    g_sunTime += 0.08f * ((float)sp / 256.0f);
  }
  if (in->mode == M_FLUID) fluidStep(in->speed);
}

// Archimedean centreline, r = 0.40 + 9.05 * (theta / thetaMax),
// thetaMax = 1.52 turns. The radial pitch is about 6 px, so a 3-4 px
// arm leaves a 1-2 px dark gap. Rebuilt once per frame.
// Angle is -spin, one turn every 9 s, the same direction as before.
static const uint8_t SP_N = 96;
static const float SP_TURNS = 1.52f;
static const float SP_R0 = 0.40f;
static const float SP_R1 = 9.45f;
static const float SP_SIGMA = 1.22f;
static float g_spX[SP_N];
static float g_spY[SP_N];
static float g_spU[SP_N];
static uint32_t g_spNow = 0xffffffffu;

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
    g_spX[i] = rad * ca;
    g_spY[i] = rad * sa;
    g_spU[i] = (float)i / (float)(SP_N - 1);
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
  float dx = (float)x - 7.5f;
  float dy = (float)y - 7.5f;
  float best = 1.0e9f;
  float u = 0.0f;
  for (uint8_t i = 0; i < SP_N; i++) {
    float ddx = dx - g_spX[i];
    float ddy = dy - g_spY[i];
    float d2 = ddx * ddx + ddy * ddy;
    if (d2 < best) {
      best = d2;
      u = g_spU[i];
    }
  }
  // Gaussian of distance to the centreline. Sigma 1.22 px.
  float gauss = expf(-best / (2.0f * SP_SIGMA * SP_SIGMA));
  // 188 violet at the inner end, 130 cyan at the outer end.
  // Speed slides the whole range and keeps the spread.
  int hue = (int)(188.0f + u * (130.0f - 188.0f)) + ((int)speed - 50);
  if (hue < 0) hue += 256;
  if (hue > 255) hue -= 256;
  float satArm = 155.0f + u * (255.0f - 155.0f);
  float peak = 255.0f + u * (225.0f - 255.0f);
  float val = 40.0f + (peak - 40.0f) * gauss;
  float w = gauss / 0.42f;
  if (w > 1.0f) w = 1.0f;
  float sat = 200.0f + (satArm - 200.0f) * w;
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
  float sc = beatScale(now, speed);
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
    float breathe = 0.82f + 0.38f * sinf(g_plasmaTime * 0.37f);
    float zoom = breathe * (0.50f + (float)g_plasmaZoom * 0.32f);
    float ca = cosf(g_plasmaAngle);
    float sa = sinf(g_plasmaAngle);
    float dx = ((float)x - 7.5f) * zoom;
    float dy = ((float)y - 7.5f) * zoom;
    float rx = dx * ca - dy * sa;
    float ry = dx * sa + dy * ca;
    float v = sinf(rx * 0.90f + g_plasmaTime)
            + sinf(ry * 0.70f - g_plasmaTime * 0.85f)
            + sinf((rx + ry) * 0.48f + g_plasmaTime * 0.40f);
    float n = (v + 3.0f) * (1.0f / 6.0f);
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;
    uint8_t hue = (uint8_t)(n * 210.0f + g_plasmaTime * 14.0f);
    uint8_t val = (uint8_t)(70.0f + n * 185.0f);
    hsvToRgb(hue, 235, val, r, g, b);
    return;
  }
  if (in->mode == M_SUN) {
    float dx = (float)x - 7.5f;
    float dy = (float)y - 7.5f;
    float dist = sqrtf(dx * dx + dy * dy);
    float ang = atan2f(dy, dx);
    float pulse = 0.86f + 0.14f * sinf(g_sunTime * 1.7f);
    float coreR = (1.45f + (float)g_sunSize * 0.62f) * pulse;
    float core = coreR / (dist + 0.42f);
    float ray = sinf(ang * (5.0f + (float)g_sunSize) + g_sunTime * 1.8f);
    ray = 0.55f + 0.45f * ray;
    float grit = 0.62f + 0.38f * sinf(ang * 13.0f - g_sunTime * 2.6f + dist * 0.9f);
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
  if (in->mode == M_HEART) paintHeart(x, y, now, in->speed, r, g, b);
}
