// Host check for the Off breath. The picture is lamp_core.cpp.
// g++ -std=c++17 -Wall -Wextra -O2 -I sketches/MatrixLamp_R4_Wifi tools/off_breath_test.cpp sketches/MatrixLamp_R4_Wifi/lamp_core.cpp -o /tmp/off_breath_test
// /tmp/off_breath_test
//
// Timing in matrix_modes.h: fade up 3500, hold 1000, fade down 3500,
// dark 1000, frame 20. Each fade is three steady thirds: off, dim red,
// peak yellow. Peak raw is (6, 6, 0). Dim is the red channel only.
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
  uint8_t kind; // 0 black, 1 dim red, 2 peak yellow
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

// n == 1 means one legal pixel: dim red or peak yellow, never brighter.
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
      int dim = (p[0] == peak && p[1] == 0 && p[2] == 0);
      int full = (p[0] == peak && p[1] == peak && p[2] == 0);
      if (!dim && !full) h.n += 1000;
      if (lamp_fx_scale(p[0], applied) > shownPeak) h.n += 10000;
      if (lamp_fx_scale(p[1], applied) > shownPeak) h.n += 10000;
      h.kind = full ? 2 : (dim ? 1 : 0);
    }
  }
  if (h.n == 1 && !logicalOf(h.x, h.y, &h.lx, &h.ly)) h.n += 50;
  return h;
}

static const uint32_t T_END = 108000;

struct Sample {
  uint8_t n;
  uint8_t kind;
  int8_t lx, ly;
  uint8_t r, g;
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
    s->kind = h.kind;
    s->lx = (int8_t)h.lx;
    s->ly = (int8_t)h.ly;
    s->r = h.r;
    s->g = h.g;
  }
}

static const Sample* at(uint32_t t) { return &g_fr[t / 20u]; }

// 0 when every frame in [a, b) is the same steady colour on one pixel.
static int steady(uint32_t a, uint32_t b, uint8_t kind) {
  int bad = 0;
  const Sample* first = at(a);
  for (uint32_t t = a; t < b; t += 20u) {
    const Sample* s = at(t);
    if (kind == 0) {
      if (s->n != 0) bad++;
      continue;
    }
    if (s->n != 1 || s->kind != kind) bad++;
    if (s->lx != first->lx || s->ly != first->ly) bad++;
  }
  return bad;
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
    acc ^= h.kind;
    acc *= 16777619u;
    if (h.n != 0 && h.n != 1) (*bad)++;
    if (h.n == 1 && h.kind == 0) (*bad)++;
    if (h.n == 1 && (h.r != peak || h.b != 0)) (*bad)++;
    if (h.n == 1 && h.kind == 2 && h.g != peak) (*bad)++;
    if (h.n == 1 && h.kind == 1 && h.g != 0) (*bad)++;
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
  expect(s0->n == 1 && s0->kind == 2 && s0->lx == 0 && s0->ly == 15 && s0->r == peak &&
             s0->g == peak,
         "starts at logical (0, 15) at the old peak");

  int bad = 0;
  for (uint32_t t = 0; t <= T_END; t += 20u) {
    if (at(t)->n != 0 && at(t)->n != 1) bad++;
  }
  expect(bad == 0, "every frame is black, dim red, or peak yellow");

  expect(steady(80, 1000, 2) == 0, "fade down starts on steady peak yellow");
  expect(at(80)->lx == 0 && at(80)->ly == 15, "the first step is still the home pixel");
  expect(steady(1400, 2100, 1) == 0, "fade down middle is steady dim red");
  expect(at(1400)->lx == 0 && at(1400)->ly == 15 && at(1400)->g == 0,
         "dim red is the home pixel with green off");
  expect(steady(2600, 3400, 0) == 0, "fade down ends off, with no blinks");
  expect(steady(3700, 4400, 0) == 0, "the dark gap stays off");
  expect(steady(4700, 5400, 0) == 0, "fade up starts off");
  expect(steady(5900, 6600, 1) == 0, "fade up middle is steady dim red");
  expect(steady(7200, 7900, 2) == 0, "fade up ends on steady peak yellow");
  expect(steady(8200, 8900, 2) == 0, "the hold is steady peak yellow");
  const Sample* hold = at(8500);
  expect(hold->n == 1 && hold->kind == 2 && !(hold->lx == 0 && hold->ly == 15),
         "first new pixel is not the home pixel");
  expect(at(5900)->lx == hold->lx && at(5900)->ly == hold->ly,
         "dim and peak of one breath are the same LED");

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
      if (w->n != 1 || w->kind != 2 || w->lx != s->lx || w->ly != s->ly || w->g != peak)
        holdBad++;
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
  expect(back.n == 1 && back.kind == 2 && back.lx == 0 && back.ly == 15 && back.r == peak &&
             back.g == peak,
         "re-entering off restarts at the home peak");

  if (g_fails) {
    printf("%d failed\n", g_fails);
    return 1;
  }
  printf("all ok\n");
  return 0;
}
