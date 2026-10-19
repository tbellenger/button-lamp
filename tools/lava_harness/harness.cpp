// Drives a SpiralLamp sketch on the host. tools/lava_harness/build.sh
// preprocess-injects Arduino prototypes and compiles this file against stub
// headers. The default sketch is SpiralLamp_v2 (the fire trail). Pass "clean"
// as build.sh's second argument to build SpiralLamp_clean instead.
//
// Args: A0 A1 frameMs seconds logFrames [ppmPath]
// With logFrames=0 the per-frame trace is quiet. The run still prints SUMMARY
// peaks and a few TRAIL rows (rise, a later rise, apex, fall) plus FLICKER_G.
// ppmPath, if given, is a PPM strip of those four frames (channels ×5 so the
// brightness cap of 50 is visible).
#include "sketch_pre.cpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

// Light switch = rocker D7, active low (ON = grounded).
// Mushroom = D11 NC: released = LOW (arms Chaos), pressed = HIGH = Lava.
static const uint8_t PIN_LIGHT = 7;
static const uint8_t PIN_MUSHROOM = 11;

static int pixelLit(uint16_t i) {
  return strip.shown(i, 0) | strip.shown(i, 1) | strip.shown(i, 2);
}

// One lava frame for ball 0.
// row: * ball (palette red), W white-yellow, Y yellow, O orange, r red tail, . off
// bri: brightest shown channel as a digit, 9 ≈ BRIGHTNESS_CAP.
struct TrailSnap {
  int got;
  const char *phase;
  unsigned long tMs;
  int vel;
  double led;
  int behind;
  int ahead;
  int maxCh;
  char row[LED_COUNT + 1];
  char bri[LED_COUNT + 1];
  uint8_t rgb[LED_COUNT][3];
};

static char colourOf(uint8_t r, uint8_t g, uint8_t b, int body) {
  if ((r | g | b) == 0) return '.';
  if (body && g == 0 && b == 0) return '*';
  if (b > 0 && (int)g * 3 >= (int)r) return 'W';
  if (g > 0 && (int)g * 2 >= (int)r) return 'Y';
  if (g > 0) return 'O';
  return 'r';
}

static void fillSnap(TrailSnap *s, const char *phase, unsigned long tMs,
                     int vel, double led, double otherLed) {
  s->got = 1;
  s->phase = phase;
  s->tMs = tMs;
  s->vel = vel;
  s->led = led;
  s->maxCh = 0;
  s->behind = 0;
  s->ahead = 0;
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    uint8_t r = strip.shown(i, 0);
    uint8_t g = strip.shown(i, 1);
    uint8_t b = strip.shown(i, 2);
    s->rgb[i][0] = r;
    s->rgb[i][1] = g;
    s->rgb[i][2] = b;
    int ch = r;
    if (g > ch) ch = g;
    if (b > ch) ch = b;
    if (ch > s->maxCh) s->maxCh = ch;
    int body = fabs((double)i - led) <= 1.15;
    s->row[i] = colourOf(r, g, b, body);
    int digit = (ch * 9) / 50;
    if (digit > 9) digit = 9;
    s->bri[i] = (char)('0' + digit);
  }
  s->row[LED_COUNT] = 0;
  s->bri[LED_COUNT] = 0;

  int dirBehind = (vel > 0) ? -1 : (vel < 0) ? 1 : 0;
  if (dirBehind == 0) return;
  for (int pass = 0; pass < 2; pass++) {
    int dir = (pass == 0) ? dirBehind : -dirBehind;
    int count = 0;
    for (int i = (int)lround(led) + dir; i >= 0 && i < LED_COUNT; i += dir) {
      if (fabs((double)i - led) <= 1.15) continue;
      if (s->row[i] == '.') break;
      if (fabs((double)i - otherLed) <= 2.0 && fabs((double)i - led) > 4.0) break;
      count++;
      if (count > 30) break;
    }
    if (pass == 0) s->behind = count;
    else s->ahead = count;
  }
}

static void tailSeq(const TrailSnap *s, char *out, int cap) {
  int n = 0;
  if (!s->got || s->vel == 0 || cap < 2) {
    out[0] = 0;
    return;
  }
  int dir = (s->vel > 0) ? -1 : 1;
  int i0 = (int)lround(s->led);
  for (int i = i0 + dir; i >= 0 && i < LED_COUNT && n < cap - 1; i += dir) {
    char c = s->row[i];
    if (c == '.') {
      if (n > 0) break;
      continue;
    }
    if (c == '*') continue;
    out[n++] = c;
  }
  out[n] = 0;
}

