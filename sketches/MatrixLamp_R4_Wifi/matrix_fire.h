// 4-bit fire for the 12x8 matrix. y = 0 is the top row, y = 7 is the ember bed.
// The bottom row is a solid hot bed (every pixel 12..15). Two or three wide
// plumes (about 3 columns at the core, shoulders one column further out) rise
// together, cool as they climb, and a rare draft lets one tongue lick row 0.
// Host test:
//   g++ -std=c++17 -DMATRIX_FIRE_TEST -x c++ matrix_fire.h -o /tmp/matrix_fire_test

#pragma once

#include <stdint.h>
#include <string.h>

static const int MATRIX_FIRE_W = 12;
static const int MATRIX_FIRE_H = 8;

#if defined(MATRIX_FIRE_TEST)
#include <stdlib.h>
static int matrixRand(int a, int b) {
  if (b <= a) return a;
  return a + (rand() % (b - a));
}
#else
static int matrixRand(int a, int b) {
  if (b <= a) return a;
  return (int)random((long)a, (long)b);
}
#endif

static int8_t g_fireWind = 0;
static uint8_t g_fireWindTick = 0;
static int8_t g_fireSpot[3] = {1, 6, 10};
static uint8_t g_fireSpotReady = 1;
static uint8_t g_fireSurgeSpot = 0;
static uint8_t g_fireSurgeLeft = 0;
static uint8_t g_fireSinceSurge = 0;

// Cool subtracted at each destination row (y = 0 is the top). Upper rows lose
// more, so tips thin out without a hard black cap. Core draft is added back
// on the columns of a wide plume (distance 0 or 1).
static const uint8_t kFireCoolLo[8] = {6, 4, 4, 1, 2, 0, 3, 0};
static const uint8_t kFireCoolHi[8] = {12, 8, 10, 3, 5, 2, 6, 1};
static const uint8_t kFireDraftCore[8] = {2, 2, 1, 1, 1, 1, 0, 0};

static int fireClampX(int x) {
  if (x < 0) return 0;
  if (x >= MATRIX_FIRE_W) return MATRIX_FIRE_W - 1;
  return x;
}

static void fireSeparateSpots() {
  for (int pass = 0; pass < 3; pass++) {
    for (int a = 0; a < 3; a++) {
      for (int b = a + 1; b < 3; b++) {
        int da = (int)g_fireSpot[a];
        int db = (int)g_fireSpot[b];
        int d = da - db;
        if (d < 0) d = -d;
        if (d >= 3) continue;
        if (da <= db) {
          if (da > 0) g_fireSpot[a] = (int8_t)(da - 1);
          if (db < MATRIX_FIRE_W - 1) g_fireSpot[b] = (int8_t)(db + 1);
        } else {
          if (db > 0) g_fireSpot[b] = (int8_t)(db - 1);
          if (da < MATRIX_FIRE_W - 1) g_fireSpot[a] = (int8_t)(da + 1);
        }
      }
    }
  }
}

static void matrixFireReset() {
  g_fireWind = 0;
  g_fireWindTick = 0;
  g_fireSpot[0] = 1;
  g_fireSpot[1] = 6;
  g_fireSpot[2] = 10;
  g_fireSpotReady = 1;
  g_fireSurgeSpot = 0;
  g_fireSurgeLeft = 0;
  g_fireSinceSurge = 0;
}

static int fireNearest(int x, int* bestOut) {
  int best = 99;
  int which = 0;
  for (int s = 0; s < 3; s++) {
    int d = x - (int)g_fireSpot[s];
    if (d < 0) d = -d;
    if (d < best) {
      best = d;
      which = s;
    }
  }
  *bestOut = best;
  return which;
}

