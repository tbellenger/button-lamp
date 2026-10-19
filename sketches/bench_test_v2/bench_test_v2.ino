/* =====================================================================
   Spiral Lamp -- BENCH TEST v2 (every switch, both pots)
   ---------------------------------------------------------------------
   Adds the panel parts to the working v1 bench test ONE AT A TIME.
   Same pins as the full lamp sketch (SpiralLamp_clean.ino):

     D9   LED strip DIN (through the 330 ohm resistor)
     D4   arcade button 1 ("white")  -> next pattern
     D8   arcade button 2 ("red")    -> next brightness preset
                                        (shifts colour instead once the slide pot is live)
     A0   slide pot                  -> live brightness (capped at BRIGHTNESS_CAP)
     A1   rotary pot                 -> animation speed (hue in SOLID mode)
     D5   key switch (NO)            -> RAINBOW override
     D6   modifier switch            -> mirror + reverse direction
     D7   power switch               -> ON = lamp on, OFF = blackout (like the real lamp)
     D10  wave switch                -> travelling brightness wave on top of the pattern
     D11  mushroom stop (NC)         -> CHAOS sparkle while the mushroom is pressed

   Every switch goes from its pin to GND (INPUT_PULLUP, no resistors).
   Pots: outer legs to 5V and GND, wiper (middle leg) to A0 / A1.

   "Not wired yet" is safe:
     - D5/D6/D10 unwired read HIGH = OFF.
     - D7 is ignored (lamp stays ON) until the pin has been seen LOW once,
       i.e. until you wire the switch and flick it ON.
     - D11 is a normally-closed stop button: released = closed to GND = LOW,
       pressed = contact opens = HIGH. An unwired pin also reads HIGH, so
       Chaos only arms after D11 has been seen LOW (button wired, released).
     - Pots are read with the internal pull-up on, so an unwired pin reads a
       steady 1023 instead of floating. A pot only counts as connected once
       it has been seen below POT_SEEN_BELOW (move it off the top end once).
       Until then D8 presets set brightness and speed stays 1x.
     - Each input can also be switched off below with USE_xxx 0.

   Serial Monitor at 115200 prints a status line 4x a second.
   Library: Adafruit NeoPixel. Board: Uno / Nano (ATmega328P).
   ===================================================================== */

#include <Adafruit_NeoPixel.h>

// ------------------------- EASY-TO-CHANGE SETTINGS -------------------------
#define PIN_LED        9
#define LED_COUNT      160    // BTF FCOB 5 mm, 160/m, full metre
#define LED_TYPE       (NEO_GRB + NEO_KHZ800)
// Lengths that were 24 and 48 on the 144-LED strip (one sixth and one third).
#define COMET_TAIL     (LED_COUNT / 6)   // 26 LEDs
#define WAVE_PERIOD    (LED_COUNT / 3)   // 53 LEDs: three humps along the strip

#define PIN_BTN1       4     // next pattern
#define PIN_BTN2       8     // brightness preset / colour shift
#define PIN_KEY        5     // key switch NO -> rainbow
#define PIN_MOD        6     // modifier -> mirror + reverse
#define PIN_POWER      7     // power -> blackout when OFF
#define PIN_WAVE       10    // wave overlay
#define PIN_CHAOS      11    // mushroom NC -> chaos sparkle
#define PIN_SLIDE      A0    // brightness
#define PIN_ROTARY     A1    // speed / hue

// 1 = use that input, 0 = ignore it completely (reads as OFF)
#define USE_KEY        1
#define USE_MOD        1
#define USE_POWER      1
#define USE_WAVE       1
#define USE_CHAOS      1
#define USE_SLIDE_POT  1
#define USE_ROTARY_POT 1

#define CHAOS_NC       1     // 1 = mushroom wired on its NC contact (pressed = HIGH)
                             // 0 = wired on an NO contact (pressed = LOW)

