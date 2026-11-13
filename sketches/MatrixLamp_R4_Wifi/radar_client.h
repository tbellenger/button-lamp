// Plain-HTTP fetch of /radar.bin. Included once from the sketch, after
// g_mode, g_wifi, g_http, g_otaActive, and g_ghPend.
//
// One socket, short timeouts. Connect is the only blocking call (about
// 1.2 s if the host is down). Reads are one modem command per loop.
// The lamp, the page, and OTA are not held across loops. A transfer is
// dropped if an OTA or a GitHub fetch starts.
//
// Defaults, until the dw-api radar route publishes a port and path.
// The page can replace host, port, and path; those are stored in prefs.
#ifndef RADAR_HOST
#define RADAR_HOST "76.13.99.44"
#endif
#ifndef RADAR_PORT
#define RADAR_PORT 8787
#endif
#ifndef RADAR_PATH
#define RADAR_PATH "/radar.bin"
#endif

#pragma once

#include <string.h>
#include "radar_format.h"

static const uint32_t RADAR_POLL_OK_MS = 300000ul;
static const uint32_t RADAR_POLL_RETRY_MS = 30000ul;
static const uint32_t RADAR_STALE_S = 1200ul;
static const uint32_t RADAR_STALE_MS = 1200000ul;
static const uint8_t RF_IDLE = 0;
static const uint8_t RF_HDR = 1;
static const uint8_t RF_BODY = 2;

static WiFiClient g_rCli;
static uint8_t g_rPhase = RF_IDLE;
static uint8_t g_rArm = 0;
static uint32_t g_rNext = 0;
static uint32_t g_rSince = 0;
static uint32_t g_rConnChk = 0;
static char g_rHdr[192];
static uint16_t g_rHdrN = 0;
static uint16_t g_rNeed = 0;
static uint16_t g_rFill = 0;
static uint8_t g_rHead[RADAR_HEAD_BYTES];
static uint8_t g_rShow[RADAR_FRAMES_MAX * RADAR_FRAME_BYTES];
static uint8_t g_rCount = 0;
static uint8_t g_rFlags = 0;
static int8_t g_rMax = 0;
static uint32_t g_rUnix = 0;
static uint16_t g_rIv = 300;
static uint32_t g_rGotMs = 0;
static uint32_t g_rClock = 0;
static uint8_t g_rFresh = 0;
static uint8_t g_rRain = 0;
static char g_rHost[40];
static char g_rPath[32];
static uint16_t g_rPort = RADAR_PORT;

static const char* radarHost() { return g_rHost; }
static uint16_t radarPort() { return g_rPort; }
static const char* radarPath() { return g_rPath; }
static uint8_t radarFrameCount() { return g_rCount; }
static uint8_t radarFresh() { return g_rFresh; }
static uint8_t radarRain() { return (g_rFresh && (g_rFlags & 1u)) ? 1 : 0; }
static int8_t radarMaxDbz() { return g_rMax; }

static int radarAgeSec() {
  if (!g_rUnix || !g_rClock) return -1;
  if (g_rClock < g_rUnix) return 0;
  uint32_t age = g_rClock - g_rUnix;
  if (age > 86400ul) return 86400;
  return (int)age;
}

static void radarOnSample(uint8_t ok, uint8_t fresh, uint8_t rain);

static void radarShowCommit(uint8_t n) {
  g_rCount = n;
  if (!n) lamp_fx_radar_set(0, 0);
  else lamp_fx_radar_set(g_rShow, n);
}

static void radarFail(const __FlashStringHelper* why) {
  uint8_t wiped = g_rFill > RADAR_HEAD_BYTES;
  g_rCli.stop();
  g_rPhase = RF_IDLE;
  g_rFill = 0;
  g_rNeed = 0;
  g_rFresh = 0;
  g_rNext = millis() + RADAR_POLL_RETRY_MS;
  if (wiped) radarShowCommit(0);
  Serial.print(F("Radar "));
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
  return true;
}

static uint8_t radarUnixStale(uint32_t unixTime) {
  g_rClock = (uint32_t)WiFi.getTime();
  if (!unixTime) return 1;
  if (!g_rClock || g_rClock < unixTime) return 0;
  return (g_rClock - unixTime) > RADAR_STALE_S;
}

static void radarCommit() {
  RadarInfo info;
  uint16_t total = 0;
  if (!radarDecodeHead(g_rHead, &info, &total) || total != g_rFill ||
      (g_rNeed && g_rFill != g_rNeed)) {
    radarFail(F("bad"));
    return;
  }
  g_rCli.stop();
  g_rPhase = RF_IDLE;
  g_rFlags = info.flags;
  g_rMax = info.maxDbz;
  g_rUnix = info.newest;
  g_rIv = info.interval ? info.interval : 300;
  g_rGotMs = millis();
  g_rFill = 0;
  radarShowCommit(info.frames);
  uint8_t stale = radarUnixStale(info.newest);
  g_rFresh = stale ? 0 : 1;
  g_rRain = (info.flags & 1u) ? 1 : 0;
  g_rNext = millis() + RADAR_POLL_OK_MS;
  Serial.print(F("Radar "));
  Serial.print(info.frames);
  Serial.print(F(" rain "));
  Serial.println(g_rRain);
  radarOnSample(1, g_rFresh, g_rFresh ? g_rRain : 0);
}

