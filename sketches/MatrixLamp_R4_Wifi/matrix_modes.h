// Spiral-lamp modes, drawn on the 16x16 panel.
// y = 0 is the top of the picture, y = 15 the bottom (the end nearest the
// fader on the real lamp). A 1D effect walks a serpentine path that starts
// at the bottom-left and climbs, so "LED 0" stays the bottom of the strip.
//
//   fire     switches at rest. The matrix fire in the sketch, not this file.
//   jumbo    rocker on. Bottom-up wipe of the jumbo colour. Toggle cycles it.
//   classic  rocker on after the arcade sequence rlrrlrll. Solid, knob colour.
//   rainbow  key. Hue runs along the path. Toggle reverses it.
//   wave     covered toggle. A band ripples up the rows.
//   wave-mod wave + toggle. The band crossfades a colour pair.
//   wave-rainbow  wave + key. Each row a hue, band still travels.
//   wave-epic     wave + key + toggle. One bright flashing row.
//   secret   rocker + toggle + key + wave. Knob fills from the bottom.
//   lava     mushroom. Two balls toss on the grid. Fire trail if enabled.
//   music    toggle only. A short phrase walks the path (no MIDI on this lamp).

#pragma once

#include <stdint.h>
#include <string.h>

static const uint8_t M_FIRE = 0;
static const uint8_t M_JUMBO = 1;
static const uint8_t M_CLASSIC = 2;
static const uint8_t M_RAINBOW = 3;
static const uint8_t M_WAVE = 4;
static const uint8_t M_WAVE_MOD = 5;
static const uint8_t M_WAVE_RAINBOW = 6;
static const uint8_t M_WAVE_EPIC = 7;
static const uint8_t M_SECRET = 8;
static const uint8_t M_LAVA = 9;
static const uint8_t M_MUSIC = 10;

struct ModeIn {
  uint8_t mode;
  uint8_t speed;
  uint8_t fader;
  uint8_t top;
  uint8_t bottom;
  uint8_t fireTrail;
  uint8_t toggle;
};

static int modeSpeed256(uint8_t s) {
  if (s <= 50) return 64 + (int)s * (256 - 64) / 50;
  return 256 + (int)(s - 50) * (1024 - 256) / 50;
}

static int effectPath(int x, int y) {
  int yb = (PANEL_H - 1) - y;
  int xx = (yb & 1) ? (PANEL_W - 1 - x) : x;
  return yb * PANEL_W + xx;
}

static void mixTo(uint8_t* r, uint8_t* g, uint8_t* b,
                  uint8_t ar, uint8_t ag, uint8_t ab,
                  uint8_t br, uint8_t bg, uint8_t bb, int t256) {
  if (t256 < 0) t256 = 0;
  if (t256 > 256) t256 = 256;
  *r = (uint8_t)((ar * (256 - t256) + br * t256) >> 8);
  *g = (uint8_t)((ag * (256 - t256) + bg * t256) >> 8);
  *b = (uint8_t)((ab * (256 - t256) + bb * t256) >> 8);
}

static void lavaPalette(uint8_t i, uint8_t* r, uint8_t* g, uint8_t* b) {
  static const uint8_t P[7][3] = {
    {255, 0, 0}, {170, 0, 255}, {0, 0, 255}, {0, 255, 0},
    {255, 112, 0}, {255, 50, 0}, {0, 0, 0}
  };
  i = (uint8_t)(i % 7);
  *r = P[i][0];
  *g = P[i][1];
  *b = P[i][2];
}

static void scalePix(uint8_t* r, uint8_t* g, uint8_t* b, int level) {
  if (level < 0) level = 0;
  if (level > 255) level = 255;
  *r = (uint8_t)((uint16_t)(*r) * (uint16_t)level / 255);
  *g = (uint8_t)((uint16_t)(*g) * (uint16_t)level / 255);
  *b = (uint8_t)((uint16_t)(*b) * (uint16_t)level / 255);
}

static void maxPix(uint8_t* r, uint8_t* g, uint8_t* b, uint8_t ar, uint8_t ag, uint8_t ab) {
  if (ar > *r) *r = ar;
  if (ag > *g) *g = ag;
  if (ab > *b) *b = ab;
}