#define BRIGHTNESS_CAP 50    // 160 LEDs on the 5 V 3 A supply: full white ~1.9 A (60 mA/LED at 255)
const uint8_t BRIGHT_PRESETS[] = { 5, 10, 20, 35, 50 };   // D8 steps (all <= cap)
#define BOOT_PRESET    2     // start at 20/255

#define POT_DEADBAND   6     // ignore pot changes smaller than this (0-1023 counts)
#define POT_SEEN_BELOW 1000  // a pot counts as wired once it reads below this
#define DEBOUNCE_MS    20
#define STATUS_MS      250   // Serial status line period
#define FRAME_MS       16    // ~60 fps
#define CHAOS_SLOT_MS  110   // sparkle re-pick period: ~9 Hz, single pixels only (not a strobe)
// ---------------------------------------------------------------------------

Adafruit_NeoPixel strip(LED_COUNT, PIN_LED, LED_TYPE);

// ------------------------- Debounced switch --------------------------------
struct Sw {
  uint8_t pin, stable, lastRaw, seenLow;
  uint32_t since;
  void begin(uint8_t p) {
    pin = p; pinMode(pin, INPUT_PULLUP);
    stable = lastRaw = digitalRead(pin); since = millis();
    seenLow = (stable == LOW);
  }
  // returns +1 on a debounced HIGH->LOW edge, -1 on LOW->HIGH, else 0
  int8_t update(uint32_t now) {
    uint8_t raw = digitalRead(pin);
    if (raw != lastRaw) { lastRaw = raw; since = now; }
    if (raw != stable && (now - since) >= DEBOUNCE_MS) {
      stable = raw;
      if (stable == LOW) seenLow = 1;
      Serial.print(F("  [D")); Serial.print(pin);
      Serial.println(stable == LOW ? F("] LOW (closed to GND)") : F("] HIGH (open)"));
      return stable == LOW ? 1 : -1;
    }
    return 0;
  }
  bool low() const { return stable == LOW; }
};

Sw sBtn1, sBtn2, sKey, sMod, sPower, sWave, sChaos;

// ------------------------- Pot (smoothed + deadband) ------------------------
struct Pot {
  uint8_t pin; bool seen; int16_t raw; int16_t ema8; int16_t value;  // ema8 = raw*8
  void begin(uint8_t p) {
    pin = p; pinMode(pin, INPUT_PULLUP);   // unwired pin -> steady 1023, not floating
    raw = analogRead(pin); ema8 = raw << 3; value = raw; seen = (raw < POT_SEEN_BELOW);
  }
  void update() {
    raw = analogRead(pin);
    if (raw < POT_SEEN_BELOW) seen = true;
    ema8 += (raw - (ema8 >> 3));          // EMA, alpha = 1/8
    int16_t s = ema8 >> 3;
    if (abs(s - value) >= POT_DEADBAND || (s <= 2 && value != 0) || (s >= 1021 && value != 1023))
      value = (s <= 2) ? 0 : ((s >= 1021) ? 1023 : s);
  }
};

Pot potSlide, potRot;

// ------------------------- State -------------------------------------------
enum { M_SOLID, M_RAINBOW, M_COMET, M_BREATHE, M_THEATER, M_COUNT };
const char N0[] PROGMEM = "SOLID";
const char N1[] PROGMEM = "RAINBOW";
const char N2[] PROGMEM = "COMET";
const char N3[] PROGMEM = "BREATHE";
const char N4[] PROGMEM = "THEATER";
const char* const MODE_NAMES[M_COUNT] PROGMEM = { N0, N1, N2, N3, N4 };
void printMode(uint8_t m) {
  char buf[8]; strncpy_P(buf, (const char*)pgm_read_word(&MODE_NAMES[m]), sizeof(buf));
  buf[7] = 0; Serial.print(buf);
}

