#include <Adafruit_NeoPixel.h>
#include <math.h>

// ======== Forward decls for Arduino auto-prototypes ========
struct IAnim;
void addAnimation(IAnim* a, uint32_t now);
// ===========================================================

// ---------------------- Pins & Strip ------------------------
#define PIN_LED        9
#define LED_COUNT      60

#define PIN_ROCKER     7   // Active LOW (main power)
#define PIN_TOGGLE     6   // Active LOW (BG rainbow when key OFF; advances Wave colors)
#define PIN_ARCADE     8   // Active LOW (red arcade button)   -> 'L'
#define PIN_GREEN      4   // Active LOW (white/green button)  -> 'R'
#define PIN_KEY        5   // Active LOW (segmented rainbow override)
#define PIN_FADER      A0  // Analog brightness fader
#define PIN_SPEED_KNOB A1  // Analog rotary pot (global speed / secret fill-count)
#define PIN_WAVE       10  // Active LOW (Toggle with red cover: Wave effect OVERRIDE)
#define PIN_CHAOS      11  // Active LOW (Big red switch: Chaos mode OVERRIDE)

Adafruit_NeoPixel strip(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);

// ---------------------- Tunables ---------------------------
// Global canonical colors (your preferred values)
const uint8_t YELLOW_R = 255, YELLOW_G = 112, YELLOW_B = 0;  // updated yellow (G=112)
const uint8_t ORANGE_R = 255, ORANGE_G =  50,  ORANGE_B = 0; // orange (G=50)

const uint8_t  BRIGHTNESS_BOOT   = 250;
const uint8_t  BRIGHTNESS_MIN    = 5;
const uint8_t  BRIGHTNESS_MAX    = 255;
const float    BRIGHTNESS_SMOOTH = 0.25f;   // EMA for brightness

// ---- Global Color Calibration (helps tame green/push red) ----
float CAL_R_GAIN = 1.06f;  // slight red lift
float CAL_G_GAIN = 0.94f;  // slight green cut
float CAL_B_GAIN = 1.00f;  // blue unchanged

// Lamp off-white (default when no rainbow)
const uint8_t  LAMP_R = 255, LAMP_G = 240, LAMP_B = 210;

// Rocker wipe base timings (scaled by speed live)
const uint16_t WIPE_MS_ON  = 800;
const uint16_t WIPE_MS_OFF = 800;

// BG rainbow (when key OFF and toggle ON)
const uint16_t RAINBOW_FADE_MS = 1000;

// Key-mode segmented palette
const uint8_t  SEG_SOLID_LEDS   = 3;
const float    SEG_EDGE_WIDTH   = 1.0f;
const uint16_t SEG_MS_PER_LED   = 120;
const uint8_t  SEG_VAL          = 220;

// Red/Green dot traversal
const float    DOT_TRAVEL_TIME_S = 0.5f;
const float    DOT_VEL = LED_COUNT / DOT_TRAVEL_TIME_S; // LEDs/sec
const float    GREEN_SPEED_SCALE = 0.5f;

// Key center-out timings (base; knob scales these at runtime)
const uint16_t KEY_CENTER_ON_MS  = 800;
const uint16_t KEY_CENTER_OFF_MS = 800;

// Crossfade (Classic -> Color) base duration (scaled by speed)
const uint16_t MODE_FADE_MS = 350;

// ---------------------- Brightness/Fader -------------------
uint8_t g_brightness = BRIGHTNESS_BOOT;
uint8_t g_brightnessTarget = BRIGHTNESS_BOOT;
bool    g_lockBrightness = false;             // used during OFF wipe & Fill-Count
uint8_t g_lockedBrightness = BRIGHTNESS_BOOT;

static inline uint8_t clamp8(int v) { return (v < 0) ? 0 : (v > 255 ? 255 : (uint8_t)v); }

// ---------------------- Helpers ----------------------------
static inline uint8_t clamp8i(int v){ return (v<0)?0:((v>255)?255:(uint8_t)v); }

// Calibrated per-pixel setter (applies global color gains)
static inline void setPixelRGB(uint16_t i, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t rc = clamp8i((int)(r * CAL_R_GAIN + 0.5f));
  uint8_t gc = clamp8i((int)(g * CAL_G_GAIN + 0.5f));
  uint8_t bc = clamp8i((int)(b * CAL_B_GAIN + 0.5f));
  strip.setPixelColor(i, rc, gc, bc);
}

static inline uint32_t packRGB(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline void unpackRGB(uint32_t c, uint8_t &r, uint8_t &g, uint8_t &b) {
  r = (c >> 16) & 0xFF; g = (c >> 8) & 0xFF; b = 0xFF & c;
}
void clearFrame() { for (uint16_t i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, 0, 0, 0); }

static uint32_t hsvToRgb(uint16_t h16, uint8_t s, uint8_t v) {
  uint8_t r, g, b;
  uint8_t region = (h16 >> 8) / 43;
  uint16_t remainder = (h16 >> 8) - (region * 43);
  uint8_t p = (uint16_t)v * (255 - s) / 255;
  uint8_t q = (uint16_t)v * (255 - ((uint16_t)s * remainder) / 43) / 255;
  uint8_t t = (uint16_t)v * (255 - ((uint16_t)s * (43 - remainder)) / 43) / 255;
  switch (region) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default:r = v; g = p; b = q; break;
  }
  return packRGB(r,g,b);
}
static inline uint16_t degToHue16(uint16_t deg) { return (uint32_t)deg * 65535UL / 360UL; }

uint8_t readFaderMapped() {
  int raw = analogRead(PIN_FADER);          // 0..1023
  int mapped = BRIGHTNESS_MIN + (long)raw * (BRIGHTNESS_MAX - BRIGHTNESS_MIN) / 1023L;
  return clamp8(mapped);
}

void updateBrightnessFromFader() {
  g_brightnessTarget = readFaderMapped();
  if (!g_lockBrightness) {
    float cur = g_brightness;
    float tgt = g_brightnessTarget;
    float out = (1.0f - BRIGHTNESS_SMOOTH) * cur + BRIGHTNESS_SMOOTH * tgt;
    g_brightness = clamp8((int)(out + 0.5f));
  } else {
    g_brightness = g_lockedBrightness;
  }
  strip.setBrightness(g_brightness);
}

// ---------------------- Global Speed (A1) ------------------
float g_speedTarget = 1.0f;
float g_speed       = 1.0f;

