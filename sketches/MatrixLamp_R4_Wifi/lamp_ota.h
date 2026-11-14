// GitHub OTA for the Uno R4 WiFi, through the core's OTAUpdate library.
// The ESP32-S3 stages the image (bridge firmware 0.6.0, Arduino_ESP32_OTA).
// There is no ArduinoOTA port, no password, and no local upload.
//
// Install update checks manifest.json on the matrixlamp-latest release.
// The same normalized version is "Up to date". A newer version resolves
// GitHub's 302 (OTAUpdate does not follow redirects; the AT line is the
// final https URL) and calls startDownload. loop() polls progress.
// verify() then update("/update.bin") hands the file to BOSSA, which
// resets the RA4M1 only. setup() then restarts the ESP32.
#pragma once

#include <OTAUpdate.h>
#include <WiFiSSLClient.h>
#include <string.h>
#include <stdlib.h>
#include "at_read.h"
#include "ota_ca.h"
#include "ver_cmp.h"

// Core 1.6.0 memory_regions.ld: FLASH_LENGTH 0x40000, FLASH_IMAGE_START 0x4000.
// The linker length is that difference. boards.txt upload.maximum_size is
// the whole 256 KB, which includes the 16 KB bootloader.
static const uint32_t SKETCH_MAX_BYTES = 262144u - 16384u;

// chAT's command buffer on bridge 0.6.0 is 1024 bytes (SudoMaker/chAT).
// AT+OTADOWNLOADSTART=<url>,/update.bin\r\n has to fit, and a comma in the
// URL would split the arguments. A live release-asset Location in
// November 2026 was 933 bytes (967 with the command), under this cap.
// WiFiS3 Modem::write still formats that line into a 128-byte tx_buff
// (MAX_BUFF_SIZE). OTAUpdate::startDownload therefore chops the URL and
// returns Error::Modem (-26) without the bridge ever seeing the request.
// The sketch writes the line to Serial2 itself.
static const size_t OTA_AT_LINE_MAX = 1024;

static const char SETUP_PAGE[] =
  "<!DOCTYPE html><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
  "<title>Setup</title><h1>Matrix Lamp</h1>"
  "<p>WiFi is MatrixLamp-Setup. Saving restarts the lamp onto your network.</p>"
  "<form method=POST action=/setup>"
  "WiFi name<br><input name=ssid required maxlength=32><br>"
  "WiFi password<br><input name=pass type=password maxlength=63><br>"
  "<button>Save</button></form>";

static const char GH_MANIFEST[] =
  "/tbellenger/button-lamp/releases/download/matrixlamp-latest/manifest.json";
static const char GH_OTA_NAME[] = "MatrixLamp_R4_Wifi.ino.ota";
static const char OTA_SPIFFS_NAME[] = "/update.bin";

static OTAUpdate g_otaUpd;
static uint8_t g_otaPhase = 0;
static int g_otaExpect = 0;
static int g_otaProg = 0;
static uint32_t g_otaPollAt = 0;
static uint32_t g_otaStallAt = 0;

static void wifiResetModem();
static void wifiDrop(uint32_t now);
static void wifiBegin();
static void serial2Drain(uint32_t budgetMs);
static void wdtPet();

static void otaSet(const char* st, const char* msg) {
  g_otaState = st;
  size_t i = 0;
  if (msg) {
    for (; msg[i] && msg[i] != '\n' && i + 1 < sizeof g_otaMsg; i++) g_otaMsg[i] = msg[i];
  }
  g_otaMsg[i] = 0;
}

static void otaReply(int code, const char* reason, const char* msg) {
  httpBegin(code, reason, "text/plain; charset=utf-8", msg, (uint16_t)strlen(msg));
}

static void otaModemRest() {
  WiFi.setTimeout(0);
  modem.timeout(MODEM_CMD_BUDGET_MS);
}

static void otaIdle(const char* st, const char* msg) {
  g_otaPhase = 0;
  g_otaActive = 0;
  otaModemRest();
  if (st) otaSet(st, msg);
}

static bool ciEq(const char* a, const char* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char ca = a[i];
    char cb = b[i];
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
    if (ca != cb) return false;
  }
  return true;
}

