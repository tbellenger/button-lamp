// Preset ids shared by the sketch, the effect core, and the browser preview.
#pragma once

#include <stdint.h>

static const uint8_t M_FIRE = 0;
static const uint8_t M_RAINBOW = 1;
static const uint8_t M_METABALLS = 2;
static const uint8_t M_PLASMA = 3;
static const uint8_t M_SUN = 4;
static const uint8_t M_SPIRAL = 5;
static const uint8_t M_FLUID = 6;
static const uint8_t M_HEART = 7;
static const uint8_t M_MATRIX = 8;
static const uint8_t M_LIGHTNING = 9;
static const uint8_t M_OFF = 10;
static const uint8_t M_RADAR = 11;
static const uint8_t M_COUNT = 12;

// MLP2 stores this byte. Ids already on a lamp must keep their numbers.
static_assert(M_LIGHTNING == 9 && M_OFF == 10 && M_RADAR == 11 && M_COUNT == 12,
              "mode ids");

static const uint8_t META_MIN = 1;
static const uint8_t META_MAX = 8;