// One simulation step. `heat` is 96 levels, row-major, y = 0 at the top.
static void matrixFireStep(uint8_t* heat) {
  if (!g_fireSpotReady) matrixFireReset();
  if (++g_fireWindTick >= 8) {
    g_fireWindTick = 0;
    int next = (int)g_fireWind + matrixRand(0, 3) - 1;
    if (next < -1) next = -1;
    if (next > 1) next = 1;
    g_fireWind = (int8_t)next;
    int s = matrixRand(0, 3);
    int p = (int)g_fireSpot[s] + matrixRand(0, 3) - 1;
    if (p < 0) p = 0;
    if (p > MATRIX_FIRE_W - 1) p = MATRIX_FIRE_W - 1;
    g_fireSpot[s] = (int8_t)p;
    fireSeparateSpots();
  }

  if (g_fireSurgeLeft > 0) {
    g_fireSurgeLeft--;
  } else if (++g_fireSinceSurge >= 48) {
    g_fireSinceSurge = 0;
    g_fireSurgeSpot = (uint8_t)matrixRand(0, 3);
    g_fireSurgeLeft = 8;
  }

  uint8_t row[MATRIX_FIRE_W];
  for (int y = 0; y < MATRIX_FIRE_H - 1; y++) {
    for (int x = 0; x < MATRIX_FIRE_W; x++) {
      int sx = fireClampX(x + (int)g_fireWind);
      int c = heat[(y + 1) * MATRIX_FIRE_W + sx];
      int l = heat[(y + 1) * MATRIX_FIRE_W + fireClampX(sx - 1)];
      int r = heat[(y + 1) * MATRIX_FIRE_W + fireClampX(sx + 1)];
      int ll = heat[(y + 1) * MATRIX_FIRE_W + fireClampX(sx - 2)];
      int rr = heat[(y + 1) * MATRIX_FIRE_W + fireClampX(sx + 2)];
      // Centre three columns count double so a 2–4 column base rises as one
      // tongue instead of a 1-pixel spike. ±2 pulls the shoulders along.
      int v = (c * 2 + l * 2 + r * 2 + ll + rr) / 8;

      int best = 99;
      int which = fireNearest(x, &best);
      int cool = matrixRand((int)kFireCoolLo[y], (int)kFireCoolHi[y]);
      int draft = 0;
      if (best <= 1) draft = (int)kFireDraftCore[y];
      else if (best == 2) cool += 2;
      else cool += matrixRand(1, 4);

      // Row 6 sits on the wide bases: less loss there so the body lifts
      // across 2–4 columns together.
      if (y == 6 && best <= 1) draft += 1;

      int surging = (g_fireSurgeLeft > 0 && which == (int)g_fireSurgeSpot);
      if (surging && best <= 1) {
        draft += 5;
        cool /= 2;
      } else if (surging && best == 2) {
        draft += 2;
      }

      v = v - cool + draft;
      if (v < 0) v = 0;
      if (v > 15) v = 15;
      // No row is forced off. A lick that reaches the top stays visible
      // (2..8) and then recedes; dim noise on row 0 is not a lick.
      if (y == 0) {
        if (v < 2) v = 0;
        if (v > 8) v = 8;
      }
      row[x] = (uint8_t)v;
    }
    memcpy(heat + y * MATRIX_FIRE_W, row, MATRIX_FIRE_W);
  }

  // Ember bed: every column stays very hot. The wandering bases (d <= 1,
  // three columns, and the d == 2 shoulders) are the hottest.
  for (int x = 0; x < MATRIX_FIRE_W; x++) {
    int best = 99;
    fireNearest(x, &best);
    int v;
    if (best <= 1) v = 14 + matrixRand(0, 2);       // 14..15
    else if (best == 2) v = 13 + matrixRand(0, 3);  // 13..15
    else v = 12 + matrixRand(0, 3);                 // 12..14
    heat[(MATRIX_FIRE_H - 1) * MATRIX_FIRE_W + x] = (uint8_t)v;
  }
}

#if defined(MATRIX_FIRE_TEST)
#include <stdio.h>

static const char kFireGlyph[16] = {
  ' ', '.', '.', ':', ':', '-', '-', '=',
  '=', '+', '+', '*', '*', '#', '#', '@'
};

static int g_fireFails = 0;
static void fireExpect(bool cond, const char* msg) {
  if (!cond) {
    printf("FAIL %s\n", msg);
    g_fireFails++;
  } else {
    printf("ok   %s\n", msg);
  }
}

static void firePrintFrame(const uint8_t* heat, const char* title) {
  printf("%s\n", title);
  for (int y = 0; y < 8; y++) {
    printf("%d ", y);
    for (int x = 0; x < 12; x++) {
      int v = heat[y * 12 + x];
      if (v < 0) v = 0;
      if (v > 15) v = 15;
      putchar(kFireGlyph[v]);
    }
    printf("\n");
  }
}