const float SPEED_EMA        = 0.45f;
const float SPEED_DB         = 0.006f;
const float SPEED_MIN        = 0.05f;
const float SPEED_MAX        = 4.0f;
const float SPEED_SLEW_UP    = 8.0f;
const float SPEED_SLEW_DOWN  = 6.0f;
const float SPEED_SNAP_JUMP  = 0.10f;

uint32_t g_lastSpeedUpdateMs = 0;
float    g_lastKnobNorm = 0.5f;

static inline float mapDialToSpeed(float norm) {
  if (norm < 0.5f) return 0.05f + (norm / 0.5f) * (1.0f - 0.05f);
  return 1.0f + ((norm - 0.5f) / 0.5f) * (4.0f - 1.0f);
}

void updateSpeedFromKnob(uint32_t now) {
  int raw = analogRead(PIN_SPEED_KNOB);   // 0..1023
  float rawNorm = raw / 1023.0f;

  if (fabsf(rawNorm - g_lastKnobNorm) < SPEED_DB) {
    rawNorm = g_lastKnobNorm;
  }

  float norm = (1.0f - SPEED_EMA) * g_lastKnobNorm + SPEED_EMA * rawNorm;

  bool bigJump = fabsf(norm - g_lastKnobNorm) > SPEED_SNAP_JUMP;

  g_lastKnobNorm = norm;

  float target = mapDialToSpeed(norm);
  if (target < SPEED_MIN) target = SPEED_MIN;
  if (target > SPEED_MAX) target = SPEED_MAX;

  if (g_lastSpeedUpdateMs == 0) {
    g_speed = g_speedTarget = target;
    g_lastSpeedUpdateMs = now;
    return;
  }

  uint32_t dtMs = now - g_lastSpeedUpdateMs;
  g_lastSpeedUpdateMs = now;
  float dt = dtMs / 1000.0f;

  if (bigJump) {
    g_speed = target;
  } else {
    float slew = (target > g_speed) ? SPEED_SLEW_UP : SPEED_SLEW_DOWN;
    float maxDelta = slew * dt;
    float delta = target - g_speed;
    if (delta >  maxDelta) delta =  maxDelta;
    if (delta < -maxDelta) delta = -maxDelta;
    g_speed += delta;
  }

  g_speedTarget = target;
}

static inline uint16_t scaledMs(uint16_t baseMs) {
  float s = (g_speed < 0.1f) ? 0.1f : g_speed;
  return (uint16_t)(baseMs / s);
}

// ---------------------- Input Manager ----------------------
struct DebouncedInput {
  uint8_t pin; bool activeLow; uint8_t stable, lastStable; uint32_t lastChangeMs;
  void begin(uint8_t p, bool isActiveLow) { pin = p; activeLow = isActiveLow; pinMode(pin, INPUT_PULLUP); stable = lastStable = digitalRead(pin); lastChangeMs = millis(); }
  uint8_t update(uint32_t now, uint16_t debounceMs = 15) {
    uint8_t raw = digitalRead(pin);
    if (raw != stable && (now - lastChangeMs) >= debounceMs) {
      lastStable = stable; stable = raw; lastChangeMs = now;
      if (lastStable == LOW && stable == HIGH) return 1; // rose
      if (lastStable == HIGH && stable == LOW)  return 2; // fell
    }
    return 0;
  }
  bool pressed() const { return activeLow ? (stable == LOW) : (stable == HIGH); }
};
DebouncedInput inRocker, inToggle, inArcade, inKey, inGreen, inWave, inChaos;

// Global held states for arcade buttons
bool g_arcadeHeld = false;  // red (L)
bool g_greenHeld  = false;  // white (R)

// ---------------------- Rocker Modes -----------------------
enum RockerMode : uint8_t { MODE_COLOR = 0, MODE_CLASSIC = 1 };
RockerMode g_rockerMode = MODE_COLOR;
RockerMode g_rockerModePrev = MODE_COLOR;

// --- Classic entry via sequence ---
const char* CLASSIC_SEQ = "RLRRLRLL";   // R=white button, L=red button
const uint8_t CLASSIC_SEQ_LEN = 8;
uint8_t g_seqIdx = 0; // how many correct presses matched so far

// ---------------------- Animation Base ---------------------
struct IAnim {
  bool active = false; virtual ~IAnim() {}
  virtual void start(uint32_t now) { active = true; (void)now; }
  virtual void update(uint32_t now) = 0;
  virtual void render() = 0;
  virtual bool finished() const = 0;
};

// ---------------------- BG (toggle ROYGBIV) ----------------
struct BgManager {
  uint8_t curR = LAMP_R, curG = LAMP_G, curB = LAMP_B;
  uint8_t tgtR = LAMP_R, tgtG = LAMP_G, tgtB = LAMP_B;
  uint8_t startR = LAMP_R, startG = LAMP_G, startB = LAMP_B;
  bool powerOn = false;
  bool rainbowEnabled = false;
  uint32_t phaseStart = 0;

  static const uint8_t RAINBOW_COUNT = 7;
  uint16_t rainbowHue[RAINBOW_COUNT] = {
    degToHue16(0),  degToHue16(25), degToHue16(60),
    degToHue16(120), degToHue16(240), degToHue16(275), degToHue16(300)
  };
  uint8_t idx = 0;

  void setPower(bool on, uint32_t now) {
    powerOn = on;
    phaseStart = now;
  }

  void setRainbowEnabled(bool enabled, uint32_t now) {
    if (rainbowEnabled == enabled) return;
    rainbowEnabled = enabled;
    if (!rainbowEnabled) {
      tgtR = curR; tgtG = curG; tgtB = curB;
    } else {
      uint32_t rgb = hsvToRgb(rainbowHue[idx], 255, 220);
      tgtR = (rgb >> 16) & 0xFF; tgtG = (rgb >> 8) & 0xFF; tgtB = rgb & 0xFF;
      startR = curR; startG = curG; startB = curB; phaseStart = now;
    }
  }

  void nextRainbowTarget(uint32_t now) {
    idx = (idx + 1) % RAINBOW_COUNT;
    uint32_t rgb = hsvToRgb(rainbowHue[idx], 255, 220);
    startR = curR; startG = curG; startB = curB;
    tgtR = (rgb >> 16) & 0xFF; tgtG = (rgb >> 8) & 0xFF; tgtB = rgb & 0xFF;
    phaseStart = now;
  }