static bool ciStarts(const char* s, const char* pfx) {
  return ciEq(s, pfx, strlen(pfx));
}

static bool reqPath(char* path, size_t n) {
  const char* p = g_req;
  if (strncmp(p, "POST ", 5) == 0) p += 5;
  else if (strncmp(p, "GET ", 4) == 0) p += 4;
  else return false;
  const char* e = p;
  while (*e && *e != ' ' && *e != '?' && *e != '\r' && *e != '\n') e++;
  size_t len = (size_t)(e - p);
  if (len == 0 || len + 1 > n) return false;
  memcpy(path, p, len);
  path[len] = 0;
  return true;
}

static bool headerValue(const char* name, char* out, size_t n) {
  out[0] = 0;
  size_t nlen = strlen(name);
  const char* end = strstr(g_req, "\r\n\r\n");
  if (!end) return false;
  const char* p = g_req;
  while (p < end) {
    if ((size_t)(end - p) >= nlen && ciEq(p, name, nlen) && p[nlen] == ':') {
      p += nlen + 1;
      while (*p == ' ' || *p == '\t') p++;
      size_t i = 0;
      while (p < end && *p != '\r' && *p != '\n' && i + 1 < n) out[i++] = *p++;
      out[i] = 0;
      return true;
    }
    const char* nl = strstr(p, "\r\n");
    if (!nl || nl >= end) break;
    p = nl + 2;
  }
  return false;
}

static long headerLong(const char* name) {
  char buf[16];
  if (!headerValue(name, buf, sizeof buf)) return -1;
  return strtol(buf, 0, 10);
}

static void urlDecode(char* s) {
  char* w = s;
  for (char* p = s; *p; p++) {
    if (*p == '+') *w++ = ' ';
    else if (*p == '%' && p[1] && p[2]) {
      char hex[3] = {p[1], p[2], 0};
      *w++ = (char)strtol(hex, 0, 16);
      p += 2;
    } else *w++ = *p;
  }
  *w = 0;
}

static bool formGet(const char* body, const char* key, char* out, size_t n) {
  out[0] = 0;
  size_t klen = strlen(key);
  const char* p = body;
  while (p && *p) {
    if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
      p += klen + 1;
      size_t i = 0;
      while (*p && *p != '&') {
        if (i + 1 >= n) return false;
        out[i++] = *p++;
      }
      out[i] = 0;
      urlDecode(out);
      return true;
    }
    const char* amp = strchr(p, '&');
    p = amp ? amp + 1 : 0;
  }
  return false;
}

static void setupSave() {
  if (!g_setupAp) {
    otaReply(404, "Not Found", "not found\n");
    return;
  }
  char* hdr = strstr(g_req, "\r\n\r\n");
  if (!hdr) {
    otaReply(400, "Bad Request", "body\n");
    return;
  }
  char ssid[33];
  char pass[64];
  if (!formGet(hdr + 4, "ssid", ssid, sizeof ssid) || !ssid[0]) {
    otaReply(400, "Bad Request", "ssid required\n");
    return;
  }
  if (!formGet(hdr + 4, "pass", pass, sizeof pass)) pass[0] = 0;
  matrixPause();
  LampCreds c;
  credsClear(&c);
  strncpy(c.ssid, ssid, 32);
  strncpy(c.pass, pass, 63);
  if (!credsSave(c)) {
    matrixResume();
    otaReply(500, "Error", "save failed\n");
    return;
  }
  Serial.println(F("Setup saved"));
  g_rebootAfter = 1;
  g_rebootAt = millis() + 1500u;
  otaReply(200, "OK", "Saved. Restarting.\n");
}

static void otaQueueInstall() {
  if (g_setupAp) {
    otaReply(409, "Conflict", "join the home wifi first\n");
    return;
  }
  if (g_otaPhase || g_otaActive || g_ghPend || g_ghJob) {
    otaReply(409, "Conflict", "update already running\n");
    return;
  }
  g_ghJob = 1;
  otaSet("check", "checking");
  otaReply(200, "OK", "ok install\n");
}

