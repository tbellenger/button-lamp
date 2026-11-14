// Fetch of http://api.drunkwallaby.com/radar.bin (port 80).
// Included once from the sketch, after g_mode, g_wifi, g_http, g_otaActive,
// and g_ghPend.
//
// The picture may pause while a refresh is in flight. The fetch is one
// blocking exchange: a connect timeout long enough that the ESP finishes
// the AT command, then 64-byte reads into a staging buffer. The panel keeps
// the previous frames if the new body is short, late, or not a radar file.
// It does not run while an OTA download owns the bridge, and it does not
// start while the web server is mid-request.
// WiFiClient::read() is not used. A read larger than 64 bytes overflows
// the 512-byte Serial2 ring.
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
static char g_rHdr[256];
static uint16_t g_rHdrN = 0;
static uint16_t g_rNeed = 0;
static uint16_t g_rFill = 0;
static uint8_t g_rShow[RADAR_FRAMES_MAX * RADAR_FRAME_BYTES];

// Staging reuses the HTTP and OTA scratch buffers. radarPoll only runs
// while the server is idle and no update is in progress, so those arrays
// are not live. A second 3 KB frame buffer does not fit beside the stack
// and the 8 KB heap the core reserves.
static const uint16_t RADAR_SCRATCH =
    (uint16_t)(sizeof(g_json) + sizeof(g_req) + sizeof(g_ghPath));
static_assert(RADAR_SCRATCH >= RADAR_BODY_MAX, "radar body does not fit in scratch");

static uint8_t* radarScratch(uint16_t off) {
  if (off < sizeof g_json) return (uint8_t*)g_json + off;
  off = (uint16_t)(off - (uint16_t)sizeof g_json);
  if (off < sizeof g_req) return (uint8_t*)g_req + off;
  off = (uint16_t)(off - (uint16_t)sizeof g_req);
  return (uint8_t*)g_ghPath + off;
}

static void radarScratchCopy(uint8_t* dst, uint16_t off, uint16_t n) {
  for (uint16_t i = 0; i < n; i++) dst[i] = *radarScratch((uint16_t)(off + i));
}
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

static void radarStop() {
  modem.timeout(MODEM_CMD_BUDGET_MS);
  uint32_t t0 = millis();
  radarLog(F("radar stop b"));
  radarCli().stop();
  Serial.print(millis());
  Serial.print(F(" radar stop e "));
  Serial.println(millis() - t0);
  modem.timeout(MODEM_CMD_BUDGET_MS);
  g_rPhase = RF_IDLE;
}

// Failure leaves g_rShow alone. The loop on the panel stays up.
static void radarFail(const __FlashStringHelper* why) {
  if (g_rPhase != RF_IDLE) radarStop();
  g_rFill = 0;
  g_rNeed = 0;
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
  return true;
}

static bool radarReadSome(uint8_t* buf, int cap, uint32_t deadline, int* nout) {
  *nout = 0;
  while ((int32_t)(deadline - millis()) > 0) {
    modem.timeout(400);
    int avail = radarCli().available();
    modem.timeout(MODEM_CMD_BUDGET_MS);
    if (avail <= 0) {
      delay(15);
      continue;
    }
    if (avail > cap) avail = cap;
    if (avail > (int)AT_READ_MAX) avail = (int)AT_READ_MAX;
    modem.timeout(400);
    int n = radarCli().readAt(buf, (size_t)avail);
    modem.timeout(MODEM_CMD_BUDGET_MS);
    if (n > 0) {
      *nout = n;
      return true;
    }
    delay(15);
  }
  return false;
}

static bool radarTake(const uint8_t* buf, int n, int* used) {
  *used = 0;
  for (int i = 0; i < n; i++) {
    if (g_rPhase == RF_HDR) {
      if ((size_t)g_rHdrN + 1 >= sizeof g_rHdr) {
        radarFail(F("hdr"));
        return false;
      }
      g_rHdr[g_rHdrN++] = (char)buf[i];
      g_rHdr[g_rHdrN] = 0;
      if (g_rHdrN >= 4 && memcmp(g_rHdr + g_rHdrN - 4, "\r\n\r\n", 4) == 0) {
        if (!radarHdrReady()) return false;
        g_rPhase = RF_BODY;
        g_rFill = 0;
      }
      continue;
    }
    if (g_rFill >= g_rNeed || g_rFill >= RADAR_BODY_MAX) {
      radarFail(F("big"));
      return false;
    }
    *radarScratch(g_rFill++) = buf[i];
    if (g_rFill >= g_rNeed) {
      *used = i + 1;
      return true;
    }
  }
  *used = n;
  return true;
}

static void radarCommitStage() {
  RadarInfo info;
  uint16_t total = 0;
  uint8_t head[RADAR_HEAD_BYTES];
  if (g_rFill < RADAR_HEAD_BYTES) {
    radarFail(F("bad"));
    return;
  }
  radarScratchCopy(head, 0, RADAR_HEAD_BYTES);
  if (!radarDecodeHead(head, &info, &total) ||
      total != g_rFill || total != g_rNeed || info.frames > RADAR_FRAMES_MAX) {
    radarFail(F("bad"));
    return;
  }
  radarScratchCopy(g_rShow, RADAR_HEAD_BYTES,
                   (uint16_t)((uint16_t)info.frames * RADAR_FRAME_BYTES));
  radarStop();
  g_rFlags = info.flags;
  g_rMax = info.maxDbz;
  g_rUnix = info.newest;
  g_rGotMs = millis();
  g_rFill = 0;
  g_rNeed = 0;
  radarShowCommit(info.frames);
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

static void radarFetch(uint32_t now) {
  AtClient& c = radarCli();
  c.stop();
  c.setConnectionTimeout(3000);
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
  modem.timeout(800);
  size_t w = c.write((const uint8_t*)kReq, sizeof kReq - 1);
  modem.timeout(MODEM_CMD_BUDGET_MS);
  if (w != sizeof kReq - 1) {
    radarFail(F("write"));
    return;
  }
  g_rPhase = RF_HDR;
  g_rHdrN = 0;
  g_rHdr[0] = 0;
  g_rFill = 0;
  g_rNeed = 0;
  uint32_t deadline = now + RADAR_FETCH_MS;
  while (g_rPhase != RF_IDLE) {
    uint8_t tmp[AT_READ_MAX];
    int n = 0;
    if (!radarReadSome(tmp, (int)AT_READ_MAX, deadline, &n)) {
      radarFail(F("time"));
      return;
    }
    int used = 0;
    if (!radarTake(tmp, n, &used)) return;
    if (g_rPhase == RF_BODY && g_rNeed && g_rFill >= g_rNeed) {
      radarCommitStage();
      return;
    }
  }
}

static void radarPoll(uint32_t now) {
  if (g_mode != M_RADAR && !g_autoRadar) {
    g_rArm = 0;
    return;
  }
  if (g_http != PH_IDLE || g_otaActive || g_ghPend) return;
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
  if ((int32_t)(now - g_rNext) < 0) return;
  g_rNext = now + RADAR_POLL_FAIL_MS;
  radarFetch(now);
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
