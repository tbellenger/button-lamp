// JSON frame parser for MatrixLamp_R4_Wifi.
// Accepts the posted shapes documented in README.md. No Arduino types.

#pragma once

#include <stdint.h>
#include <string.h>

enum FrameKind : uint8_t { FRAME_FIRE = 0, FRAME_CUSTOM = 1, FRAME_ANIMATION = 2 };

static const int FRAME_ANIM_MAX = 16;
static const int FRAME_INTERVAL_MIN = 20;
static const int FRAME_INTERVAL_MAX = 5000;
static const int FRAME_INTERVAL_DEFAULT = 100;

static int frameScaleLevel(int v) {
  if (v <= 0) return 0;
  if (v <= 15) return v;
  if (v >= 255) return 15;
  return (v * 15 + 127) / 255;
}

static const char* frameSkipWs(const char* p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  return p;
}

static bool frameWordIs(const char* p, const char* word) {
  size_t n = strlen(word);
  if (strncmp(p, word, n) != 0) return false;
  char c = p[n];
  return c == 0 || c == '"' || c == '&' || c == ' ' || c == '\t' ||
         c == '\r' || c == '\n' || c == '}' || c == ',';
}

static bool frameParseModeToken(const char* m, uint8_t* kind) {
  if (!m) return false;
  if (frameWordIs(m, "fire") || frameWordIs(m, "loop")) {
    *kind = FRAME_FIRE;
    return true;
  }
  if (frameWordIs(m, "custom")) {
    *kind = FRAME_CUSTOM;
    return true;
  }
  if (frameWordIs(m, "animation")) {
    *kind = FRAME_ANIMATION;
    return true;
  }
  return false;
}

static int frameClampInterval(int v) {
  if (v < FRAME_INTERVAL_MIN) return FRAME_INTERVAL_MIN;
  if (v > FRAME_INTERVAL_MAX) return FRAME_INTERVAL_MAX;
  return v;
}

static const char* frameFindKey(const char* body, const char* key) {
  if (!body) return 0;
  size_t n = strlen(key);
  const char* p = body;
  while (*p) {
    if (*p != '"') { p++; continue; }
    p++;
    if (strncmp(p, key, n) == 0 && p[n] == '"') {
      const char* q = frameSkipWs(p + n + 1);
      if (*q == ':') return frameSkipWs(q + 1);
    }
    while (*p && *p != '"') {
      if (*p == '\\' && p[1]) p += 2;
      else p++;
    }
    if (*p == '"') p++;
  }
  return 0;
}

static const char* frameMatchBracket(const char* p) {
  if (*p != '{' && *p != '[') return 0;
  char close = (*p == '{') ? '}' : ']';
  int depth = 0;
  while (*p) {
    if (*p == '"') {
      p++;
      while (*p && *p != '"') {
        if (*p == '\\' && p[1]) p += 2;
        else p++;
      }
      if (*p == '"') p++;
      continue;
    }
    if (*p == '{' || *p == '[') depth++;
    else if (*p == '}' || *p == ']') {
      depth--;
      if (depth == 0) return (*p == close) ? p : 0;
    }
    p++;
  }
  return 0;
}

static bool frameParseInt(const char*& p, int* out) {
  p = frameSkipWs(p);
  int sign = 1;
  if (*p == '-') { sign = -1; p++; }
  if (*p < '0' || *p > '9') return false;
  long n = 0;
  while (*p >= '0' && *p <= '9') {
    n = n * 10 + (*p - '0');
    if (n > 1000000L) return false;
    p++;
  }
  if (*p == '.' || *p == 'e' || *p == 'E') return false;
  *out = (int)(sign < 0 ? -n : n);
  return true;
}

static bool frameParseBoolOrInt(const char*& p, int* out) {
  p = frameSkipWs(p);
  if (strncmp(p, "true", 4) == 0 && frameWordIs(p, "true")) {
    *out = 1;
    p += 4;
    return true;
  }
  if (strncmp(p, "false", 5) == 0 && frameWordIs(p, "false")) {
    *out = 0;
    p += 5;
    return true;
  }
  return frameParseInt(p, out);
}

