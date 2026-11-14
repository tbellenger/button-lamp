// Compare the running firmware version to manifest.json "version".
// The release notes body and the git tag are not inputs.
//
// CI writes the same string into FW_VERSION and manifest.json:
//   <7 hex chars of the commit>-<GitHub run number>
//   example: f8347da-22
// The moving tag is matrixlamp-<that string>. A leading "v" or a
// "matrixlamp-" prefix is stripped before the compare, so the tag form
// and the compiled string are the same version.
//
// Semver (1.2.3, v1.2.3) compares major, minor, patch as numbers.
// <sha>-<run> compares the run number (it only increases). Equal runs
// still have to be the same string. Anything else matches only when the
// normalized strings are equal; a different string counts as newer.
//
// "Up to date" is normalized string equality (case-insensitive, prefix
// stripped). A download happens only when the remote compares newer.
// An older release is not an update.
#pragma once

#include <stdint.h>
#include <string.h>

static char fwLower(char c) {
  if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
  return c;
}

static int fwCiPrefix(const char* s, const char* p) {
  while (*p) {
    if (fwLower(*s) != fwLower(*p)) return 0;
    s++;
    p++;
  }
  return 1;
}

static void fwVersionNorm(const char* in, char* out, unsigned n) {
  if (!out || n == 0) return;
  out[0] = 0;
  if (!in) return;
  while (*in == ' ' || *in == '\t' || *in == '\r' || *in == '\n') in++;
  if ((in[0] == 'v' || in[0] == 'V') && in[1]) in++;
  static const char kPrefix[] = "matrixlamp-";
  if (fwCiPrefix(in, kPrefix)) in += sizeof kPrefix - 1;
  if ((in[0] == 'v' || in[0] == 'V') && in[1]) in++;
  unsigned i = 0;
  while (*in && *in != ' ' && *in != '\t' && *in != '\r' && *in != '\n' && i + 1 < n) {
    out[i++] = fwLower(*in++);
  }
  out[i] = 0;
}

static int fwParseU(const char* s, const char** end, int* out) {
  if (*s < '0' || *s > '9') return 0;
  int v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    if (v > 1000000) return 0;
    s++;
  }
  *out = v;
  *end = s;
  return 1;
}

// Packs major.minor.patch. Returns 0 if this is not dotted semver.
static int fwSemverKey(const char* s, int* key) {
  const char* p = s;
  int major = 0, minor = 0, patch = 0;
  if (!fwParseU(p, &p, &major) || *p != '.') return 0;
  p++;
  if (!fwParseU(p, &p, &minor)) return 0;
  if (*p == '.') {
    p++;
    if (!fwParseU(p, &p, &patch)) return 0;
  }
  if (*p != 0 && *p != '-' && *p != '+') return 0;
  *key = major * 1000000 + minor * 1000 + patch;
  return 1;
}

static int fwHexRun(const char* s, unsigned long* run, const char** shaEnd) {
  const char* p = s;
  int hex = 0;
  while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
    hex++;
    p++;
  }
  if (hex < 4 || hex > 40 || *p != '-') return 0;
  p++;
  if (*p < '0' || *p > '9') return 0;
  unsigned long r = 0;
  while (*p >= '0' && *p <= '9') {
    r = r * 10ul + (unsigned long)(*p - '0');
    if (r > 10000000ul) return 0;
    p++;
  }
  if (*p != 0) return 0;
  *run = r;
  *shaEnd = s + hex;
  return 1;
}

// >0 remote is newer, 0 same, <0 remote is older.
static int fwVersionCmp(const char* localRaw, const char* remoteRaw) {
  char local[28];
  char remote[28];
  fwVersionNorm(localRaw, local, sizeof local);
  fwVersionNorm(remoteRaw, remote, sizeof remote);
  int lk = 0, rk = 0;
  if (fwSemverKey(local, &lk) && fwSemverKey(remote, &rk)) {
    if (lk == rk) return strcmp(local, remote) == 0 ? 0 : 1;
    return rk > lk ? 1 : -1;
  }
  unsigned long lr = 0, rr = 0;
  const char* lsha = 0;
  const char* rsha = 0;
  if (fwHexRun(local, &lr, &lsha) && fwHexRun(remote, &rr, &rsha)) {
    if (lr != rr) return rr > lr ? 1 : -1;
    return strcmp(local, remote) == 0 ? 0 : 1;
  }
  return strcmp(local, remote) == 0 ? 0 : 1;
}

// 1: remote is an update the lamp should install. 0: same version, or the
// release is older than what is running.
static int fwShouldInstall(const char* localRaw, const char* remoteRaw) {
  char local[28];
  char remote[28];
  fwVersionNorm(localRaw, local, sizeof local);
  fwVersionNorm(remoteRaw, remote, sizeof remote);
  if (local[0] && strcmp(local, remote) == 0) return 0;
  return fwVersionCmp(localRaw, remoteRaw) > 0 ? 1 : 0;
}

#if defined(FW_VER_TEST)
#include <stdio.h>
int main() {
  char n[28];
  fwVersionNorm("  v1.2.3 ", n, sizeof n);
  if (strcmp(n, "1.2.3") != 0) return 1;
  fwVersionNorm("matrixlamp-v1.2.3", n, sizeof n);
  if (strcmp(n, "1.2.3") != 0) return 2;
  fwVersionNorm("matrixlamp-f8347da-22", n, sizeof n);
  if (strcmp(n, "f8347da-22") != 0) return 3;
  if (fwVersionCmp("f8347da-22", "f8347da-22") != 0) return 4;
  if (fwVersionCmp("f8347da-22", "matrixlamp-f8347da-22") != 0) return 5;
  if (fwVersionCmp("f8347da-22", "vf8347da-22") != 0) return 6;
  if (fwVersionCmp("41d87cd-19", "f8347da-22") <= 0) return 7;
  if (fwVersionCmp("f8347da-22", "41d87cd-19") >= 0) return 8;
  if (fwVersionCmp("1.2.3", "v1.2.3") != 0) return 9;
  if (fwVersionCmp("1.2.3", "1.2.4") <= 0) return 10;
  if (fwVersionCmp("1.4.0", "1.3.9") >= 0) return 11;
  if (fwVersionCmp("local", "f8347da-22") <= 0) return 12;
  if (fwVersionCmp("f8347da-22", "  f8347da-22\n") != 0) return 13;
  fwVersionNorm("MatrixLamp-F8347DA-22", n, sizeof n);
  if (strcmp(n, "f8347da-22") != 0) return 14;
  if (fwShouldInstall("f8347da-22", "f8347da-22") != 0) return 15;
  if (fwShouldInstall("f8347da-22", "matrixlamp-f8347da-22") != 0) return 16;
  if (fwShouldInstall("f8347da-22", "MatrixLamp-F8347DA-22") != 0) return 17;
  if (fwShouldInstall("f8347da-22", "f8347da-23") != 1) return 18;
  if (fwShouldInstall("f8347da-23", "f8347da-22") != 0) return 19;
  if (fwShouldInstall("local", "f8347da-22") != 1) return 20;
  printf("ok\n");
  return 0;
}
#endif