static void fireSample(unsigned seed) {
  srand(seed);
  uint8_t heat[96];
  memset(heat, 0, 96);
  matrixFireReset();
  for (int i = 0; i < 40; i++) matrixFireStep(heat);

  if (seed == 1) {
    for (int f = 0; f < 8; f++) {
      char title[32];
      snprintf(title, sizeof title, "frame %d", 40 + f);
      firePrintFrame(heat, title);
      matrixFireStep(heat);
    }
    // The eight frames above consumed steps. Rewarm from the same seed so
    // the stats below match a plain 40-frame warmup plus 600 samples.
    srand(seed);
    memset(heat, 0, 96);
    matrixFireReset();
    for (int i = 0; i < 40; i++) matrixFireStep(heat);
  }

  const int FRAMES = 600;
  long sum[8] = {0};
  long nz[8] = {0};
  long ge12[8] = {0};
  int mx[8] = {0};
  int mn[8] = {99, 99, 99, 99, 99, 99, 99, 99};
  int posMin[8] = {99, 99, 99, 99, 99, 99, 99, 99};
  long runPx8[8] = {0};
  long runN8[8] = {0};
  long runPx5[8] = {0};
  long runN5[8] = {0};
  long wideBase = 0;
  long wideBaseN = 0;
  int sawLick = 0;
  uint8_t lick[96];
  for (int f = 0; f < FRAMES; f++) {
    matrixFireStep(heat);
    for (int y = 0; y < 8; y++) {
      int run8 = 0;
      int run5 = 0;
      for (int x = 0; x < 12; x++) {
        int v = heat[y * 12 + x];
        sum[y] += v;
        if (v < mn[y]) mn[y] = v;
        if (v > mx[y]) mx[y] = v;
        if (v > 0) {
          nz[y]++;
          if (v < posMin[y]) posMin[y] = v;
        }
        if (v >= 12) ge12[y]++;
        if (v >= 8) run8++;
        else if (run8) {
          runPx8[y] += run8;
          runN8[y]++;
          run8 = 0;
        }
        if (v >= 5) run5++;
        else if (run5) {
          runPx5[y] += run5;
          runN5[y]++;
          run5 = 0;
        }
      }
      if (run8) {
        runPx8[y] += run8;
        runN8[y]++;
      }
      if (run5) {
        runPx5[y] += run5;
        runN5[y]++;
      }
    }
    int row0 = 0;
    for (int x = 0; x < 12; x++) {
      int v = heat[7 * 12 + x];
      int best = 99;
      fireNearest(x, &best);
      if (best <= 1) {
        wideBaseN++;
        if (v >= 14) wideBase++;
      }
      if (heat[x] > 0) row0 = 1;
    }
    if (row0 && !sawLick) {
      sawLick = 1;
      memcpy(lick, heat, 96);
    }
  }
  const long samples = (long)FRAMES * 12;
  printf("seed %u\n", seed);
  printf("row  avg   nz%%   min  max  >=12%%\n");
  for (int y = 0; y < 8; y++) {
    printf("%d   %5.2f  %5.1f  %3d  %3d  %5.1f\n", y,
           (double)sum[y] / (double)samples,
           100.0 * (double)nz[y] / (double)samples,
           mn[y], mx[y],
           100.0 * (double)ge12[y] / (double)samples);
  }
  if (sawLick) firePrintFrame(lick, "lick reaching row 0");

  double avg[8];
  for (int y = 0; y < 8; y++) avg[y] = (double)sum[y] / (double)samples;
  double meanRun6 = runN8[6] ? (double)runPx8[6] / (double)runN8[6] : 0;
  double meanRun4 = runN5[4] ? (double)runPx5[4] / (double)runN5[4] : 0;

  fireExpect(mn[7] >= 12 && ge12[7] == samples, "row 7 is always >= 12");
  fireExpect(avg[7] >= 13.5 && avg[7] <= 14.5, "row 7 avg is 13.5..14.5");
  fireExpect(mx[7] <= 15 && mx[7] >= 14, "row 7 reaches 14..15");
  fireExpect(wideBaseN > 0 && wideBase == wideBaseN, "d<=1 bases stay 14..15");
  fireExpect(avg[6] >= 8.0 && avg[6] <= 12.0, "row 6 avg is 8..12");
  fireExpect(avg[5] >= 8.0 && avg[5] <= 12.0, "row 5 avg is 8..12");
  fireExpect(avg[4] >= 4.0 && avg[4] <= 7.0, "row 4 avg is 4..7");
  fireExpect(avg[3] >= 4.0 && avg[3] <= 7.0, "row 3 avg is 4..7");
  fireExpect(nz[2] * 100 >= samples * 15 && nz[2] * 100 <= samples * 35,
             "row 2 nz is 15..35%");
  fireExpect(nz[1] * 100 >= samples * 5 && nz[1] * 100 <= samples * 18,
             "row 1 nz is 5..18%");
  fireExpect(nz[0] * 100 >= samples * 1 && nz[0] * 100 <= samples * 8,
             "row 0 nz is 1..8%");
  fireExpect(mx[0] >= 2 && mx[0] <= 8 && posMin[0] >= 2 && posMin[0] <= 8,
             "row 0 licks are brightness 2..8");
  fireExpect(meanRun6 >= 3.0, "row 6 bright flames are at least 3 wide");
  fireExpect(meanRun4 >= 2.5, "row 4 flame bodies are wider than one pixel");
  printf("  mean run row6>=8 %.2f  row4>=5 %.2f\n", meanRun6, meanRun4);
}

int main() {
  fireSample(1);
  fireSample(2);
  fireSample(7);

  if (g_fireFails) {
    printf("%d failed\n", g_fireFails);
    return 1;
  }
  printf("all passed\n");
  return 0;
}
#endif