static bool frameParsePixelObj(const char* start, const char* end,
                               int* x, int* y, int* on, int* br) {
  *x = *y = *on = *br = -1;
  const char* p = start + 1;
  while (p < end) {
    p = frameSkipWs(p);
    if (p >= end) break;
    if (*p == ',') { p++; continue; }
    if (*p != '"') return false;
    const char* key = ++p;
    while (p < end && *p != '"') p++;
    if (p >= end) return false;
    size_t klen = (size_t)(p - key);
    p++;
    p = frameSkipWs(p);
    if (p >= end || *p != ':') return false;
    p++;
    const char* vs = frameSkipWs(p);
    if (vs < end && *vs == '"') {
      p = vs + 1;
      while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) p += 2;
        else p++;
      }
      if (p < end && *p == '"') p++;
      continue;
    }
    if (vs < end && (*vs == '{' || *vs == '[')) {
      const char* e = frameMatchBracket(vs);
      if (!e || e >= end) return false;
      p = e + 1;
      continue;
    }
    int val = 0;
    p = vs;
    if (!frameParseBoolOrInt(p, &val)) return false;
    if (klen == 1 && key[0] == 'x') *x = val;
    else if (klen == 1 && key[0] == 'y') *y = val;
    else if (klen == 2 && key[0] == 'o' && key[1] == 'n') *on = val ? 1 : 0;
    else if (klen == 10 && strncmp(key, "brightness", 10) == 0) *br = val;
  }
  return true;
}

static bool frameApplyPixel(uint8_t* frame, int x, int y, int on, int br,
                            int* seq, bool allowSeq) {
  if (on < 0 && br < 0) return false;
  int level = (on == 0) ? 0 : (br >= 0 ? frameScaleLevel(br) : 15);
  if (x < 0 && y < 0) {
    if (!allowSeq || *seq < 0 || *seq >= 96) return false;
    frame[*seq] = (uint8_t)level;
    (*seq)++;
    return true;
  }
  if (x < 0 || y < 0 || x > 11 || y > 7) return false;
  frame[y * 12 + x] = (uint8_t)level;
  return true;
}

// Top-level object key only. Nested objects and arrays are skipped, so a
// per-frame "interval" is not mistaken for the animation interval.
static const char* frameFindTopKey(const char* body, const char* key) {
  if (!body || !key) return 0;
  const char* p = frameSkipWs(body);
  if (*p != '{') return 0;
  p++;
  size_t klen = strlen(key);
  while (*p) {
    p = frameSkipWs(p);
    if (*p == '}') return 0;
    if (*p == ',') { p++; continue; }
    if (*p != '"') return 0;
    const char* ks = ++p;
    while (*p && *p != '"') {
      if (*p == '\\' && p[1]) p += 2;
      else p++;
    }
    if (*p != '"') return 0;
    size_t n = (size_t)(p - ks);
    p++;
    p = frameSkipWs(p);
    if (*p != ':') return 0;
    p = frameSkipWs(p + 1);
    bool match = (n == klen && strncmp(ks, key, klen) == 0);
    if (match) return p;
    if (*p == '"') {
      p++;
      while (*p && *p != '"') {
        if (*p == '\\' && p[1]) p += 2;
        else p++;
      }
      if (*p == '"') p++;
    } else if (*p == '{' || *p == '[') {
      const char* e = frameMatchBracket(p);
      if (!e) return 0;
      p = e + 1;
    } else {
      while (*p && *p != ',' && *p != '}') p++;
    }
  }
  return 0;
}

static const char* frameFindKeyRange(const char* start, const char* end, const char* key) {
  if (!start || !end || !key || start >= end) return 0;
  size_t n = strlen(key);
  const char* p = start;
  while (p < end && *p) {
    if (*p != '"') { p++; continue; }
    p++;
    if ((size_t)(end - p) > n && strncmp(p, key, n) == 0 && p[n] == '"') {
      const char* q = frameSkipWs(p + n + 1);
      if (q < end && *q == ':') return frameSkipWs(q + 1);
    }
    while (p < end && *p && *p != '"') {
      if (*p == '\\' && (p + 1) < end && p[1]) p += 2;
      else p++;
    }
    if (p < end && *p == '"') p++;
  }
  return 0;
}