  void update(uint32_t now) {
    if (!powerOn) return;
    if (!rainbowEnabled) { curR = tgtR; curG = tgtG; curB = tgtB; return; }

    float s = (g_speed < 0.1f) ? 0.1f : g_speed;
    uint32_t effectiveMs = (uint32_t)(RAINBOW_FADE_MS / s);
    if (effectiveMs < 10) effectiveMs = 10;

    uint32_t t = now - phaseStart;
    if (t >= effectiveMs) { curR = tgtR; curG = tgtG; curB = tgtB; nextRainbowTarget(now); return; }
    float u = (float)t / (float)effectiveMs;
    curR = (uint8_t)((1 - u) * startR + u * tgtR);
    curG = (uint8_t)((1 - u) * startG + u * tgtG);
    curB = (uint8_t)((1 - u) * startB + u * tgtB);
  }

  void renderBackground() {
    if (!powerOn) { clearFrame(); return; }
    for (uint16_t i = 0; i < LED_COUNT; i++) setPixelRGB(i, curR, curG, curB);
  }
} bg;

// ------------------ Segmented Scrolling (KEY) -------------
struct SegmentedRainbow {
  static const uint8_t PAL_N = 6;
  const uint8_t palette[PAL_N][3] = {
    {255,   0,   0},                      // Red
    {ORANGE_R, ORANGE_G, ORANGE_B},       // Orange (global)
    {YELLOW_R, YELLOW_G, YELLOW_B},       // Yellow (global)
    {  0, 255,   0},                      // Green
    {  0,   0, 255},                      // Blue
    {170,   0, 255}                       // Purple
  };
  float phase = 0.0f;
  uint32_t lastMs = 0;
  const float BLOCK = (float)SEG_SOLID_LEDS;

  static void scaleToVal(uint8_t inR, uint8_t inG, uint8_t inB, uint8_t &r, uint8_t &g, uint8_t &b) {
    uint8_t mx = (inR>inG?inR:inG); if (inB>mx) mx = inB;
    if (mx == 0) { r = g = b = 0; return; }
    r = (uint16_t)inR * SEG_VAL / mx;
    g = (uint16_t)inG * SEG_VAL / mx;
    b = (uint16_t)inB * SEG_VAL / mx;
  }
  void getPal(uint8_t idx, uint8_t &r, uint8_t &g, uint8_t &b) const {
    const uint8_t* c = palette[idx % PAL_N];
    scaleToVal(c[0], c[1], c[2], r, g, b);
  }

  void reset(uint32_t now) { lastMs = now; }

  void update(uint32_t now, float msPerLed, float dirSign) {
    uint32_t dt_ms = now - lastMs; lastMs = now;
    if (dt_ms == 0) return;
    phase += dirSign * ((float)dt_ms / msPerLed) * g_speed;
    float cycle = BLOCK * PAL_N;
    while (phase >= cycle) phase -= cycle;
    while (phase <    0)   phase += cycle;
  }

  void sample(int i, uint8_t &r, uint8_t &g, uint8_t &b) const {
    float cycle = BLOCK * PAL_N;
    float edge = SEG_EDGE_WIDTH;
    float pos = fmodf(i + phase, cycle);
    if (pos < 0) pos += cycle;

    int   blockIdx = (int)floorf(pos / BLOCK);
    float within   = pos - blockIdx * BLOCK;
    int   nextIdx  = (blockIdx + 1) % PAL_N;

    uint8_t r0,g0,b0,r1,g1,b1;
    getPal(blockIdx, r0,g0,b0);
    getPal(nextIdx,  r1,g1,b1);

    float w = (within <= (BLOCK - edge)) ? 0.0f : ((within >= BLOCK) ? 1.0f : ((within-(BLOCK-edge))/edge));
    r = (uint8_t)((1.0f - w) * r0 + w * r1);
    g = (uint8_t)((1.0f - w) * g0 + w * g1);
    b = (uint8_t)((1.0f - w) * b0 + w * b1);
  }

  void render() const {
    for (int i = 0; i < LED_COUNT; i++) {
      uint8_t r,g,b; sample(i, r,g,b);
      setPixelRGB(i, r,g,b);
    }
  }
} segRainbow;

// ---------------------- Reversible Rocker Wipe -------------
struct RockerWipe {
  bool     active = false;
  float    litFrac = 0.0f;
  float    targetFrac = 0.0f;
  uint16_t baseMs = 800;     // store unscaled base duration
  uint32_t lastMs = 0;

  static inline uint16_t effectiveMs(uint16_t base){
    float s = (g_speed < 0.1f) ? 0.1f : g_speed;
    uint32_t eff = (uint32_t)(base / s);
    return (eff < 10) ? 10 : (uint16_t)eff;
  }

  void requestOn(uint32_t now, uint16_t base) {
    baseMs = base;
    targetFrac = 1.0f;
    active = true;
    if (lastMs == 0) lastMs = now;
  }
  void requestOff(uint32_t now, uint16_t base) {
    baseMs = base;
    targetFrac = 0.0f;
    active = true;
    if (lastMs == 0) lastMs = now;
  }
  bool finished() const { return !active; }

  void update(uint32_t now) {
    if (!active) return;
    uint32_t dtMs = (lastMs == 0) ? 0 : (now - lastMs);
    lastMs = now;
    if (dtMs == 0) return;

    float step = (float)dtMs / (float)effectiveMs(baseMs);
    if (targetFrac > litFrac) {
      litFrac += step;
      if (litFrac >= targetFrac) litFrac = targetFrac;
    } else if (targetFrac < litFrac) {
      litFrac -= step;
      if (litFrac <= targetFrac) litFrac = targetFrac;
    }

    if (litFrac == targetFrac) {
      active = false;
      lastMs = 0;
    }
  }

  void apply(uint32_t /*now*/) {
    int lit = (int)floorf(litFrac * LED_COUNT + 0.5f);
    if (lit < 0) lit = 0;
    if (lit > LED_COUNT) lit = LED_COUNT;
    for (int i = lit; i < LED_COUNT; i++) setPixelRGB(i, 0,0,0);
  }
} rockerWipe;

// ---------------------- Key Center-Out ---------------------
struct KeyCenterWipe {
  enum Mode { BG_TO_KEY, KEY_TO_BG } mode = BG_TO_KEY;
  bool active = false;
  uint32_t startMs = 0;
  uint16_t baseMs = 800;   // store unscaled

  static inline uint16_t effectiveMs(uint16_t base){
    float s = (g_speed < 0.1f) ? 0.1f : g_speed;
    uint32_t eff = (uint32_t)(base / s);
    return (eff < 10) ? 10 : (uint16_t)eff;
  }

  void start(Mode m, uint16_t ms, uint32_t now) { mode = m; baseMs = ms; startMs = now; active = true; }
  bool finished(uint32_t now) const { return !active || (now - startMs) >= effectiveMs(baseMs); }

