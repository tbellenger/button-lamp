// JSON frame parser for MatrixLamp_R4_Wifi.
// Accepts the posted shapes documented in README.md. No Arduino types.

#pragma once

#include <stdint.h>
#include <string.h>

enum FrameKind : uint8_t { FRAME_FIRE = 0, FRAME_CUSTOM = 1 };

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
  return false;
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

// On success, leds is updated only when *pixels is set. *err is null.
// On failure leds is left unchanged and *err is "json", "pixel", or "mode".
static bool frameParseBody(const char* body, uint8_t leds[96],
                           bool* pixels, bool* mode, uint8_t* kind,
                           const char** err) {
  *pixels = false;
  *mode = false;
  *err = "json";
  if (!body) return false;
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
    arr = frameSkipWs(arr);
    if (*arr != '[') {
      *err = "json";
      return false;
    }
    const char* end = frameMatchBracket(arr);
    if (!end) return false;
    const char* q = arr + 1;
    int seq = 0;
    int count = 0;
    bool anyObj = false;
    bool anyNum = false;
    while (q < end) {
      q = frameSkipWs(q);
      if (q >= end) break;
      if (*q == ',') { q++; continue; }
      if (*q == '{' ) {
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

  if (g_fails) {
    printf("%d failed\n", g_fails);
    return 1;
  }
  printf("all passed\n");
  return 0;
}
#endif
