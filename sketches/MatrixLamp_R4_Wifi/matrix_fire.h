// Full-colour fire for a 16x16 matrix. y = 0 is the top row, y = 15 is the
// ember bed. Heat is 0..255. The bottom row stays hot: plume cores reach
// white, the rest of the bed stays in the yellow/orange. Three wide plumes
// (three columns at the core, shoulders one column further out)
// rise together, cool as they climb, and a rare draft lets one tongue lick
// row 0. That lick is capped below white so the tip still reads as flame.
//
// Intensity 50 is the tuned baseline. 0 cools harder (shorter flames).
// 100 cools less and drafts harder (taller flames).
//
// Host test:
//   g++ -std=c++17 -DMATRIX_FIRE_TEST -x c++ matrix_fire.h -o /tmp/matrix_fire_test

#pragma once

#include <stdint.h>
#include <string.h>

static const int MATRIX_FIRE_W = 16;
static const int MATRIX_FIRE_H = 16;
static const int MATRIX_FIRE_N = MATRIX_FIRE_W * MATRIX_FIRE_H;
static const int MATRIX_FIRE_SPOTS = 3;

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
static int8_t g_fireSpot[MATRIX_FIRE_SPOTS] = {1, 8, 14};
static uint8_t g_fireSpotReady = 1;
static uint8_t g_fireSurgeSpot = 0;
static uint8_t g_fireSurgeLeft = 0;
static uint8_t g_fireSinceSurge = 0;
// 0..100. 50 matches the tables below with no extra scale.
static uint8_t g_fireIntensity = 50;

// Cool subtracted at each destination row (y = 0 is the top). Upper rows lose
// more, so tips thin out. Core draft is added back on the centre of a plume
// (distance 0..1) and, at half strength, on the shoulders (distance 2).
// Tuned in heat units 0..255.
static const uint8_t kFireCoolLo[16] = {
  110, 88, 72, 58, 46, 36, 28, 22, 16, 12, 9, 7, 6, 4, 2, 0
};
static const uint8_t kFireCoolHi[16] = {
  190, 156, 128, 104, 86, 70, 56, 44, 34, 26, 20, 16, 14, 10, 6, 2
};
static const uint8_t kFireDraftCore[16] = {
  22, 20, 18, 16, 14, 12, 11, 9, 8, 6, 5, 4, 3, 2, 1, 0
};

struct FireRgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// Dark / deep red at the cool edge, through orange and yellow, to white only
// in the last stretch of the heat range (ember cores and the hottest cells).
static FireRgb firePalette(uint8_t heat) {
  static const uint8_t kH[] = {0, 28, 64, 104, 148, 188, 220, 242, 255};
  static const uint8_t kR[] = {0, 96, 210, 255, 255, 255, 255, 255, 255};
  static const uint8_t kG[] = {0, 0, 0, 36, 110, 196, 244, 255, 255};
  static const uint8_t kB[] = {0, 0, 0, 0, 0, 0, 18, 150, 255};
  FireRgb c;
  if (heat == 0) {
    c.r = c.g = c.b = 0;
    return c;
  }
  int i = 0;
  while (i < 7 && heat > kH[i + 1]) i++;
  int h0 = kH[i];
  int h1 = kH[i + 1];
  int span = h1 - h0;
  int t = (span == 0) ? 0 : (int)heat - h0;
  c.r = (uint8_t)(kR[i] + (int)(kR[i + 1] - kR[i]) * t / span);
  c.g = (uint8_t)(kG[i] + (int)(kG[i + 1] - kG[i]) * t / span);
  c.b = (uint8_t)(kB[i] + (int)(kB[i + 1] - kB[i]) * t / span);
  return c;
}

static int fireClampX(int x) {
  if (x < 0) return 0;
  if (x >= MATRIX_FIRE_W) return MATRIX_FIRE_W - 1;
  return x;
}

