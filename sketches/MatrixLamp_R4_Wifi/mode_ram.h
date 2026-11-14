// One scratch region for the presets that are not on screen together.
// Radar frames are 26 * 128 bytes, already nibble-packed; the renderer
// decodes a pixel when it is drawn. Fire heat, fluid, the matrix rain
// field, the polar tables, and the spiral cache occupy the same bytes.
// Visiting radar and coming back rebuilds those tables. The web buffers
// stay separate so a page fetch during playback does not wipe the frames.
#pragma once

#include "radar_format.h"

struct LampFxScratch {
  uint8_t heat[256];
  uint8_t dye[256];
  uint8_t dyeH[256];
  uint8_t polDist[256];
  uint8_t polAng[256];
  uint8_t rainRgb[256 * 3];
  int16_t spX[96];
  int16_t spY[96];
  uint8_t spU[96];
};

union LampModeRam {
  uint8_t radar[RADAR_FRAMES_MAX * RADAR_FRAME_BYTES];
  LampFxScratch fx;
};

static_assert(sizeof(LampFxScratch) <= sizeof(((LampModeRam*)0)->radar),
              "preset scratch must fit in the radar frame store");

extern LampModeRam g_modeRam;
