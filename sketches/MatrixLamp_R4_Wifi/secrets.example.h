// Copy this file to secrets.h (same folder) and fill it in.
// secrets.h is gitignored. Do not commit real WiFi credentials or the OTA password.
//
// A USB build writes these into the lamp's data flash when they differ from
// what is already stored. A firmware file built by GitHub Actions has no
// secrets.h, so it keeps the WiFi name and OTA password already on the lamp.
// Leave OTA_PASSWORD empty to keep an OTA password that is already stored.
// An empty OTA password, and none stored yet, makes the lamp refuse updates.
#pragma once

#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
#define OTA_PASSWORD ""
