// Copy this file to secrets.h (same folder) and fill it in.
// secrets.h is gitignored. Do not commit real WiFi credentials.
//
// A USB build writes these into the lamp's data flash when they differ from
// what is already stored. A firmware file built by GitHub Actions has no
// secrets.h, so it keeps the WiFi name already on the lamp.
#pragma once

#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"

// Optional. The bridge's OTA client drops the query string on a signed
// GitHub asset URL, so Install update needs a URL that answers 200 with
// Content-Length and no redirect. A plain-HTTP file on the lamp's API
// host is the one that fits: http://api.drunkwallaby.com/matrixlamp/MatrixLamp_R4_Wifi.ino.ota
// Leave this unset until that file exists. A GitHub Actions build has no
// secrets.h and keeps using the release URL.
// #define OTA_DIRECT_URL "http://api.drunkwallaby.com/matrixlamp/MatrixLamp_R4_Wifi.ino.ota"
