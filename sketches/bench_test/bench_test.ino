/* =====================================================================
   Spiral Lamp -- BENCH TEST (breadboard sanity check)
   ---------------------------------------------------------------------
   Purpose: prove the three basics BEFORE building the box:
     1. The LED strip works (it lights up on boot at LOW brightness).
     2. Button 1 is wired right (each press = next colour / pattern).
     3. Button 2 is wired right (each press = next brightness step).
   Everything the board sees is printed to the Serial Monitor at 115200.

   Wiring (see wiring.md / wiring_diagram.png for the full picture):
     - LED strip DIN  -> 330-470 ohm resistor -> PIN_LED (D9)
     - LED strip GND  -> Arduino GND (ALWAYS, even on USB power)
     - LED strip +5V  -> the 5 V 3 A supply (GND still tied to Arduino GND).
                         USB / Arduino 5V only if brightness stays at 5 or 10.
     - Button 1: one leg -> PIN_BTN1 (D4), the DIAGONALLY opposite leg -> GND
     - Button 2: one leg -> PIN_BTN2 (D8), the DIAGONALLY opposite leg -> GND
     No resistors on the buttons: the internal pull-ups are used, so a pin
     reads HIGH when released and LOW when pressed.

   Library: "Adafruit NeoPixel" (Library Manager). Same library the plans use.
   Board:   Arduino Uno / Nano (ATmega328P). Works on any AVR board.
   ===================================================================== */

#include <Adafruit_NeoPixel.h>

// ------------------------- EASY-TO-CHANGE SETTINGS -------------------------
#define PIN_LED        9      // strip data (DIN). Plans use D9.
#define LED_COUNT      160    // BTF FCOB 5 mm, 160/m, full metre = 160 LEDs
#define LED_TYPE       (NEO_GRB + NEO_KHZ800)  // WS2812B = GRB. If red/green look
                                               // swapped, try NEO_RGB.
#define PIN_BTN1       4      // "white" arcade button in the plans (D4)
#define PIN_BTN2       8      // "red"   arcade button in the plans (D8)

// Brightness steps cycled by button 2 (0-255). Keep them LOW on USB power:
// 160 LEDs at full white can pull ~9.6 A; a USB port gives ~0.5 A.
const uint8_t BRIGHT_STEPS[] = { 5, 10, 20, 40 };  // 160 LEDs: keep <=10 on USB, 20+ needs the 5 V 3 A supply
#define BOOT_BRIGHT_INDEX 1   // start at 10/255 (~0.4 A), safe on USB with 160 LEDs

#define DEBOUNCE_MS    25     // button must be stable this long to count
#define SCAN_OTHER_PINS 1     // 1 = also report ANY other pin D2..D12 that changes
                              //     (handy to find a button wired to the wrong pin)
// ---------------------------------------------------------------------------

Adafruit_NeoPixel strip(LED_COUNT, PIN_LED, LED_TYPE);

// ------------------------- Debounced button -------------------------------
// Classic "must be stable for DEBOUNCE_MS" debounce. Reports one press per
// physical press, on the press (HIGH -> LOW) edge.
struct Button {
  uint8_t  pin;
  uint8_t  stable;       // debounced level
  uint8_t  lastRaw;      // last raw reading
  uint32_t rawSinceMs;   // when lastRaw last changed
  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    stable = lastRaw = digitalRead(pin);
    rawSinceMs = millis();
  }
  // returns true exactly once per press
  bool pressedEdge(uint32_t now) {
    uint8_t raw = digitalRead(pin);
    if (raw != lastRaw) { lastRaw = raw; rawSinceMs = now; }
    if (raw != stable && (now - rawSinceMs) >= DEBOUNCE_MS) {
      stable = raw;
      Serial.print(F("  [D")); Serial.print(pin);
      Serial.println(stable == LOW ? F("] pressed (LOW)") : F("] released (HIGH)"));
      return stable == LOW;
    }
    return false;
  }
};

Button btn1, btn2;

// ------------------------- Modes -----------------------------------------
enum { M_RED, M_GREEN, M_BLUE, M_WHITE, M_RAINBOW, M_CHASE, M_COUNT };
const char* const MODE_NAMES[M_COUNT] = { "solid RED", "solid GREEN", "solid BLUE",
                                          "solid WHITE", "moving RAINBOW", "CHASE dot" };
uint8_t mode = M_RAINBOW;
uint8_t brightIdx = BOOT_BRIGHT_INDEX;
const uint8_t N_BRIGHT = sizeof(BRIGHT_STEPS) / sizeof(BRIGHT_STEPS[0]);

void fillAll(uint32_t c) { for (uint16_t i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, c); }

