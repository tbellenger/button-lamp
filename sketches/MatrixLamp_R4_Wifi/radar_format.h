// /radar.bin — plain HTTP body the lamp fetches. No padding.
// Offsets are exact; a Rust struct must be packed or written byte by byte.
//
//   0  char magic[4]           'R','D','R','1'
//   4  uint8 version           1 (any other version is rejected)
//   5  uint8 frame_count       0..26, oldest frame first
//   6  uint8 flags             bit 0 = rain_nearby. Other bits ignored.
//   7  int8  max_dbz_near_home signed dBZ, or 0 when there is no echo
//   8  uint32 newest_frame_unix  little-endian Unix time of frame_count-1
//  12  uint16 frame_interval_s   little-endian seconds between frames
//  14  frames
//
// Each frame is 128 bytes, 16x16, row-major, y = 0 is north, x = 0 is west.
// Two cells per byte, high nibble first: byte = (level[x] << 4) | level[x+1]
// for even x. Level 0 is no echo or below 5 dBZ. Levels 1..15 are 5 dBZ
// steps: 1 is 5–10, 2 is 10–15, …, 14 is 70–75, 15 is 75 and above.
// A cell at level >= 4 is about 20 dBZ or more.
//
// Total body length is 14 + frame_count * 128. The HTTP response must be
// 200, with Content-Length set to that size. Chunked transfer is rejected.
// The lamp does not speak TLS.
//
// Playback (not in the file): about 4 frames/s, then the newest frame is
// held for 1 s. Home is logical cell (7, 7). The firmware applies the
// panel rotation; this file is north-up.
#pragma once

#include <stdint.h>

static const uint8_t RADAR_VERSION = 1;
static const uint8_t RADAR_FRAMES_MAX = 26;
static const uint16_t RADAR_FRAME_BYTES = 128;
static const uint16_t RADAR_HEAD_BYTES = 14;
static const uint16_t RADAR_BODY_MAX =
    (uint16_t)(RADAR_HEAD_BYTES + (uint16_t)RADAR_FRAMES_MAX * RADAR_FRAME_BYTES);

struct RadarInfo {
  uint8_t frames;
  uint8_t flags;
  int8_t maxDbz;
  uint32_t newest;
  uint16_t interval;
};

static inline uint32_t radarLoad32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static inline uint16_t radarLoad16(const uint8_t* p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

// True when the 14-byte header is version 1 and frame_count is in range.
// *total is the required body size, including the header.
static inline bool radarDecodeHead(const uint8_t* b, RadarInfo* o, uint16_t* total) {
  if (b[0] != 'R' || b[1] != 'D' || b[2] != 'R' || b[3] != '1') return false;
  if (b[4] != RADAR_VERSION) return false;
  uint8_t n = b[5];
  if (n > RADAR_FRAMES_MAX) return false;
  o->frames = n;
  o->flags = b[6];
  o->maxDbz = (int8_t)b[7];
  o->newest = radarLoad32(b + 8);
  o->interval = radarLoad16(b + 12);
  *total = (uint16_t)(RADAR_HEAD_BYTES + (uint16_t)n * RADAR_FRAME_BYTES);
  return true;
}

static inline uint8_t radarLevelAt(const uint8_t* frame, int x, int y) {
  const uint8_t* row = frame + (uint16_t)y * 8u;
  uint8_t byte = row[x >> 1];
  if (x & 1) return (uint8_t)(byte & 0x0f);
  return (uint8_t)(byte >> 4);
}

// Oldest frame is index 0. Each frame shows for 250 ms. The newest stays
// up for 1000 ms, then the loop starts again.
static inline uint8_t radarFrameIndex(uint32_t now, uint8_t count) {
  if (count <= 1) return 0;
  uint32_t walk = (uint32_t)(count - 1u) * 250u;
  uint32_t cycle = walk + 1000u;
  uint32_t t = now % cycle;
  if (t >= walk) return (uint8_t)(count - 1u);
  return (uint8_t)(t / 250u);
}

// ceil((index + 1) * 16 / count), so the bar is full on the newest frame.
static inline uint8_t radarBarLength(uint8_t index, uint8_t count) {
  if (count == 0) return 0;
  uint16_t n = (uint16_t)(((uint16_t)(index + 1u) * 16u + (uint16_t)count - 1u) / count);
  if (n > 16u) n = 16u;
  return (uint8_t)n;
}

#if defined(RADAR_PARSE_TEST)
#include <stdio.h>
#include <string.h>
int main() {
  uint8_t b[14 + 128];
  memset(b, 0, sizeof b);
  memcpy(b, "RDR1", 4);
  b[4] = 1;
  b[5] = 1;
  b[6] = 1;
  b[7] = (uint8_t)(int8_t)42;
  b[8] = 0x78;
  b[9] = 0x56;
  b[10] = 0x34;
  b[11] = 0x12;
  b[12] = 0x2c;
  b[13] = 0x01;
  b[14] = 0x10;
  RadarInfo info;
  uint16_t tot = 0;
  if (!radarDecodeHead(b, &info, &tot)) return 1;
  if (info.frames != 1 || info.flags != 1 || info.maxDbz != 42) return 2;
  if (info.newest != 0x12345678ul || info.interval != 300) return 3;
  if (tot != 142) return 4;
  if (radarLevelAt(b + 14, 0, 0) != 1 || radarLevelAt(b + 14, 1, 0) != 0) return 5;
  b[4] = 2;
  if (radarDecodeHead(b, &info, &tot)) return 6;
  if (radarBarLength(0, 26) != 1 || radarBarLength(25, 26) != 16) return 7;
  if (radarBarLength(0, 1) != 16 || radarBarLength(15, 16) != 16) return 8;
  if (radarFrameIndex(0, 4) != 0) return 9;
  if (radarFrameIndex(250, 4) != 1) return 10;
  if (radarFrameIndex(749, 4) != 2) return 11;
  if (radarFrameIndex(750, 4) != 3) return 12;
  if (radarFrameIndex(1749, 4) != 3) return 13;
  if (radarFrameIndex(1750, 4) != 0) return 14;
  printf("ok\n");
  return 0;
}
#endif
