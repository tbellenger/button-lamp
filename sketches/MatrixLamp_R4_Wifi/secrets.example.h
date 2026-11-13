// Copy this file to secrets.h (same folder) and fill it in.
// secrets.h is gitignored. Do not commit real WiFi credentials.
//
// A USB build writes these into the lamp's data flash when they differ from
// what is already stored. A firmware file built by GitHub Actions has no
// secrets.h, so it keeps the WiFi name already on the lamp.
#pragma once

#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
