// WiFi name and password in the RA4M1 data flash (8 KB at 0x40100000).
// The credential block is the first 1 KB, at 0x40100000. Preferences are
// the next block, at 0x40100400. A code-flash update does not erase either.
// A USB build that compiled in secrets.h copies those values when they
// differ. A CI build has no secrets.h and keeps whatever is already stored.
//
// This does not use the EEPROM library. That library links the whole
// BlockDevice filesystem and blows the OTA size limit.
#pragma once

#include <r_flash_lp.h>
#include <stddef.h>
#include <string.h>

static const uint32_t LAMP_CREDS_MAGIC = 0x31434C4Du; // bytes 'M','L','C','1'
static const uint16_t LAMP_CREDS_VER = 1;
static const uint32_t LAMP_DF_ADDR = 0x40100000u;
static const uint32_t LAMP_DF_BLOCK = 0x400u;

struct LampCreds {
  uint32_t magic;
  uint16_t ver;
  uint16_t reserved;
  char ssid[33];
  char pass[64];
  // 33 bytes kept so records written by older firmware still validate.
  // Nothing reads them.
  char unused[33];
  uint32_t crc;
};

static LampCreds g_creds;
static uint8_t g_setupAp = 0;
static uint8_t g_forceSetup = 0;

static uint32_t lampCrc32(const void* data, size_t n) {
  const uint8_t* p = (const uint8_t*)data;
  uint32_t c = 0xffffffffu;
  for (size_t i = 0; i < n; i++) {
    c ^= p[i];
    for (int b = 0; b < 8; b++) {
      uint32_t mask = (uint32_t)-(int32_t)(c & 1u);
      c = (c >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~c;
}

static uint32_t credsCrc(const LampCreds* c) {
  return lampCrc32(c, offsetof(LampCreds, crc));
}

static void credsClear(LampCreds* c) {
  memset(c, 0, sizeof *c);
  c->magic = LAMP_CREDS_MAGIC;
  c->ver = LAMP_CREDS_VER;
}

static bool credsValid(const LampCreds* c) {
  if (c->magic != LAMP_CREDS_MAGIC || c->ver != LAMP_CREDS_VER) return false;
  if (c->crc != credsCrc(c)) return false;
  if (c->ssid[32] != 0 || c->pass[63] != 0 || c->unused[32] != 0) return false;
  return c->ssid[0] != 0;
}

// Data flash is read a byte at a time. A wide load can return a stale
// value from the flash buffer right after a program.
static void dfRead(uint32_t addr, void* dst, size_t n) {
  const volatile uint8_t* s = (const volatile uint8_t*)addr;
  uint8_t* d = (uint8_t*)dst;
  for (size_t i = 0; i < n; i++) d[i] = s[i];
}

static bool credsLoad() {
  LampCreds c;
  dfRead(LAMP_DF_ADDR, &c, sizeof c);
  if (!credsValid(&c)) return false;
  g_creds = c;
  return true;
}

static bool credsSave(const LampCreds& in) {
  LampCreds c = in;
  c.magic = LAMP_CREDS_MAGIC;
  c.ver = LAMP_CREDS_VER;
  c.ssid[32] = 0;
  c.pass[63] = 0;
  c.unused[32] = 0;
  c.crc = credsCrc(&c);

  uint8_t buf[160];
  memset(buf, 0xff, sizeof buf);
  memcpy(buf, &c, sizeof c);
  uint32_t n = (sizeof c + 3u) & ~3u;

  flash_lp_instance_ctrl_t ctrl;
  flash_cfg_t cfg;
  memset(&ctrl, 0, sizeof ctrl);
  memset(&cfg, 0, sizeof cfg);
  cfg.data_flash_bgo = false;
  cfg.ipl = BSP_IRQ_DISABLED;
  cfg.irq = FSP_INVALID_VECTOR;
  cfg.err_ipl = BSP_IRQ_DISABLED;
  cfg.err_irq = FSP_INVALID_VECTOR;

  // Program twice if the first image does not read back. A success code
  // from the flash controller is not enough: the portal used to say
  // "Saved" when the next boot still found a blank block and opened the AP.
  for (uint8_t attempt = 0; attempt < 2; attempt++) {
    if (R_FLASH_LP_Open(&ctrl, &cfg) != FSP_SUCCESS) return false;
    __disable_irq();
    fsp_err_t er = R_FLASH_LP_Erase(&ctrl, LAMP_DF_ADDR, 1);
    fsp_err_t wr = FSP_ERR_WRITE_FAILED;
    if (er == FSP_SUCCESS) wr = R_FLASH_LP_Write(&ctrl, (uint32_t)buf, LAMP_DF_ADDR, n);
    __enable_irq();
    R_FLASH_LP_Close(&ctrl);
    if (wr != FSP_SUCCESS) continue;
    delay(2);
    LampCreds back;
    dfRead(LAMP_DF_ADDR, &back, sizeof back);
    if (memcmp(&back, &c, sizeof c) == 0 && credsValid(&back)) {
      g_creds = c;
      (void)LAMP_DF_BLOCK;
      return true;
    }
  }
  return false;
}

#ifndef LAMP_COMPILED_SECRETS
#define LAMP_COMPILED_SECRETS 0
#endif

static void credsBoot() {
  bool ok = credsLoad();
  if (LAMP_COMPILED_SECRETS && WIFI_SSID[0]) {
    bool ssidDiff = !ok || strcmp(g_creds.ssid, WIFI_SSID) != 0;
    bool passDiff = !ok || strcmp(g_creds.pass, WIFI_PASS) != 0;
    if (!ok || ssidDiff || passDiff) {
      LampCreds c;
      credsClear(&c);
      strncpy(c.ssid, WIFI_SSID, 32);
      strncpy(c.pass, WIFI_PASS, 63);
      if (credsSave(c)) {
        ok = true;
        Serial.println(F("Stored WiFi from secrets.h"));
      } else {
        Serial.println(F("Could not store WiFi"));
      }
    }
  }
  if (ok) {
    Serial.print(F("WiFi creds "));
    Serial.println(g_creds.ssid);
  } else {
    Serial.println(F("WiFi creds missing"));
  }
  if (g_forceSetup || !ok) g_setupAp = 1;
}

// UI prefs live in the next 1 KB data-flash block. The credential block
// at LAMP_DF_ADDR is never erased by this.
static const uint32_t LAMP_PREFS_MAGIC_V2 = 0x32504C4Du; // 'M','L','P','2'
static const uint32_t LAMP_PREFS_MAGIC = 0x33504C4Du; // 'M','L','P','3'
static const uint32_t LAMP_PREFS_ADDR = LAMP_DF_ADDR + LAMP_DF_BLOCK;

struct LampPrefsV2 {
  uint32_t magic;
  uint8_t mode;
  uint8_t fader;
  uint8_t speed;
  uint8_t balls;
  uint32_t crc;
};

struct LampPrefs {
  uint32_t magic;
  uint8_t mode;
  uint8_t fader;
  uint8_t speed;
  uint8_t balls; // 1..8, or 0 when this byte was never written
  uint8_t autoRadar;
  uint8_t pad0;
  uint16_t radarPort; // 0 means the compiled default
  char host[40];
  char path[32];
  uint32_t crc;
};

static_assert(sizeof(LampPrefs) == 88, "prefs size");
static_assert(offsetof(LampPrefs, crc) == 84, "prefs crc");

static bool prefsLoad(uint8_t* mode, uint8_t* fader, uint8_t* speed, uint8_t* balls,
                      uint8_t* autoRadar, uint16_t* port, char* host, size_t hostN,
                      char* path, size_t pathN) {
  LampPrefs p;
  // Byte at a time, same as the credential block. A wide memcpy reads
  // the data-flash buffer and returns stale bytes, so a valid record
  // looks blank and the lamp boots into Fire.
  dfRead(LAMP_PREFS_ADDR, &p, sizeof p);
  if (hostN) host[0] = 0;
  if (pathN) path[0] = 0;
  *autoRadar = 0;
  *port = 0;
  if (p.magic == LAMP_PREFS_MAGIC &&
      p.crc == lampCrc32(&p, offsetof(LampPrefs, crc)) &&
      p.host[sizeof p.host - 1] == 0 && p.path[sizeof p.path - 1] == 0) {
    *mode = p.mode;
    *fader = p.fader;
    *speed = p.speed;
    *balls = p.balls;
    *autoRadar = p.autoRadar;
    *port = p.radarPort;
    if (host && hostN) {
      strncpy(host, p.host, hostN - 1);
      host[hostN - 1] = 0;
    }
    if (path && pathN) {
      strncpy(path, p.path, pathN - 1);
      path[pathN - 1] = 0;
    }
    return true;
  }
  LampPrefsV2 old;
  memcpy(&old, &p, sizeof old);
  if (old.magic != LAMP_PREFS_MAGIC_V2) return false;
  if (old.crc != lampCrc32(&old, offsetof(LampPrefsV2, crc))) return false;
  *mode = old.mode;
  *fader = old.fader;
  *speed = old.speed;
  *balls = old.balls;
  return true;
}

static bool prefsSave(uint8_t mode, uint8_t fader, uint8_t speed, uint8_t balls,
                      uint8_t autoRadar, uint16_t port, const char* host, const char* path) {
  LampPrefs p;
  memset(&p, 0, sizeof p);
  p.magic = LAMP_PREFS_MAGIC;
  p.mode = mode;
  p.fader = fader;
  p.speed = speed;
  p.balls = balls;
  // 0 off, 2 explicit on. Never store 1: that was the old implicit default.
  p.autoRadar = autoRadar ? 2 : 0;
  p.radarPort = port;
  if (host) strncpy(p.host, host, sizeof p.host - 1);
  if (path) strncpy(p.path, path, sizeof p.path - 1);
  p.crc = lampCrc32(&p, offsetof(LampPrefs, crc));

  uint8_t buf[96];
  memset(buf, 0xff, sizeof buf);
  memcpy(buf, &p, sizeof p);
  uint32_t n = (sizeof p + 3u) & ~3u;

  flash_lp_instance_ctrl_t ctrl;
  flash_cfg_t cfg;
  memset(&ctrl, 0, sizeof ctrl);
  memset(&cfg, 0, sizeof cfg);
  cfg.data_flash_bgo = false;
  cfg.ipl = BSP_IRQ_DISABLED;
  cfg.irq = FSP_INVALID_VECTOR;
  cfg.err_ipl = BSP_IRQ_DISABLED;
  cfg.err_irq = FSP_INVALID_VECTOR;

  if (R_FLASH_LP_Open(&ctrl, &cfg) != FSP_SUCCESS) return false;
  __disable_irq();
  fsp_err_t er = R_FLASH_LP_Erase(&ctrl, LAMP_PREFS_ADDR, 1);
  fsp_err_t wr = FSP_ERR_WRITE_FAILED;
  if (er == FSP_SUCCESS) wr = R_FLASH_LP_Write(&ctrl, (uint32_t)buf, LAMP_PREFS_ADDR, n);
  __enable_irq();
  R_FLASH_LP_Close(&ctrl);
  return wr == FSP_SUCCESS;
}
