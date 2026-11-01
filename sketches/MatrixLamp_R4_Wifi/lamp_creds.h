// WiFi and OTA password in the RA4M1 data flash (8 KB at 0x40100000).
// Code-flash OTA does not erase this. A USB build that compiled in secrets.h
// copies those values when they differ. A CI build has no secrets.h and keeps
// whatever is already stored.
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
  char ota[33];
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
  if (c->ssid[32] != 0 || c->pass[63] != 0 || c->ota[32] != 0) return false;
  return c->ssid[0] != 0;
}

static bool credsLoad() {
  LampCreds c;
  memcpy(&c, (const void*)LAMP_DF_ADDR, sizeof c);
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
  c.ota[32] = 0;
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

  if (R_FLASH_LP_Open(&ctrl, &cfg) != FSP_SUCCESS) return false;
  __disable_irq();
  fsp_err_t er = R_FLASH_LP_Erase(&ctrl, LAMP_DF_ADDR, 1);
  fsp_err_t wr = FSP_ERR_WRITE_FAILED;
  if (er == FSP_SUCCESS) wr = R_FLASH_LP_Write(&ctrl, (uint32_t)buf, LAMP_DF_ADDR, n);
  __enable_irq();
  R_FLASH_LP_Close(&ctrl);
  if (wr != FSP_SUCCESS) return false;
  g_creds = c;
  (void)LAMP_DF_BLOCK;
  return true;
}

#ifndef LAMP_COMPILED_SECRETS
#define LAMP_COMPILED_SECRETS 0
#endif

static void credsBoot() {
  bool ok = credsLoad();
  if (LAMP_COMPILED_SECRETS && WIFI_SSID[0]) {
    bool ssidDiff = !ok || strcmp(g_creds.ssid, WIFI_SSID) != 0;
    bool passDiff = !ok || strcmp(g_creds.pass, WIFI_PASS) != 0;
    bool otaDiff = OTA_PASSWORD[0] && (!ok || strcmp(g_creds.ota, OTA_PASSWORD) != 0);
    if (!ok || ssidDiff || passDiff || otaDiff) {
      LampCreds c;
      credsClear(&c);
      strncpy(c.ssid, WIFI_SSID, 32);
      strncpy(c.pass, WIFI_PASS, 63);
      if (OTA_PASSWORD[0]) strncpy(c.ota, OTA_PASSWORD, 32);
      else if (ok) strncpy(c.ota, g_creds.ota, 32);
      if (credsSave(c)) {
        ok = true;
        Serial.println(F("Stored WiFi from secrets.h"));
      } else {
        Serial.println(F("Could not store WiFi"));
      }
    }
  }
  if (g_forceSetup || !ok) g_setupAp = 1;
}

static bool passwordEq(const char* got, const char* want) {
  if (!want || !want[0]) return false;
  size_t n = strlen(want);
  size_t m = got ? strlen(got) : 0;
  uint8_t diff = (uint8_t)(n ^ m);
  size_t lim = n > m ? n : m;
  if (lim > 64) lim = 64;
  for (size_t i = 0; i < lim; i++) {
    char a = i < n ? want[i] : 0;
    char b = (got && i < m) ? got[i] : 0;
    diff = (uint8_t)(diff | (uint8_t)(a ^ b));
  }
  return diff == 0 && n == m;
}
