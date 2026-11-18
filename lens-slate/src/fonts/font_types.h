#pragma once
// Font data structures used by the font files in this folder.
// The fonts come from the LilyGo-EPD47 library (GPL-3.0), converted with its
// fontconvert.py; they're kept here so the layouts work on every board.

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t width;            // bitmap size in pixels
  uint8_t height;
  uint8_t advance_x;        // distance to the next character
  int16_t left;             // offset from cursor to bitmap's top-left
  int16_t top;
  uint16_t compressed_size; // zlib-compressed size of the bitmap
  uint32_t data_offset;     // offset into GFXfont::bitmap
} GFXglyph;

typedef struct {
  uint32_t first;           // first code point in this range
  uint32_t last;
  uint32_t offset;          // index of `first` in the glyph array
} UnicodeInterval;

typedef struct {
  uint8_t *bitmap;
  GFXglyph *glyph;
  UnicodeInterval *intervals;
  uint32_t interval_count;
  bool compressed;
  uint8_t advance_y;        // line height
  int32_t ascender;
  int32_t descender;
} GFXfont;