static int otaDispatchPost(uint32_t now) {
  char path[48];
  if (!reqPath(path, sizeof path)) {
    otaReply(400, "Bad Request", "path\n");
    return 1;
  }
  bool small = strcmp(path, "/setup") == 0 || strcmp(path, "/api/ota/install") == 0;
  if (!small) {
    otaReply(404, "Not Found", "not found\n");
    return 1;
  }
  long len = headerLong("Content-Length");
  if (len < 0) len = 0;
  const char* hdr = strstr(g_req, "\r\n\r\n");
  uint32_t at = (uint32_t)(hdr + 4 - g_req);
  if (at + (uint32_t)len >= sizeof g_req) {
    otaReply(413, "Payload Too Large", "body\n");
    return 1;
  }
  uint32_t have = g_reqLen - at;
  if (have < (uint32_t)len) {
    if ((uint32_t)(now - g_clientMs) >= 8000) {
      otaReply(408, "Request Timeout", "timeout\n");
      return 1;
    }
    return 0;
  }
  if (strcmp(path, "/setup") == 0) setupSave();
  else otaQueueInstall();
  return 1;
}

static void startSetupAp() {
  Serial.println(F("Setup AP MatrixLamp-Setup"));
  uint8_t st = WiFi.beginAP("MatrixLamp-Setup");
  if (st != WL_AP_LISTENING) Serial.println(F("AP failed"));
  IPAddress ip = WiFi.softAPIP();
  if (ip[0] == 0) ip = WiFi.localIP();
  if (ip[0] == 0) ip = IPAddress(192, 168, 4, 1);
  rememberIp(ip);
  g_server.begin();
  g_serverUp = 1;
  g_wifi = WIFI_ONLINE;
  Serial.println(F("server started"));
  Serial.print(F("Open http://"));
  Serial.println(ip);
}

struct GhHead {
  int code;
  long length;
  uint8_t chunked;
  uint8_t loc;
  char status[64];
  char ctype[32];
  int got;
  int seen;
  int phase;
};

// The manifest is parsed from g_json. That buffer is free here: the page
// response has already been sent, and the signed download URL is written
// into it only after the fields are copied out. The old g_manifest[480]
// rejected the live file (Content-Length 512) in manifestHead, which
// ghOnce reported as "github read failed" just after the release-assets
// connect. 8c451aa still has that 480-byte cap, so the published
// manifest stays under 480 bytes as well.
static int g_manifestN = 0;

static void ghNote(const GhHead* h, const char* why) {
  Serial.print(F("OTA http "));
  Serial.print(why);
  Serial.print(F(" phase "));
  Serial.print(h->phase);
  Serial.print(F(" code "));
  Serial.print(h->code);
  Serial.print(F(" len "));
  Serial.print(h->length);
  Serial.print(F(" chunk "));
  Serial.print((int)h->chunked);
  Serial.print(F(" got "));
  Serial.print(h->got);
  Serial.print(F(" seen "));
  Serial.print(h->seen);
  Serial.print(F(" stat "));
  Serial.print(h->status[0] ? h->status : "-");
  Serial.print(F(" type "));
  Serial.println(h->ctype[0] ? h->ctype : "-");
}

static AtSslClient& ghClient() {
  static AtSslClient c;
  return c;
}

static bool ghSend(WiFiSSLClient& c, const char* s) {
  size_t n = strlen(s);
  size_t off = 0;
  while (off < n) {
    size_t take = n - off;
    if (take > 200) take = 200;
    if (c.write((const uint8_t*)s + off, take) != take) return false;
    off += take;
  }
  return true;
}