static uint8_t g_modeWas = 255;
static uint16_t g_jProg = 0;
static uint8_t g_jIdx = 0;
static uint32_t g_jMs = 0;
static uint32_t g_rainPhase = 0;
static int16_t g_ballX[2];
static int16_t g_ballY[2];
static int16_t g_ballVx[2];
static int16_t g_ballVy[2];
static uint8_t g_ballSlot[2];
static const int8_t kLaunch[5] = {18, 26, 40, 30, 22};

struct NotePulse {
  int16_t pos;
  int8_t vel;
  uint8_t hue;
  uint8_t life;
};
static NotePulse g_notes[4];
static uint8_t g_phrase = 0;
static uint32_t g_noteMs = 0;
static const uint8_t kPhrase[8] = {0, 4, 7, 0, 9, 5, 2, 7};

static void modesResetBalls() {
  g_ballX[0] = 7 * 16;
  g_ballY[0] = 14 * 16;
  g_ballVx[0] = 5;
  g_ballVy[0] = -40;
  g_ballSlot[0] = 2;
  g_ballX[1] = 9 * 16;
  g_ballY[1] = 1 * 16;
  g_ballVx[1] = -4;
  g_ballVy[1] = 26;
  g_ballSlot[1] = 0;
}

static void modesResetNotes() {
  memset(g_notes, 0, sizeof g_notes);
  g_phrase = 0;
  g_noteMs = 0;
}

static void jumboRgb(uint8_t idx, uint8_t* r, uint8_t* g, uint8_t* b) {
  static const uint8_t C[7][3] = {
    {255, 255, 255},
    {255, 0, 0}, {170, 0, 255}, {0, 0, 255},
    {0, 255, 0}, {255, 112, 0}, {255, 50, 0}
  };
  if (idx > 6) idx = 0;
  *r = C[idx][0];
  *g = C[idx][1];
  *b = C[idx][2];
}

static void classicRgb(uint8_t speed, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (speed <= 50) {
    int t = (int)speed * 256 / 50;
    mixTo(r, g, b, 255, 50, 0, 255, 112, 0, t);
  } else {
    int t = (int)(speed - 50) * 256 / 50;
    mixTo(r, g, b, 255, 112, 0, 255, 255, 255, t);
  }
}

static void secretGradient(int path, uint8_t* r, uint8_t* g, uint8_t* b) {
  static const uint8_t G[7][3] = {
    {148, 0, 211}, {75, 0, 130}, {0, 0, 255}, {0, 255, 0},
    {255, 112, 0}, {255, 50, 0}, {255, 0, 0}
  };
  int f = path * 6;
  int k = f / 255;
  int t = (f - k * 255) * 256 / 255;
  if (k > 5) { k = 5; t = 256; }
  mixTo(r, g, b, G[k][0], G[k][1], G[k][2], G[k + 1][0], G[k + 1][1], G[k + 1][2], t);
}

static void secretSolid(uint8_t fader, uint8_t* r, uint8_t* g, uint8_t* b) {
  static const uint8_t S[6][3] = {
    {0, 0, 255}, {0, 255, 0}, {255, 112, 0}, {255, 50, 0}, {255, 0, 0}, {148, 0, 211}
  };
  int f = (int)fader * 5;
  int k = f / 100;
  int t = (f - k * 100) * 256 / 100;
  if (k > 4) { k = 4; t = 256; }
  mixTo(r, g, b, S[k][0], S[k][1], S[k][2], S[k + 1][0], S[k + 1][1], S[k + 1][2], t);
}

static void noteRgb(uint8_t h, uint8_t* r, uint8_t* g, uint8_t* b) {
  static const uint8_t T[12][3] = {
    {255, 40, 40}, {255, 120, 20}, {255, 200, 40}, {180, 255, 40},
    {40, 255, 80}, {20, 255, 180}, {40, 200, 255}, {40, 80, 255},
    {140, 40, 255}, {220, 40, 255}, {255, 40, 180}, {255, 40, 100}
  };
  h = (uint8_t)(h % 12);
  *r = T[h][0];
  *g = T[h][1];
  *b = T[h][2];
}

