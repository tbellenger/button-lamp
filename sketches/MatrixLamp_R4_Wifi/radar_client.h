// Background fetch of http://api.drunkwallaby.com/radar.bin (port 80).
// Included once from the sketch, after g_mode, g_wifi, g_http, g_otaActive,
// and g_ghPend.
//
// The ESP32-S3 bridge runs one AT command at a time, and that command blocks
// the RA4M1 until modem.timeout. available() and connected() are AT commands.
// Calling them every loop, or aborting connect/DNS before the ESP finishes,
// wedges the bridge: the IP scroll and the panel stop because loop() never
// reaches them again. This file therefore:
//   - waits 20 s after WiFi is up before the first poll (the scroll runs)
//   - does at most one modem call per loop, and not while the web server
//     or a GitHub update owns the bridge
//   - connects with one AT command (DNS + TCP inside the ESP) and waits
//     longer than that command so the bridge stays in sync
//   - reads on a 40 ms pace, 64 bytes per AT+CLIENTRECEIVE, never WiFiClient::read()
//   - keeps the bytes in a staging buffer until the header checks pass,
//     then copies them into the frame the panel paints
//
// Host is api.drunkwallaby.com so Caddy can route. There is no page field
// and no prefs host.
#ifndef RADAR_HOST
#define RADAR_HOST "api.drunkwallaby.com"
#endif
#ifndef RADAR_PORT
#define RADAR_PORT 80
#endif
#ifndef RADAR_PATH
#define RADAR_PATH "/radar.bin"
#endif

#pragma once

#include <string.h>
#include "radar_format.h"

static const uint32_t RADAR_POLL_OK_MS = 300000ul;
static const uint32_t RADAR_POLL_FAIL_MS = 600000ul;
static const uint32_t RADAR_FIRST_MS = 20000ul;
static const uint32_t RADAR_STALE_MS = 1200000ul;
static const uint32_t RADAR_FETCH_MS = 8000ul;
static const uint32_t RADAR_PACE_MS = 40ul;
static const uint8_t RF_IDLE = 0;
static const uint8_t RF_HDR = 1;
static const uint8_t RF_BODY = 2;

static AtClient& radarCli() {
  static AtClient c;
  return c;
}

static uint8_t g_rPhase = RF_IDLE;
static uint8_t g_rArm = 0;
static uint32_t g_rNext = 0;
static uint32_t g_rStart = 0;
static uint32_t g_rPace = 0;
static char g_rHdr[256];
static uint16_t g_rHdrN = 0;
static uint16_t g_rNeed = 0;
static uint16_t g_rExpect = 0;
static uint16_t g_rFill = 0;
static uint8_t g_rWrote = 0;
static uint8_t g_rHead[RADAR_HEAD_BYTES];
static uint8_t g_rShow[RADAR_FRAMES_MAX * RADAR_FRAME_BYTES];
static uint8_t g_rCount = 0;
static uint8_t g_rFlags = 0;
static int8_t g_rMax = 0;
static uint32_t g_rUnix = 0;
static uint32_t g_rGotMs = 0;
static uint8_t g_rFresh = 0;
static uint8_t g_rRain = 0;
// 0 off. 1 means the page toggle is on (prefs store that as 2).
static uint8_t g_autoRadar = 0;

static uint8_t radarFrameCount() { return g_rCount; }
static uint8_t radarFresh() { return g_rFresh; }
static uint8_t radarRain() { return (g_rFresh && (g_rFlags & 1u)) ? 1 : 0; }

static void radarOnSample(uint8_t ok, uint8_t fresh, uint8_t rain);

static void radarLog(const __FlashStringHelper* s) {
  Serial.print(millis());
  Serial.print(' ');
  Serial.println(s);
}

static void radarShowCommit(uint8_t n) {
  g_rCount = n;
  if (!n) lamp_fx_radar_set(0, 0);
  else lamp_fx_radar_set(g_rShow, n);
}