// headersOnly stops once the status and headers are known. A 302 still
// returns at the end of Location so the Content-Security-Policy is never
// pulled across the 512-byte Serial2 ring. Reads are AT_READ_MAX (64).
static int ghReadResponse(AtSslClient& c, GhHead* h, int (*onHead)(GhHead*),
                          int (*onBody)(const uint8_t*, int), uint8_t headersOnly) {
  h->code = 0;
  h->length = -1;
  h->chunked = 0;
  h->loc = 0;
  h->status[0] = 0;
  h->ctype[0] = 0;
  h->got = 0;
  h->seen = 0;
  h->phase = 0;
  g_ghPath[0] = 0;
  char line[96];
  int ln = 0;
  int cap = 0;
  int locN = 0;
  int phase = 0;
  long got = 0;
  uint32_t idle = millis();
  while ((uint32_t)(millis() - idle) < 8000) {
    if (phase == 1 && h->length >= 0 && got >= h->length) return 1;
    if (c.available() <= 0) {
      delay(5);
      continue;
    }
    uint8_t buf[AT_READ_MAX];
    int a = c.available();
    if (a > (int)AT_READ_MAX) a = (int)AT_READ_MAX;
    int n = c.readAt(buf, (size_t)a);
    if (n <= 0) continue;
    idle = millis();
    h->seen += n;
    int i = 0;
    if (phase == 0) {
      for (; i < n; i++) {
        uint8_t b = buf[i];
        if (b == '\n') {
          if (ln && line[ln - 1] == '\r') ln--;
          line[ln] = 0;
          if (cap) {
            g_ghPath[locN] = 0;
            cap = 0;
            if (h->code >= 300 && h->code < 400 && h->loc) return 1;
          }
          if (ln == 0) {
            phase = 1;
            h->phase = 1;
            if (onHead && !onHead(h)) {
              ghNote(h, "head");
              return 0;
            }
            if (headersOnly) return 1;
            if (h->chunked && h->length < 0) {
              ghNote(h, "chunked");
              otaSet("error", "chunked");
              return 0;
            }
            if (h->code >= 300 && h->code < 400) return 1;
            if (h->length == 0) return 1;
            i++;
            break;
          }
          if (h->code == 0) {
            size_t si = 0;
            while (line[si] && si + 1 < sizeof h->status) {
              h->status[si] = line[si];
              si++;
            }
            h->status[si] = 0;
            const char* sp = strchr(line, ' ');
            if (sp) h->code = atoi(sp + 1);
          } else if (ciStarts(line, "Content-Length:")) {
            h->length = strtol(line + 15, 0, 10);
          } else if (ciStarts(line, "Content-Type:")) {
            const char* v = line + 13;
            while (*v == ' ' || *v == '\t') v++;
            size_t ti = 0;
            while (v[ti] && v[ti] != ';' && ti + 1 < sizeof h->ctype) {
              h->ctype[ti] = v[ti];
              ti++;
            }
            h->ctype[ti] = 0;
          } else if (ciStarts(line, "Transfer-Encoding:") && strstr(line, "chunked")) {
            h->chunked = 1;
          }
          ln = 0;
        } else if (cap) {
          if (b != '\r') {
            if (locN >= (int)sizeof g_ghPath - 1) {
              otaSet("error", "url too long");
              return 0;
            }
            g_ghPath[locN++] = (char)b;
          }
        } else if (ln < 90) {
          line[ln++] = (char)b;
          if (ln == 9 && ciEq(line, "Location:", 9)) {
            cap = 1;
            locN = 0;
            h->loc = 1;
          }
        }
      }
    }
    if (phase == 1 && i < n) {
      int bn = n - i;
      if (h->length >= 0 && got + bn > h->length) bn = (int)(h->length - got);
      if (bn > 0) {
        if (onBody && !onBody(buf + i, bn)) {
          h->got = (int)got;
          ghNote(h, "body");
          return 0;
        }
        got += bn;
        h->got = (int)got;
      }
    }
  }
  h->phase = phase;
  h->got = (int)got;
  if (phase == 1 && h->length >= 0 && got >= h->length) return 1;
  ghNote(h, "timeout");
  return 2;
}

static bool ghTakeLocation() {
  char* p = g_ghPath;
  while (*p == ' ' || *p == '\t') p++;
  if (strncmp(p, "https://", 8) == 0) {
    p += 8;
    char* slash = strchr(p, '/');
    if (!slash || slash == p) return false;
    size_t hn = (size_t)(slash - p);
    if (hn == 0 || hn >= sizeof g_ghHost) return false;
    char host[64];
    memcpy(host, p, hn);
    host[hn] = 0;
    memmove(g_ghPath, slash, strlen(slash) + 1);
    memcpy(g_ghHost, host, hn + 1);
    return g_ghPath[0] == '/';
  }
  if (*p == '/') {
    if (p != g_ghPath) memmove(g_ghPath, p, strlen(p) + 1);
    return true;
  }
  return false;
}