// One frame array: 96 numbers (nested row brackets are ignored) or up to 96
// pixel objects. Objects merge onto `base` when base is set, otherwise onto
// black. On failure dest is left unchanged.
static bool frameParseLedArray(const char* arr, const uint8_t* base, uint8_t dest[96],
                               const char** err) {
  *err = "json";
  if (!arr) return false;
  arr = frameSkipWs(arr);
  if (*arr != '[') return false;
  const char* end = frameMatchBracket(arr);
  if (!end) return false;
  uint8_t temp[96];
  if (base) memcpy(temp, base, 96);
  else memset(temp, 0, 96);
  const char* q = arr + 1;
  int seq = 0;
  int count = 0;
  bool anyObj = false;
  bool anyNum = false;
  while (q < end) {
    q = frameSkipWs(q);
    if (q >= end) break;
    if (*q == ',') { q++; continue; }
    if (*q == '{') {
      const char* oe = frameMatchBracket(q);
      if (!oe || oe > end) return false;
      int x, y, on, br;
      if (!frameParsePixelObj(q, oe, &x, &y, &on, &br)) return false;
      if (!frameApplyPixel(temp, x, y, on, br, &seq, true)) {
        *err = "pixel";
        return false;
      }
      anyObj = true;
      count++;
      if (count > 96) {
        *err = "pixel";
        return false;
      }
      q = oe + 1;
    } else if (*q == '-' || (*q >= '0' && *q <= '9')) {
      int v = 0;
      if (!frameParseInt(q, &v)) return false;
      if (anyObj || seq >= 96) {
        *err = "pixel";
        return false;
      }
      temp[seq++] = (uint8_t)frameScaleLevel(v);
      anyNum = true;
    } else if (*q == '[' || *q == ']') {
      q++;
    } else {
      return false;
    }
  }
  if (anyObj && anyNum) return false;
  if (anyNum && seq != 96) {
    *err = "pixel";
    return false;
  }
  if (anyObj && count < 1) {
    *err = "pixel";
    return false;
  }
  if (!anyObj && !anyNum) {
    *err = "pixel";
    return false;
  }
  memcpy(dest, temp, 96);
  *err = 0;
  return true;
}

static bool frameParseIntervalAt(const char* p, int* out) {
  if (!p) return false;
  int v = 0;
  const char* q = p;
  if (!frameParseInt(q, &v)) return false;
  *out = frameClampInterval(v);
  return true;
}

// On success, leds is updated only when *pixels is set. *err is null.
// On failure leds is left unchanged and *err is "json", "pixel", or "mode".
static bool frameParseBody(const char* body, uint8_t leds[96],
                           bool* pixels, bool* mode, uint8_t* kind,
                           const char** err) {
  *pixels = false;
  *mode = false;
  *err = "json";
  if (!body) return false;
  if (frameFindTopKey(body, "frames")) {
    *err = "frames";
    return false;
  }
  uint8_t temp[96];
  memcpy(temp, leds, 96);

  bool hasMode = false;
  uint8_t modeKind = FRAME_FIRE;
  const char* modeAt = frameFindKey(body, "mode");
  if (modeAt) {
    hasMode = true;
    modeAt = frameSkipWs(modeAt);
    if (*modeAt != '"' || !frameParseModeToken(modeAt + 1, &modeKind)) {
      *err = "mode";
      return false;
    }
  }

  const char* p = frameSkipWs(body);
  const char* arr = frameFindKey(body, "pixels");
  if (!arr) arr = frameFindKey(body, "leds");
  if (!arr) arr = frameFindKey(body, "frame");
  if (!arr && *p == '[') arr = p;

  bool pix = false;
  if (arr) {
    if (!frameParseLedArray(arr, leds, temp, err)) return false;
    pix = true;
  } else if (frameFindKey(body, "x") && frameFindKey(body, "y")) {
    const char* obj = strchr(body, '{');
    const char* end = obj ? frameMatchBracket(obj) : 0;
    if (!obj || !end) return false;
    int x, y, on, br;
    if (!frameParsePixelObj(obj, end, &x, &y, &on, &br)) return false;
    int seq = 0;
    if (!frameApplyPixel(temp, x, y, on, br, &seq, false)) {
      *err = "pixel";
      return false;
    }
    pix = true;
  } else if (!hasMode) {
    return false;
  }

  if (pix) memcpy(leds, temp, 96);
  *pixels = pix;
  *mode = hasMode;
  if (hasMode) *kind = modeKind;
  *err = 0;
  return true;
}