static void radarFail(const __FlashStringHelper* why) {
  modem.timeout(MODEM_CMD_BUDGET_MS);
  if (g_rPhase != RF_IDLE) {
    uint32_t t0 = millis();
    radarLog(F("radar stop b"));
    radarCli().stop();
    Serial.print(millis());
    Serial.print(F(" radar stop e "));
    Serial.println(millis() - t0);
  }
  g_rPhase = RF_IDLE;
  g_rFill = 0;
  g_rNeed = 0;
  g_rExpect = 0;
  if (g_rWrote) {
    g_rWrote = 0;
    radarShowCommit(0);
  }
  g_rFresh = 0;
  g_rNext = millis() + RADAR_POLL_FAIL_MS;
  Serial.print(millis());
  Serial.print(F(" radar fail "));
  Serial.println(why);
  radarOnSample(0, 0, 0);
}

static const char* radarHdrValue(const char* key) {
  size_t klen = strlen(key);
  for (uint16_t i = 0; i + klen + 1 < g_rHdrN; i++) {
    size_t k = 0;
    for (; k < klen; k++) {
      char c = g_rHdr[i + k];
      if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
      if (c != key[k]) break;
    }
    if (k != klen || g_rHdr[i + klen] != ':') continue;
    const char* v = g_rHdr + i + klen + 1;
    while (*v == ' ') v++;
    return v;
  }
  return 0;
}

static bool radarHdrReady() {
  const char* sp = strchr(g_rHdr, ' ');
  if (!sp || sp[1] != '2' || sp[2] != '0' || sp[3] != '0') {
    radarFail(F("http"));
    return false;
  }
  const char* te = radarHdrValue("transfer-encoding");
  if (te && strstr(te, "chunked")) {
    radarFail(F("chunked"));
    return false;
  }
  const char* cl = radarHdrValue("content-length");
  if (!cl) {
    radarFail(F("len"));
    return false;
  }
  long n = 0;
  for (const char* p = cl; *p && *p != '\r' && *p != '\n'; p++) {
    if (*p < '0' || *p > '9') {
      radarFail(F("len"));
      return false;
    }
    n = n * 10 + (*p - '0');
    if (n > RADAR_BODY_MAX) {
      radarFail(F("len"));
      return false;
    }
  }
  if (n < (long)RADAR_HEAD_BYTES) {
    radarFail(F("len"));
    return false;
  }
  g_rNeed = (uint16_t)n;
  g_rFill = 0;
  g_rExpect = 0;
  return true;
}

static void radarCommit() {
  RadarInfo info;
  uint16_t total = 0;
  if (g_rFill < RADAR_HEAD_BYTES || !radarDecodeHead(g_rHead, &info, &total) ||
      total != g_rFill || total != g_rNeed || info.frames > RADAR_FRAMES_MAX) {
    radarFail(F("bad"));
    return;
  }
  g_rWrote = 0;
  modem.timeout(MODEM_CMD_BUDGET_MS);
  uint32_t t0 = millis();
  radarLog(F("radar stop b"));
  radarCli().stop();
  Serial.print(millis());
  Serial.print(F(" radar stop e "));
  Serial.println(millis() - t0);
  g_rPhase = RF_IDLE;
  g_rFlags = info.flags;
  g_rMax = info.maxDbz;
  g_rUnix = info.newest;
  g_rGotMs = millis();
  g_rFill = 0;
  g_rNeed = 0;
  g_rExpect = 0;
  radarShowCommit(info.frames);
  // Age is time since this body arrived. WiFi.getTime() is another AT
  // command and is not used here.
  g_rFresh = info.newest ? 1 : 0;
  g_rRain = (info.flags & 1u) ? 1 : 0;
  g_rNext = millis() + RADAR_POLL_OK_MS;
  Serial.print(millis());
  Serial.print(F(" radar ok "));
  Serial.print(info.frames);
  Serial.print(F(" rain "));
  Serial.println(g_rRain);
  radarOnSample(1, g_rFresh, g_rFresh ? g_rRain : 0);
}