static void pairRgb(uint8_t step, int t256, uint8_t* r, uint8_t* g, uint8_t* b) {
  static const uint8_t A[6][3] = {
    {0, 0, 255}, {0, 255, 0}, {255, 130, 0}, {255, 50, 0}, {255, 0, 0}, {170, 0, 255}
  };
  static const uint8_t B[6][3] = {
    {0, 255, 0}, {255, 130, 0}, {255, 50, 0}, {255, 0, 0}, {170, 0, 255}, {0, 0, 255}
  };
  step = (uint8_t)(step % 6);
  mixTo(r, g, b, A[step][0], A[step][1], A[step][2], B[step][0], B[step][1], B[step][2], t256);
}

static void bounceBall(uint8_t i, int floorY, int dir) {
  g_ballSlot[i] = (uint8_t)((g_ballSlot[i] + 1) % 5);
  int mag = kLaunch[g_ballSlot[i]];
  g_ballVy[i] = (int16_t)(dir < 0 ? -mag : mag);
  g_ballY[i] = (int16_t)floorY;
}

static void modesStep(const ModeIn* in, uint32_t now) {
  int sp = modeSpeed256(in->speed);
  if (in->mode != g_modeWas) {
    if (in->mode == M_JUMBO || in->mode == M_CLASSIC) g_jProg = 0;
    if (in->mode == M_LAVA) modesResetBalls();
    if (in->mode == M_MUSIC) modesResetNotes();
    g_jMs = now;
    g_noteMs = now;
    g_modeWas = in->mode;
  }

  if (in->mode == M_JUMBO || in->mode == M_CLASSIC) {
    int add = (8 * sp) / 256;
    if (add < 1) add = 1;
    if (g_jProg < 280) {
      g_jProg = (uint16_t)(g_jProg + add);
      if (g_jProg > 280) g_jProg = 280;
    }
    if (in->mode == M_JUMBO && in->toggle) {
      uint32_t gap = (uint32_t)(700 * 256 / sp);
      if (gap < 80) gap = 80;
      if ((uint32_t)(now - g_jMs) >= gap) {
        g_jMs = now;
        g_jIdx = (uint8_t)((g_jIdx + 1) % 7);
      }
    }
  }

  if (in->mode == M_RAINBOW) {
    int dir = in->toggle ? -1 : 1;
    int add = (6 * sp) / 256;
    if (add < 1) add = 1;
    g_rainPhase = (uint32_t)((int32_t)g_rainPhase + (int32_t)dir * add);
  }

  if (in->mode == M_LAVA) {
    for (uint8_t i = 0; i < 2; i++) {
      int y = (int)g_ballY[i] + (int)g_ballVy[i];
      int x = (int)g_ballX[i] + (int)g_ballVx[i];
      int vy = (int)g_ballVy[i] + 4;
      if (x < 16) { x = 16; g_ballVx[i] = (int16_t)(-g_ballVx[i]); }
      if (x > 14 * 16) { x = 14 * 16; g_ballVx[i] = (int16_t)(-g_ballVx[i]); }
      if (y >= 14 * 16) bounceBall(i, 14 * 16, -1);
      else if (y <= 8) bounceBall(i, 8, +1);
      else {
        if (vy > 120) vy = 120;
        if (vy < -120) vy = -120;
        g_ballVy[i] = (int16_t)vy;
        g_ballY[i] = (int16_t)y;
      }
      g_ballX[i] = (int16_t)x;
    }
  }

  if (in->mode == M_MUSIC) {
    uint32_t gap = (uint32_t)(520 * 256 / sp);
    if (gap < 90) gap = 90;
    if ((uint32_t)(now - g_noteMs) >= gap) {
      g_noteMs = now;
      for (uint8_t i = 0; i < 4; i++) {
        if (g_notes[i].life == 0) {
          g_notes[i].pos = 0;
          g_notes[i].vel = (int8_t)(2 + sp / 256);
          if (g_notes[i].vel < 1) g_notes[i].vel = 1;
          g_notes[i].hue = kPhrase[g_phrase];
          g_notes[i].life = 255;
          g_phrase = (uint8_t)((g_phrase + 1) % 8);
          break;
        }
      }
    }
    for (uint8_t i = 0; i < 4; i++) {
      if (!g_notes[i].life) continue;
      g_notes[i].pos = (int16_t)(g_notes[i].pos + g_notes[i].vel);
      if (g_notes[i].life > 4) g_notes[i].life = (uint8_t)(g_notes[i].life - 4);
      else g_notes[i].life = 0;
      if (g_notes[i].pos > 255) g_notes[i].life = 0;
    }
  }
}