static bool frameReadTopInterval(const char* body, int* out, const char** err) {
  const char* keys[2] = {"intervalMs", "interval"};
  for (int i = 0; i < 2; i++) {
    const char* v = frameFindTopKey(body, keys[i]);
    if (!v) continue;
    if (!frameParseIntervalAt(v, out)) {
      *err = "interval";
      return false;
    }
    return true;
  }
  *out = FRAME_INTERVAL_DEFAULT;
  return true;
}

static bool frameReadScopedInterval(const char* start, const char* end, int* out,
                                    bool* present, const char** err) {
  const char* keys[3] = {"duration", "interval", "intervalMs"};
  *present = false;
  for (int i = 0; i < 3; i++) {
    const char* v = frameFindKeyRange(start, end, keys[i]);
    if (!v || v >= end) continue;
    *present = true;
    if (!frameParseIntervalAt(v, out)) {
      *err = "interval";
      return false;
    }
    return true;
  }
  return true;
}

// Parse {"frames":[...], "interval" | "intervalMs": N}.
// Each frame is a 96-value array (flat or 8x12), pixel objects, or
// {"leds"|"frame"|"pixels":[...], "duration"|"interval"|"intervalMs": N}.
// On failure the sketch must not keep the result. The host test leaves
// its output buffers unchanged; the device build may write framesOut
// before it returns false.
static bool frameParseAnimation(const char* body, uint8_t* framesOut, uint16_t* durationsOut,
                                uint8_t* count, uint16_t* interval, const char** err) {
  if (count) *count = 0;
  if (interval) *interval = FRAME_INTERVAL_DEFAULT;
  *err = "frames";
  if (!body || !framesOut || !durationsOut || !count || !interval) return false;

  int top = FRAME_INTERVAL_DEFAULT;
  if (!frameReadTopInterval(body, &top, err)) return false;

  const char* arr = frameFindTopKey(body, "frames");
  if (!arr) return false;
  arr = frameSkipWs(arr);
  if (*arr != '[') return false;
  const char* end = frameMatchBracket(arr);
  if (!end) {
    *err = "json";
    return false;
  }

  // The device build parses straight into the caller's buffer (the sketch
  // passes the JSON response buffer, then copies into g_anim only on
  // success). The host test keeps a side buffer so a rejected body does
  // not change the previous frames. Neither path puts 1.5 KB on the
  // R4's 1 KB main stack.
#if defined(FRAME_JSON_TEST)
  uint8_t temp[FRAME_ANIM_MAX * 96];
  uint16_t durs[FRAME_ANIM_MAX];
  uint8_t* dest = temp;
  uint16_t* destDur = durs;
#else
  uint8_t* dest = framesOut;
  uint16_t* destDur = durationsOut;
#endif
  uint8_t n = 0;
  const char* q = arr + 1;
  while (q < end) {
    q = frameSkipWs(q);
    if (q >= end) break;
    if (*q == ',') { q++; continue; }
    if (n >= FRAME_ANIM_MAX) return false;
    uint8_t one[96];
    int dur = top;
    if (*q == '[') {
      const char* e2 = 0;
      if (!frameParseLedArray(q, 0, one, &e2)) {
        *err = e2 ? e2 : "frames";
        return false;
      }
      const char* bracketEnd = frameMatchBracket(q);
      if (!bracketEnd) {
        *err = "json";
        return false;
      }
      q = bracketEnd + 1;
    } else if (*q == '{') {
      const char* oe = frameMatchBracket(q);
      if (!oe || oe > end) {
        *err = "json";
        return false;
      }
      bool hasDur = false;
      int pd = top;
      if (!frameReadScopedInterval(q, oe, &pd, &hasDur, err)) return false;
      if (hasDur) dur = pd;
      const char* led = frameFindKeyRange(q, oe, "pixels");
      if (!led) led = frameFindKeyRange(q, oe, "leds");
      if (!led) led = frameFindKeyRange(q, oe, "frame");
      if (led && led < oe && *frameSkipWs(led) == '[') {
        const char* e2 = 0;
        if (!frameParseLedArray(frameSkipWs(led), 0, one, &e2)) {
          *err = e2 ? e2 : "frames";
          return false;
        }
      } else {
        int x, y, on, br;
        if (!frameParsePixelObj(q, oe, &x, &y, &on, &br)) {
          *err = "json";
          return false;
        }
        memset(one, 0, 96);
        int seq = 0;
        if (!frameApplyPixel(one, x, y, on, br, &seq, false)) {
          *err = "pixel";
          return false;
        }
      }
      q = oe + 1;
    } else {
      return false;
    }
    memcpy(dest + (size_t)n * 96, one, 96);
    destDur[n] = (uint16_t)dur;
    n++;
  }
  if (n < 1) return false;
#if defined(FRAME_JSON_TEST)
  memcpy(framesOut, temp, (size_t)n * 96);
  memcpy(durationsOut, durs, (size_t)n * sizeof(uint16_t));
#endif
  *count = n;
  *interval = (uint16_t)top;
  *err = 0;
  return true;
}

