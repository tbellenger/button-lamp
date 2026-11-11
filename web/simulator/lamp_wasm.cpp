// Emscripten entry. The animation is lamp_core.cpp, the same file the sketch links.
#include <stdint.h>
#include <string.h>

#include "lamp_core.h"

static uint32_t g_now = 0;
static uint8_t g_show[LAMP_FX_N * 3];

extern "C" {

void lamp_init(void) {
  g_now = 0;
  lamp_fx_init();
  lamp_fx_configure(M_FIRE, 50, 50);
}

void lamp_set(int mode, int speed, int fader) {
  if (mode < 0) mode = 0;
  if (speed < 0) speed = 0;
  if (fader < 0) fader = 0;
  lamp_fx_configure((uint8_t)mode, (uint8_t)speed, (uint8_t)fader);
}

void lamp_mark_dirty(void) { lamp_fx_mark_dirty(); }

int lamp_mode_count(void) { return M_COUNT; }

const char* lamp_mode_name(int mode) {
  if (mode < 0) mode = 0;
  return lamp_fx_mode_name((uint8_t)mode);
}

void lamp_balls_add(void) {
  lamp_fx_balls_add();
  lamp_fx_mark_dirty();
}
void lamp_balls_remove(void) {
  lamp_fx_balls_remove();
  lamp_fx_mark_dirty();
}
void lamp_balls_set(int n) {
  if (n < 0) n = 0;
  lamp_fx_balls_set((uint8_t)n);
  lamp_fx_mark_dirty();
}
int lamp_balls(void) { return lamp_fx_balls(); }

int lamp_step(int dt_ms) {
  if (dt_ms < 0) dt_ms = 0;
  if (dt_ms > 250) dt_ms = 250;
  g_now += (uint32_t)dt_ms;
  return lamp_fx_render(g_now);
}

// Brightness-scaled RGB in visual order, matching NeoPixel show().
const uint8_t* lamp_frame(int fader) {
  if (fader < 0) fader = 0;
  if (fader > 100) fader = 100;
  uint8_t applied = lamp_fx_applied((uint8_t)fader);
  const uint8_t* raw = lamp_fx_frame();
  for (int i = 0; i < LAMP_FX_N * 3; i++) {
    g_show[i] = lamp_fx_scale(raw[i], applied);
  }
  return g_show;
}

int lamp_applied(int fader) {
  if (fader < 0) fader = 0;
  if (fader > 100) fader = 100;
  return lamp_fx_applied((uint8_t)fader);
}

}