static void paintLava(const ModeIn* in, int x, int y, uint8_t* r, uint8_t* g, uint8_t* b) {
  int px = x * 16 + 8;
  int py = y * 16 + 8;
  *r = *g = *b = 0;
  for (uint8_t i = 0; i < 2; i++) {
    uint8_t cr, cg, cb;
    lavaPalette(i == 0 ? in->bottom : in->top, &cr, &cg, &cb);
    int32_t dx = (int32_t)px - g_ballX[i];
    int32_t dy = (int32_t)py - g_ballY[i];
    int32_t dist2 = dx * dx + dy * dy;
    const int rad = 34;
    if (dist2 <= (int32_t)rad * rad) {
      int dist = 0;
      while ((int32_t)(dist + 1) * (dist + 1) <= dist2 && dist < rad) dist++;
      int level = (rad - dist) * 255 / rad;
      uint8_t pr = cr, pg = cg, pb = cb;
      scalePix(&pr, &pg, &pb, level);
      maxPix(r, g, b, pr, pg, pb);
      continue;
    }
    if (!in->fireTrail) continue;
    int32_t vx = g_ballVx[i];
    int32_t vy = g_ballVy[i];
    int32_t spd2 = vx * vx + vy * vy;
    if (spd2 < 36) continue;
    int32_t behind = -(dx * vx + dy * vy);
    if (behind <= 0) continue;
    int spd = 1;
    while ((int32_t)(spd + 1) * (spd + 1) <= spd2 && spd < 200) spd++;
    int along = (int)(behind / spd);
    int lat2 = (int)(dist2 - (int32_t)along * along);
    if (lat2 > 26 * 26) continue;
    int tail = 36 + spd * 2;
    if (tail > 120) tail = 120;
    if (along > tail) continue;
    int t = along * 255 / tail;
    uint8_t fr, fg, fb;
    if (t < 80) {
      fr = 255;
      fg = (uint8_t)(40 + 50 * t / 80);
      fb = 0;
    } else if (t < 160) {
      fr = 255;
      fg = (uint8_t)(90 + 165 * (t - 80) / 80);
      fb = 0;
    } else {
      fr = 255;
      fg = 255;
      fb = (uint8_t)(255 * (t - 160) / 95);
    }
    int level = (tail - along) * 255 / tail;
    scalePix(&fr, &fg, &fb, level);
    maxPix(r, g, b, fr, fg, fb);
  }
}

static void paintMusic(int x, int y, uint32_t now, uint8_t* r, uint8_t* g, uint8_t* b) {
  int path = effectPath(x, y);
  *r = *g = *b = 0;
  int idlePos = (int)((now / 28) % 256);
  int idleD = path - idlePos;
  if (idleD < 0) idleD = -idleD;
  if (idleD < 10) {
    uint8_t ir, ig, ib;
    noteRgb((uint8_t)((now / 700) % 12), &ir, &ig, &ib);
    scalePix(&ir, &ig, &ib, (10 - idleD) * 8);
    maxPix(r, g, b, ir, ig, ib);
  }
  for (uint8_t i = 0; i < 4; i++) {
    if (!g_notes[i].life) continue;
    int d = path - g_notes[i].pos;
    if (d < 0) d = -d;
    if (d > 10) continue;
    uint8_t nr, ng, nb;
    noteRgb(g_notes[i].hue, &nr, &ng, &nb);
    int level = (10 - d) * (int)g_notes[i].life / 10;
    scalePix(&nr, &ng, &nb, level);
    maxPix(r, g, b, nr, ng, nb);
  }
}