#if defined(FRAME_JSON_TEST)
#include <stdio.h>
#include <stdlib.h>

static int g_fails = 0;
static void expect(bool cond, const char* msg) {
  if (!cond) {
    printf("FAIL %s\n", msg);
    g_fails++;
  } else {
    printf("ok   %s\n", msg);
  }
}

static void fill(uint8_t* leds, uint8_t v) { memset(leds, v, 96); }

static bool parse(const char* body, uint8_t* leds, bool* pix, bool* mode, uint8_t* kind, const char** err) {
  return frameParseBody(body, leds, pix, mode, kind, err);
}

int main() {
  uint8_t leds[96];
  bool pix = false, mode = false;
  uint8_t kind = 9;
  const char* err = 0;

  expect(frameScaleLevel(0) == 0, "scale 0");
  expect(frameScaleLevel(15) == 15, "scale 15");
  expect(frameScaleLevel(16) == 1, "scale 16");
  expect(frameScaleLevel(128) == 8, "scale 128");
  expect(frameScaleLevel(255) == 15, "scale 255");
  expect(frameScaleLevel(256) == 15, "scale 256");
  expect(frameScaleLevel(-3) == 0, "scale neg");

  fill(leds, 7);
  expect(parse("{\"x\":0,\"y\":1,\"on\":true,\"brightness\":8}", leds, &pix, &mode, &kind, &err)
         && pix && !mode && leds[12] == 8 && leds[0] == 7 && err == 0, "single");

  fill(leds, 3);
  expect(!parse("{\"x\":12,\"y\":0,\"on\":true}", leds, &pix, &mode, &kind, &err)
         && leds[0] == 3 && err && !strcmp(err, "pixel"), "bad x");

  fill(leds, 1);
  expect(parse("{\"x\":11,\"y\":7,\"on\":false,\"brightness\":15}", leds, &pix, &mode, &kind, &err)
         && leds[7 * 12 + 11] == 0 && leds[0] == 1, "on false");

  fill(leds, 1);
  expect(parse("{\"x\":2,\"y\":0,\"on\":true}", leds, &pix, &mode, &kind, &err)
         && leds[2] == 15, "on implies 15");

  fill(leds, 1);
  expect(parse("{\"x\":3,\"y\":0,\"brightness\":255}", leds, &pix, &mode, &kind, &err)
         && leds[3] == 15, "brightness 255");
  expect(parse("{\"x\":4,\"y\":0,\"brightness\":16}", leds, &pix, &mode, &kind, &err)
         && leds[4] == 1 && leds[3] == 15, "brightness 16 keeps earlier");

  fill(leds, 4);
  expect(parse("{\"pixels\":[{\"x\":2,\"y\":3,\"on\":true,\"brightness\":4},{\"x\":0,\"y\":0,\"on\":false}]}",
               leds, &pix, &mode, &kind, &err)
         && pix && leds[3 * 12 + 2] == 4 && leds[0] == 0 && leds[1] == 4, "pixels");

  fill(leds, 9);
  expect(parse("{\"leds\":[{\"on\":true,\"brightness\":2},{\"brightness\":0}]}",
               leds, &pix, &mode, &kind, &err)
         && leds[0] == 2 && leds[1] == 0 && leds[2] == 9, "sequential objects");

  fill(leds, 5);
  char body[800];
  strcpy(body, "{\"frame\":[");
  for (int i = 0; i < 96; i++) {
    char n[8];
    sprintf(n, "%s%d", (i ? "," : ""), (i == 95) ? 14 : 0);
    strcat(body, n);
  }
  strcat(body, "]}");
  expect(parse(body, leds, &pix, &mode, &kind, &err) && pix && leds[0] == 0 && leds[95] == 14, "96 flat");

  strcpy(body, "[");
  for (int y = 0; y < 8; y++) {
    strcat(body, y ? ",[" : "[");
    for (int x = 0; x < 12; x++) {
      char n[8];
      sprintf(n, "%s%d", (x ? "," : ""), (y == 7 && x == 11) ? 200 : 1);
      strcat(body, n);
    }
    strcat(body, "]");
  }
  strcat(body, "]");
  fill(leds, 0);
  expect(parse(body, leds, &pix, &mode, &kind, &err) && leds[0] == 1 && leds[95] == frameScaleLevel(200),
         "nested 8x12");

  char shortb[64];
  strcpy(shortb, "[0,1,2]");
  fill(leds, 6);
  expect(!parse(shortb, leds, &pix, &mode, &kind, &err) && leds[0] == 6, "short array");

  fill(leds, 6);
  expect(parse("{\"mode\":\"loop\"}", leds, &pix, &mode, &kind, &err)
         && !pix && mode && kind == FRAME_FIRE && leds[0] == 6, "mode loop");
  expect(parse("{\"mode\":\"fire\"}", leds, &pix, &mode, &kind, &err)
         && mode && kind == FRAME_FIRE, "mode fire");
  expect(parse("{\"mode\":\"custom\"}", leds, &pix, &mode, &kind, &err)
         && mode && kind == FRAME_CUSTOM, "mode custom");
  expect(!parse("{\"mode\":\"nope\"}", leds, &pix, &mode, &kind, &err), "mode nope");
  expect(!parse("{\"mode\":\"looper\"}", leds, &pix, &mode, &kind, &err), "mode looper");
  expect(!parse("", leds, &pix, &mode, &kind, &err), "empty");
  expect(!parse("{}", leds, &pix, &mode, &kind, &err), "empty object");

  fill(leds, 2);
  expect(parse("{\"mode\":\"fire\",\"x\":1,\"y\":2,\"brightness\":9}", leds, &pix, &mode, &kind, &err)
         && pix && mode && kind == FRAME_FIRE && leds[2 * 12 + 1] == 9, "mode and pixel");

  uint8_t k = 9;
  expect(frameParseModeToken("fire", &k) && k == FRAME_FIRE, "token fire");
  expect(frameParseModeToken("loop&x=1", &k) && k == FRAME_FIRE, "token loop query");
  expect(frameParseModeToken("custom\"", &k) && k == FRAME_CUSTOM, "token custom");
  expect(!frameParseModeToken("firework", &k), "token firework");
  expect(frameParseModeToken("animation\"", &k) && k == FRAME_ANIMATION, "token animation");
  expect(!frameParseModeToken("animations", &k), "token animations");

  fill(leds, 4);
  expect(parse("{\"mode\":\"animation\"}", leds, &pix, &mode, &kind, &err)
         && !pix && mode && kind == FRAME_ANIMATION && leds[0] == 4, "mode animation");

  fill(leds, 8);
  expect(!parse("{\"frames\":[[1]],\"interval\":100}", leds, &pix, &mode, &kind, &err)
         && leds[0] == 8 && err && !strcmp(err, "frames"), "body ignores frames key");

  static char big[16384];
  uint8_t frames[FRAME_ANIM_MAX * 96];
  uint16_t durs[FRAME_ANIM_MAX];
  uint8_t count = 0;
  uint16_t iv = 0;
  memset(frames, 0xab, sizeof frames);

  strcpy(big, "{\"intervalMs\":80,\"frames\":[[");
  for (int i = 0; i < 96; i++) {
    char n[8];
    sprintf(n, "%s%d", (i ? "," : ""), (i == 0) ? 255 : 0);
    strcat(big, n);
  }
  strcat(big, "],[");
  for (int y = 0; y < 8; y++) {
    if (y) strcat(big, ",[");
    else strcat(big, "[");
    for (int x = 0; x < 12; x++) {
      char n[8];
      sprintf(n, "%s%d", (x ? "," : ""), (y == 1 && x == 2) ? 9 : 1);
      strcat(big, n);
    }
    strcat(big, "]");
  }
  strcat(big, "]]}");
  expect(frameParseAnimation(big, frames, durs, &count, &iv, &err)
         && count == 2 && iv == 80 && err == 0
         && frames[0] == 15 && frames[1] == 0
         && frames[96 + 1 * 12 + 2] == 9 && frames[96] == 1
         && durs[0] == 80 && durs[1] == 80, "two frames intervalMs");

  strcpy(big, "{\"frames\":[{\"duration\":15,\"leds\":[");
  for (int i = 0; i < 96; i++) {
    char n[8];
    sprintf(n, "%s%d", (i ? "," : ""), (i == 5) ? 4 : 0);
    strcat(big, n);
  }
  strcat(big, "]},{\"interval\":250,\"pixels\":[{\"x\":3,\"y\":4,\"on\":true,\"brightness\":8}]}]}");
  memset(frames, 0xab, sizeof frames);
  expect(frameParseAnimation(big, frames, durs, &count, &iv, &err)
         && count == 2 && iv == FRAME_INTERVAL_DEFAULT
         && durs[0] == FRAME_INTERVAL_MIN && frames[5] == 4
         && durs[1] == 250 && frames[96 + 4 * 12 + 3] == 8
         && frames[96] == 0, "per-frame duration and default interval");

  strcpy(big, "{\"interval\":9000,\"frames\":[{\"intervalMs\":1,\"frame\":[");
  for (int i = 0; i < 96; i++) {
    char n[8];
    sprintf(n, "%s%d", (i ? "," : ""), 2);
    strcat(big, n);
  }
  strcat(big, "]}]}");
  expect(frameParseAnimation(big, frames, durs, &count, &iv, &err)
         && count == 1 && iv == FRAME_INTERVAL_MAX && durs[0] == FRAME_INTERVAL_MIN
         && frames[0] == 2, "interval clamps");

  // Top-level interval sits after a per-frame interval and must still win.
  strcpy(big, "{\"frames\":[{\"interval\":40,\"leds\":[");
  for (int i = 0; i < 96; i++) {
    char n[8];
    sprintf(n, "%s%d", (i ? "," : ""), 3);
    strcat(big, n);
  }
  strcat(big, "]}],\"interval\":200}");
  expect(frameParseAnimation(big, frames, durs, &count, &iv, &err)
         && iv == 200 && durs[0] == 40 && frames[0] == 3, "top interval after frames");

  expect(!frameParseAnimation("{\"frames\":[]}", frames, durs, &count, &iv, &err)
         && err && !strcmp(err, "frames"), "empty frames");
  expect(!frameParseAnimation("{\"interval\":100}", frames, durs, &count, &iv, &err),
         "interval without frames");
  expect(!frameParseAnimation("{\"frames\":[[0,1,2]]}", frames, durs, &count, &iv, &err)
         && err && !strcmp(err, "pixel"), "short animation frame");
  expect(!frameParseAnimation("{\"interval\":\"fast\",\"frames\":[[0]]}", frames, durs, &count, &iv, &err)
         && err && !strcmp(err, "interval"), "bad interval");

  strcpy(big, "{\"interval\":-5,\"frames\":[");
  for (int f = 0; f < FRAME_ANIM_MAX + 1; f++) {
    if (f) strcat(big, ",");
    strcat(big, "[");
    for (int i = 0; i < 96; i++) {
      char n[8];
      sprintf(n, "%s%d", (i ? "," : ""), f == 3 && i == 7 ? 6 : 0);
      strcat(big, n);
    }
    strcat(big, "]");
  }
  strcat(big, "]}");
  uint8_t sentinel = frames[0];
  expect(!frameParseAnimation(big, frames, durs, &count, &iv, &err)
         && frames[0] == sentinel && err && !strcmp(err, "frames"), "17th frame rejected");

  // Drop the extra frame and the same payload must fit.
  char* cut = strrchr(big, ',');
  if (cut) {
    // The last frame starts at the comma before its '['. Remove that frame.
    char* bracket = strrchr(big, '[');
    // Walk back to the comma that introduces the 17th frame.
    char* comma = bracket ? bracket - 1 : 0;
    while (comma && comma > big && *comma != ',') comma--;
    if (comma && *comma == ',') strcpy(comma, "]}");
  }
  expect(frameParseAnimation(big, frames, durs, &count, &iv, &err)
         && count == FRAME_ANIM_MAX && iv == FRAME_INTERVAL_MIN
         && frames[3 * 96 + 7] == 6 && durs[15] == FRAME_INTERVAL_MIN, "16 frames");

  if (g_fails) {
    printf("%d failed\n", g_fails);
    return 1;
  }
  printf("all passed\n");
  return 0;
}
#endif