static bool ghOnce(const char* path, int (*onHead)(GhHead*), int (*onBody)(const uint8_t*, int),
                   GhHead* h, uint8_t headersOnly) {
  strncpy(g_ghPath, path, sizeof g_ghPath - 1);
  g_ghPath[sizeof g_ghPath - 1] = 0;
  AtSslClient& c = ghClient();
  c.stop();
  uint32_t t0 = millis();
  Serial.print(t0);
  Serial.print(F(" GitHub TLS b "));
  Serial.println(g_ghHost);
  modem.timeout(12000);
  int connected = c.connect(g_ghHost, 443);
  if (!connected) {
    delay(350);
    c.stop();
    connected = c.connect(g_ghHost, 443);
  }
  Serial.print(millis());
  Serial.print(F(" GitHub TLS e "));
  Serial.print(connected);
  Serial.print(' ');
  Serial.println(millis() - t0);
  if (!connected) {
    modem.timeout(MODEM_CMD_BUDGET_MS);
    otaSet("error", "tls connect failed");
    return false;
  }
  bool sent = ghSend(c, "GET ") && ghSend(c, g_ghPath) &&
              ghSend(c, " HTTP/1.0\r\nHost: ") && ghSend(c, g_ghHost) &&
              ghSend(c, "\r\nUser-Agent: MatrixLamp\r\nConnection: close\r\n\r\n");
  modem.timeout(8000);
  int rd = 0;
  if (!sent) {
    Serial.print(F("OTA http fail send path "));
    Serial.println((unsigned)strlen(g_ghPath));
  } else {
    rd = ghReadResponse(c, h, onHead, onBody, headersOnly);
  }
  modem.timeout(MODEM_CMD_BUDGET_MS);
  c.stop();
  if (rd == 2) {
    otaSet("error", "timeout");
    return false;
  }
  if (rd != 1) {
    if (!g_otaMsg[0]) otaSet("error", "github read failed");
    return false;
  }
  ghNote(h, "ok");
  if (!headersOnly && h->chunked) {
    otaSet("error", "chunked");
    return false;
  }
  return true;
}

static const char* jsonVal(const char* j, const char* key) {
  char pat[32];
  snprintf(pat, sizeof pat, "\"%s\"", key);
  const char* p = strstr(j, pat);
  if (!p) return 0;
  p += strlen(pat);
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  if (*p != ':') return 0;
  p++;
  while (*p == ' ' || *p == '\t') p++;
  return p;
}

static long jsonLong(const char* j, const char* key) {
  const char* p = jsonVal(j, key);
  if (!p) return -1;
  return strtol(p, 0, 10);
}

static bool jsonString(const char* j, const char* key, char* out, size_t n) {
  const char* p = jsonVal(j, key);
  if (!p || *p != '"') return false;
  p++;
  size_t i = 0;
  while (*p && *p != '"') {
    if (*p == '\\') return false;
    if (i + 1 >= n) return false;
    out[i++] = *p++;
  }
  out[i] = 0;
  return *p == '"';
}

static int manifestBody(const uint8_t* p, int n) {
  if (g_manifestN + n >= (int)sizeof g_json) {
    otaSet("error", "manifest too big");
    return 0;
  }
  memcpy(g_json + g_manifestN, p, (size_t)n);
  g_manifestN += n;
  g_json[g_manifestN] = 0;
  return 1;
}

static int manifestHead(GhHead* h) {
  if (h->code != 200) return 1;
  if (h->length < 2 || h->length >= (long)sizeof g_json) {
    otaSet("error", "manifest too big");
    return 0;
  }
  return 1;
}

static bool fileNameOk(const char* s) {
  if (!s || !s[0]) return false;
  for (const char* p = s; *p; p++) {
    char c = *p;
    bool okc = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    if (!okc) return false;
  }
  return true;
}

