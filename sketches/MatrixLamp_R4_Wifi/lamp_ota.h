// WiFi OTA for the Uno R4 WiFi.
// The image is staged with ArduinoOTA's InternalStorage (Renesas code flash).
// The IDE network port is not started (NO_OTA_NETWORK): this sketch already
// has an HTTP server and its own mDNS responder.
#pragma once

#include <WiFiSSLClient.h>
#include <string.h>
#include <stdlib.h>

static const char SETUP_PAGE[] =
  "<!DOCTYPE html><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
  "<title>Setup</title><h1>Matrix Lamp</h1>"
  "<p>WiFi is MatrixLamp-Setup. Saving restarts the lamp onto your network.</p>"
  "<form method=POST action=/setup>"
  "WiFi name<br><input name=ssid required maxlength=32><br>"
  "WiFi password<br><input name=pass type=password maxlength=63><br>"
  "OTA password<br><input name=ota required maxlength=32><br>"
  "<button>Save</button></form>";

static const char GH_MANIFEST[] =
  "/tbellenger/button-lamp/releases/download/matrixlamp-latest/manifest.json";

static void otaFinishUpload();

extern "C" uint8_t __ROM_Start;

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

static void otaSolid(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = g_panel.Color(r, g, b);
  for (int i = 0; i < PANEL_N; i++) g_panel.setPixelColor((uint16_t)i, c);
  g_panel.show();
}

static void otaPaint(int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  if (pct == g_otaPaint) return;
  g_otaPaint = pct;
  g_otaPct = pct;
  int lit = (pct <= 0) ? 1 : (PANEL_N * pct) / 100;
  if (lit < 1) lit = 1;
  if (pct >= 100) lit = PANEL_N;
  g_panel.clear();
  for (int i = 0; i < lit; i++) {
    g_panel.setPixelColor(panelIndex(i % PANEL_W, i / PANEL_W), g_panel.Color(255, 80, 0));
  }
  g_panel.show();
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

static bool imageOk() {
  if (g_otaHeadN < 8 || g_otaGot != g_otaLen || g_otaFail) return false;
  if (g_otaHead[0] == 0x7f && g_otaHead[1] == 'E') return false;
  uint32_t sp = 0;
  uint32_t rh = 0;
  memcpy(&sp, g_otaHead, 4);
  memcpy(&rh, g_otaHead + 4, 4);
  uint32_t start = (uint32_t)&__ROM_Start;
  bool spOk = (sp & 3u) == 0 && sp >= 0x20000100u && sp <= 0x20008000u;
  uint32_t reset = rh & ~1u;
  bool rhOk = (rh & 1u) != 0 && reset >= start && reset < start + g_otaLen;
  return spOk && rhOk;
}

static void otaCloseStorage() {
  if (!g_otaOpened) return;
  InternalStorage.close();
  g_otaOpened = 0;
}

static void otaResumeLamp() {
  g_otaActive = 0;
  g_otaPaint = -1;
  matrixResume();
  g_dirty = 1;
}

static bool otaAuthed() {
  if (g_setupAp) {
    otaReply(409, "Conflict", "join the home wifi first\n");
    return false;
  }
  if (!g_creds.ota[0]) {
    otaReply(403, "Forbidden", "ota disabled\n");
    otaSet("error", "ota disabled");
    return false;
  }
  char pass[40];
  headerValue("X-OTA-Password", pass, sizeof pass);
  if (!passwordEq(pass, g_creds.ota)) {
    otaReply(401, "Unauthorized", "bad password\n");
    return false;
  }
  return true;
}

static void crcBegin() { g_crcState = 0xffffffffu; }

static void crcAdd(const uint8_t* p, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    g_crcState ^= p[i];
    for (int b = 0; b < 8; b++) {
      uint32_t mask = (uint32_t)-(int32_t)(g_crcState & 1u);
      g_crcState = (g_crcState >> 1) ^ (0xEDB88320u & mask);
    }
  }
}

static uint32_t crcEnd() { return ~g_crcState; }