  void apply(uint32_t now, uint8_t baseR, uint8_t baseG, uint8_t baseB, SegmentedRainbow& seg, bool baseActive) {
    if (!active) return;
    uint16_t effMs = effectiveMs(baseMs);
    uint32_t t = now - startMs; if (t > effMs) t = effMs;
    float u = (float)t / (float)effMs;

    float half = (LED_COUNT - 1) / 2.0f;
    float R = u * (half + 0.5f);

    uint8_t br = baseActive ? baseR : 0;
    uint8_t bgG = baseActive ? baseG : 0;
    uint8_t bb = baseActive ? baseB : 0;

    for (int i = 0; i < LED_COUNT; i++) {
      float dist = fabsf(i - half);
      bool inside = (dist <= R);

      if (mode == BG_TO_KEY) {
        if (inside) { uint8_t r,g,b; seg.sample(i, r,g,b); setPixelRGB(i, r,g,b); }
        else        { setPixelRGB(i, br, bgG, bb); }
      } else {
        if (inside) { setPixelRGB(i, br, bgG, bb); }
        else        { uint8_t r,g,b; seg.sample(i, r,g,b); setPixelRGB(i, r,g,b); }
      }
    }
    if (t >= effMs) active = false;
  }
} keyWipe;

// ---------------------- Dots & Pools -----------------------
struct IAnim; struct RedDot; struct ColorDot;

// extra "bounce" frame at the ends for smoother reversal
struct RedDot : public IAnim {
  float pos = -1.0f; float vel = DOT_VEL; uint32_t lastMs = 0;
  uint16_t edgeIndex = 0;
  uint8_t  bounceFrames = 0;

  void start(uint32_t now) override { IAnim::start(now); pos = 0.0f; vel = DOT_VEL; lastMs = now; bounceFrames = 0; }
  void update(uint32_t now) override {
    float dt = (now - lastMs) / 1000.0f; if (dt < 0) dt = 0; lastMs = now;
    if (bounceFrames > 0) return;

    pos += vel * dt * g_speed; const float maxP = (float)(LED_COUNT - 1);
    while (true) {
      if (pos < 0.0f) {
        if (g_arcadeHeld) { edgeIndex = 0; pos = -pos; vel = -vel; bounceFrames = 1; }
        else { active = false; pos = 0.0f; }
        break;
      } else if (pos > maxP) {
        if (g_arcadeHeld) { edgeIndex = (uint16_t)maxP; pos = 2.0f*maxP - pos; vel = -vel; bounceFrames = 1; }
        else { active = false; pos = maxP; }
        break;
      } else break;
    }
  }
  void render() override {
    if (!active) return;
    if (bounceFrames > 0) { if (edgeIndex < LED_COUNT) setPixelRGB(edgeIndex, 255, 0, 0); bounceFrames--; return; }
    int head = (int)floor(pos + 0.5f);
    int tail = (vel >= 0) ? head - 1 : head + 1;
    if (head >= 0 && head < LED_COUNT) setPixelRGB(head, 255, 0, 0);
    if (tail >= 0 && tail < LED_COUNT) setPixelRGB(tail, 255, 0, 0);
  }
  bool finished() const override { return !active; }
};

struct ColorDot : public IAnim {
  float pos = -1.0f; float vel = DOT_VEL * GREEN_SPEED_SCALE; uint32_t lastMs = 0;
  uint8_t cr=0, cg=255, cb=0; uint16_t edgeIndex = 0;
  uint8_t bounceFrames = 0;

  void setColor(uint8_t r, uint8_t g, uint8_t b) { cr=r; cg=g; cb=b; }
  void start(uint32_t now) override { IAnim::start(now); pos = 0.0f; vel = DOT_VEL * GREEN_SPEED_SCALE; lastMs = now; bounceFrames = 0; }
  void update(uint32_t now) override {
    float dt = (now - lastMs) / 1000.0f; if (dt < 0) dt = 0; lastMs = now;
    if (bounceFrames > 0) return;

    pos += vel * dt * g_speed; const float maxP = (float)(LED_COUNT - 1);
    while (true) {
      if (pos < 0.0f) {
        if (g_greenHeld) { edgeIndex = 0; pos = -pos; vel = -vel; bounceFrames = 1; }
        else { active = false; pos = 0.0f; }
        break;
      } else if (pos > maxP) {
        if (g_greenHeld) { edgeIndex = (uint16_t)maxP; pos = 2.0f*maxP - pos; vel = -vel; bounceFrames = 1; }
        else { active = false; pos = maxP; }
        break;
      } else break;
    }
  }
  void render() override {
    if (!active) return;
    if (bounceFrames > 0) { if (edgeIndex < LED_COUNT) setPixelRGB(edgeIndex, cr, cg, cb); bounceFrames--; return; }
    int head = (int)floor(pos + 0.5f);
    int tail = (vel >= 0) ? head - 1 : head + 1;
    if (head >= 0 && head < LED_COUNT) setPixelRGB(head, cr, cg, cb);
    if (tail >= 0 && tail < LED_COUNT) setPixelRGB(tail, cr, cg, cb);
  }
  bool finished() const override { return !active; }
};

const uint8_t MAX_ANIMS = 24;
IAnim* anims[MAX_ANIMS] = { nullptr };
RedDot   redPool[12];
ColorDot greenPool[12];

void addAnimation(IAnim* a, uint32_t now) {
  for (uint8_t i = 0; i < MAX_ANIMS; i++) {
    if (anims[i] == nullptr || !anims[i]->active || anims[i]->finished()) { a->start(now); anims[i] = a; return; }
  }
}
void spawnRedDot(uint32_t now) {
  for (uint8_t i = 0; i < 12; i++) {
    if (!redPool[i].active || redPool[i].finished()) { redPool[i].active = false; addAnimation(&redPool[i], now); return; }
  }
}
uint8_t g_greenColorIdx = 0;
const uint8_t GREEN_PALETTE[6][3] = {
  {  0,255,  0},
  {YELLOW_R, YELLOW_G, YELLOW_B},     // Yellow (global)
  {ORANGE_R, ORANGE_G, ORANGE_B},     // Orange (global)
  {255,  0,  0},
  {160,  0,255},
  {  0,  0,255}
};
void spawnGreenDot(uint32_t now) {
  uint8_t r = GREEN_PALETTE[g_greenColorIdx][0];
  uint8_t g = GREEN_PALETTE[g_greenColorIdx][1];
  uint8_t b = GREEN_PALETTE[g_greenColorIdx][2];
  g_greenColorIdx = (g_greenColorIdx + 1) % 6;

  for (uint8_t i = 0; i < 12; i++) {
    if (!greenPool[i].active || greenPool[i].finished()) {
      greenPool[i].active = false;
      greenPool[i].setColor(r,g,b);
      addAnimation(&greenPool[i], now);
      return;
    }
  }
}

