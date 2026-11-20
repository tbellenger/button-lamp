/*
  WifiFirmwareCheck

  Prints the Uno R4 WiFi ESP32-S3 bridge firmware version on Serial at 115200.
  It does not join a network and does not need a WiFi name or password.

  0.5.0 or newer is required for the Arduino OTAUpdate non-blocking download
  (OTAUpdate::startDownload and downloadProgress). Those send
  AT+OTADOWNLOADSTART and AT+OTADOWNLOADPROGRESS, added in
  arduino/uno-r4-wifi-usb-bridge pull request 53 and shipped in bridge
  firmware 0.5.0. The core's OTANonBlocking example states the same floor.

  tools/WifiFirmwareCheck/check_wifi_fw.sh compiles, uploads, and opens the
  monitor. See tools/WifiFirmwareCheck/README.md.

  Uploading this sketch replaces whatever is on the board, including
  MatrixLamp_R4_Wifi. Reflash the lamp afterwards with
  tools/upload_matrixlamp.sh (Windows: tools\upload_matrixlamp.bat).
*/

#include <WiFiS3.h>
#include <string.h>

// Optional. check_wifi_fw.sh writes this from `arduino-cli core list` and
// deletes it after the build. The Renesas core does not publish its own
// version macro (the number lives in platform.txt).
#if defined(__has_include)
#  if __has_include("renesas_core_version.h")
#    include "renesas_core_version.h"
#  endif
#endif

static const int kNeedMajor = 0;
static const int kNeedMinor = 5;
static const int kNeedPatch = 0;

static const char kNoAnswer[] = "99.99.99";

static char g_fw[16];
static char g_verdict[12];
static uint8_t g_status = WL_IDLE_STATUS;
static bool g_readable = false;
static uint32_t g_nextLineMs = 0;

static int readNum(const char*& p) {
  if (*p < '0' || *p > '9') return -1;
  int v = 0;
  while (*p >= '0' && *p <= '9') {
    v = v * 10 + (*p - '0');
    if (v > 1000000) return -1;
    p++;
  }
  return v;
}

// major.minor or major.minor.patch. A leading v is ignored. Returns 0 if
// the text is not a version.
static int parseVersion(const char* s, int* major, int* minor, int* patch) {
  if (!s || !s[0]) return 0;
  if (*s == 'v' || *s == 'V') s++;
  const char* p = s;
  int maj = readNum(p);
  if (maj < 0 || *p != '.') return 0;
  p++;
  int min = readNum(p);
  if (min < 0) return 0;
  int pat = 0;
  if (*p == '.') {
    p++;
    pat = readNum(p);
    if (pat < 0) return 0;
  }
  *major = maj;
  *minor = min;
  *patch = pat;
  return 1;
}

static bool atLeast(int major, int minor, int patch) {
  if (major != kNeedMajor) return major > kNeedMajor;
  if (minor != kNeedMinor) return minor > kNeedMinor;
  return patch >= kNeedPatch;
}

static void copyToken(char* dst, size_t n, const char* src) {
  size_t i = 0;
  if (!dst || n == 0) return;
  dst[0] = 0;
  if (!src) return;
  while (src[i] && src[i] != ' ' && src[i] != '\t' && src[i] != '\r' && src[i] != '\n' && i + 1 < n) {
    dst[i] = src[i];
    i++;
  }
  dst[i] = 0;
}

static const char* statusName(uint8_t st) {
  if (st == WL_NO_MODULE) return "WL_NO_MODULE";
  if (st == WL_IDLE_STATUS) return "WL_IDLE_STATUS";
  if (st == WL_NO_SSID_AVAIL) return "WL_NO_SSID_AVAIL";
  if (st == WL_SCAN_COMPLETED) return "WL_SCAN_COMPLETED";
  if (st == WL_CONNECTED) return "WL_CONNECTED";
  if (st == WL_CONNECT_FAILED) return "WL_CONNECT_FAILED";
  if (st == WL_CONNECTION_LOST) return "WL_CONNECTION_LOST";
  if (st == WL_DISCONNECTED) return "WL_DISCONNECTED";
  if (st == WL_AP_LISTENING) return "WL_AP_LISTENING";
  if (st == WL_AP_CONNECTED) return "WL_AP_CONNECTED";
  if (st == WL_AP_FAILED) return "WL_AP_FAILED";
  return "other";
}

static void setVerdict(const char* text) {
  copyToken(g_verdict, sizeof g_verdict, text);
}

// Ask the bridge. Does not call WiFi.begin().
static void refresh() {
  const char* reported;

  g_status = WiFi.status();
  reported = WiFi.firmwareVersion();
  copyToken(g_fw, sizeof g_fw, reported);

  g_readable = false;
  setVerdict("unknown");
  if (g_status == WL_NO_MODULE) return;
  if (!g_fw[0] || strcmp(g_fw, kNoAnswer) == 0) return;

  int major = 0;
  int minor = 0;
  int patch = 0;
  if (!parseVersion(g_fw, &major, &minor, &patch)) return;
  g_readable = true;
  setVerdict(atLeast(major, minor, patch) ? "yes" : "no");
}

