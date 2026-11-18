// Stand-in for the LilyGo library header when building previews on a computer:
// just the font types the font files and gfx.cpp need.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define EPD_WIDTH 960
#define EPD_HEIGHT 540

typedef struct {
  uint8_t width, height, advance_x;
  int16_t left, top;
  uint16_t compressed_size;
  uint32_t data_offset;
} GFXglyph;

typedef struct {
  uint32_t first, last, offset;
} UnicodeInterval;

typedef struct {
  uint8_t *bitmap;
  GFXglyph *glyph;
  UnicodeInterval *intervals;
  uint32_t interval_count;
  bool compressed;
  uint8_t advance_y;
  int32_t ascender;
  int32_t descender;
} GFXfont;