// ---------------------- Wave Effect (D10) — Simplified ------------------
struct WaveEffect {
  // group size (same as before)
  static const uint8_t GROUP = 3;
  static const uint8_t MAX_GROUPS = (LED_COUNT + GROUP - 1) / GROUP;

  // simple tunables
  uint16_t baseCycleMs   = 1200;  // one full bright->dark->bright cycle per group
  uint16_t groupDelayMs  = 40;    // stagger start by group index

  // state
  bool     active         = false;
  uint32_t startMs        = 0;
  uint32_t lastMs         = 0;

  // per-group phase 0..1, and hue index
  float    phase[MAX_GROUPS];
  uint8_t  hueIdx[MAX_GROUPS];

  // track “have we already advanced on this trough?”
  bool     troughArmed[MAX_GROUPS];

  // color-advance control (your first toggle)
  bool     colorAdvanceEnabled = false;

  // publish if any group passed a wrap this frame (not strictly needed now)
  bool     wrappedThisFrame = false;

  // exact requested color order:
  // (blue, purple, red, orange, yellow, green) -> degrees: 240, 275, 0, 25, 60, 120
  static const uint8_t WHEEL_N = 6;
  const uint16_t WHEEL[WHEEL_N] = {240, 275, 0, 25, 60, 120};

  static inline uint32_t wheelRgb(uint16_t deg){
    switch(deg){
      case 240: return packRGB(0,0,255);                                // Blue
      case 275: return packRGB(170,0,255);                               // Purple
      case   0: return packRGB(255,0,0);                                 // Red
      case  25: return packRGB(ORANGE_R, ORANGE_G, ORANGE_B);            // Orange (global)
      case  60: return packRGB(YELLOW_R, YELLOW_G, YELLOW_B);            // Yellow (global)
      case 120: return packRGB(0,255,0);                                 // Green
    }
    return packRGB(0,0,0);
  }

  inline float cycleHz() const {
    float s = (g_speed < 0.1f) ? 0.1f : g_speed;
    float ms = baseCycleMs / s; if (ms < 10.0f) ms = 10.0f;
    return 1000.0f / ms;
  }
  inline uint32_t startOffsetMs(uint8_t gi) const {
    float s = (g_speed < 0.1f) ? 0.1f : g_speed;
    uint32_t d = (uint32_t)((groupDelayMs / s) * gi);
    return (d < 1 ? 1 : d);
  }

  // brightness curve 0..1 using cosine (darkest at trough ~0)
  static inline float waveBrightness(float p) {
    // p in [0,1): br = (1 - cos(2πp)) / 2  => 0 at troughs p=0,1 and 1 at crest p=0.5
    return 0.5f * (1.0f - cosf(6.2831853f * p));
  }

  // “near trough” predicate so we can detect/advance colors
  static inline bool nearTrough(float p) {
    // window around 0 (and implicitly 1). Adjust 0.06 if you want tighter/looser
    const float W = 0.06f;
    return (p <= W) || (p >= 1.0f - W);
  }

  void begin(uint32_t now){
    active = true; startMs = lastMs = now; wrappedThisFrame = false;
    for (uint8_t gi=0; gi<MAX_GROUPS; gi++){
      // stagger phase so each group enters later as if start delay
      float offset = 0.0f;
      uint32_t so  = startOffsetMs(gi);
      if (so > 0) offset = - (so / (float)baseCycleMs); // negative means “not yet started”
      phase[gi] = fmodf(offset, 1.0f); if (phase[gi] < 0) phase[gi] += 1.0f;

      // alternate palette starting points to create the checkerboard effect
      hueIdx[gi] = (gi % 2 == 0) ? 0 : 1; // 0: Blue, 1: Purple
      troughArmed[gi] = true;             // ready to advance on the FIRST trough
    }
  }

  void endNow(){ active = false; }

  // Turn color stepping on/off. When turning ON, immediately advance any group
  // that is *currently* at the trough so the effect starts “wherever it is.”
  void setColorAdvanceEnabled(bool en){
    if (en && !colorAdvanceEnabled) {
      for (uint8_t gi=0; gi<MAX_GROUPS; gi++){
        if (nearTrough(phase[gi])) {
          hueIdx[gi] = (hueIdx[gi] + 1) % WHEEL_N;  // immediate kick if at trough now
          troughArmed[gi] = false;                  // don’t double-advance until leaving trough
        }
      }
    }
    colorAdvanceEnabled = en;
  }

  void render(uint32_t now, bool toggleOn){
    if (!active) return;

    // keep external flag fresh
    wrappedThisFrame = false;

    // time step
    uint32_t dtMs = (lastMs == 0) ? 0 : (now - lastMs);
    lastMs = now;
    float dt = dtMs / 1000.0f;
    float f  = cycleHz();

    // ensure internal toggle state matches caller
    setColorAdvanceEnabled(toggleOn);

    for (uint8_t gi=0, base=0; gi<MAX_GROUPS; gi++, base+=GROUP){
      float prev = phase[gi];
      phase[gi] += dt * f;
      if (phase[gi] >= 1.0f) { phase[gi] -= floorf(phase[gi]); wrappedThisFrame = true; }

      bool atTrough = nearTrough(phase[gi]);

      if (!atTrough) {
        troughArmed[gi] = true;  // left trough window – re-arm for next pass
      } else if (troughArmed[gi] && colorAdvanceEnabled) {
        hueIdx[gi] = (hueIdx[gi] + 1) % WHEEL_N;  // step once on entrance to trough
        troughArmed[gi] = false;
      }

      float br = waveBrightness(phase[gi]);   // 0..1
      uint32_t rgb = wheelRgb(WHEEL[hueIdx[gi]]);
      uint8_t r=(rgb>>16)&0xFF, g=(rgb>>8)&0xFF, b=rgb&0xFF;
      r = (uint8_t)(r * br);
      g = (uint8_t)(g * br);
      b = (uint8_t)(b * br);

      for (uint8_t i=0;i<GROUP && base+i<LED_COUNT;i++) {
        setPixelRGB(base+i, r,g,b);
      }
    }
  }
} wave;