static void printSnap(const TrailSnap *s) {
  if (!s->got) {
    printf("TRAIL phase=%s MISSING\n", s->phase ? s->phase : "?");
    return;
  }
  int lo = (int)lround(s->led) - 14;
  int hi = (int)lround(s->led) + 14;
  if (lo < 0) lo = 0;
  if (hi >= LED_COUNT) hi = LED_COUNT - 1;
  char seq[48];
  tailSeq(s, seq, (int)sizeof(seq));
  printf("TRAIL phase=%s t_ms=%lu vel=%d led=%.3f behind=%d ahead=%d maxCh=%d seq=%s\n",
         s->phase, s->tMs, s->vel, s->led, s->behind, s->ahead, s->maxCh, seq);
  printf("  leds %d..%d\n  col ", lo, hi);
  for (int i = lo; i <= hi; i++) putchar(s->row[i]);
  printf("\n  bri ");
  for (int i = lo; i <= hi; i++) putchar(s->bri[i]);
  printf("\n");
}

// Channels are scaled x5 so a BRIGHTNESS_CAP of 50 is visible. The file is a
// stack of the captured frames, LED 0 at the left.
static void writePpm(const char *path, TrailSnap *snaps, int n) {
  const int cell = 6;
  const int gap = 4;
  int width = LED_COUNT * cell;
  int height = n * cell + (n > 0 ? (n - 1) * gap : 0);
  FILE *fp = fopen(path, "w");
  if (!fp) {
    perror(path);
    return;
  }
  fprintf(fp, "P3\n# lava fire trail, shown channels *5 (BRIGHTNESS_CAP is 50)\n%d %d\n255\n",
          width, height);
  for (int s = 0; s < n; s++) {
    if (s) {
      for (int y = 0; y < gap; y++)
        for (int x = 0; x < width; x++) fprintf(fp, "0 0 0\n");
    }
    for (int y = 0; y < cell; y++) {
      for (int i = 0; i < LED_COUNT; i++) {
        int r = 0, g = 0, b = 0;
        if (snaps[s].got) {
          r = snaps[s].rgb[i][0] * 5;
          g = snaps[s].rgb[i][1] * 5;
          b = snaps[s].rgb[i][2] * 5;
          if (r > 255) r = 255;
          if (g > 255) g = 255;
          if (b > 255) b = 255;
        }
        for (int x = 0; x < cell; x++) fprintf(fp, "%d %d %d\n", r, g, b);
      }
    }
  }
  fclose(fp);
  printf("TRAIL_PPM %s\n", path);
}

