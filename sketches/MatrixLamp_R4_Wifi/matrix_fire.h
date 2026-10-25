// 4-bit fire for the 12x8 matrix. y = 0 is the top row, y = 7 is the ember bed.
// Heat moves up from the row below, then cools. A height cap keeps the top dark.
// Host test: g++ -std=c++17 -DMATRIX_FIRE_TEST -x c++ matrix_fire.h -o /tmp/matrix_fire_test

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
static int8_t g_fireSpot[3] = {2, 6, 10};
static uint8_t g_fireSpotReady = 1;

static void matrixFireReset() {
  g_fireWind = 0;
  g_fireWindTick = 0;
  g_fireSpot[0] = 2;
  g_fireSpot[1] = 6;
  g_fireSpot[2] = 10;
  g_fireSpotReady = 1;
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
  }

  uint8_t row[MATRIX_FIRE_W];
  for (int y = 0; y < MATRIX_FIRE_H - 1; y++) {
    for (int x = 0; x < MATRIX_FIRE_W; x++) {
      int sx = x + (int)g_fireWind;
      if (sx < 0) sx = 0;
      if (sx > MATRIX_FIRE_W - 1) sx = MATRIX_FIRE_W - 1;
      int xm = (x == 0) ? 0 : x - 1;
      int xp = (x == MATRIX_FIRE_W - 1) ? MATRIX_FIRE_W - 1 : x + 1;
      int below = heat[(y + 1) * MATRIX_FIRE_W + sx];
      int left = heat[(y + 1) * MATRIX_FIRE_W + xm];
      int right = heat[(y + 1) * MATRIX_FIRE_W + xp];
      // Favour the cell underneath so a tongue stays narrow as it rises.
      int v = (below * 3 + left + right) / 5;

      // About 1..2 lost on every row. The upper half loses another 1..3,
      // and rows 0..2 lose enough more that only a rare spark survives.
      v -= matrixRand(1, 3);
      if (y <= 4) v -= matrixRand(1, 4);
      if (y <= 2) v -= matrixRand(4, 8);

      int cap = 12;
      if (y <= 1) cap = 0;
      else if (y == 2) cap = 3;
      else if (y <= 4) cap = 7;

      if (v < 0) v = 0;
      if (v > cap) v = cap;

      if (y <= 1) {
        v = 0;
      } else if (y == 2) {
        // Almost always dark. A tongue that reached row 3 or 4 can throw
        // one dim spark; row 0 and row 1 stay black.
        int deeper = heat[(y + 2) * MATRIX_FIRE_W + sx];
        if (v > 0 && matrixRand(0, 4) != 0) v = 0;
        if (v > 3) v = 3;
        if (v == 0 && (below >= 4 || deeper >= 5) && matrixRand(0, 8) == 0) {
          v = matrixRand(1, 4);
        }
      }

      row[x] = (uint8_t)v;
    }
    memcpy(heat + y * MATRIX_FIRE_W, row, MATRIX_FIRE_W);
  }

  // Bottom row: a few hot tongues, cooler neighbours, and dark gaps.
  // Not a solid bar of 11..15.
  for (int x = 0; x < MATRIX_FIRE_W; x++) {
    int best = 99;
    for (int s = 0; s < 3; s++) {
      int d = x - (int)g_fireSpot[s];
      if (d < 0) d = -d;
      if (d < best) best = d;
    }
    int v;
    if (best == 0) v = 10 + matrixRand(0, 6);      // 10..15
    else if (best == 1) v = 2 + matrixRand(0, 7);  // 2..8
    else v = matrixRand(0, 5);                     // 0..4
    heat[(MATRIX_FIRE_H - 1) * MATRIX_FIRE_W + x] = (uint8_t)v;
  }
}

#if defined(MATRIX_FIRE_TEST)
#include <stdio.h>

static int g_fireFails = 0;
static void fireExpect(bool cond, const char* msg) {
  if (!cond) {
    printf("FAIL %s\n", msg);
    g_fireFails++;
  } else {
    printf("ok   %s\n", msg);
  }
}

static void fireSample(unsigned seed) {
  srand(seed);
  uint8_t heat[96];
  memset(heat, 0, 96);
  matrixFireReset();
  for (int i = 0; i < 40; i++) matrixFireStep(heat);

  const int FRAMES = 600;
  long sum[8] = {0};
  long nz[8] = {0};
  long hot[8] = {0};
  long gap[8] = {0};
  long mid[8] = {0};
  int mx[8] = {0};
  for (int f = 0; f < FRAMES; f++) {
    matrixFireStep(heat);
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 12; x++) {
        int v = heat[y * 12 + x];
        sum[y] += v;
        if (v > mx[y]) mx[y] = v;
        if (v > 0) nz[y]++;
        if (v >= 10) hot[y]++;
        if (v <= 4) gap[y]++;
        if (v >= 3 && v <= 12) mid[y]++;
      }
    }
  }
  const long samples = (long)FRAMES * 12;
  printf("seed %u\n", seed);
  printf("row  avg   nz%%   max  hot%%  <=4%%\n");
  for (int y = 0; y < 8; y++) {
    printf("%d   %5.2f  %5.1f  %3d  %5.1f  %5.1f\n", y,
           (double)sum[y] / (double)samples,
           100.0 * (double)nz[y] / (double)samples,
           mx[y],
           100.0 * (double)hot[y] / (double)samples,
           100.0 * (double)gap[y] / (double)samples);
  }

  fireExpect(mx[0] == 0 && nz[0] == 0, "row 0 stays dark");
  fireExpect(mx[1] == 0 && nz[1] == 0, "row 1 stays dark");
  fireExpect(mx[2] <= 3, "row 2 sparks are dim");
  fireExpect(nz[2] > 0 && nz[2] * 100 < samples * 8, "row 2 sparks are rare");
  fireExpect(mx[3] <= 7 && mx[4] <= 7, "rows 3-4 cap at 7");
  fireExpect(nz[3] * 100 < samples * 45 && nz[4] * 100 < samples * 45,
             "rows 3-4 mostly dark");
  fireExpect(nz[3] + nz[4] > 20, "rows 3-4 still lick");
  fireExpect(mx[5] <= 12 && mx[6] <= 12, "rows 5-6 cap at 12");
  fireExpect(mid[5] > samples / 5 && mid[6] > samples / 5, "rows 5-6 carry flame");
  fireExpect(hot[7] > samples / 10, "bottom has hot tongues");
  fireExpect(gap[7] > samples / 8, "bottom has cool gaps");
  fireExpect(hot[7] * 100 < samples * 55, "bottom is not a solid 11-15 bar");
  fireExpect(mx[7] <= 15 && mx[7] >= 10, "bottom hot range reaches the teens");
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
