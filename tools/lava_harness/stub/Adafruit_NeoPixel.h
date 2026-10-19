#pragma once
#include <stdint.h>
#include <stddef.h>

#define NEO_RGB  ((1 << 6) | (0 << 4) | (2 << 2) | (1))
#define NEO_RBG  ((1 << 6) | (0 << 4) | (1 << 2) | (2))
#define NEO_GRB  ((1 << 6) | (1 << 4) | (0 << 2) | (2))
#define NEO_GBR  ((1 << 6) | (1 << 4) | (2 << 2) | (0))
#define NEO_BRG  ((1 << 6) | (2 << 4) | (0 << 2) | (1))
#define NEO_BGR  ((1 << 6) | (2 << 4) | (1 << 2) | (0))
#define NEO_KHZ800 0x0000
#define NEO_KHZ400 0x0100

// Records setPixelColor() and applies setBrightness() the way Adafruit does
// in show(): channel = (channel * brightness) >> 8.
class Adafruit_NeoPixel {
 public:
  Adafruit_NeoPixel(uint16_t n, uint8_t pin, uint16_t type)
      : n_(n), pin_(pin), type_(type), brightness_(255), raw_(0), shown_(0) {}
  ~Adafruit_NeoPixel() {
    delete[] raw_;
    delete[] shown_;
  }
  void begin() {
    delete[] raw_;
    delete[] shown_;
    raw_ = new uint8_t[(size_t)n_ * 3]();
    shown_ = new uint8_t[(size_t)n_ * 3]();
  }
  void setBrightness(uint8_t b) { brightness_ = b; }
  uint8_t getBrightness() const { return brightness_; }
  void clear() {
    if (!raw_) return;
    for (uint16_t i = 0; i < (uint16_t)(n_ * 3); i++) raw_[i] = 0;
  }
  void setPixelColor(uint16_t i, uint8_t r, uint8_t g, uint8_t b) {
    if (!raw_ || i >= n_) return;
    raw_[(size_t)i * 3] = r;
    raw_[(size_t)i * 3 + 1] = g;
    raw_[(size_t)i * 3 + 2] = b;
  }
  void show() {
    if (!raw_ || !shown_) return;
    for (uint16_t i = 0; i < n_; i++) {
      for (uint8_t c = 0; c < 3; c++) {
        uint8_t v = raw_[(size_t)i * 3 + c];
        if (brightness_ != 255) v = (uint8_t)(((uint16_t)v * brightness_) >> 8);
        shown_[(size_t)i * 3 + c] = v;
      }
    }
  }
  uint16_t numPixels() const { return n_; }
  uint8_t shown(uint16_t i, uint8_t c) const {
    if (!shown_ || i >= n_ || c > 2) return 0;
    return shown_[(size_t)i * 3 + c];
  }

 private:
  uint16_t n_;
  uint8_t pin_;
  uint16_t type_;
  uint8_t brightness_;
  uint8_t *raw_;
  uint8_t *shown_;
};
