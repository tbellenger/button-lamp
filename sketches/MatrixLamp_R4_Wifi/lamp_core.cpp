#include "lamp_core.h"
#include "mode_ram.h"

#include <string.h>

LampModeRam g_modeRam;

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <stdlib.h>
// Arduino's random(lo, hi) is lo inclusive, hi exclusive. The fire uses
// that. The host build supplies the same shape so the step code is unchanged.
static long random(long lo, long hi) {
  if (hi <= lo) return lo;
  return lo + (rand() % (hi - lo));
}
#endif

// matrix_modes.h reads PANEL_H. It is 16 on this lamp.
static const int PANEL_W = 16;
static const int PANEL_H = 16;

#include "matrix_fire.h"
#include "matrix_modes.h"

// The seated panel. Rotate 3 puts logical y = 15 (the ember row) on the
// desk. Serpentine is the wiring, applied only when the index is chosen.
static const uint8_t FX_ROTATE = 3;
static const bool FX_SERPENTINE = true;
static const bool FX_SERPENTINE_ODD = true;
static const bool FX_FLIP_X = false;
static const bool FX_FLIP_Y = false;

static const uint8_t FX_BRIGHTNESS_CAP = 40;
static const uint16_t FX_LED_MA_FULL = 60;
static const uint16_t FX_POWER_BUDGET_MA = 2500;

static uint8_t (&g_heat)[LAMP_FX_N] = g_modeRam.fx.heat;
static uint8_t g_rgb[LAMP_FX_N * 3];
static_assert(LAMP_FX_N == 256, "heat overlay is 256 bytes");
static uint8_t g_fxMode = M_FIRE;
static uint8_t g_fxSpeed = 50;
static uint8_t g_fxFader = 50;
static uint8_t g_fxDirty = 1;
static uint8_t g_fxStarted = 0;
static uint32_t g_fxLast = 0;

static void orient(int* x, int* y) {
  int xx = *x;
  int yy = *y;
  uint8_t rot = (uint8_t)(FX_ROTATE & 3);
  if (rot == 1) {
    int nx = (LAMP_FX_W - 1) - yy;
    yy = xx;
    xx = nx;
  } else if (rot == 2) {
    xx = (LAMP_FX_W - 1) - xx;
    yy = (LAMP_FX_H - 1) - yy;
  } else if (rot == 3) {
    int ny = (LAMP_FX_H - 1) - xx;
    xx = yy;
    yy = ny;
  }
  if (FX_FLIP_X) xx = (LAMP_FX_W - 1) - xx;
  if (FX_FLIP_Y) yy = (LAMP_FX_H - 1) - yy;
  if (xx < 0) xx = 0;
  if (yy < 0) yy = 0;
  if (xx >= LAMP_FX_W) xx = LAMP_FX_W - 1;
  if (yy >= LAMP_FX_H) yy = LAMP_FX_H - 1;
  *x = xx;
  *y = yy;
}

uint16_t lamp_fx_led_index(int x, int y) {
  if (FX_SERPENTINE) {
    bool odd = (y & 1) != 0;
    if (odd == FX_SERPENTINE_ODD) x = (LAMP_FX_W - 1) - x;
  }
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= LAMP_FX_W) x = LAMP_FX_W - 1;
  if (y >= LAMP_FX_H) y = LAMP_FX_H - 1;
  return (uint16_t)(y * LAMP_FX_W + x);
}

uint16_t lamp_fx_led_from_logical(int x, int y) {
  orient(&x, &y);
  return lamp_fx_led_index(x, y);
}

static void write_visual(int lx, int ly, uint8_t r, uint8_t g, uint8_t b) {
  int x = lx;
  int y = ly;
  orient(&x, &y);
  uint8_t* p = g_rgb + ((y * LAMP_FX_W + x) * 3);
  p[0] = r;
  p[1] = g;
  p[2] = b;
}

static uint32_t period_ms() {
  uint32_t period = 20u + (uint32_t)(100 - g_fxSpeed) * 130u / 100u;
  if (period < 16u) period = 16u;
  return period;
}

uint8_t lamp_fx_brightness_cap() {
  uint32_t denom = (uint32_t)LAMP_FX_N * (uint32_t)FX_LED_MA_FULL;
  uint32_t fromBudget = 255;
  if (denom > 0) {
    fromBudget = ((uint32_t)FX_POWER_BUDGET_MA * 255ul) / denom;
    if (fromBudget < 1) fromBudget = 1;
  }
  uint32_t cap = FX_BRIGHTNESS_CAP;
  if (fromBudget < cap) cap = fromBudget;
  if (cap > 255) cap = 255;
  return (uint8_t)cap;
}