// ---------------------- CHAOS Effect (D11) -----------------
struct ChaosEffect {
  bool     active = false;
  uint32_t lastUpdate = 0;
  uint32_t baseIntervalMs = 180;

  static const uint8_t PAL_N = 12;
  const uint8_t pal[PAL_N][3] = {
    {255,  64,   0},
    {ORANGE_R, ORANGE_G, ORANGE_B},     // Orange (global)
    {160, 255,   0},
    {  0, 255, 170},
    { 90,   0, 255},
    {255,   0, 120},
    {255, 255, 255},
    {255,   0,   0},
    {  0, 255,   0},
    {  0,  0, 255},
    {170,   0, 255},
    {YELLOW_R, YELLOW_G, YELLOW_B}      // Yellow (global)
  };

  float  lastKnobNorm = 0.5f;
  uint32_t motionSettleMs = 160;
  uint32_t lastMotionMs = 0;

  void begin(uint32_t now) { active = true; lastUpdate = 0; lastMotionMs = now; lastKnobNorm = g_lastKnobNorm; }
  void end() { active = false; }

  void maybeMarkKnobMotion(uint32_t now) {
    float cur = g_lastKnobNorm;
    if (fabsf(cur - lastKnobNorm) > 0.02f) { lastMotionMs = now; lastKnobNorm = cur; }
  }
  inline uint32_t effectiveInterval(uint32_t now) const {
    float s = (g_speed < 0.1f) ? 0.1f : g_speed;
    uint32_t iv = (uint32_t)(baseIntervalMs / s); if (iv < 20) iv = 20;
    if (now - lastMotionMs < motionSettleMs) { uint32_t fast = iv / 4; if (fast < 10) fast = 10; return fast; }
    return iv;
  }

  void render(uint32_t now) {
    if (!active) return;
    maybeMarkKnobMotion(now);

    if (lastUpdate == 0 || (now - lastUpdate) >= effectiveInterval(now)) {
      lastUpdate = now;

      uint16_t pos = 0;
      clearFrame();
      while (pos < LED_COUNT) {
        uint8_t len = 1 + (random(4)); // 1..4
        if (pos + len > LED_COUNT) len = LED_COUNT - pos;
        const uint8_t* c = pal[random(PAL_N)];
        for (uint8_t i=0; i<len; i++) setPixelRGB(pos+i, c[0], c[1], c[2]);
        pos += len;
      }

      uint8_t blacks = (LED_COUNT / 12); if (blacks < 1) blacks = 1;
      for (uint8_t k=0; k<blacks; k++) {
        uint16_t i = random(LED_COUNT);
        uint8_t blen = 1 + (random(2)); // 1..2
        for (uint8_t j=0; j<blen && (i+j)<LED_COUNT; j++) setPixelRGB(i+j, 0,0,0);
      }
    }
  }
} chaos;

// ---------------------- Fill-Count Mode --------------------
struct FillCountMode {
  bool active = false;
  uint32_t lastMs = 0;
  uint8_t savedBrightness = BRIGHTNESS_BOOT;

  static const uint8_t SOLID_N = 7;
  const uint8_t SOLID[SOLID_N][3] = {
    {255,255,255},                               // White
    {255,  0,  0},                               // Red
    {ORANGE_R, ORANGE_G, ORANGE_B},              // Orange (global)
    {YELLOW_R, YELLOW_G, YELLOW_B},              // Yellow (global)
    {  0,255,  0},                               // Green
    {  0,  0,255},                               // Blue
    {170,  0,255}                                // Purple
  };

  static const uint8_t STRIPE_N = 6;
  const uint8_t STRIPE[STRIPE_N][3] = {
    {255,  0,  0},                               // Red
    {ORANGE_R, ORANGE_G, ORANGE_B},              // Orange (global)
    {YELLOW_R, YELLOW_G, YELLOW_B},              // Yellow (global)
    {  0,255,  0},                               // Green
    {  0,  0, 255},                              // Blue
    {170,  0, 255}                               // Purple
  };

  static inline void lerp_u8(uint8_t a, uint8_t b, float t, uint8_t &o){
    float x = (1.0f - t) * a + t * b; if (x < 0) x = 0; if (x > 255) x = 255; o = (uint8_t)(x + 0.5f);
  }
  static inline void lerpRGB(const uint8_t c0[3], const uint8_t c1[3], float t, uint8_t &r, uint8_t &g, uint8_t &b){
    lerp_u8(c0[0], c1[0], t, r);
    lerp_u8(c0[1], c1[1], t, g);
    lerp_u8(c0[2], c1[2], t, b);
  }

  void faderToColor(uint16_t idx, float f, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (f < 0.0f) f = 0.0f; if (f > 1.0f) f = 1.0f;

    if (f < 0.30f) {
      uint8_t which = (idx % STRIPE_N);
      const uint8_t* base = STRIPE[which];
      float t = f / 0.30f;
      uint8_t wr, wg, wb; lerpRGB(base, (const uint8_t[3]){255,255,255}, t, wr, wg, wb);
      r = wr; g = wg; b = wb;
      return;
    } else if (f < 0.32f) {
      r = g = b = 255;
      return;
    } else {
      float u = (f - 0.32f) / (1.0f - 0.32f); if (u < 0) u = 0; if (u > 1) u = 1;
      float seg = u * (SOLID_N - 1);
      int i0 = (int)floorf(seg);
      float t = seg - i0;
      int i1 = (i0 + 1 < (int)SOLID_N) ? (i0 + 1) : ((int)SOLID_N - 1);
      lerpRGB(SOLID[i0], SOLID[i1], t, r,g,b);
      return;
    }
  }

  uint16_t litCountFromKnob() {
    int raw = analogRead(PIN_SPEED_KNOB);
    float norm = raw / 1023.0f;
    uint16_t n = (uint16_t)(1 + (uint16_t)(norm * (LED_COUNT - 1) + 0.5f));
    if (n < 1) n = 1; if (n > LED_COUNT) n = LED_COUNT;
    return n;
  }

  void begin(uint32_t now) { 
    active = true; lastMs = now;
    savedBrightness = g_brightness;
    g_lockBrightness = true;
    g_lockedBrightness = savedBrightness;
    strip.setBrightness(savedBrightness);
  }
  void end() { 
    active = false; 
    g_lockBrightness = false; 
  }

  void render(uint32_t /*now*/) {
    clearFrame();

    uint16_t lit = litCountFromKnob();
    int fRaw = analogRead(PIN_FADER);
    float f = fRaw / 1023.0f;

    for (uint16_t i = 0; i < lit; i++) {
      uint8_t r,g,b; faderToColor(i, f, r,g,b);
      setPixelRGB(i, r, g, b);
    }
  }
} fillCount;