static int hexNib(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool parseHex32(const char* s, uint32_t* out) {
  if (!s) return false;
  uint32_t v = 0;
  for (int i = 0; i < 8; i++) {
    int n = hexNib(s[i]);
    if (n < 0) return false;
    v = (v << 4) | (uint32_t)n;
  }
  if (s[8] != 0) return false;
  *out = v;
  return true;
}

static void otaConsume(const uint8_t* p, uint32_t n) {
  if (!n || g_otaFail) return;
  crcAdd(p, n);
  for (uint32_t i = 0; i < n; i++) {
    if (g_otaHeadN < 8) g_otaHead[g_otaHeadN++] = p[i];
    if (InternalStorage.write(p[i]) != 1) {
      g_otaFail = 1;
      return;
    }
    g_otaGot++;
  }
  int pct = g_otaLen ? (int)((g_otaGot * 100ul) / g_otaLen) : 0;
  otaPaint(pct);
}

static void otaFail(const char* msg) {
  otaCloseStorage();
  Serial.print(F("OTA rejected: "));
  Serial.println(msg);
  otaSet("error", msg);
  otaSolid(180, 0, 0);
  delay(1000);
  otaResumeLamp();
  otaReply(400, "Bad Request", msg);
}

// ArduinoOTA 1.1.1 apply() erases (pageAlignedLength / pageSize) + 1 blocks.
// pageAlignedLength is already a whole number of 2 KB blocks, so the extra
// block is the first block of the staged image (the vector table). The copy
// then resets into 0xFF and the panel stays on the last frame. Do not call it.
extern "C" fsp_err_t r_flash_lp_cf_erase(flash_lp_instance_ctrl_t* p_ctrl,
                                        uint32_t block_address,
                                        uint32_t num_blocks,
                                        uint32_t block_size);
extern "C" fsp_err_t r_flash_lp_cf_write(flash_lp_instance_ctrl_t* p_ctrl,
                                        uint32_t src,
                                        uint32_t dest,
                                        uint32_t num_bytes);

static bool otaCommit() {
  uint32_t start = (uint32_t)&__ROM_Start;
  uint32_t maxSketch = (262144u - start) / 2u;
  maxSketch = (maxSketch / 2048u) * 2048u;
  uint32_t storage = start + maxSketch;
  uint32_t pages = (g_otaLen / 2048u) + 1u;
  uint32_t bytes = pages * 2048u;
  if (pages == 0 || storage + bytes > 262144u) {
    Serial.println(F("OTA size"));
    return false;
  }
  Serial.print(F("OTA apply "));
  Serial.println(pages);
  otaSolid(255, 255, 255);
  flash_lp_instance_ctrl_t ctrl;
  flash_cfg_t cfg;
  memset(&ctrl, 0, sizeof ctrl);
  memset(&cfg, 0, sizeof cfg);
  cfg.data_flash_bgo = false;
  cfg.ipl = BSP_IRQ_DISABLED;
  cfg.irq = FSP_INVALID_VECTOR;
  cfg.err_ipl = BSP_IRQ_DISABLED;
  cfg.err_irq = FSP_INVALID_VECTOR;
  if (R_FLASH_LP_Open(&ctrl, &cfg) != FSP_SUCCESS) {
    Serial.println(F("OTA flash open"));
    return false;
  }
  __disable_irq();
  fsp_err_t rv = r_flash_lp_cf_erase(&ctrl, start, pages, 2048u);
  uint32_t off = 0;
  while (rv == FSP_SUCCESS && off < bytes) {
    uint8_t buf[128];
    const uint8_t* s = (const uint8_t*)(storage + off);
    for (int i = 0; i < 128; i++) buf[i] = s[i];
    rv = r_flash_lp_cf_write(&ctrl, (uint32_t)buf, start + off, 128u);
    off += 128u;
  }
  if (rv != FSP_SUCCESS || off != bytes) {
    __enable_irq();
    Serial.println(F("OTA copy failed"));
    __disable_irq();
  }
  NVIC_SystemReset();
  return false;
}

static void otaBeginUpload() {
  if (!otaAuthed()) return;
  long len = headerLong("Content-Length");
  long cap = InternalStorage.maxSize();
  if (len < 4096) {
    otaReply(400, "Bad Request", "short image\n");
    return;
  }
  if (len > cap) {
    otaReply(413, "Payload Too Large", "image too large\n");
    return;
  }
  g_wantCrc = 0;
  char crcHex[12];
  if (headerValue("X-OTA-Crc32", crcHex, sizeof crcHex) && crcHex[0]) {
    if (!parseHex32(crcHex, &g_crcExpect)) {
      otaReply(400, "Bad Request", "bad crc\n");
      return;
    }
    g_wantCrc = 1;
  }
  matrixPause();
  g_otaActive = 1;
  g_otaPaint = -1;
  otaSet("upload", "receiving");
  otaPaint(0);
  g_otaLen = (uint32_t)len;
  g_otaGot = 0;
  g_otaFail = 0;
  g_otaHeadN = 0;
  crcBegin();
  if (!InternalStorage.open((int)len)) {
    otaResumeLamp();
    otaSet("error", "flash open failed");
    otaReply(500, "Error", "flash open failed\n");
    return;
  }
  g_otaOpened = 1;
  Serial.print(F("OTA receive "));
  Serial.println(g_otaLen);
  const char* hdr = strstr(g_req, "\r\n\r\n");
  const uint8_t* body = (const uint8_t*)hdr + 4;
  uint32_t have = g_reqLen - (uint32_t)(body - (const uint8_t*)g_req);
  if (have > g_otaLen) have = g_otaLen;
  otaConsume(body, have);
  if (g_otaFail) {
    otaFail("write failed\n");
    return;
  }
  if (g_otaGot >= g_otaLen) otaFinishUpload();
  else {
    g_clientMs = millis();
    g_http = PH_OTA;
  }
}

static void otaFinishUpload() {
  uint32_t sum = crcEnd();
  otaCloseStorage();
  if (!imageOk()) {
    otaFail("bad image\n");
    return;
  }
  if (g_wantCrc && sum != g_crcExpect) {
    otaFail("bad crc\n");
    return;
  }
  otaSet("applying", "rebooting");
  Serial.println(F("OTA stored"));
  g_otaApply = 1;
  otaReply(200, "OK", "ok\n");
}

static void otaHttpRead(uint32_t now) {
  if (!g_client.connected() && g_client.available() <= 0) {
    otaFail("incomplete\n");
    return;
  }
  int avail = g_client.available();
  if (avail <= 0) {
    if ((uint32_t)(now - g_clientMs) > 20000) otaFail("timeout\n");
    return;
  }
  uint8_t buf[128];
  if (avail > (int)sizeof buf) avail = (int)sizeof buf;
  int n = g_client.read(buf, (size_t)avail);
  if (n <= 0) return;
  g_clientMs = now;
  uint32_t left = g_otaLen - g_otaGot;
  if ((uint32_t)n > left) n = (int)left;
  otaConsume(buf, (uint32_t)n);
  if (g_otaFail) {
    otaFail("write failed\n");
    return;
  }
  if (g_otaGot >= g_otaLen) otaFinishUpload();
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
  char ota[33];
  if (!formGet(hdr + 4, "ssid", ssid, sizeof ssid) || !ssid[0]) {
    otaReply(400, "Bad Request", "ssid required\n");
    return;
  }
  if (!formGet(hdr + 4, "pass", pass, sizeof pass)) pass[0] = 0;
  if (!formGet(hdr + 4, "ota", ota, sizeof ota) || !ota[0]) {
    otaReply(400, "Bad Request", "ota password required\n");
    return;
  }
  matrixPause();
  LampCreds c;
  credsClear(&c);
  strncpy(c.ssid, ssid, 32);
  strncpy(c.pass, pass, 63);
  strncpy(c.ota, ota, 32);
  if (!credsSave(c)) {
    matrixResume();
    otaReply(500, "Error", "save failed\n");
    return;
  }
  Serial.println(F("Setup saved"));
  g_rebootAfter = 1;
  otaReply(200, "OK", "Saved. Restarting.\n");
}

static void otaQueueGithub(uint8_t job) {
  if (!otaAuthed()) return;
  g_ghJob = job;
  otaSet(job == 1 ? "check" : "download", "starting");
  otaReply(200, "OK", job == 1 ? "ok check\n" : "ok apply\n");
}

// Returns 0 while the body is still arriving, 1 when a response was queued.
static int otaDispatchPost(uint32_t now) {
  char path[48];
  if (!reqPath(path, sizeof path)) {
    otaReply(400, "Bad Request", "path\n");
    return 1;
  }
  bool stream = strcmp(path, "/update") == 0;
  bool small = strcmp(path, "/setup") == 0 || strcmp(path, "/api/ota/check") == 0 ||
               strcmp(path, "/api/ota/apply") == 0;
  if (!stream && !small) {
    otaReply(404, "Not Found", "not found\n");
    return 1;
  }
  if (stream) {
    otaBeginUpload();
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
  else if (strcmp(path, "/api/ota/check") == 0) otaQueueGithub(1);
  else otaQueueGithub(2);
  return 1;
}

static bool bothHeld() {
  uint32_t t = millis();
  while ((uint32_t)(millis() - t) < 500) {
    if (digitalRead(PIN_RED) != LOW || digitalRead(PIN_WHITE) != LOW) return false;
    delay(10);
  }
  return true;
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
  Serial.print(F("Open http://"));
  Serial.println(ip);
}

struct GhHead {
  int code;
  long length;
  uint8_t chunked;
  uint8_t loc;
};

static WiFiSSLClient& ghClient() {
  static WiFiSSLClient c;
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

static int ghReadResponse(WiFiSSLClient& c, GhHead* h,
                          int (*onHead)(GhHead*), int (*onBody)(const uint8_t*, int)) {
  h->code = 0;
  h->length = -1;
  h->chunked = 0;
  h->loc = 0;
  g_ghPath[0] = 0;
  char line[96];
  int ln = 0;
  int cap = 0;
  int locN = 0;
  int phase = 0;
  long got = 0;
  uint32_t idle = millis();
  // Return 1 when the body is complete, 2 when the socket stalls.
  // WiFiSSLClient::connected() stays true after GitHub closes, and available()
  // blocks inside a modem command, so do not wait on EOF past Content-Length.
  while ((uint32_t)(millis() - idle) < 8000) {
    if (phase == 1 && h->length >= 0 && got >= h->length) return 1;
    if (c.available() <= 0) {
      delay(5);
      continue;
    }
    uint8_t buf[128];
    int a = c.available();
    if (a > (int)sizeof buf) a = (int)sizeof buf;
    int n = c.read(buf, (size_t)a);
    if (n <= 0) continue;
    idle = millis();
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
          }
          if (ln == 0) {
            phase = 1;
            if (onHead && !onHead(h)) return 0;
            // A real length wins. Chunked with no length cannot be stored safely.
            if (h->chunked && h->length < 0) return 0;
            if (h->code >= 300 && h->code < 400) return 1;
            if (h->length == 0) return 1;
            i++;
            break;
          }
          if (h->code == 0) {
            const char* sp = strchr(line, ' ');
            if (sp) h->code = atoi(sp + 1);
          } else if (ciStarts(line, "Content-Length:")) {
            h->length = strtol(line + 15, 0, 10);
          } else if (ciStarts(line, "Transfer-Encoding:") && strstr(line, "chunked")) {
            h->chunked = 1;
          }
          ln = 0;
        } else if (cap) {
          if (b != '\r') {
            if (locN >= (int)sizeof g_ghPath - 1) return 0;
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
        if (onBody && !onBody(buf + i, bn)) return 0;
        got += bn;
      }
    }
  }
  if (phase == 1 && h->length >= 0 && got >= h->length) return 1;
  Serial.println(F("OTA body timeout"));
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

static bool ghOnce(const char* path, int (*onHead)(GhHead*), int (*onBody)(const uint8_t*, int), GhHead* h) {
  strncpy(g_ghPath, path, sizeof g_ghPath - 1);
  g_ghPath[sizeof g_ghPath - 1] = 0;
  WiFiSSLClient& c = ghClient();
  c.stop();
  if (!c.connect(g_ghHost, 443)) {
    otaSet("error", "tls connect failed");
    return false;
  }
  bool sent = ghSend(c, "GET ") && ghSend(c, g_ghPath) &&
              ghSend(c, " HTTP/1.0\r\nHost: ") && ghSend(c, g_ghHost) &&
              ghSend(c, "\r\nUser-Agent: MatrixLamp\r\nConnection: close\r\n\r\n");
  // Short modem timeout: a stalled SSL read must not sit forever.
  modem.timeout(1500);
  int rd = sent ? ghReadResponse(c, h, onHead, onBody) : 0;
  modem.timeout(400);
  c.stop();
  modem.timeout(20000);
  if (rd == 2) {
    otaSet("error", "timeout");
    otaSolid(0, 0, 160);
    delay(1000);
    return false;
  }
  if (rd != 1) {
    if (!g_otaMsg[0]) otaSet("error", "github read failed");
    return false;
  }
  if (h->chunked) {
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
  if (g_manifestN + n >= (int)sizeof g_manifest) return 0;
  memcpy(g_manifest + g_manifestN, p, (size_t)n);
  g_manifestN += n;
  g_manifest[g_manifestN] = 0;
  return 1;
}

static int manifestHead(GhHead* h) {
  if (h->code != 200) return 1;
  if (h->length < 2 || h->length >= (long)sizeof g_manifest) return 0;
  return 1;
}

static bool ghFetchManifest() {
  strncpy(g_ghHost, "github.com", sizeof g_ghHost);
  g_manifestN = 0;
  g_manifest[0] = 0;
  g_otaMsg[0] = 0;
  for (int hop = 0; hop < 4; hop++) {
    GhHead h;
    const char* path = (hop == 0) ? GH_MANIFEST : g_ghPath;
    // On hop 0 the path is the constant. ghOnce copies it.
    // On later hops g_ghPath already holds the redirect path, but ghOnce
    // also copies its path argument into g_ghPath. Pass the constant only
    // for hop 0; afterwards the location parser has filled g_ghPath, so
    // copy it aside first.
    char pathBuf[1400];
    if (hop == 0) {
      strncpy(pathBuf, path, sizeof pathBuf - 1);
      pathBuf[sizeof pathBuf - 1] = 0;
    } else {
      memcpy(pathBuf, g_ghPath, sizeof pathBuf);
    }
    if (!ghOnce(pathBuf, manifestHead, manifestBody, &h)) return false;
    if (h.code == 200) {
      g_ghVer[0] = 0;
      g_ghFile[0] = 0;
      g_ghHave = 0;
      g_ghSize = jsonLong(g_manifest, "size");
      if (!jsonString(g_manifest, "version", g_ghVer, sizeof g_ghVer)) {
        otaSet("error", "bad manifest");
        return false;
      }
      char crcHex[12];
      if (!jsonString(g_manifest, "crc32", crcHex, sizeof crcHex) || !parseHex32(crcHex, &g_ghCrc)) {
        otaSet("error", "bad manifest");
        return false;
      }
      g_ghHave = 1;
      if (!jsonString(g_manifest, "file", g_ghFile, sizeof g_ghFile)) {
        strncpy(g_ghFile, "MatrixLamp_R4_Wifi.ino.bin", sizeof g_ghFile - 1);
      }
      for (char* s = g_ghFile; *s; s++) {
        char c = *s;
        bool okc = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!okc) {
          otaSet("error", "bad manifest");
          return false;
        }
      }
      if (g_ghSize < 4096) {
        otaSet("error", "bad manifest");
        return false;
      }
      if (strcmp(g_ghVer, FW_VERSION) == 0) otaSet("checked", "up to date");
      else otaSet("checked", "update available");
      Serial.print(F("GitHub firmware "));
      Serial.println(g_ghVer);
      return true;
    }
    if (h.code >= 300 && h.code < 400 && h.loc && ghTakeLocation()) continue;
    otaSet("error", "manifest http");
    return false;
  }
  otaSet("error", "too many redirects");
  return false;
}

static int binHead(GhHead* h) {
  if (h->code != 200) return 1;
  if (h->length != g_ghSize || h->length > InternalStorage.maxSize()) {
    otaSet("error", "size mismatch");
    return 0;
  }
  g_otaLen = (uint32_t)h->length;
  g_otaGot = 0;
  g_otaFail = 0;
  g_otaHeadN = 0;
  crcBegin();
  otaPaint(0);
  if (!InternalStorage.open((int)h->length)) {
    otaSet("error", "flash open failed");
    return 0;
  }
  g_otaOpened = 1;
  return 1;
}

static int binBody(const uint8_t* p, int n) {
  if (g_otaFail || !g_otaOpened) return 0;
  if ((uint32_t)n > g_otaLen - g_otaGot) n = (int)(g_otaLen - g_otaGot);
  otaConsume(p, (uint32_t)n);
  return g_otaFail ? 0 : 1;
}

static bool ghFetchBin() {
  strncpy(g_ghHost, "github.com", sizeof g_ghHost);
  snprintf(g_ghPath, sizeof g_ghPath,
           "/tbellenger/button-lamp/releases/download/matrixlamp-latest/%s", g_ghFile);
  g_otaMsg[0] = 0;
  for (int hop = 0; hop < 4; hop++) {
    char pathBuf[1400];
    memcpy(pathBuf, g_ghPath, sizeof pathBuf);
    GhHead h;
    if (!ghOnce(pathBuf, binHead, binBody, &h)) return false;
    if (h.code == 200) {
      if (g_otaGot != g_otaLen) {
        otaSet("error", "incomplete");
        return false;
      }
      uint32_t sum = crcEnd();
      otaCloseStorage();
      if (!imageOk()) {
        otaSet("error", "bad image");
        Serial.println(F("OTA bad image"));
        otaSolid(180, 0, 0);
        delay(1000);
        return false;
      }
      if (sum != g_ghCrc) {
        otaSet("error", "bad crc");
        Serial.print(F("OTA bad crc "));
        Serial.println(sum, HEX);
        otaSolid(180, 0, 0);
        delay(1000);
        return false;
      }
      otaSet("applying", "rebooting");
      if (!otaCommit()) {
        otaSet("error", "apply failed");
        otaSolid(180, 0, 0);
        delay(1000);
        return false;
      }
      return false;
    }
    if (h.code >= 300 && h.code < 400 && h.loc && ghTakeLocation()) continue;
    otaSet("error", "download http");
    return false;
  }
  otaSet("error", "too many redirects");
  return false;
}

static void githubRun(uint8_t job) {
  matrixPause();
  g_otaActive = 1;
  g_otaPaint = -1;
  otaPaint(0);
  modem.timeout(20000);
  WiFi.setTimeout(20000);
  bool ok = false;
  if (job == 1) ok = ghFetchManifest();
  else {
    otaSet("download", "github");
    if (!g_ghFile[0] || !g_ghHave || g_ghSize < 4096) ok = ghFetchManifest();
    else ok = true;
    if (ok && strcmp(g_ghVer, FW_VERSION) == 0) {
      otaSet("checked", "up to date");
      ok = false;
    } else if (ok) {
      ok = ghFetchBin();
    }
  }
  WiFi.setTimeout(0);
  modem.timeout(MODEM_CMD_BUDGET_MS);
  otaCloseStorage();
  if (!ok) {
    if (strcmp(g_otaState, "checked") != 0) {
      if (!g_otaMsg[0]) otaSet("error", "github");
      else if (strcmp(g_otaState, "error") != 0) otaSet("error", g_otaMsg);
    }
    Serial.print(F("GitHub: "));
    Serial.println(g_otaMsg);
    otaResumeLamp();
    return;
  }
  // apply() resets on success. Reaching here means the check finished.
  otaResumeLamp();
}