static void modesColor(const ModeIn* in, int x, int y, uint32_t now, uint8_t* r, uint8_t* g, uint8_t* b) {
  *r = *g = *b = 0;
  int sp = modeSpeed256(in->speed);
  if (in->mode == M_JUMBO || in->mode == M_CLASSIC) {
    jumboRgb(in->mode == M_CLASSIC ? 0 : g_jIdx, r, g, b);
    if (in->mode == M_CLASSIC) classicRgb(in->speed, r, g, b);
    int yb = (PANEL_H - 1) - y;
    int need = yb * 16;
    int bright = (int)g_jProg - need;
    if (bright < 0) bright = 0;
    if (bright > 32) bright = 32;
    scalePix(r, g, b, bright * 8);
    return;
  }
  if (in->mode == M_RAINBOW) {
    int path = effectPath(x, y);
    int t = (int)((g_rainPhase + (uint32_t)path * 8) & 255);
    int seg = t * 6 / 256;
    int u = (t * 6 - seg * 256);
    static const uint8_t R[7] = {255, 255, 0, 0, 0, 170, 255};
    static const uint8_t G[7] = {0, 80, 255, 255, 0, 0, 0};
    static const uint8_t B[7] = {0, 0, 0, 40, 255, 255, 0};
    if (seg > 5) seg = 5;
    mixTo(r, g, b, R[seg], G[seg], B[seg], R[seg + 1], G[seg + 1], B[seg + 1], u);
    return;
  }
  if (in->mode == M_WAVE || in->mode == M_WAVE_MOD ||
      in->mode == M_WAVE_RAINBOW || in->mode == M_WAVE_EPIC) {
    int yb = (PANEL_H - 1) - y;
    int span = PANEL_H * 6;
    int head = (int)(((uint32_t)now * (uint32_t)sp / 256u) / 18u) % (uint32_t)span;
    int d = yb * 6 - (head % (PANEL_H * 6));
    if (d < 0) d = -d;
    if (d > PANEL_H * 3) d = PANEL_H * 6 - d;
    int level = 255 - d * 28;
    if (level < 0) level = 0;
    if (in->mode == M_WAVE_EPIC) {
      level = (d < 4) ? 255 : 0;
      pairRgb((uint8_t)((now / 140) % 6), 0, r, g, b);
    } else if (in->mode == M_WAVE_MOD) {
      int t = (int)((now / 12) & 255);
      pairRgb((uint8_t)((now / 1800) % 6), t, r, g, b);
    } else if (in->mode == M_WAVE_RAINBOW) {
      int t = (yb * 24 + (int)(now / 16)) & 255;
      int seg = t * 6 / 256;
      int u = t * 6 - seg * 256;
      static const uint8_t R[7] = {255, 255, 0, 0, 0, 170, 255};
      static const uint8_t G[7] = {0, 90, 255, 200, 0, 0, 0};
      static const uint8_t B[7] = {0, 0, 0, 40, 255, 255, 0};
      if (seg > 5) seg = 5;
      mixTo(r, g, b, R[seg], G[seg], B[seg], R[seg + 1], G[seg + 1], B[seg + 1], u);
    } else {
      *r = 255;
      *g = 255;
      *b = 255;
    }
    scalePix(r, g, b, level);
    return;
  }
  if (in->mode == M_SECRET) {
    int path = effectPath(x, y);
    int lit = (int)in->speed * 256 / 100;
    if (path >= lit) return;
    uint8_t sr, sg, sb, gr, gg, gb;
    secretSolid(in->fader, &sr, &sg, &sb);
    secretGradient(path, &gr, &gg, &gb);
    int w = 0;
    if (in->fader < 15) w = (15 - (int)in->fader) * 256 / 15;
    mixTo(r, g, b, sr, sg, sb, gr, gg, gb, w);
    return;
  }
  if (in->mode == M_LAVA) {
    paintLava(in, x, y, r, g, b);
    return;
  }
  if (in->mode == M_MUSIC) {
    paintMusic(x, y, now, r, g, b);
    return;
  }
}