static bool radarTake(uint8_t byte) {
  if (g_rPhase == RF_HDR) {
    if (g_rHdrN + 1 >= sizeof g_rHdr) {
      radarFail(F("hdr"));
      return false;
    }
    g_rHdr[g_rHdrN++] = (char)byte;
    g_rHdr[g_rHdrN] = 0;
    if (g_rHdrN >= 4 && memcmp(g_rHdr + g_rHdrN - 4, "\r\n\r\n", 4) == 0) {
      if (!radarHdrReady()) return false;
      g_rPhase = RF_BODY;
    }
    return true;
  }
  if (g_rFill >= RADAR_BODY_MAX) {
    radarFail(F("big"));
    return false;
  }
  if (g_rFill < RADAR_HEAD_BYTES) {
    g_rHead[g_rFill++] = byte;
    if (g_rFill < RADAR_HEAD_BYTES) return true;
    RadarInfo info;
    uint16_t total = 0;
    if (!radarDecodeHead(g_rHead, &info, &total) || total != g_rNeed ||
        info.frames > RADAR_FRAMES_MAX) {
      radarFail(F("head"));
      return false;
    }
    g_rExpect = total;
    Serial.print(millis());
    Serial.print(F(" radar head "));
    Serial.println(total);
    if (g_rFill >= g_rExpect) {
      radarCommit();
      return false;
    }
    return true;
  }
  uint16_t off = (uint16_t)(g_rFill - RADAR_HEAD_BYTES);
  if (off >= RADAR_FRAMES_MAX * RADAR_FRAME_BYTES) {
    radarFail(F("big"));
    return false;
  }
  g_rShow[off] = byte;
  g_rFill++;
  g_rWrote = 1;
  if (g_rExpect && g_rFill >= g_rExpect) {
    radarCommit();
    return false;
  }
  return true;
}

static void radarRead(uint32_t now) {
  if ((uint32_t)(now - g_rStart) > RADAR_FETCH_MS) {
    radarFail(F("time"));
    return;
  }
  if ((uint32_t)(now - g_rPace) < RADAR_PACE_MS) return;
  g_rPace = now;
  uint32_t t0 = millis();
  Serial.print(t0);
  Serial.println(F(" radar av b"));
  modem.timeout(400);
  int avail = radarCli().available();
  uint32_t t1 = millis();
  modem.timeout(MODEM_CMD_BUDGET_MS);
  Serial.print(t1);
  Serial.print(F(" radar av e "));
  Serial.print(avail);
  Serial.print(' ');
  Serial.println(t1 - t0);
  if (avail <= 0) return;
  if (avail > (int)AT_READ_MAX) avail = (int)AT_READ_MAX;
  uint8_t tmp[AT_READ_MAX];
  t0 = millis();
  Serial.print(t0);
  Serial.println(F(" radar rd b"));
  int n = radarCli().readAt(tmp, (size_t)avail);
  t1 = millis();
  modem.timeout(MODEM_CMD_BUDGET_MS);
  Serial.print(t1);
  Serial.print(F(" radar rd e "));
  Serial.print(n);
  Serial.print(' ');
  Serial.println(t1 - t0);
  if (n <= 0) return;
  for (int i = 0; i < n; i++) {
    if (!radarTake(tmp[i])) return;
  }
}