static void fireSeparateSpots() {
  for (int pass = 0; pass < 4; pass++) {
    for (int a = 0; a < MATRIX_FIRE_SPOTS; a++) {
      for (int b = a + 1; b < MATRIX_FIRE_SPOTS; b++) {
        int da = (int)g_fireSpot[a];
        int db = (int)g_fireSpot[b];
        int d = da - db;
        if (d < 0) d = -d;
        if (d >= 6) continue;
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
  g_fireSpot[1] = 8;
  g_fireSpot[2] = 14;
  g_fireSpotReady = 1;
  g_fireSurgeSpot = 0;
  g_fireSurgeLeft = 0;
  g_fireSinceSurge = 0;
}

static void matrixFireSetIntensity(uint8_t pct) {
  if (pct > 100) pct = 100;
  g_fireIntensity = pct;
}

static int fireNearest(int x, int* bestOut) {
  int best = 99;
  int which = 0;
  for (int s = 0; s < MATRIX_FIRE_SPOTS; s++) {
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

static int fireScaleCool(int cool) {
  // intensity 50 -> 100%, 0 -> 150%, 100 -> 55%.
  int mul = 150 - (int)g_fireIntensity;
  if (mul < 50) mul = 50;
  return cool * mul / 100;
}

static int fireScaleDraft(int draft) {
  // intensity 50 -> 100%, 100 -> 145%, 0 -> 55%.
  int mul = 55 + (int)g_fireIntensity;
  if (mul > 160) mul = 160;
  return draft * mul / 100;
}

// One simulation step. `heat` is 256 levels, row-major, y = 0 at the top.
static void matrixFireStep(uint8_t* heat) {
  if (!g_fireSpotReady) matrixFireReset();
  if (++g_fireWindTick >= 8) {
    g_fireWindTick = 0;
    int next = (int)g_fireWind + matrixRand(0, 3) - 1;
    if (next < -1) next = -1;
    if (next > 1) next = 1;
    g_fireWind = (int8_t)next;
    int s = matrixRand(0, MATRIX_FIRE_SPOTS);
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
    g_fireSurgeSpot = (uint8_t)matrixRand(0, MATRIX_FIRE_SPOTS);
    g_fireSurgeLeft = 10;
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
      // Centre three columns count double so a wide base rises as one tongue
      // instead of a 1-pixel spike. ±2 pulls the shoulders along.
      int v = (c * 2 + l * 2 + r * 2 + ll + rr) / 8;

      int best = 99;
      int which = fireNearest(x, &best);
      int cool = matrixRand((int)kFireCoolLo[y], (int)kFireCoolHi[y] + 1);
      int draft = 0;
      if (best <= 1) draft = (int)kFireDraftCore[y];
      else if (best == 2) draft = (int)kFireDraftCore[y] / 2;
      else if (best == 3) cool += 22;
      else cool += matrixRand(28, 56);

      // The row just above the bed lifts across the wide bases together.
      if (y == MATRIX_FIRE_H - 2 && best <= 2) draft += 8;

      int surging = (g_fireSurgeLeft > 0 && which == (int)g_fireSurgeSpot);
      if (surging && best <= 1) {
        draft += 48;
        cool /= 2;
      } else if (surging && best == 2) {
        draft += 22;
      } else if (surging && best == 3) {
        draft += 8;
      }

      v = v - fireScaleCool(cool) + fireScaleDraft(draft);
      if (v < 0) v = 0;
      if (v > 255) v = 255;
      // White (heat past ~240) stays on the bed and the base of a plume.
      // Above that the body is yellow, and the tips stay orange.
      if (y < 12 && v > 228) v = 228;
      if (y < 5 && v > 176) v = 176;
      // A lick that reaches the top stays visible and then recedes. Dim
      // noise on row 0 is not a lick.
      if (y == 0) {
        if (v < 40) v = 0;
        if (v > 150) v = 150;
      }
      row[x] = (uint8_t)v;
    }
    memcpy(heat + y * MATRIX_FIRE_W, row, MATRIX_FIRE_W);
  }

  // Ember bed: every column stays hot. Wandering cores (d <= 1, three
  // columns) are the only cells that sit in the white band. Shoulders are
  // yellow. The gaps between plumes stay orange.
  int bedLift = ((int)g_fireIntensity - 50) / 2;
  for (int x = 0; x < MATRIX_FIRE_W; x++) {
    int best = 99;
    fireNearest(x, &best);
    int v;
    if (best <= 1) v = 246 + matrixRand(0, 10);       // 246..255 white
    else if (best == 2) v = 214 + matrixRand(0, 24);  // 214..237 yellow
    else if (best <= 4) v = 176 + matrixRand(0, 28);  // 176..203 orange
    else v = 150 + matrixRand(0, 26);                 // 150..175 deep orange
    v += bedLift;
    if (v < 120) v = 120;
    // Shoulders and gaps stay out of the white band even when intensity
    // lifts the whole bed. Only the plume cores (d <= 1) go white.
    if (best > 1 && v > 236) v = 236;
    if (v > 255) v = 255;
    heat[(MATRIX_FIRE_H - 1) * MATRIX_FIRE_W + x] = (uint8_t)v;
  }
}

#if defined(MATRIX_FIRE_TEST)
#include <stdio.h>

static const char kFireGlyph[] = " .:-=+*#%@";

static int g_fireFails = 0;
static void fireExpect(bool cond, const char* msg) {
  if (!cond) {
    printf("FAIL %s\n", msg);
    g_fireFails++;
  } else {
    printf("ok   %s\n", msg);
  }
}

static char fireGlyph(int v) {
  if (v <= 0) return ' ';
  int n = (int)sizeof kFireGlyph - 2;
  int i = (v * n) / 256;
  if (i > n) i = n;
  return kFireGlyph[i];
}

static void firePrintFrame(const uint8_t* heat, const char* title) {
  printf("%s\n", title);
  for (int y = 0; y < MATRIX_FIRE_H; y++) {
    printf("%2d ", y);
    for (int x = 0; x < MATRIX_FIRE_W; x++) putchar(fireGlyph(heat[y * MATRIX_FIRE_W + x]));
    printf("\n");
  }
}

static void firePaletteChecks() {
  FireRgb z = firePalette(0);
  fireExpect(z.r == 0 && z.g == 0 && z.b == 0, "heat 0 is black");
  FireRgb deep = firePalette(32);
  fireExpect(deep.r > 40 && deep.g == 0 && deep.b == 0, "cool edge is deep red");
  FireRgb red = firePalette(70);
  fireExpect(red.r > 180 && red.g < 40 && red.b == 0, "low flame is red");
  FireRgb orange = firePalette(140);
  fireExpect(orange.r > 240 && orange.g > 60 && orange.g < 180 && orange.b == 0,
             "mid flame is orange");
  FireRgb yellow = firePalette(200);
  fireExpect(yellow.r > 240 && yellow.g > 200 && yellow.b < 40, "hot flame is yellow");
  FireRgb white = firePalette(255);
  fireExpect(white.r == 255 && white.g == 255 && white.b == 255, "heat 255 is white");
  FireRgb almost = firePalette(220);
  fireExpect(almost.b < 40, "white stays off below the hottest band");
  FireRgb hot = firePalette(250);
  fireExpect(hot.b > 180 && hot.r == 255 && hot.g == 255, "only the top of the range is white");
}

static void fireSample(unsigned seed) {
  srand(seed);
  uint8_t heat[MATRIX_FIRE_N];
  memset(heat, 0, sizeof heat);
  matrixFireReset();
  for (int i = 0; i < 48; i++) matrixFireStep(heat);

  if (seed == 1) {
    for (int f = 0; f < 6; f++) {
      char title[32];
      snprintf(title, sizeof title, "frame %d", 48 + f);
      firePrintFrame(heat, title);
      matrixFireStep(heat);
    }
    srand(seed);
    memset(heat, 0, sizeof heat);
    matrixFireReset();
    for (int i = 0; i < 48; i++) matrixFireStep(heat);
  }

  const int FRAMES = 500;
  long sum[16];
  long nz[16];
  int mx[16];
  int mn[16];
  long whiteBed = 0;
  long bedN = 0;
  long coreWhite = 0;
  long coreN = 0;
  long edgeWhite = 0;
  long edgeN = 0;
  long runPx6 = 0;
  long runN6 = 0;
  memset(sum, 0, sizeof sum);
  memset(nz, 0, sizeof nz);
  for (int y = 0; y < 16; y++) {
    mx[y] = 0;
    mn[y] = 999;
  }
  int sawLick = 0;
  uint8_t lick[MATRIX_FIRE_N];
  for (int f = 0; f < FRAMES; f++) {
    matrixFireStep(heat);
    for (int y = 0; y < 16; y++) {
      int run = 0;
      for (int x = 0; x < 16; x++) {
        int v = heat[y * 16 + x];
        sum[y] += v;
        if (v < mn[y]) mn[y] = v;
        if (v > mx[y]) mx[y] = v;
        if (v > 0) nz[y]++;
        if (y == 6) {
          if (v >= 48) run++;
          else if (run) {
            runPx6 += run;
            runN6++;
            run = 0;
          }
        }
      }
      if (y == 6 && run) {
        runPx6 += run;
        runN6++;
      }
    }
    int row0 = 0;
    for (int x = 0; x < 16; x++) {
      int v = heat[15 * 16 + x];
      int best = 99;
      fireNearest(x, &best);
      bedN++;
      if (v >= 242) whiteBed++;
      if (best <= 1) {
        coreN++;
        if (v >= 242) coreWhite++;
      } else if (best >= 4) {
        edgeN++;
        if (v >= 242) edgeWhite++;
      }
      if (heat[x] > 0) row0 = 1;
    }
    if (row0 && !sawLick) {
      sawLick = 1;
      memcpy(lick, heat, sizeof lick);
    }
  }
  const long samples = (long)FRAMES * 16;
  printf("seed %u intensity %u\n", seed, (unsigned)g_fireIntensity);
  printf("row  avg    nz%%   min  max\n");
  double avg[16];
  for (int y = 0; y < 16; y++) {
    avg[y] = (double)sum[y] / (double)samples;
    printf("%2d  %6.1f  %5.1f  %3d  %3d\n", y, avg[y],
           100.0 * (double)nz[y] / (double)samples, mn[y], mx[y]);
  }
  if (sawLick) firePrintFrame(lick, "lick reaching row 0");
  double meanRun = runN6 ? (double)runPx6 / (double)runN6 : 0;
  printf("  mean run row6>=48 %.2f\n", meanRun);
  printf("  bed white %.1f%%  core white %.1f%% of %ld  edge white %.1f%% of %ld\n",
         100.0 * (double)whiteBed / (double)bedN,
         coreN ? 100.0 * (double)coreWhite / (double)coreN : 0, coreN,
         edgeN ? 100.0 * (double)edgeWhite / (double)edgeN : 0, edgeN);

  fireExpect(mn[15] >= 140, "bottom row stays hot");
  fireExpect(avg[15] >= 190 && avg[15] <= 245, "bottom row average is hot but not all white");
  fireExpect(coreN > 0 && coreWhite * 100 >= coreN * 80, "plume cores on the bed are white-hot");
  fireExpect(edgeN > 0 && edgeWhite * 10 <= edgeN, "bed edges outside the plumes stay out of white");
  fireExpect(whiteBed * 100 < bedN * 70, "white covers less than 70% of the bed");
  fireExpect(avg[14] > avg[8] && avg[8] > avg[2], "heat falls from the bed toward the tips");
  fireExpect(avg[12] >= 80, "the body above the bed is still bright");
  fireExpect(nz[0] * 1000 >= samples * 3 && nz[0] * 100 <= samples * 12,
             "top row licks are occasional");
  fireExpect(mx[0] >= 40 && mx[0] <= 150, "top licks stay below white");
  fireExpect(mx[4] <= 176, "upper tips are not white");
  fireExpect(meanRun >= 3.0 && meanRun <= 12.0, "mid flames are wide and still separate");
  fireExpect(nz[2] * 100 <= samples * 55, "upper rows are tips, not a solid field");
}

static double fireRowAvg(unsigned seed, uint8_t intensity, int row) {
  srand(seed);
  uint8_t heat[MATRIX_FIRE_N];
  memset(heat, 0, sizeof heat);
  matrixFireReset();
  matrixFireSetIntensity(intensity);
  for (int i = 0; i < 240; i++) matrixFireStep(heat);
  long sum = 0;
  const int frames = 80;
  for (int f = 0; f < frames; f++) {
    matrixFireStep(heat);
    for (int x = 0; x < MATRIX_FIRE_W; x++) sum += heat[row * MATRIX_FIRE_W + x];
  }
  return (double)sum / (double)(frames * MATRIX_FIRE_W);
}

int main() {
  firePaletteChecks();
  fireSample(1);
  fireSample(2);
  fireSample(7);
  double tall = fireRowAvg(9, 100, 2);
  double shortF = fireRowAvg(9, 0, 2);
  printf("intensity row2 avg  high %.1f  low %.1f\n", tall, shortF);
  fireExpect(tall > shortF + 15.0, "higher intensity lifts the tips");

  if (g_fireFails) {
    printf("%d failed\n", g_fireFails);
    return 1;
  }
  printf("all passed\n");
  return 0;
}
#endif