static bool ghFetchManifest() {
  strncpy(g_ghHost, "github.com", sizeof g_ghHost);
  g_manifestN = 0;
  g_json[0] = 0;
  g_otaMsg[0] = 0;
  for (int hop = 0; hop < 4; hop++) {
    char pathBuf[1400];
    if (hop == 0) {
      strncpy(pathBuf, GH_MANIFEST, sizeof pathBuf - 1);
      pathBuf[sizeof pathBuf - 1] = 0;
    } else {
      memcpy(pathBuf, g_ghPath, sizeof pathBuf);
    }
    GhHead h;
    if (!ghOnce(pathBuf, manifestHead, manifestBody, &h, 0)) return false;
    if (h.code == 200) {
      g_ghNewer = 0;
      g_ghVer[0] = 0;
      g_ghFile[0] = 0;
      g_ghSha[0] = 0;
      g_ghSize = jsonLong(g_json, "size");
      if (!jsonString(g_json, "version", g_ghVer, sizeof g_ghVer)) {
        otaSet("error", "bad manifest");
        return false;
      }
      jsonString(g_json, "sha", g_ghSha, sizeof g_ghSha);
      if (g_ghSize < 4096 || (uint32_t)g_ghSize > SKETCH_MAX_BYTES) {
        otaSet("error", "bad manifest");
        return false;
      }
      if (!jsonString(g_json, "ota", g_ghFile, sizeof g_ghFile) || !fileNameOk(g_ghFile)) {
        strncpy(g_ghFile, GH_OTA_NAME, sizeof g_ghFile - 1);
        g_ghFile[sizeof g_ghFile - 1] = 0;
      }
      return true;
    }
    if (h.code >= 300 && h.code < 400 && h.loc && ghTakeLocation()) continue;
    otaSet("error", "manifest http");
    return false;
  }
  otaSet("error", "too many redirects");
  return false;
}

static bool otaUrlFromParts() {
  size_t hn = strlen(g_ghHost);
  size_t pn = strlen(g_ghPath);
  if (!hn || g_ghPath[0] != '/') return false;
  // g_json is free: this runs from loop with no HTTP response in flight.
  // The signed URL has to live until startDownload copies it to the ESP32.
  if (8 + hn + pn + 1 > sizeof g_json) return false;
  memcpy(g_json, "https://", 8);
  memcpy(g_json + 8, g_ghHost, hn);
  memcpy(g_json + 8 + hn, g_ghPath, pn + 1);
  return true;
}

static bool otaAtLineFits(const char* url) {
  if (!url || strchr(url, ',')) return false;
  size_t n = strlen("AT+OTADOWNLOADSTART=") + strlen(url) + strlen(",/update.bin\r\n");
  return n < OTA_AT_LINE_MAX;
}

static bool otaUartWrite(const uint8_t* p, size_t n) {
  size_t off = 0;
  uint32_t idle = millis();
  while (off < n) {
    wdtPet();
    if ((uint32_t)(millis() - idle) > 2000u) return false;
    int room = Serial2.availableForWrite();
    if (room <= 0) {
      delay(1);
      continue;
    }
    size_t take = n - off;
    if (take > (size_t)room) take = (size_t)room;
    if (take > 32u) take = 32u;
    size_t wrote = Serial2.write(p + off, take);
    if (wrote == 0) {
      delay(1);
      continue;
    }
    off += wrote;
    idle = millis();
  }
  return true;
}

// startDownload's reply is +OTADOWNLOADSTART:<content-length> then OK.
// A negative number is an Arduino_ESP32_OTA::Error. No reply is the same
// -26 the library returns when modem.write fails.
static int otaReadStartSize(uint32_t waitMs) {
  char win[64];
  uint8_t n = 0;
  win[0] = 0;
  const char* key = "+OTADOWNLOADSTART:";
  uint32_t start = millis();
  while ((uint32_t)(millis() - start) < waitMs) {
    wdtPet();
    if (Serial2.available() <= 0) {
      delay(2);
      continue;
    }
    char c = (char)Serial2.read();
    if (n + 1 >= sizeof win) {
      memmove(win, win + 1, sizeof win - 2);
      n = (uint8_t)(sizeof win - 2);
    }
    win[n++] = c;
    win[n] = 0;
    char* p = strstr(win, key);
    if (!p) continue;
    char* nl = strchr(p, '\n');
    if (!nl) continue;
    p += strlen(key);
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
  }
  return (int)OTAUpdate::Error::Modem;
}