static void printCoreVersion() {
#if defined(RENESAS_CORE_VERSION)
  Serial.print(F("Renesas core: arduino:renesas_uno "));
  Serial.println(RENESAS_CORE_VERSION);
#elif defined(ARDUINO_RENESAS_CORE_VERSION)
  Serial.print(F("Renesas core: "));
  Serial.println(ARDUINO_RENESAS_CORE_VERSION);
#elif defined(ARDUINO_RENESAS_UNO_VERSION)
  Serial.print(F("Renesas core: "));
  Serial.println(ARDUINO_RENESAS_UNO_VERSION);
#else
  Serial.println(F("Renesas core: (this core does not publish a version macro)"));
#endif
}

static void printDetails() {
  int major = 0;
  int minor = 0;
  int patch = 0;
  int latestMajor = 0;
  int latestMinor = 0;
  int latestPatch = 0;
  bool haveLatest;

  Serial.println(F("WifiFirmwareCheck"));
  Serial.println(F("ESP32-S3 bridge version only. No WiFi join."));

  Serial.print(F("WiFi.status() = "));
  Serial.print(g_status);
  Serial.print(F(" ("));
  Serial.print(statusName(g_status));
  Serial.println(F(")"));
  if (g_status == WL_NO_MODULE) {
    Serial.println(F("WL_NO_MODULE: the ESP32-S3 bridge did not answer."));
  } else if (g_status == WL_IDLE_STATUS || g_status == WL_DISCONNECTED) {
    Serial.println(F("No network joined. That status is expected."));
  }

  Serial.print(F("WiFi.firmwareVersion() = "));
  Serial.println(g_fw[0] ? g_fw : "(empty)");
  if (!g_fw[0] || strcmp(g_fw, kNoAnswer) == 0) {
    Serial.println(F("WiFiS3 returns 99.99.99 when AT+FWVERSION? fails. That is not a real version."));
  }

  Serial.print(F("WIFI_FIRMWARE_LATEST_VERSION = "));
  Serial.println(F(WIFI_FIRMWARE_LATEST_VERSION));

  Serial.print(F(">= 0.5.0: "));
  Serial.println(g_verdict);
  if (g_readable) {
    Serial.println(F("0.5.0 is the OTAUpdate non-blocking download (AT+OTADOWNLOADSTART / AT+OTADOWNLOADPROGRESS)."));
  }

  haveLatest = parseVersion(WIFI_FIRMWARE_LATEST_VERSION, &latestMajor, &latestMinor, &latestPatch);
  if (g_readable && haveLatest && parseVersion(g_fw, &major, &minor, &patch)) {
    bool older = false;
    if (major != latestMajor) older = major < latestMajor;
    else if (minor != latestMinor) older = minor < latestMinor;
    else older = patch < latestPatch;
    if (older) {
      Serial.println(F("This bridge is older than WIFI_FIRMWARE_LATEST_VERSION in the installed core."));
    } else {
      Serial.println(F("This bridge matches or is newer than WIFI_FIRMWARE_LATEST_VERSION in the installed core."));
    }
  }

  printCoreVersion();
}

static void printVersionLine() {
  Serial.print(F("fw "));
  Serial.print(g_fw[0] ? g_fw : "(empty)");
  Serial.print(F(" latest "));
  Serial.print(F(WIFI_FIRMWARE_LATEST_VERSION));
  Serial.print(F(" >= 0.5.0 "));
  Serial.print(g_verdict);
#if defined(RENESAS_CORE_VERSION)
  Serial.print(F(" core "));
  Serial.print(RENESAS_CORE_VERSION);
#elif defined(ARDUINO_RENESAS_CORE_VERSION)
  Serial.print(F(" core "));
  Serial.print(ARDUINO_RENESAS_CORE_VERSION);
#elif defined(ARDUINO_RENESAS_UNO_VERSION)
  Serial.print(F(" core "));
  Serial.print(ARDUINO_RENESAS_UNO_VERSION);
#endif
  Serial.println();
}

void setup() {
  uint32_t waitStart;

  Serial.begin(115200);
  waitStart = millis();
  while (!Serial && (uint32_t)(millis() - waitStart) < 5000) {
    delay(10);
  }

  refresh();
  printDetails();
  printVersionLine();
  g_nextLineMs = millis() + 5000;
}

void loop() {
  bool wasReadable;

  if ((int32_t)(millis() - g_nextLineMs) < 0) return;
  wasReadable = g_readable;
  // A dead bridge makes firmwareVersion() wait on the modem timeout.
  // Retry only until a real version arrives, then reprint the cached line.
  if (!g_readable) refresh();
  g_nextLineMs = millis() + 5000;
  if (!wasReadable && g_readable) printDetails();
  printVersionLine();
}
