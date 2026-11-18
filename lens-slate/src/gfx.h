#pragma once
// Minimal drawing layer for the 960x540 e-paper, used in PORTRAIT orientation
// (540 wide x 960 tall). Shades are 0 (black) .. 15 (white).
//
// The LilyGo library only draws landscape, so this does its own rotation and
// text rendering from the library's fonts.

#include <stddef.h>
#include <stdint.h>

#include "fonts/font_types.h"

namespace gfx {

constexpr int W = 540;
constexpr int H = 960;

// Framebuffer: the panel's native 960 x 540 landscape layout, 4 bits per
// pixel, two pixels per byte (even x in the low nibble).
constexpr int FB_WIDTH = 960;
constexpr int FB_HEIGHT = 540;
constexpr int FB_BYTES = FB_WIDTH * FB_HEIGHT / 2;

constexpr uint8_t BLACK = 0;
constexpr uint8_t DARK = 5;
constexpr uint8_t MID = 9;
constexpr uint8_t LIGHT = 12;
constexpr uint8_t WHITE = 15;

// fb is the library-format framebuffer (960*540/2 bytes, 4 bits per pixel).
// flip rotates the picture 180 degrees (USB port at the other end).
// invert draws white on black.
void begin(uint8_t *fb, bool flip, bool invert = false);
void clear(uint8_t shade = WHITE);

void pixel(int x, int y, uint8_t shade);
uint8_t getPixel(int x, int y);
uint8_t screenPixel(int x, int y);  // what the panel will show (after invert)
void fillRect(int x, int y, int w, int h, uint8_t shade);
void hline(int x, int y, int w, int t, uint8_t shade = BLACK);
void vline(int x, int y, int h, int t, uint8_t shade = BLACK);
void frame(int x, int y, int w, int h, int t, uint8_t shade = BLACK);

// Text is UTF-8. y is the baseline. Missing characters draw as '?'.
int textWidth(const GFXfont *font, const char *s);
void text(const GFXfont *font, const char *s, int x, int y, uint8_t fg = BLACK);
void textRight(const GFXfont *font, const char *s, int rightX, int y, uint8_t fg = BLACK);
void textCentered(const GFXfont *font, const char *s, int centerX, int y, uint8_t fg = BLACK);

// Copies s into out, shortened with "..." so it fits in maxW pixels.
void fit(const GFXfont *font, const char *s, int maxW, char *out, size_t outSize);

// Vertical position of a baseline that centres digits/capitals in a box.
int centredBaseline(const GFXfont *font, int top, int height);

}  // namespace gfx