// ---------------------- Classic Lamp Rendering -------------
// Perceptual helpers for smoother gradient (gamma ≈ 2.2)
static inline float smoothstep01(float t){ if(t<0) t=0; if(t>1) t=1; return t*t*(3.0f-2.0f*t); }
static inline float srgb_to_linear(uint8_t c){ return powf(c / 255.0f, 2.2f); }
static inline uint8_t linear_to_srgb(float x){ if (x<0) x=0; if (x>1) x=1; return (uint8_t)(powf(x, 1.0f/2.2f)*255.0f + 0.5f); }
static inline uint8_t lerp_gamma(uint8_t a, uint8_t b, float t){
  float al = srgb_to_linear(a);
  float bl = srgb_to_linear(b);
  float xl = (1.0f - t) * al + t * bl;
  return linear_to_srgb(xl);
}

// Classic: Left = Orange  -> Right = White
static inline void classicColorFromKnob(uint8_t &r, uint8_t &g, uint8_t &b) {
  const uint8_t C_OR[3] = {ORANGE_R, ORANGE_G, ORANGE_B};
  const uint8_t C_WH[3] = {255, 255, 255};

  float t = g_lastKnobNorm;              // 0..1 left->right
  if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;

  float u = smoothstep01(t);             // perceptually smoother
  r = lerp_gamma(C_OR[0], C_WH[0], u);
  g = lerp_gamma(C_OR[1], C_WH[1], u);
  b = lerp_gamma(C_OR[2], C_WH[2], u);
}

static inline void renderClassicBackground(bool powerOn) {
  if (!powerOn) { clearFrame(); return; }
  uint8_t r,g,b; classicColorFromKnob(r,g,b);
  for (uint16_t i = 0; i < LED_COUNT; i++) setPixelRGB(i, r,g,b);
}

// ---------------------- Mode Crossfade ---------------------
struct ModeCrossfade {
  bool active = false;
  uint32_t startMs = 0;
  uint16_t durMs = 300;
  uint8_t fromR=0, fromG=0, fromB=0;
  uint8_t toR=0, toG=0, toB=0;

  void begin(uint32_t now, uint16_t ms, uint8_t fr, uint8_t fg, uint8_t fb, uint8_t tr, uint8_t tg, uint8_t tb){
    active = true; startMs = now; durMs = ms;
    fromR = fr; fromG = fg; fromB = fb;
    toR = tr; toG = tg; toB = tb;
  }
  bool finished(uint32_t now) const { return !active || (now - startMs) >= durMs; }

  void render(uint32_t now, bool powerOn){
    if (!active) return;
    uint32_t t = now - startMs; if (t > durMs) t = durMs;
    float u = (float)t / (float)durMs;
    uint8_t r = (uint8_t)((1-u)*fromR + u*toR + 0.5f);
    uint8_t g = (uint8_t)((1-u)*fromG + u*toG + 0.5f);
    uint8_t b = (uint8_t)((1-u)*fromB + u*toB + 0.5f);
    if (!powerOn) { r=g=b=0; }
    for (uint16_t i=0;i<LED_COUNT;i++) setPixelRGB(i,r,g,b);
    if (t >= durMs) active = false;
  }
} modeFade;

// ---------------------- Setup/Loop -------------------------
bool systemPowerOn = false;
bool g_pendingPowerOff = false;

static void renderBGFrame() { bg.renderBackground(); }
static void renderKeyFrame() { segRainbow.render(); }

void renderBase_Color(bool powerOn) {
  if (!powerOn) { clearFrame(); return; }
  renderBGFrame();
}
void renderBase_Classic(bool powerOn) {
  renderClassicBackground(powerOn);
}

void setup() {
  randomSeed(analogRead(A0));
  strip.begin();
  strip.setBrightness(g_brightness);
  strip.show();

  inRocker.begin(PIN_ROCKER, true);
  inToggle.begin(PIN_TOGGLE, true);
  inArcade.begin(PIN_ARCADE, true);
  inGreen.begin(PIN_GREEN, true);
  inKey.begin(PIN_KEY, true);
  inWave.begin(PIN_WAVE, true);
  inChaos.begin(PIN_CHAOS, true);

  bg.setPower(false, millis());
  segRainbow.reset(millis());

  g_brightness = g_brightnessTarget = readFaderMapped();
  strip.setBrightness(g_brightness);
  strip.show();
}

