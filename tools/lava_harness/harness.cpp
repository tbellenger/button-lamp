// Drives the real SpiralLamp_clean sketch on the host.
// Build via tools/lava_harness/build.sh, which preprocess-injects Arduino
// prototypes and then compiles this file against stub headers.
#include "sketch_pre.cpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

// Light switch = rocker D7, active low (ON = grounded).
// Mushroom = D11 NC: released = LOW (arms Chaos), pressed = HIGH = Lava.
static const uint8_t PIN_LIGHT = 7;
static const uint8_t PIN_MUSHROOM = 11;

static int pixelLit(uint16_t i) {
  return strip.shown(i, 0) | strip.shown(i, 1) | strip.shown(i, 2);
}

int main(int argc, char **argv) {
  int a0 = (argc > 1) ? atoi(argv[1]) : 1023;
  int a1 = (argc > 2) ? atoi(argv[2]) : 512;
  int frameMs = (argc > 3) ? atoi(argv[3]) : 16;
  int seconds = (argc > 4) ? atoi(argv[4]) : 60;
  int logFrames = (argc > 5) ? atoi(argv[5]) : 1;
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
  return 0;
}
