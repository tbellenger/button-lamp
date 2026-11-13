// Shared Matrix Lamp effect core. The sketch and the browser preview both
// call this. No NeoPixel, WiFi, or (on the host build) Arduino types.
#pragma once

#include <stdint.h>
#include "lamp_ids.h"

static const int LAMP_FX_W = 16;
static const int LAMP_FX_H = 16;
static const int LAMP_FX_N = LAMP_FX_W * LAMP_FX_H;

// Boot the effects and warm the fire the same way the sketch always has.
void lamp_fx_init();

// The sketch keeps the saved mode, speed, and fader. Pass them in before
// each render. A change forces the next frame even if the period has not
// elapsed, which is what the old g_dirty flag did.
void lamp_fx_configure(uint8_t mode, uint8_t speed, uint8_t fader);
void lamp_fx_mark_dirty();

// Absolute millisecond clock (millis() on the board, a running sum in the
// preview). Returns 1 when a new frame was written.
int lamp_fx_render(uint32_t now_ms);

// Visual order after PANEL_ROTATE 3: row-major RGB, 16*16*3.
// These are the colors setPixelColor used to receive, before NeoPixel
// brightness. y = 0 is the top of the mounted panel.
const uint8_t* lamp_fx_frame();

// Strip index for a visual cell, including the serpentine row flip.
uint16_t lamp_fx_led_index(int x, int y);
// Strip index for a logical cell: rotate, then serpentine. This is the
// old panelIndex().
uint16_t lamp_fx_led_from_logical(int x, int y);

// Same cap and scale the sketch used: fader 0..100 mapped through the
// power cap, then NeoPixel stores brightness+1 and multiplies on show.
uint8_t lamp_fx_brightness_cap();
uint8_t lamp_fx_applied(uint8_t fader);
uint8_t lamp_fx_scale(uint8_t channel, uint8_t applied);

const char* lamp_fx_mode_name(uint8_t mode);
int lamp_fx_mode_from_name(const char* s);
// Packed radar frames (128 bytes each, north up). count 0 clears the loop.
void lamp_fx_radar_set(const uint8_t* packed, uint8_t count);
void lamp_fx_balls_add();
void lamp_fx_balls_remove();
void lamp_fx_balls_set(uint8_t n);
uint8_t lamp_fx_balls();
uint8_t lamp_fx_plasma_zoom();
uint8_t lamp_fx_sun_size();
uint8_t lamp_fx_rotate();
