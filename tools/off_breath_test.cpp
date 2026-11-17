// Host check for the Off breath. The picture is lamp_core.cpp.
// g++ -std=c++17 -Wall -Wextra -O2 -I sketches/MatrixLamp_R4_Wifi tools/off_breath_test.cpp sketches/MatrixLamp_R4_Wifi/lamp_core.cpp -o /tmp/off_breath_test
// /tmp/off_breath_test
//
// Timing in matrix_modes.h: fade up 3500, hold 1000, fade down 3500,
// dark 1000, frame 20. Peak raw yellow is (6, 6, 0).
#include "lamp_core.h"

#include <stdio.h>
#include <string.h>

static int g_fails = 0;

static void expect(bool cond, const char* msg) {
  if (!cond) {
    printf("FAIL %s\n", msg);
    g_fails++;
  } else {
    printf("ok   %s\n", msg);
  }
}

// Same floor paintOff uses.
static uint8_t peakChannel(uint8_t applied) {
  const uint8_t raw = 6;
  if (lamp_fx_scale(raw, applied) != 0) return raw;
  uint16_t mul = (uint16_t)applied + 1u;
  uint16_t need = (uint16_t)((256u + mul - 1u) / mul);
  if (need > 255u) return raw;
  return (uint8_t)need;
}

struct Hit {
  int n;
  int x, y;
  int lx, ly;
  uint8_t r, g, b;
};

static bool logicalOf(int vx, int vy, int* lx, int* ly) {
  uint16_t strip = lamp_fx_led_index(vx, vy);
  int found = 0;
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      if (lamp_fx_led_from_logical(x, y) != strip) continue;
      *lx = x;
      *ly = y;
      found++;
    }
  }
  return found == 1;
}

// n == 1 means one pixel at today's peak. Anything else bumps n.
static Hit scan(uint8_t fader) {
  Hit h;
  memset(&h, 0, sizeof h);
  h.x = h.y = h.lx = h.ly = -1;
  uint8_t applied = lamp_fx_applied(fader);
  uint8_t peak = peakChannel(applied);
  uint8_t shownPeak = lamp_fx_scale(peak, applied);
  const uint8_t* fb = lamp_fx_frame();
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      const uint8_t* p = fb + ((y * 16 + x) * 3);
      if (p[0] == 0 && p[1] == 0 && p[2] == 0) continue;
      h.n++;
      h.x = x;
      h.y = y;
      h.r = p[0];
      h.g = p[1];
      h.b = p[2];
      if (p[2] != 0 || p[0] != p[1]) h.n += 100;
      if (p[0] != peak) h.n += 1000;
      if (lamp_fx_scale(p[0], applied) > shownPeak) h.n += 10000;
    }
  }
  if (h.n == 1 && !logicalOf(h.x, h.y, &h.lx, &h.ly)) h.n += 50;
  return h;
}

static const uint32_t T_END = 108000;

struct Sample {
  uint8_t n;
  int8_t lx, ly;
  uint8_t r;
};

static Sample g_fr[T_END / 20 + 1];

static void record(uint8_t speed, uint8_t fader) {
  lamp_fx_init();
  lamp_fx_configure(M_OFF, speed, fader);
  for (uint32_t t = 0; t <= T_END; t += 20u) {
    lamp_fx_render(t);
    Hit h = scan(fader);
    Sample* s = &g_fr[t / 20u];
    s->n = (uint8_t)(h.n > 255 ? 255 : h.n);
    s->lx = (int8_t)h.lx;
    s->ly = (int8_t)h.ly;
    s->r = h.r;
  }
}

static const Sample* at(uint32_t t) { return &g_fr[t / 20u]; }

static int onPerMil(uint32_t a, uint32_t b) {
  int on = 0;
  int frames = 0;
  for (uint32_t t = a; t < b; t += 20u) {
    frames++;
    if (at(t)->n == 1) on++;
  }
  return frames ? (on * 1000) / frames : -1;
}

static uint32_t runHash(uint8_t speed, uint8_t fader, int* bad) {
  lamp_fx_init();
  lamp_fx_configure(M_OFF, speed, fader);
  uint8_t peak = peakChannel(lamp_fx_applied(fader));
  uint32_t acc = 2166136261u;
  for (uint32_t t = 0; t <= 20000u; t += 20u) {
    lamp_fx_render(t);
    Hit h = scan(fader);
    acc ^= (uint32_t)(h.lx + 17);
    acc *= 16777619u;
    acc ^= (uint32_t)(h.ly + 29);
    acc *= 16777619u;
    acc ^= (uint32_t)(h.n == 1 ? 1u : 0u);
    acc *= 16777619u;
    if (h.n != 0 && h.n != 1) (*bad)++;
    if (h.n == 1 && (h.r != peak || h.g != peak || h.b != 0)) (*bad)++;
  }
  return acc;
}