uint8_t lamp_fx_applied(uint8_t fader) {
  uint8_t cap = lamp_fx_brightness_cap();
  return (uint8_t)((uint16_t)fader * cap / 100u);
}

uint8_t lamp_fx_scale(uint8_t channel, uint8_t applied) {
  // Adafruit_NeoPixel::setBrightness stores b+1 and show() does this.
  return (uint8_t)(((uint16_t)channel * ((uint16_t)applied + 1u)) >> 8);
}

void lamp_fx_init() {
  modesBoot();
  matrixFireReset();
  matrixFireSetIntensity(50);
  memset(g_heat, 0, sizeof g_heat);
  for (uint8_t i = 0; i < 40; i++) matrixFireStep(g_heat);
  memset(g_rgb, 0, sizeof g_rgb);
  g_fxStarted = 0;
  g_fxLast = 0;
  g_fxDirty = 1;
}

void lamp_fx_configure(uint8_t mode, uint8_t speed, uint8_t fader) {
  if (mode >= M_COUNT) mode = M_FIRE;
  if (speed > 100) speed = 100;
  if (fader > 100) fader = 100;
  if (mode != g_fxMode) {
    uint8_t prev = g_fxMode;
    if (prev == M_RADAR && mode != M_RADAR) modesAfterRadar();
    if (mode == M_FIRE && prev == M_RADAR) {
      matrixFireReset();
      matrixFireSetIntensity(50);
      memset(g_heat, 0, sizeof g_heat);
      for (uint8_t i = 0; i < 40; i++) matrixFireStep(g_heat);
    }
  }
  if (mode != g_fxMode || speed != g_fxSpeed || fader != g_fxFader) g_fxDirty = 1;
  g_fxMode = mode;
  g_fxSpeed = speed;
  g_fxFader = fader;
}

void lamp_fx_mark_dirty() { g_fxDirty = 1; }

int lamp_fx_render(uint32_t now) {
  uint32_t period = period_ms();
  uint32_t elapsed = g_fxStarted ? (uint32_t)(now - g_fxLast) : period;
  if (!g_fxDirty && elapsed < period) return 0;
  if (elapsed > period * 3u) elapsed = period * 3u;
  g_fxLast = now;
  g_fxStarted = 1;
  g_fxDirty = 0;
  if (g_fxMode == M_FIRE) {
    matrixFireSetIntensity(50);
    uint32_t steps = elapsed / period;
    if (steps < 1u) steps = 1u;
    if (steps > 3u) steps = 3u;
    for (uint32_t s = 0; s < steps; s++) matrixFireStep(g_heat);
    for (int y = 0; y < LAMP_FX_H; y++) {
      for (int x = 0; x < LAMP_FX_W; x++) {
        FireRgb c = firePalette(g_heat[y * LAMP_FX_W + x]);
        write_visual(x, y, c.r, c.g, c.b);
      }
    }
  } else {
    ModeIn in;
    in.mode = g_fxMode;
    in.speed = g_fxSpeed;
    in.fader = g_fxFader;
    modesStep(&in, now, elapsed, period);
    for (int y = 0; y < LAMP_FX_H; y++) {
      for (int x = 0; x < LAMP_FX_W; x++) {
        uint8_t r, g, b;
        modesColor(&in, x, y, now, &r, &g, &b);
        write_visual(x, y, r, g, b);
      }
    }
  }
  return 1;
}

const uint8_t* lamp_fx_frame() { return g_rgb; }

const char* lamp_fx_mode_name(uint8_t mode) { return modesName(mode); }
int lamp_fx_mode_from_name(const char* s) { return modesFromName(s); }
void lamp_fx_radar_set(const uint8_t* packed, uint8_t count) {
  modesRadarSet(packed, count);
  g_fxDirty = 1;
}
uint8_t* lamp_fx_radar_bytes() { return g_modeRam.radar; }
void lamp_fx_balls_add() { modesMetaAdd(); }
void lamp_fx_balls_remove() { modesMetaRemove(); }
void lamp_fx_balls_set(uint8_t n) { modesMetaSet(n); }
uint8_t lamp_fx_balls() { return modesMetaCount(); }
uint8_t lamp_fx_plasma_zoom() { return modesPlasmaZoomLevel(); }
uint8_t lamp_fx_sun_size() { return modesSunSizeLevel(); }
uint8_t lamp_fx_rotate() { return FX_ROTATE; }