void render(uint32_t now) {
  switch (mode) {
    case M_RED:   fillAll(strip.Color(255, 0, 0)); break;
    case M_GREEN: fillAll(strip.Color(0, 255, 0)); break;
    case M_BLUE:  fillAll(strip.Color(0, 0, 255)); break;
    case M_WHITE: fillAll(strip.Color(255, 255, 255)); break;
    case M_RAINBOW: {
      uint16_t base = (uint16_t)(now * 20);           // scrolls along the strip
      for (uint16_t i = 0; i < LED_COUNT; i++)
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(base + i * (65536UL / LED_COUNT))));
      break;
    }
    case M_CHASE: {                                   // one dot running DIN -> end
      uint16_t pos = (now / 40) % LED_COUNT;
      strip.clear();
      strip.setPixelColor(pos, strip.Color(255, 160, 0));
      break;
    }
  }
  strip.show();
}

// Boot self-test: wipe R, G, B one LED at a time so you can see
// (a) data reaches the LAST LED, and (b) the colour order is right.
void bootWipe() {
  const uint32_t cols[3] = { strip.Color(255, 0, 0), strip.Color(0, 255, 0), strip.Color(0, 0, 255) };
  const char* names[3] = { "RED", "GREEN", "BLUE" };
  for (uint8_t c = 0; c < 3; c++) {
    Serial.print(F("Boot wipe: ")); Serial.println(names[c]);
    for (uint16_t i = 0; i < LED_COUNT; i++) { strip.setPixelColor(i, cols[c]); strip.show(); delay(8); }
    delay(250);
  }
}

#if SCAN_OTHER_PINS
uint16_t otherMask = 0, otherLast = 0;
void scanOtherPinsBegin() {
  for (uint8_t p = 2; p <= 12; p++) {
    if (p == PIN_LED || p == PIN_BTN1 || p == PIN_BTN2) continue;
    pinMode(p, INPUT_PULLUP);
    otherMask |= (1u << p);
    if (digitalRead(p)) otherLast |= (1u << p);
  }
}
void scanOtherPins() {
  static uint32_t lastScan = 0;
  if (millis() - lastScan < 30) return;   // crude debounce: 30 ms sampling
  lastScan = millis();
  for (uint8_t p = 2; p <= 12; p++) {
    if (!(otherMask & (1u << p))) continue;
    uint16_t bit = (1u << p);
    bool hi = digitalRead(p);
    if (hi != ((otherLast & bit) != 0)) {
      if (hi) otherLast |= bit; else otherLast &= ~bit;
      Serial.print(F("  NOTE: something on D")); Serial.print(p);
      Serial.println(hi ? F(" went HIGH (released/open)") : F(" went LOW (pressed/closed to GND)"));
    }
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("=== Spiral Lamp bench test ==="));
  Serial.print(F("LED data pin D")); Serial.print(PIN_LED);
  Serial.print(F(", LED count ")); Serial.println(LED_COUNT);
  Serial.print(F("Button 1 on D")); Serial.print(PIN_BTN1);
  Serial.print(F(" (cycles colour), Button 2 on D")); Serial.print(PIN_BTN2);
  Serial.println(F(" (cycles brightness)"));

  btn1.begin(PIN_BTN1);
  btn2.begin(PIN_BTN2);
#if SCAN_OTHER_PINS
  scanOtherPinsBegin();
#endif

  // A button that reads LOW at boot is either held, or wired to the wrong legs
  // (straight across a 4-pin tactile switch = always connected).
  if (digitalRead(PIN_BTN1) == LOW) Serial.println(F("WARNING: button 1 reads PRESSED at boot - check it isn't wired across the always-connected legs"));
  if (digitalRead(PIN_BTN2) == LOW) Serial.println(F("WARNING: button 2 reads PRESSED at boot - check it isn't wired across the always-connected legs"));

  strip.begin();
  strip.setBrightness(BRIGHT_STEPS[brightIdx]);
  strip.clear(); strip.show();
  bootWipe();

  Serial.print(F("Mode: ")); Serial.print(MODE_NAMES[mode]);
  Serial.print(F(" | brightness ")); Serial.println(BRIGHT_STEPS[brightIdx]);
  Serial.println(F("Press the buttons..."));
}

void loop() {
  uint32_t now = millis();

  if (btn1.pressedEdge(now)) {
    mode = (mode + 1) % M_COUNT;
    Serial.print(F("Button 1 -> mode: ")); Serial.println(MODE_NAMES[mode]);
  }
  if (btn2.pressedEdge(now)) {
    brightIdx = (brightIdx + 1) % N_BRIGHT;
    strip.setBrightness(BRIGHT_STEPS[brightIdx]);
    Serial.print(F("Button 2 -> brightness ")); Serial.print(BRIGHT_STEPS[brightIdx]);
    Serial.println(F("/255"));
  }
#if SCAN_OTHER_PINS
  scanOtherPins();
#endif

  render(now);
}