uint8_t  mode = M_RAINBOW;
uint8_t  presetIdx = BOOT_PRESET;
uint16_t hueShift = 0;          // D8 shifts colour once the slide pot is live
uint32_t animT = 0;             // animation clock (ms, scaled by speed)
uint16_t animFrac = 0;
uint32_t lastFrame = 0, lastStatus = 0;
uint8_t  bright = 20;
uint16_t speedQ8 = 256;         // 256 = 1.0x

const uint8_t N_PRESETS = sizeof(BRIGHT_PRESETS);

// ------------------------- Effective switch states --------------------------
bool slideLive()  { return USE_SLIDE_POT && potSlide.seen; }
bool rotLive()    { return USE_ROTARY_POT && potRot.seen; }
bool keyOn()      { return USE_KEY  && sKey.low(); }
bool modOn()      { return USE_MOD  && sMod.low(); }
bool waveOn()     { return USE_WAVE && sWave.low(); }
bool powerWired() { return USE_POWER && sPower.seenLow; }
bool powerOn()    { return !powerWired() || sPower.low(); }
bool chaosArmed() { return USE_CHAOS && (CHAOS_NC ? sChaos.seenLow : true); }
bool chaosOn()    { return chaosArmed() && (CHAOS_NC ? !sChaos.low() : sChaos.low()); }

// ------------------------- Rendering ---------------------------------------
static inline uint8_t hash8(uint16_t x) {      // cheap stateless pseudo-random
  x ^= x >> 7; x *= 0x2F4B; x ^= x >> 9; x *= 0x1C6D; return (uint8_t)(x ^ (x >> 8));
}

void putScaled(uint16_t i, uint32_t c, uint8_t level /*0-255 extra dim*/) {
  c = strip.gamma32(c);
  uint16_t k = (uint16_t)bright * level / 255;   // final scale 0..BRIGHTNESS_CAP
  uint8_t r = (uint8_t)((c >> 16) & 0xFF) * k / 255;
  uint8_t g = (uint8_t)((c >> 8) & 0xFF) * k / 255;
  uint8_t b = (uint8_t)(c & 0xFF) * k / 255;
  strip.setPixelColor(i, r, g, b);
}

void render(uint32_t now) {
  if (!powerOn()) {                              // blackout; LED 0 glows faintly so you know it's alive
    strip.clear();
    strip.setPixelColor(0, 2, 0, 0);
    strip.show();
    return;
  }
  bool mirror = modOn();
  int32_t t = mirror ? -(int32_t)animT : (int32_t)animT;   // modifier also reverses direction
  uint8_t m = keyOn() ? (uint8_t)M_RAINBOW : mode;
  uint16_t baseHue = (m == M_SOLID && rotLive()) ? (uint16_t)((uint32_t)potRot.value * 64) : hueShift;
  bool chaos = chaosOn();
  uint16_t slot = now / CHAOS_SLOT_MS;
  uint8_t slotPhase = (uint8_t)(255 - (uint32_t)(now % CHAOS_SLOT_MS) * 255 / CHAOS_SLOT_MS);

  for (uint16_t i = 0; i < LED_COUNT; i++) {
    uint16_t p = i;
    if (mirror) p = (i < LED_COUNT / 2) ? i * 2 : (LED_COUNT - 1 - i) * 2;   // fold about the centre

    if (chaos) {                                 // random single-pixel sparkles, ~6% of LEDs per slot
      uint8_t h = hash8(i * 37 + slot * 101);
      if (h < 16) putScaled(i, strip.ColorHSV((uint16_t)hash8(i + slot) << 8), slotPhase);
      else        strip.setPixelColor(i, 0);
      continue;
    }

    uint32_t c; uint8_t lvl = 255;
    switch (m) {
      case M_SOLID:
        c = strip.ColorHSV(baseHue); break;
      case M_RAINBOW:
        c = strip.ColorHSV((uint16_t)(p * (65536UL / LED_COUNT) + t * 20 + hueShift)); break;
      case M_COMET: {
        int32_t head = t / 12; head %= LED_COUNT; if (head < 0) head += LED_COUNT;
        int16_t d = (int16_t)head - (int16_t)p; if (d < 0) d += LED_COUNT;
        if (d >= (int16_t)COMET_TAIL) { strip.setPixelColor(i, 0); continue; }
        c = strip.ColorHSV(hueShift + 6000);
        lvl = (uint8_t)(255 - (uint16_t)d * 240 / COMET_TAIL);  // fades out over the tail
        break;
      }
      case M_BREATHE: {
        uint8_t s = strip.sine8((uint8_t)(t / 12));
        c = strip.ColorHSV(hueShift + 40000); lvl = 20 + (uint16_t)s * 235 / 255;
        break;
      }
      default: {                                 // THEATER: every 3rd LED, marching
        int32_t step = t / 90; uint8_t ph = (uint8_t)(((step % 3) + 3) % 3);
        if ((p % 3) != ph) { strip.setPixelColor(i, 0); continue; }
        c = strip.ColorHSV((uint16_t)(hueShift + t * 8));
        break;
      }
    }
    if (waveOn()) {                              // travelling brightness wave, one third of the strip
      int32_t phase = (int32_t)((uint32_t)p * 256UL / WAVE_PERIOD) - (t / 4);
      uint8_t s = strip.sine8((uint8_t)phase);
      lvl = (uint16_t)lvl * (15 + (uint16_t)s * 240 / 255) / 255;
    }
    putScaled(i, c, lvl);
  }
  strip.show();
}