static void radarBegin(uint32_t now) {
  WiFiClient& c = radarCli();
  c.stop();
  c.setConnectionTimeout(3000);
  // The ESP does DNS and TCP inside this one command. The RA4M1 must wait
  // until that command ends. A shorter modem timeout desyncs the bridge
  // and the next AT call never returns, which freezes the scroll.
  uint32_t t0 = millis();
  Serial.print(t0);
  Serial.println(F(" radar connect b"));
  modem.timeout(4500);
  int ok = c.connect(RADAR_HOST, (uint16_t)RADAR_PORT);
  uint32_t t1 = millis();
  modem.timeout(MODEM_CMD_BUDGET_MS);
  Serial.print(t1);
  Serial.print(F(" radar connect e "));
  Serial.print(ok);
  Serial.print(' ');
  Serial.println(t1 - t0);
  if (!ok) {
    radarFail(F("connect"));
    return;
  }
  static const char kReq[] =
      "GET " RADAR_PATH " HTTP/1.0\r\n"
      "Host: " RADAR_HOST "\r\n"
      "Connection: close\r\n"
      "\r\n";
  t0 = millis();
  Serial.print(t0);
  Serial.println(F(" radar write b"));
  modem.timeout(800);
  size_t w = c.write((const uint8_t*)kReq, sizeof kReq - 1);
  t1 = millis();
  modem.timeout(MODEM_CMD_BUDGET_MS);
  Serial.print(t1);
  Serial.print(F(" radar write e "));
  Serial.println(t1 - t0);
  if (w != sizeof kReq - 1) {
    radarFail(F("write"));
    return;
  }
  g_rPhase = RF_HDR;
  g_rStart = now;
  g_rPace = now;
  g_rHdrN = 0;
  g_rHdr[0] = 0;
  g_rFill = 0;
  g_rNeed = 0;
  g_rExpect = 0;
}

static void radarPoll(uint32_t now) {
  // Background fetch only while the radar preset is showing, or after the
  // page toggle has been turned on (saved as 2, loaded into g_autoRadar).
  // The old pref value 1 was an implicit default and must not poll.
  if (g_mode != M_RADAR && !g_autoRadar) {
    if (g_rPhase != RF_IDLE) radarFail(F("off"));
    g_rArm = 0;
    return;
  }
  // The web server and GitHub update share the same AT bridge. Do not
  // start or continue a radar command while either one is using it.
  if (g_http != PH_IDLE || g_otaActive || g_ghPend) {
    if (g_rPhase != RF_IDLE) g_rStart = now;
    return;
  }
  if (g_wifi != WIFI_ONLINE || g_setupAp) {
    g_rArm = 0;
    return;
  }
  if (!g_rArm) {
    g_rArm = 1;
    g_rNext = now + RADAR_FIRST_MS;
    radarLog(F("radar wait 20s"));
    return;
  }
  if (g_rFresh && g_rGotMs && (uint32_t)(now - g_rGotMs) > RADAR_STALE_MS) {
    g_rFresh = 0;
    radarOnSample(1, 0, 0);
  }
  if (g_rPhase == RF_IDLE) {
    if ((int32_t)(now - g_rNext) < 0) return;
    g_rNext = now + RADAR_POLL_FAIL_MS;
    radarBegin(now);
    return;
  }
  radarRead(now);
}

static uint8_t g_autoHeld = 0;
static uint8_t g_autoPrev = M_FIRE;
static uint8_t g_autoBlock = 0;
static uint8_t g_clearPolls = 0;

static uint8_t prefsMode() {
  return g_autoHeld ? g_autoPrev : g_mode;
}

static void radarOnSample(uint8_t ok, uint8_t fresh, uint8_t rain) {
  if (!g_autoRadar) return;
  if (!ok || !fresh) {
    g_clearPolls = 0;
    if (g_autoHeld && g_mode == M_RADAR) {
      g_mode = g_autoPrev;
      g_autoHeld = 0;
      lamp_fx_mark_dirty();
    }
    return;
  }
  if (rain) {
    g_clearPolls = 0;
    if (g_autoBlock || g_autoHeld || g_mode == M_RADAR) return;
    g_autoPrev = g_mode;
    g_autoHeld = 1;
    g_mode = M_RADAR;
    lamp_fx_mark_dirty();
    return;
  }
  g_autoBlock = 0;
  if (g_clearPolls < 255) g_clearPolls++;
  if (g_autoHeld && g_clearPolls >= 2 && g_mode == M_RADAR) {
    g_mode = g_autoPrev;
    g_autoHeld = 0;
    g_clearPolls = 0;
    lamp_fx_mark_dirty();
  }
}

static void radarUserMode(uint8_t /*mode*/) {
  if (g_autoHeld || radarRain()) g_autoBlock = 1;
  g_autoHeld = 0;
}