static int otaStartDownload(const char* url) {
  size_t line = strlen("AT+OTADOWNLOADSTART=") + strlen(url) + strlen(",/update.bin\r\n");
  Serial.print(F("OTA at "));
  Serial.println((unsigned)line);
  serial2Drain(40);
  if (!otaUartWrite((const uint8_t*)"AT+OTADOWNLOADSTART=", strlen("AT+OTADOWNLOADSTART=")) ||
      !otaUartWrite((const uint8_t*)url, strlen(url)) ||
      !otaUartWrite((const uint8_t*)",/update.bin\r\n", strlen(",/update.bin\r\n"))) {
    Serial.println(F("OTA at write"));
    return (int)OTAUpdate::Error::Modem;
  }
  // The bridge connects and reads the HTTP headers before it answers.
  int sz = otaReadStartSize(60000);
  serial2Drain(80);
  return sz;
}

// Header-only. Stop on the first host that is not github.com: that Location
// is the signed release-asset URL, which answers 200. OTAUpdate's
// startDownload treats any other status as an error.
static bool ghResolveAsset() {
  char rel[96];
  snprintf(rel, sizeof rel,
           "/tbellenger/button-lamp/releases/download/matrixlamp-latest/%s", g_ghFile);
  strncpy(g_ghHost, "github.com", sizeof g_ghHost);
  for (int hop = 0; hop < 4; hop++) {
    char pathBuf[1400];
    if (hop == 0) {
      strncpy(pathBuf, rel, sizeof pathBuf - 1);
      pathBuf[sizeof pathBuf - 1] = 0;
    } else {
      memcpy(pathBuf, g_ghPath, sizeof pathBuf);
    }
    GhHead h;
    if (!ghOnce(pathBuf, 0, 0, &h, 1)) return false;
    if (h.code >= 300 && h.code < 400 && h.loc && ghTakeLocation()) {
      if (strcmp(g_ghHost, "github.com") != 0) return otaUrlFromParts();
      continue;
    }
    if (h.code == 200) return otaUrlFromParts();
    otaSet("error", "download http");
    return false;
  }
  otaSet("error", "too many redirects");
  return false;
}

static void otaFailPrinted(const char* msg) {
  Serial.print(F("OTA error "));
  Serial.println(msg);
  otaIdle("error", msg);
}

static void otaFailCode(const char* msg, int code) {
  Serial.print(F("OTA error "));
  Serial.print(msg);
  Serial.print(' ');
  Serial.println(code);
  otaIdle("error", msg);
}

static void otaBridgeRecover() {
  // verify/update can leave the UART in the BOSSA session. AT+RESET is
  // the same recovery boot uses. The page comes back on the station IP.
  Serial.println(F("OTA bridge recover"));
  g_otaActive = 0;
  g_otaPhase = 0;
  wifiResetModem();
  wifiDrop(millis());
  if (!g_setupAp && g_creds.ssid[0]) wifiBegin();
}

static void otaFinishDownload() {
  g_otaPhase = 0;
  g_otaPct = 100;
  Serial.println(F("OTA verify"));
  otaSet("applying", "verify");
  int v = g_otaUpd.verify();
  modem.timeout(MODEM_CMD_BUDGET_MS);
  if (v != OTAUpdate::OTA_ERROR_NONE) {
    otaFailCode("verify failed", v);
    otaBridgeRecover();
    return;
  }
  if (!savePrefs()) Serial.println(F("Prefs save failed"));
  Serial.println(F("OTA update"));
  otaSet("applying", "rebooting");
  int u = g_otaUpd.update(OTA_SPIFFS_NAME);
  // A successful update resets the RA4M1 from the ESP32 and does not return.
  modem.timeout(MODEM_CMD_BUDGET_MS);
  otaFailCode("update failed", u);
  otaBridgeRecover();
}

