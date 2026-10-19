#pragma once
// Host stand-in for the Arduino core. Enough for SpiralLamp_clean.ino.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t byte;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LED_BUILTIN 13

#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19
#define A6 20
#define A7 21

class __FlashStringHelper {};
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t *>(addr))
#define memcpy_P(dst, src, n) memcpy((dst), (src), (n))
#define memcmp_P(a, b, n) memcmp((a), (b), (n))

struct HardwareSerial {
  void begin(unsigned long) {}
  template <typename T>
  void print(const T &) {}
  template <typename T>
  void println(const T &) {}
  void println() {}
};
static HardwareSerial Serial;

static uint8_t g_pinLevel[32];
static uint8_t g_pinReady = 0;
static int g_analog[32];
static uint32_t g_millis = 0;

inline void harnessPinsInit() {
  if (g_pinReady) return;
  for (int i = 0; i < 32; i++) g_pinLevel[i] = HIGH;
  g_pinReady = 1;
}
inline void pinMode(uint8_t, uint8_t) { harnessPinsInit(); }
inline int digitalRead(uint8_t pin) {
  harnessPinsInit();
  return g_pinLevel[pin & 31];
}
inline void digitalWrite(uint8_t pin, uint8_t v) {
  harnessPinsInit();
  g_pinLevel[pin & 31] = v;
}
inline void harnessSetDigital(uint8_t pin, int v) {
  harnessPinsInit();
  g_pinLevel[pin & 31] = (uint8_t)(v ? HIGH : LOW);
}
inline int analogRead(uint8_t pin) { return g_analog[pin & 31]; }
inline void harnessSetAnalog(uint8_t pin, int v) { g_analog[pin & 31] = v; }

inline uint32_t millis() { return g_millis; }
inline uint32_t micros() { return g_millis * 1000UL; }
inline void harnessSetMillis(uint32_t ms) { g_millis = ms; }
inline void harnessAdvance(uint32_t dt) { g_millis += dt; }
inline void delay(unsigned long ms) { g_millis += (uint32_t)ms; }
inline void delayMicroseconds(unsigned int) {}

inline long random(long max) {
  if (max <= 0) return 0;
  return 0;
}
inline long random(long min, long max) {
  (void)max;
  return min;
}
inline void randomSeed(unsigned long) {}