// ------------------------- Status line -------------------------------------
void pState(const __FlashStringHelper* name, bool used, bool on) {
  Serial.print(name); Serial.print(!used ? F("--") : (on ? F("ON") : F("off")));
}

void printStatus() {
  Serial.print(F("A0="));
  if (!USE_SLIDE_POT) Serial.print(F("--"));
  else { Serial.print(potSlide.raw); if (!potSlide.seen) Serial.print(F("(not seen)")); }
  Serial.print(F(" A1="));
  if (!USE_ROTARY_POT) Serial.print(F("--"));
  else { Serial.print(potRot.raw); if (!potRot.seen) Serial.print(F("(not seen)")); }
  Serial.print(F(" | D4=")); Serial.print(sBtn1.low() ? F("DOWN") : F("up"));
  Serial.print(F(" D8="));   Serial.print(sBtn2.low() ? F("DOWN") : F("up"));
  pState(F(" D5key="), USE_KEY, keyOn());
  pState(F(" D6mod="), USE_MOD, modOn());
  Serial.print(F(" D7pwr="));
  if (!USE_POWER) Serial.print(F("--"));
  else if (!powerWired()) Serial.print(F("unwired"));
  else Serial.print(sPower.low() ? F("ON") : F("OFF"));
  pState(F(" D10wave="), USE_WAVE, waveOn());
  Serial.print(F(" D11chaos="));
  if (!USE_CHAOS) Serial.print(F("--"));
  else if (!chaosArmed()) Serial.print(F("unwired"));
  else Serial.print(chaosOn() ? F("ON") : F("off"));
  Serial.print(F(" | mode=")); printMode(keyOn() ? (uint8_t)M_RAINBOW : mode);
  Serial.print(F(" bright=")); Serial.print(bright);
  Serial.print(slideLive() ? F("(pot)") : F("(preset)"));
  Serial.print(F(" speed=")); Serial.print(speedQ8 / 256.0, 2); Serial.println('x');
}