static void otaPoll(uint32_t now) {
  if (g_otaPhase != 1) return;
  if ((uint32_t)(now - g_otaPollAt) < 400) return;
  g_otaPollAt = now;
  if (g_http != PH_IDLE) return;
  int p = g_otaUpd.downloadProgress();
  modem.timeout(MODEM_CMD_BUDGET_MS);
  if (p == -26) return;
  if (p < 0) {
    otaFailCode("download failed", p);
    otaBridgeRecover();
    return;
  }
  if (p != g_otaProg) {
    g_otaProg = p;
    g_otaStallAt = now;
    int pct = g_otaExpect > 0 ? (int)(((long)p * 100L) / g_otaExpect) : 0;
    if (pct < 0) pct = 0;
    if (pct > 99) pct = 99;
    if (pct != g_otaPct) {
      g_otaPct = pct;
      Serial.print(F("OTA progress "));
      Serial.print(p);
      Serial.print('/');
      Serial.println(g_otaExpect);
    }
  } else if ((uint32_t)(now - g_otaStallAt) > 30000u) {
    otaFailPrinted("stalled");
    otaBridgeRecover();
    return;
  }
  if (g_otaExpect > 0 && p >= g_otaExpect) otaFinishDownload();
}

static void otaRunInstall() {
  g_otaActive = 1;
  g_otaPhase = 0;
  otaSet("check", "checking");
  Serial.println(F("OTA check"));
  modem.timeout(20000);
  WiFi.setTimeout(20000);
  if (!ghFetchManifest()) {
    if (!g_otaMsg[0]) otaSet("error", "github");
    Serial.print(F("OTA error "));
    Serial.println(g_otaMsg);
    otaIdle("error", g_otaMsg);
    return;
  }
  char local[28];
  char remote[28];
  fwVersionNorm(FW_VERSION, local, sizeof local);
  fwVersionNorm(g_ghVer, remote, sizeof remote);
  int cmp = fwVersionCmp(FW_VERSION, g_ghVer);
  Serial.print(F("OTA version "));
  Serial.print(g_ghVer);
  Serial.print(F(" running "));
  Serial.print(FW_VERSION);
  Serial.print(F(" cmp "));
  Serial.println(cmp);
  if (!fwShouldInstall(FW_VERSION, g_ghVer)) {
    char msg[40];
    snprintf(msg, sizeof msg, "Up to date (v%s)", local[0] ? local : remote);
    g_ghNewer = 0;
    Serial.println(F("OTA up to date"));
    otaIdle("checked", msg);
    return;
  }
  g_ghNewer = 1;
  {
    char msg[40];
    snprintf(msg, sizeof msg, "Update available v%s", remote);
    otaSet("download", msg);
  }
  if (!ghResolveAsset() || !g_json[0]) {
    if (!g_otaMsg[0]) otaSet("error", "download http");
    Serial.print(F("OTA error "));
    Serial.println(g_otaMsg);
    otaIdle("error", g_otaMsg);
    return;
  }
  if (!otaAtLineFits(g_json)) {
    Serial.println(F("OTA error url too long"));
    otaIdle("error", "url too long");
    return;
  }
  Serial.print(F("OTA url "));
  Serial.print(g_ghHost);
  Serial.print(' ');
  Serial.println((unsigned)strlen(g_json));
  int begun = g_otaUpd.begin(OTA_SPIFFS_NAME);
  modem.timeout(MODEM_CMD_BUDGET_MS);
  if (begun != OTAUpdate::OTA_ERROR_NONE) {
    otaFailCode("begin failed", begun);
    otaBridgeRecover();
    return;
  }
  Serial.println(F("OTA begin"));
  int cert = g_otaUpd.setCACert(OTA_CA_PEM);
  modem.timeout(MODEM_CMD_BUDGET_MS);
  if (cert != OTAUpdate::OTA_ERROR_NONE) {
    otaFailCode("cert failed", cert);
    otaBridgeRecover();
    return;
  }
  int sz = otaStartDownload(g_json);
  otaModemRest();
  if (sz <= 0) {
    otaFailCode("download failed", sz);
    otaBridgeRecover();
    return;
  }
  if (sz > 400000) {
    otaFailPrinted("too big");
    otaBridgeRecover();
    return;
  }
  g_otaExpect = sz;
  g_otaProg = -1;
  g_otaPct = 0;
  g_otaPhase = 1;
  g_otaPollAt = millis();
  g_otaStallAt = millis();
  g_otaActive = 1;
  Serial.print(F("OTA download "));
  Serial.println(sz);
}

static void otaService(uint32_t now) {
  if (g_otaPhase == 1) {
    otaPoll(now);
    return;
  }
  if (!g_ghPend || g_http != PH_IDLE) return;
  g_ghPend = 0;
  otaRunInstall();
}
