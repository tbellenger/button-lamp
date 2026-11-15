// Host check: radar frames in the shared scratch are not drawn once another
// preset has written that scratch, and a published loop still plays.
// g++ -std=c++17 -Wall -Wextra -I sketches/MatrixLamp_R4_Wifi
//   sketches/MatrixLamp_R4_Wifi/lamp_core.cpp tools/radar_scratch_test.cpp
//   -o /tmp/radar_scratch_test -lm && /tmp/radar_scratch_test
#include "lamp_core.h"
#include "mode_ram.h"

#include <stdio.h>
#include <string.h>

static int g_fails = 0;

static void expect(int cond, const char* msg) {
  if (cond) return;
  fprintf(stderr, "FAIL %s\n", msg);
  g_fails++;
}

// Frame buffer is visual order after rotate 3, before the strip serpentine.
static const uint8_t* logicalPx(int x, int y) {
  int xx = x;
  int yy = y;
  int ny = (LAMP_FX_H - 1) - xx;
  xx = yy;
  yy = ny;
  return lamp_fx_frame() + ((yy * LAMP_FX_W + xx) * 3);
}

static int same3(const uint8_t* p, uint8_t r, uint8_t g, uint8_t b) {
  return p[0] == r && p[1] == g && p[2] == b;
}

static void render(uint8_t mode, uint32_t now) {
  lamp_fx_configure(mode, 50, 50);
  lamp_fx_mark_dirty();
  lamp_fx_render(now);
}

static int onlyHome() {
  const uint8_t* home = logicalPx(8, 7);
  if (!same3(home, 128, 128, 128)) return 0;
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      if (x == 8 && y == 7) continue;
      if (!same3(logicalPx(x, y), 0, 0, 0)) return 0;
    }
  }
  return 1;
}

int main() {
  size_t scratch = sizeof(LampFxScratch);
  size_t store = (size_t)RADAR_FRAMES_MAX * RADAR_FRAME_BYTES;
  expect(scratch < store, "scratch shorter than radar store");
  expect(scratch / RADAR_FRAME_BYTES == 19, "scratch covers 19 frames and part of the next");
  expect(store / RADAR_FRAME_BYTES == 26, "store is 26 frames");

  lamp_fx_init();
  uint8_t* storeB = lamp_fx_radar_bytes();
  memset(storeB, 0xAB, store);
  render(M_RADAR, 1000);
  expect(lamp_fx_radar_count() == 0, "boot does not publish frames");
  expect(onlyHome(), "unpublished scratch is the home dot");
  expect(lamp_fx_radar_take_drop() == 1, "entering radar with no loop asks for a fetch");
  expect(lamp_fx_radar_take_drop() == 0, "drop is consumed once");

  memset(storeB, 0, store);
  // Frame 0: file (0,0) level 1 (west, drawn at logical x = 15),
  // file (15,0) level 2 (east, drawn at logical x = 0),
  // file (7,7) level 15 under the home dot.
  storeB[0] = 0x10;
  storeB[7] = 0x02;
  storeB[7 * 8 + 3] = 0x0F;
  // Frame 1 is a solid level-4 field, so the step change is obvious.
  memset(storeB + RADAR_FRAME_BYTES, 0x44, RADAR_FRAME_BYTES);
  lamp_fx_radar_set(storeB, 2);
  expect(lamp_fx_radar_count() == 2, "publish count");

  render(M_RADAR, 0);
  expect(same3(logicalPx(15, 0), 130, 230, 140), "west is file x 0");
  expect(same3(logicalPx(14, 0), 0, 0, 0), "neighbour of west stays empty");
  expect(same3(logicalPx(0, 0), 40, 200, 50), "east is file x 15");
  expect(same3(logicalPx(8, 7), 128, 128, 128), "home is drawn over the echo");
  expect(same3(logicalPx(7, 15), 0, 0, 0), "bar has not reached logical x 7");
  expect(same3(logicalPx(8, 15), 40, 40, 40), "bar starts at logical x 8");
  expect(same3(logicalPx(15, 15), 40, 40, 40), "bar reaches the viewer's left");

  render(M_RADAR, 250);
  expect(same3(logicalPx(1, 0), 220, 220, 40), "second frame plays");
  expect(same3(logicalPx(0, 15), 40, 40, 40), "newest frame fills the bar");
  expect(same3(logicalPx(8, 7), 128, 128, 128), "home stays on the newest frame");

  // One published frame: the count, not the bytes sitting after it.
  storeB[RADAR_FRAME_BYTES] = 0xFF;
  lamp_fx_radar_set(storeB, 1);
  render(M_RADAR, 5000);
  expect(lamp_fx_radar_count() == 1, "partial count");
  expect(same3(logicalPx(15, 0), 130, 230, 140), "single frame stays on frame 0");
  expect(same3(logicalPx(14, 0), 0, 0, 0), "bytes past the count are not a frame");

  // The loop survives another radar configure. It does not survive Off,
  // which rebuilds the scratch on the way out of radar. 0xFF in the store
  // would be level 15 in every cell if that count stayed live.
  render(M_RADAR, 0);
  expect(lamp_fx_radar_count() == 1, "staying on radar keeps the loop");
  memset(storeB, 0xFF, store);
  render(M_OFF, 8000);
  expect(lamp_fx_radar_count() == 0, "leaving radar drops the loop");
  for (uint32_t t = 0; t < 8000; t += 250) {
    render(M_RADAR, t);
    if (!onlyHome() || lamp_fx_radar_count() != 0) {
      fprintf(stderr, "FAIL garbage frame at %u\n", t);
      g_fails++;
      break;
    }
  }

  // A publish while Off is already showing is still intact: Off does not
  // write the scratch. Entering radar plays it. Fire, matrix, spiral, and
  // fluid do write it, so the same publish is dropped on the next step.
  render(M_OFF, 9000);
  memset(storeB, 0, store);
  storeB[0] = 0x10;
  lamp_fx_radar_set(storeB, 1);
  render(M_OFF, 9100);
  expect(lamp_fx_radar_count() == 1, "off leaves an intact publish");
  render(M_RADAR, 0);
  expect(lamp_fx_radar_count() == 1, "radar enter keeps an intact publish");
  expect(same3(logicalPx(15, 0), 130, 230, 140), "intact publish still draws");

  render(M_FIRE, 10000);
  lamp_fx_radar_set(storeB, 1);
  render(M_FIRE, 10100);
  expect(lamp_fx_radar_count() == 0, "fire claims the scratch");

  lamp_fx_radar_set(storeB, 1);
  render(M_MATRIX, 11000);
  expect(lamp_fx_radar_count() == 0, "matrix rain claims the scratch");

  lamp_fx_radar_set(storeB, 1);
  render(M_SPIRAL, 12000);
  expect(lamp_fx_radar_count() == 0, "spiral claims the scratch");

  lamp_fx_radar_set(storeB, 1);
  render(M_FLUID, 13000);
  expect(lamp_fx_radar_count() == 0, "fluid claims the scratch");

  lamp_fx_radar_set(storeB, 1);
  render(M_RADAR, 0);
  expect(lamp_fx_radar_count() == 1, "republish on radar");
  render(M_LIGHTNING, 14000);
  expect(lamp_fx_radar_count() == 0, "leaving to lightning drops the loop");
  render(M_RADAR, 14000);
  expect(onlyHome(), "lightning leave then radar is the home dot");

  if (g_fails) {
    fprintf(stderr, "%d failed\n", g_fails);
    return 1;
  }
  printf("ok\n");
  return 0;
}
