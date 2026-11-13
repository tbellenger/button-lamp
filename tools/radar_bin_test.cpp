// Host check for the radar body the lamp parses.
//   curl -fsS -o /tmp/radar.bin http://api.drunkwallaby.com/radar.bin
//   g++ -std=c++17 -I sketches/MatrixLamp_R4_Wifi tools/radar_bin_test.cpp -o /tmp/radar_bin_test
//   /tmp/radar_bin_test /tmp/radar.bin
#include "radar_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load(const char* path, uint8_t* buf, size_t cap, size_t* n) {
  FILE* f = fopen(path, "rb");
  if (!f) return 0;
  *n = fread(buf, 1, cap, f);
  int extra = fgetc(f);
  fclose(f);
  if (extra != EOF) return 0;
  return 1;
}

static int accept(const uint8_t* b, size_t n) {
  if (n < RADAR_HEAD_BYTES) return 0;
  RadarInfo info;
  uint16_t total = 0;
  if (!radarDecodeHead(b, &info, &total)) return 0;
  if (total != n || info.frames > RADAR_FRAMES_MAX) return 0;
  if (info.frames == 0) return radarBarLength(0, 0) == 0;
  if (radarFrameIndex(0, info.frames) != 0) return 0;
  if (radarFrameIndex(100000u, info.frames) >= info.frames) return 0;
  if (radarBarLength(info.frames - 1, info.frames) != 16) return 0;
  return 1;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: radar_bin_test file\n");
    return 2;
  }
  uint8_t buf[RADAR_BODY_MAX + 8];
  size_t n = 0;
  if (!load(argv[1], buf, sizeof buf, &n)) {
    fprintf(stderr, "could not read %s\n", argv[1]);
    return 2;
  }
  if (!accept(buf, n)) {
    fprintf(stderr, "rejected a complete body (%u bytes)\n", (unsigned)n);
    return 1;
  }
  RadarInfo info;
  uint16_t total = 0;
  radarDecodeHead(buf, &info, &total);
  printf("ok frames %u bytes %u\n", info.frames, (unsigned)n);
  if (accept(buf, 10)) {
    fprintf(stderr, "accepted a 10-byte truncation\n");
    return 1;
  }
  if (n > 20 && accept(buf, 20)) {
    fprintf(stderr, "accepted a 20-byte truncation\n");
    return 1;
  }
  if (n > RADAR_HEAD_BYTES + 10 && accept(buf, RADAR_HEAD_BYTES + 10)) {
    fprintf(stderr, "accepted a short body\n");
    return 1;
  }
  buf[0] = 'X';
  if (accept(buf, n)) {
    fprintf(stderr, "accepted a bad magic\n");
    return 1;
  }
  printf("truncated and bad magic rejected\n");
  return 0;
}