// ------------------------- Setup / loop ------------------------------------
void bootWipe() {
  for (uint8_t c = 0; c < 3; c++) {
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, c == 0 ? 20 : 0, c == 1 ? 20 : 0, c == 2 ? 20 : 0);
      strip.show(); delay(3);
    }
    delay(150);
  }
  strip.clear(); strip.show();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("=== Spiral Lamp bench test v2 ==="));
  Serial.println(F("D4 pattern | D8 brightness preset (colour shift once A0 live) | A0 brightness | A1 speed/hue"));
  Serial.println(F("D5 key=rainbow | D6=mirror+reverse | D7 power OFF=blackout | D10=wave | D11 mushroom=chaos"));

  sBtn1.begin(PIN_BTN1); sBtn2.begin(PIN_BTN2);
  sKey.begin(PIN_KEY);   sMod.begin(PIN_MOD);   sPower.begin(PIN_POWER);
  sWave.begin(PIN_WAVE); sChaos.begin(PIN_CHAOS);
  potSlide.begin(PIN_SLIDE); potRot.begin(PIN_ROTARY);

  if (sBtn1.low()) Serial.println(F("WARNING: D4 reads pressed at boot - check COM/NO (not NC)"));
  if (sBtn2.low()) Serial.println(F("WARNING: D8 reads pressed at boot - check COM/NO (not NC)"));
  if (USE_CHAOS && CHAOS_NC && !sChaos.seenLow)
    Serial.println(F("D11 reads open: mushroom not wired yet (or pressed). Chaos arms once it reads closed."));

  bright = BRIGHT_PRESETS[presetIdx];
  strip.begin();
  strip.setBrightness(255);     // scaling is done in putScaled(), capped at BRIGHTNESS_CAP
  strip.clear(); strip.show();
  bootWipe();
  lastFrame = millis();
}

void loop() {
  uint32_t now = millis();

  if (sBtn1.update(now) == 1) {
    mode = (mode + 1) % M_COUNT;
    Serial.print(F("D4 -> pattern ")); printMode(mode); Serial.println();
  }
  if (sBtn2.update(now) == 1) {
    if (slideLive()) {
      hueShift += 65536UL / 6;
      Serial.println(F("D8 -> colour shift (slide pot owns brightness)"));
    } else {
      presetIdx = (presetIdx + 1) % N_PRESETS;
      Serial.print(F("D8 -> brightness preset ")); Serial.println(BRIGHT_PRESETS[presetIdx]);
    }
  }
  int8_t e;
  if ((e = sKey.update(now)) && USE_KEY)     Serial.println(e > 0 ? F("Key ON -> rainbow") : F("Key off"));
  if ((e = sMod.update(now)) && USE_MOD)     Serial.println(e > 0 ? F("Modifier ON -> mirror + reverse") : F("Modifier off"));
  if ((e = sPower.update(now)) && USE_POWER) Serial.println(e > 0 ? F("Power ON") : F("Power OFF -> blackout"));
  if ((e = sWave.update(now)) && USE_WAVE)   Serial.println(e > 0 ? F("Wave ON") : F("Wave off"));
  if ((e = sChaos.update(now)) && USE_CHAOS) {
    if (CHAOS_NC) Serial.println(e > 0 ? F("Mushroom released (closed) -> chaos off / armed") : F("Mushroom PRESSED (open) -> CHAOS"));
    else          Serial.println(e > 0 ? F("Chaos switch closed -> CHAOS") : F("Chaos off"));
  }

  if (now - lastFrame >= FRAME_MS) {
    uint32_t dt = now - lastFrame; lastFrame = now;

    if (USE_SLIDE_POT)  potSlide.update();
    if (USE_ROTARY_POT) potRot.update();

    bright = slideLive() ? (uint8_t)((uint32_t)potSlide.value * BRIGHTNESS_CAP / 1023)
                         : BRIGHT_PRESETS[presetIdx];
    if (bright > BRIGHTNESS_CAP) bright = BRIGHTNESS_CAP;

    // A1 -> 0.25x .. 4x (exponential, 1x at the centre). In SOLID mode A1 is hue instead.
    if (rotLive() && mode != M_SOLID)
      speedQ8 = (uint16_t)(64.0 * pow(2.0, potRot.value * 4.0 / 1023.0) + 0.5);
    else
      speedQ8 = 256;

    uint32_t adv = dt * speedQ8 + animFrac;
    animT += adv >> 8; animFrac = adv & 0xFF;

    render(now);
  }

  if (now - lastStatus >= STATUS_MS) { lastStatus = now; printStatus(); }
}
