#pragma once
// Host stand-in. PROGMEM data is ordinary memory; memcpy_P is memcpy.
#include <stdint.h>
#include <string.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t *>(addr))
#endif
#ifndef memcpy_P
#define memcpy_P(dst, src, n) memcpy((dst), (src), (n))
#endif
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t *>(addr))
#define pgm_read_dword(addr) (*reinterpret_cast<const uint32_t *>(addr))
#define pgm_read_float(addr) (*reinterpret_cast<const float *>(addr))
#define strcpy_P(dst, src) strcpy((dst), (src))
#define strlen_P(src) strlen((src))
#define memcmp_P(a, b, n) memcmp((a), (b), (n))