static bool radarTake(uint8_t byte) {
  if (g_rPhase == RF_HDR) {
    if (g_rHdrN + 1 >= sizeof g_rHdr) {
      radarFail(F("bad"));
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
  if (g_rFill < RADAR_HEAD_BYTES) g_rHead[g_rFill] = byte;
  else g_rShow[g_rFill - RADAR_HEAD_BYTES] = byte;
  g_rFill++;
  if (g_rNeed && g_rFill >= g_rNeed) {
    radarCommit();
    return false;
  }
  return true;
}

static void radarRead(uint32_t now) {
  if ((uint32_t)(now - g_rSince) > 8000ul) {
    radarFail(F("time"));
    return;
  }
  int avail = g_rCli.available();
  if (avail <= 0) {
    if ((uint32_t)(now - g_rConnChk) < 500ul) return;
    g_rConnChk = now;
    if (!g_rCli.connected()) {
      if (g_rPhase == RF_BODY && g_rNeed && g_rFill >= g_rNeed) radarCommit();
      else radarFail(F("down"));
    }
    return;
  }
  if (avail > 64) avail = 64;
  uint8_t tmp[64];
  int n = g_rCli.read(tmp, (size_t)avail);
  if (n <= 0) return;
  g_rSince = now;
  for (int i = 0; i < n; i++) {
    if (!radarTake(tmp[i])) return;
  }
}

static void radarBegin(uint32_t now) {
  const char* host = g_rHost[0] ? g_rHost : RADAR_HOST;
  const char* path = g_rPath[0] ? g_rPath : RADAR_PATH;
  uint16_t port = g_rPort ? g_rPort : (uint16_t)RADAR_PORT;
  g_rCli.setConnectionTimeout(1200);
  modem.timeout(1500);
  int ok = g_rCli.connect(host, port);
  modem.timeout(MODEM_CMD_BUDGET_MS);
  if (!ok) {
    radarFail(F("down"));
    return;
  }
  char req[160];
  int n = snprintf(req, sizeof req,
                   "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
                   path, host);
  if (n < 0 || n >= (int)sizeof req ||
      g_rCli.write((const uint8_t*)req, (size_t)n) != (size_t)n) {
    radarFail(F("down"));
    return;
  }
  g_rPhase = RF_HDR;
  g_rHdrN = 0;
  g_rHdr[0] = 0;
  g_rFill = 0;
  g_rNeed = 0;
  g_rSince = now;
  g_rConnChk = now;
}

static void radarPoll(uint32_t now) {
  if (g_otaActive || g_ghPend) {
    if (g_rPhase != RF_IDLE) {
      uint8_t wiped = g_rFill > RADAR_HEAD_BYTES;
      g_rCli.stop();
      g_rPhase = RF_IDLE;
      g_rFill = 0;
      g_rNext = now + 8000ul;
      if (wiped) radarShowCommit(0);
    }
    return;
  }
  if (g_rPhase == RF_IDLE) {
    if (g_rFresh && g_rGotMs && (uint32_t)(now - g_rGotMs) > RADAR_STALE_MS) {
      g_rFresh = 0;
      radarOnSample(1, 0, 0);
    }
    if (!g_rArm) {
      if (g_wifi != WIFI_ONLINE || g_setupAp) return;
      g_rArm = 1;
      g_rNext = now + 3000ul;
      return;
    }
    if ((int32_t)(now - g_rNext) < 0) return;
    if (g_wifi != WIFI_ONLINE || g_setupAp || g_http != PH_IDLE) return;
    radarBegin(now);
    if (g_rPhase != RF_IDLE) radarRead(now);
    return;
  }
  if (g_http != PH_IDLE) {
    g_rSince = now;
    return;
  }
  radarRead(now);
}

static void radarConfigure(const char* host, uint16_t port, const char* path, uint8_t touch) {
  if (host && host[0]) strncpy(g_rHost, host, sizeof g_rHost - 1);
  else strncpy(g_rHost, RADAR_HOST, sizeof g_rHost - 1);
  g_rHost[sizeof g_rHost - 1] = 0;
  g_rPort = port ? port : (uint16_t)RADAR_PORT;
  if (path && path[0]) strncpy(g_rPath, path, sizeof g_rPath - 1);
  else strncpy(g_rPath, RADAR_PATH, sizeof g_rPath - 1);
  g_rPath[sizeof g_rPath - 1] = 0;
  if (!touch) return;
  g_prefsDirty = 1;
  g_rNext = millis();
  g_rArm = 1;
  if (g_rPhase != RF_IDLE) {
    g_rCli.stop();
    g_rPhase = RF_IDLE;
    g_rFill = 0;
  }
}

static void radarBoot() {
  radarConfigure(0, 0, 0, 0);
  g_rIv = 300;
}

// Auto radar. Default on. A fresh rain_nearby enters M_RADAR and remembers
// the preset. Two fresh polls with the flag clear return to it. A manual
// preset change during that visit cancels the return and holds the user's
// choice until a later fresh poll says the rain has gone. Unreachable or
// stale (newest frame older than 20 min, or no good poll for 20 min) does
// not enter, and leaves if this visit was automatic.
static uint8_t g_autoRadar = 1;
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