int main(int argc, char **argv) {
  int a0 = (argc > 1) ? atoi(argv[1]) : 1023;
  int a1 = (argc > 2) ? atoi(argv[2]) : 512;
  int frameMs = (argc > 3) ? atoi(argv[3]) : 16;
  int seconds = (argc > 4) ? atoi(argv[4]) : 60;
  int logFrames = (argc > 5) ? atoi(argv[5]) : 1;
  const char *ppmPath = (argc > 6) ? argv[6] : 0;
  if (frameMs < 1) frameMs = 1;
  if (a0 < 0) a0 = 0;
  if (a0 > 1023) a0 = 1023;
  if (a1 < 0) a1 = 0;
  if (a1 > 1023) a1 = 1023;

  // Pull-ups: every switch reads HIGH (released / off) until we say otherwise.
  for (uint8_t p = 0; p < 22; p++) harnessSetDigital(p, HIGH);
  harnessSetDigital(PIN_LIGHT, LOW);      // light switch ON
  harnessSetDigital(PIN_MUSHROOM, LOW);   // mushroom released, so Chaos can arm
  harnessSetAnalog(A0, a0);               // brightness fader
  harnessSetAnalog(A1, a1);               // speed knob
  harnessSetMillis(0);

  setup();

  const int settleFrames = (100 + frameMs - 1) / frameMs;
  for (int f = 0; f < settleFrames; f++) {
    harnessAdvance((uint32_t)frameMs);
    loop();
  }
  // Press and hold the mushroom for the logged minute.
  harnessSetDigital(PIN_MUSHROOM, HIGH);

  int visMax0 = -1;
  int visMin1 = LED_COUNT;
  int frames = 0;
  const int totalFrames = seconds * 1000 / frameMs;
  TrailSnap rise = {};
  rise.phase = "rise";
  TrailSnap flicker = {};
  flicker.phase = "rise_flicker";
  TrailSnap apex = {};
  apex.phase = "apex";
  TrailSnap fall = {};
  fall.phase = "fall";
  // Green channel of the LED just behind ball 0, a few frames apart.
  // Flame flicker should make this wobble rather than step smoothly.
  int flickN = 0;
  int flickG[8];
  int flickLed[8];
  memset(flickG, 0, sizeof(flickG));
  memset(flickLed, 0, sizeof(flickLed));

  printf("# a0=%d a1=%d frame_ms=%d seconds=%d sizeof_int=%u\n",
         a0, a1, frameMs, seconds, (unsigned)sizeof(int));
  printf("# t_ms ticks pos0 vel0 led0 pos1 vel1 led1 peak0 peak1 interval_ms\n");

  for (int f = 0; f < totalFrames; f++) {
    harnessAdvance((uint32_t)frameMs);
    loop();
    frames++;

    Lava::HostView v = Lava::hostView();
    double led0 = (v.fixScale > 0) ? (double)v.pos0 / (double)v.fixScale : 0.0;
    double led1fromEnd = (v.fixScale > 0) ? (double)v.pos1 / (double)v.fixScale : 0.0;
    double center1 = (double)(LED_COUNT - 1) - led1fromEnd;

    int localMax0 = -1;
    int localMin1 = LED_COUNT;
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      if (!pixelLit(i)) continue;
      double d0 = fabs((double)i - led0);
      double d1 = fabs((double)i - center1);
      if (d0 <= d1) {
        if ((int)i > localMax0) localMax0 = (int)i;
      } else if ((int)i < localMin1) {
        localMin1 = (int)i;
      }
    }
    if (localMax0 > visMax0) visMax0 = localMax0;
    if (localMin1 < visMin1) visMin1 = localMin1;

    // Ball 0's opening toss. Sample where the tail is not cut off by LED 0:
    // long on the way up (behind the ball, lower indices), gone at the apex,
    // long again on the way down (behind the ball, higher indices).
    if (v.vel0 >= 200 && led0 >= 14.0 && led0 <= 36.0) {
      TrailSnap tmp = {};
      fillSnap(&tmp, "rise", millis(), v.vel0, led0, center1);
      int clipped = (tmp.row[0] != '.') || (tmp.row[1] != '.');
      if (!clipped && (!rise.got || tmp.behind > rise.behind)) rise = tmp;
      if (rise.got && !flicker.got && millis() >= rise.tMs + 48 &&
          tmp.tMs != rise.tMs) {
        tmp.phase = "rise_flicker";
        flicker = tmp;
      }
    }
    // A fast knob steps velocity by up to 12 per frame, so accept a near-apex
    // and keep the sample closest to zero speed.
    if (led0 >= 74.0 && v.vel0 <= 16 && v.vel0 >= -16) {
      if (!apex.got || abs(v.vel0) < abs(apex.vel)) {
        fillSnap(&apex, "apex", millis(), v.vel0, led0, center1);
      }
    }
    // Same offset behind the rising ball (orange band). Green wobbles with the
    // flame hash instead of falling smoothly as the ball climbs.
    if (flickN < 8 && v.vel0 > 180 && led0 >= 18.0 && led0 <= 32.0) {
      int led = (int)lround(led0) - 5;
      if (led >= 0 && led < LED_COUNT && strip.shown((uint16_t)led, 1) > 0) {
        flickLed[flickN] = led;
        flickG[flickN] = strip.shown((uint16_t)led, 1);
        flickN++;
      }
    }
    if (apex.got && v.vel0 <= -180 && led0 >= 14.0 && led0 <= 48.0) {
      TrailSnap tmp = {};
      fillSnap(&tmp, "fall", millis(), v.vel0, led0, center1);
      if (!fall.got || tmp.behind > fall.behind) fall = tmp;
    }

    if (logFrames) {
      printf("%lu %lu %d %d %.3f %d %d %.3f %d %d %.3f\n",
             (unsigned long)millis(), (unsigned long)v.ticks,
             (int)v.pos0, (int)v.vel0, led0,
             (int)v.pos1, (int)v.vel1, led1fromEnd,
             (int)v.peak0, (int)v.peak1, (double)v.interval);
    }
  }

  Lava::HostView end = Lava::hostView();
  double peakLed0 = (end.fixScale > 0) ? (double)end.peak0 / (double)end.fixScale : 0.0;
  double peakLed1 = (end.fixScale > 0) ? (double)end.peak1 / (double)end.fixScale : 0.0;
  double visTravel1 = (visMin1 < LED_COUNT) ? (double)(LED_COUNT - 1 - visMin1) : -1.0;
  int chaos = chaosOn() ? 1 : 0;
  int rocker = inJumbo.pressed() ? 1 : 0;

  printf("SUMMARY a0=%d a1=%d frame_ms=%d seconds=%d frames=%d ticks=%lu steps=%lu "
         "fix=%d centerFix=%ld interval_ms=%.3f chaos=%d rocker=%d armed=%u "
         "peakPos0=%d peakLed0=%.3f peakVel0=%d visMaxLed0=%d "
         "peakPos1=%d peakLed1=%.3f peakVel1=%d visMinLed1=%d visTravel1=%.3f "
         "tosses0=%u tosses1=%u\n",
         a0, a1, frameMs, seconds, frames,
         (unsigned long)end.ticks, (unsigned long)end.steps,
         (int)end.fixScale, (long)end.centerFix, (double)end.interval,
         chaos, rocker, (unsigned)chaosSeenLow,
         (int)end.peak0, peakLed0, (int)end.peakVel0, visMax0,
         (int)end.peak1, peakLed1, (int)end.peakVel1,
         visMin1 < LED_COUNT ? visMin1 : -1, visTravel1,
         (unsigned)end.tossN0, (unsigned)end.tossN1);

  double bestAfter0 = -1, bestAfter1 = -1;
  for (uint8_t ball = 0; ball < 2; ball++) {
    uint8_t n = (ball == 0) ? end.tossN0 : end.tossN1;
    uint8_t capped = n > 96 ? 96 : n;
    for (uint8_t i = 0; i < capped; i++) {
      int16_t peak = 0, vel = 0;
      Lava::hostToss(ball, i, &peak, &vel);
      double led = (end.fixScale > 0) ? (double)peak / (double)end.fixScale : 0.0;
      printf("TOSS ball=%u n=%u vel=%d pos=%d led=%.3f\n",
             (unsigned)ball, (unsigned)i, (int)vel, (int)peak, led);
      if (i > 0 && led > (ball == 0 ? bestAfter0 : bestAfter1)) {
        if (ball == 0) bestAfter0 = led;
        else bestAfter1 = led;
      }
    }
    int16_t openPeak = (ball == 0) ? end.openPeak0 : end.openPeak1;
    int16_t openVel = (ball == 0) ? end.openVel0 : end.openVel1;
    double openLed = (end.fixScale > 0) ? (double)openPeak / (double)end.fixScale : 0.0;
    printf("TOSS ball=%u n=open vel=%d pos=%d led=%.3f\n",
           (unsigned)ball, (int)openVel, (int)openPeak, openLed);
  }
  printf("AFTER_FIRST bestLed0=%.3f bestLed1=%.3f\n", bestAfter0, bestAfter1);

  printSnap(&rise);
  printSnap(&flicker);
  printSnap(&apex);
  printSnap(&fall);
  int kinds = 0;
  char seen[10];
  memset(seen, 0, sizeof(seen));
  if (rise.got) {
    for (int i = 0; i < LED_COUNT; i++) {
      char c = rise.row[i];
      if (c == 'W' || c == 'Y' || c == 'O' || c == 'r') {
        int d = rise.bri[i] - '0';
        if (d >= 0 && d <= 9 && !seen[d]) {
          seen[d] = 1;
          kinds++;
        }
      }
    }
  }
  printf("FLICKER_G");
  for (int i = 0; i < flickN; i++) printf(" %d@%d", flickG[i], flickLed[i]);
  printf("\n");
  printf("TRAIL_PROOF rise_behind=%d rise_ahead=%d flicker_behind=%d "
         "apex_behind=%d apex_ahead=%d fall_behind=%d fall_ahead=%d "
         "rise_bri_digits=%d peakLed0=%.3f peakLed1=%.3f\n",
         rise.got ? rise.behind : -1, rise.got ? rise.ahead : -1,
         flicker.got ? flicker.behind : -1,
         apex.got ? apex.behind : -1, apex.got ? apex.ahead : -1,
         fall.got ? fall.behind : -1, fall.got ? fall.ahead : -1,
         kinds, peakLed0, peakLed1);
  if (ppmPath) {
    TrailSnap shots[4] = {rise, flicker, apex, fall};
    writePpm(ppmPath, shots, 4);
  }
  return 0;
}