void loop() {
  uint32_t now = millis();

  // Debounce inputs
  uint8_t eRocker = inRocker.update(now);
  uint8_t eToggle = inToggle.update(now);
  uint8_t eArcade = inArcade.update(now);
  uint8_t eGreen  = inGreen.update(now);
  uint8_t eKey    = inKey.update(now);
  uint8_t eWave   = inWave.update(now);
  uint8_t eChaos  = inChaos.update(now);

  // Live states
  bool chaosOn  = !inChaos.pressed();     // keep as-is per your wiring comment
  bool waveOn   = inWave.pressed();
  bool keyOn    = inKey.pressed();
  bool toggleOn = inToggle.pressed();
  bool rockerOn = inRocker.pressed();
  g_arcadeHeld  = inArcade.pressed();
  g_greenHeld   = inGreen.pressed();

  // --- Inhibit Classic-mode switching when any override/alt mode is ON ---
  bool inhibitClassicSwitch = toggleOn || waveOn || keyOn || chaosOn;

  // --- Classic entry via sequence ("RLRRLRLL"), R=white button press, L=red button press ---
  if (inhibitClassicSwitch) {
    g_seqIdx = 0; // reset matcher while any override is active
  } else {
    bool pressedR = (eGreen  == 2); // white button fell
    bool pressedL = (eArcade == 2); // red button fell
    if (pressedR || pressedL) {
      char got = pressedR ? 'R' : 'L';
      char expected = CLASSIC_SEQ[g_seqIdx];

      if (got == expected) {
        g_seqIdx++;
        if (g_seqIdx >= CLASSIC_SEQ_LEN) {
          // Sequence complete -> enter Classic
          g_seqIdx = 0;
          g_rockerModePrev = g_rockerMode;
          g_rockerMode = MODE_CLASSIC;
        }
      } else {
        // Overlap-aware reset: if current press matches first char, stay at 1, else 0
        g_seqIdx = (got == CLASSIC_SEQ[0]) ? 1 : 0;
      }
    }
  }

  // Smooth brightness & speed
  updateBrightnessFromFader();
  updateSpeedFromKnob(now);

  // Secret Fill-Count activation
  bool fillCountRequested = keyOn && rockerOn && waveOn && toggleOn;
  if (!fillCount.active && fillCountRequested) fillCount.begin(now);
  if (fillCount.active && !fillCountRequested) fillCount.end();

  // ---------------- FILL-COUNT PRIORITY --------------------
  if (fillCountRequested || fillCount.active) {
    keyWipe.active = false;

    fillCount.render(now);

    if (eArcade == 2) spawnRedDot(now);
    if (eGreen  == 2) spawnGreenDot(now);
    for (uint8_t i = 0; i < MAX_ANIMS; i++) {
      IAnim* a = anims[i]; if (!a || !a->active) continue;
      a->update(now); a->render();
      if (a->finished()) { a->active = false; anims[i] = nullptr; }
    }

    strip.show();
    return;
  }

  // ===== KEY OVERRIDE (center wipe) =====
  if (eKey != 0 || keyWipe.active || inKey.pressed()) {
    // pass BASE durations; KeyCenterWipe scales dynamically with speed
    if (eKey == 2) keyWipe.start(KeyCenterWipe::BG_TO_KEY, KEY_CENTER_ON_MS,  now);
    if (eKey == 1) keyWipe.start(KeyCenterWipe::KEY_TO_BG, KEY_CENTER_OFF_MS, now);

    float effMsPerLed = SEG_MS_PER_LED;
    float dirSign = 1.0f;
    if (inToggle.pressed()) { effMsPerLed = SEG_MS_PER_LED / 2.0f; dirSign = -1.0f; }
    segRainbow.update(now, effMsPerLed, dirSign);

    // Update BG engine (even if Classic)
    bg.update(now);

    bool baseActive = (systemPowerOn || g_pendingPowerOff);
    uint8_t baseR, baseG, baseB;
    if (g_rockerMode == MODE_CLASSIC) { classicColorFromKnob(baseR, baseG, baseB); }
    else { baseR = bg.curR; baseG = bg.curG; baseB = bg.curB; }

    if (keyWipe.active) {
      keyWipe.apply(now, baseR, baseG, baseB, segRainbow, baseActive);
      if (keyWipe.finished(now)) keyWipe.active = false;
    } else {
      renderKeyFrame();
    }

    if (eArcade == 2) spawnRedDot(now);
    if (eGreen  == 2) spawnGreenDot(now);
    for (uint8_t i = 0; i < MAX_ANIMS; i++) {
      IAnim* a = anims[i]; if (!a || !a->active) continue;
      a->update(now); a->render();
      if (a->finished()) { a->active = false; anims[i] = nullptr; }
    }

    strip.show();
    return;
  }

  // ---------------- CHAOS OVERRIDE --------------------------
  if (chaosOn || chaos.active) {
    if (!chaos.active && chaosOn) chaos.begin(now);
    if (chaos.active && !chaosOn) chaos.end();

    chaos.render(now);

    if (eArcade == 2) spawnRedDot(now);
    if (eGreen  == 2) spawnGreenDot(now);
    for (uint8_t i = 0; i < MAX_ANIMS; i++) {
      IAnim* a = anims[i]; if (!a || !a->active) continue;
      a->update(now); a->render();
      if (a->finished()) { a->active = false; anims[i] = nullptr; }
    }
    strip.show();
    return;
  }

  // ----- NEW: Wave toggle immediately affects color-advance (and kicks groups at trough) -----
  if (waveOn && (eToggle == 1 || eToggle == 2)) {
    wave.setColorAdvanceEnabled(toggleOn);
  }

  // ---------------- WAVE OVERRIDE ---------------------------
  if (waveOn || wave.active) {
    if (!wave.active && waveOn) { wave.begin(now); }
    if (wave.active && !waveOn) { wave.endNow(); }

    if (eArcade == 2) spawnRedDot(now);
    if (eGreen  == 2) spawnGreenDot(now);

    wave.render(now, /*toggleOn=*/toggleOn);

    for (uint8_t i = 0; i < MAX_ANIMS; i++) {
      IAnim* a = anims[i]; if (!a || !a->active) continue;
      a->update(now); a->render();
      if (a->finished()) { a->active = false; anims[i] = nullptr; }
    }
    strip.show();
    if (wave.active) return;
  }

  // ---------------- NORMAL LAMP PATH ------------------------
  if (eRocker == 2) { // ON
    systemPowerOn = true;
    g_pendingPowerOff = false;
    bg.setPower(true, now);
    bg.setRainbowEnabled(toggleOn, now);
    rockerWipe.requestOn(now, WIPE_MS_ON);   // base Ms; dynamic speed applied inside update()
  }
  if (eRocker == 1) { // OFF
    g_pendingPowerOff = true;
    rockerWipe.requestOff(now, WIPE_MS_OFF); // base Ms; dynamic speed applied inside update()
  }

  // Exit Classic when Toggle is ON (fade rather than snap)
  if (toggleOn && g_rockerMode == MODE_CLASSIC) {
    bg.setRainbowEnabled(true, now);
    uint8_t fr, fg, fb; classicColorFromKnob(fr, fg, fb);
    uint8_t tr = bg.curR, tg = bg.curG, tb = bg.curB;
    modeFade.begin(now, scaledMs(MODE_FADE_MS), fr, fg, fb, tr, tg, tb);
    g_rockerModePrev = g_rockerMode;
    g_rockerMode = MODE_COLOR;
  }

  if (eToggle == 1 || eToggle == 2) { bg.setRainbowEnabled(toggleOn, now); }

  if (eArcade == 2) spawnRedDot(now);
  if (eGreen  == 2) spawnGreenDot(now);

  bg.update(now);
  segRainbow.update(now, SEG_MS_PER_LED, 1.0f);

  bool basePower = (systemPowerOn || g_pendingPowerOff);

  if (modeFade.active) {
    modeFade.render(now, basePower);
  } else if (g_rockerMode == MODE_CLASSIC) {
    renderBase_Classic(basePower);
  } else {
    renderBase_Color(basePower);
  }

  rockerWipe.update(now);
  rockerWipe.apply(now);

  if (g_pendingPowerOff && !rockerWipe.active) {
    g_pendingPowerOff = false;
    systemPowerOn = false;
    bg.setPower(false, now);
  }

  for (uint8_t i = 0; i < MAX_ANIMS; i++) {
    IAnim* a = anims[i]; if (!a || !a->active) continue;
    a->update(now); a->render();
    if (a->finished()) { a->active = false; anims[i] = nullptr; }
  }

  strip.show();
}