int main() {
  const uint8_t fader = 100;
  uint8_t applied = lamp_fx_applied(fader);
  uint8_t peak = peakChannel(applied);
  printf("fader %u applied %u peak raw %u shown %u\n", fader, applied, peak,
         lamp_fx_scale(peak, applied));
  expect(peak == 7, "fader 100 peak raw is 7");
  expect(lamp_fx_scale(peak, applied) == 1, "fader 100 shows as 1");
  expect(lamp_fx_scale((uint8_t)(peak - 1), applied) == 0, "one below the floor is off");

  record(50, fader);
  const Sample* s0 = at(0);
  expect(s0->n == 1 && s0->lx == 0 && s0->ly == 15 && s0->r == peak,
         "starts at logical (0, 15) at the old peak");

  int bad = 0;
  for (uint32_t t = 0; t <= T_END; t += 20u) {
    if (at(t)->n != 0 && at(t)->n != 1) bad++;
  }
  expect(bad == 0, "every frame is black or one peak pixel");

  expect(onPerMil(3800, 4200) == 0, "middle of the first dark gap is black");
  expect(onPerMil(8300, 8700) == 1000, "middle of the first hold is solid");
  const Sample* hold = at(8500);
  expect(hold->n == 1 && !(hold->lx == 0 && hold->ly == 15),
         "first new pixel is not the home pixel");

  int earlyDown = onPerMil(200, 600);
  int lateDown = onPerMil(2800, 3200);
  int earlyUp = onPerMil(4700, 5100);
  int lateUp = onPerMil(7400, 7800);
  printf("duty down %d -> %d, up %d -> %d (per 1000)\n", earlyDown, lateDown, earlyUp, lateUp);
  expect(earlyDown > 800 && lateDown < 250 && earlyDown > lateDown, "fade down eases off");
  expect(earlyUp < 250 && lateUp > 800 && lateUp > earlyUp, "fade up eases on");

  int prev = 15 * 16;
  int distinct = 1;
  uint8_t seen[256];
  memset(seen, 0, sizeof seen);
  seen[prev] = 1;
  int repeats = 0;
  int holdBad = 0;
  for (int c = 0; c < 12; c++) {
    uint32_t t = 8500u + (uint32_t)c * 9000u;
    const Sample* s = at(t);
    if (s->n != 1) {
      holdBad++;
      continue;
    }
    int id = (int)s->ly * 16 + (int)s->lx;
    if (id == prev) repeats++;
    if ((unsigned)id < 256u && !seen[id]) {
      seen[id] = 1;
      distinct++;
    }
    for (uint32_t u = t - 200u; u <= t + 200u; u += 20u) {
      const Sample* w = at(u);
      if (w->n != 1 || w->lx != s->lx || w->ly != s->ly || w->r != peak) holdBad++;
    }
    prev = id;
  }
  printf("distinct hold pixels %d, consecutive repeats %d\n", distinct, repeats);
  expect(repeats == 0, "a new pixel is never the previous one");
  expect(distinct >= 8, "pixels spread across the grid");
  expect(holdBad == 0, "each hold stays on one peak pixel");

  int badRun = 0;
  uint32_t slow = runHash(0, fader, &badRun);
  uint32_t fast = runHash(100, fader, &badRun);
  expect(slow == fast && slow != 0, "speed does not change the breath");

  uint32_t h100 = runHash(40, 100, &badRun);
  uint32_t h20 = runHash(40, 20, &badRun);
  uint32_t h0 = runHash(40, 0, &badRun);
  printf("positions fader100 %08x fader20 %08x fader0 %08x\n", h100, h20, h0);
  uint8_t p20 = peakChannel(lamp_fx_applied(20));
  uint8_t p0 = peakChannel(lamp_fx_applied(0));
  printf("fader 20 peak raw %u shown %u; fader 0 peak raw %u shown %u\n", p20,
         lamp_fx_scale(p20, lamp_fx_applied(20)), p0, lamp_fx_scale(p0, 0));
  expect(h100 == h20 && h100 == h0, "fader does not move the pixel");
  expect(badRun == 0, "no frame paints above today's peak");
  expect(p20 == 29 && lamp_fx_scale(p20, lamp_fx_applied(20)) == 1,
         "fader 20 peak is the old floor");
  expect(p0 == 6 && lamp_fx_scale(p0, 0) == 0, "fader 0 still cannot show the pixel");

  lamp_fx_init();
  lamp_fx_configure(M_FIRE, 50, 80);
  for (uint32_t t = 0; t <= 400; t += 20) lamp_fx_render(t);
  int fireLit = 0;
  const uint8_t* fb = lamp_fx_frame();
  for (int i = 0; i < 256 * 3; i++)
    if (fb[i]) fireLit++;
  expect(fireLit > 20, "fire still paints");
  lamp_fx_configure(M_OFF, 50, fader);
  lamp_fx_render(5000);
  Hit back = scan(fader);
  expect(back.n == 1 && back.lx == 0 && back.ly == 15 && back.r == peak,
         "re-entering off restarts at the home peak");

  if (g_fails) {
    printf("%d failed\n", g_fails);
    return 1;
  }
  printf("all ok\n");
  return 0;
}
