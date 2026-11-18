#include "gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SLATE_HOST_PREVIEW
#include <zlib.h>
#else
#include "zlib/zlib.h"  // bundled with the LilyGo library
#endif

namespace gfx {

static uint8_t *buf = nullptr;
static bool flipped = false;

static const int PHYS_W = 960;
static const int PHYS_H = 540;

void begin(uint8_t *fb, bool flip) {
  buf = fb;
  flipped = flip;
}

void clear(uint8_t shade) {
  uint8_t v = (shade & 0x0F) | (shade << 4);
  memset(buf, v, PHYS_W * PHYS_H / 2);
}

// Logical portrait (x, y) -> physical landscape (px, py).
static inline bool toPhys(int x, int y, int &px, int &py) {
  if (x < 0 || x >= W || y < 0 || y >= H) return false;
  if (!flipped) {
    px = y;
    py = PHYS_H - 1 - x;
  } else {
    px = PHYS_W - 1 - y;
    py = x;
  }
  return true;
}

void pixel(int x, int y, uint8_t shade) {
  int px, py;
  if (!toPhys(x, y, px, py)) return;
  uint8_t *p = &buf[py * (PHYS_W / 2) + px / 2];
  if (px & 1) *p = (*p & 0x0F) | (shade << 4);
  else *p = (*p & 0xF0) | (shade & 0x0F);
}

uint8_t getPixel(int x, int y) {
  int px, py;
  if (!toPhys(x, y, px, py)) return WHITE;
  uint8_t v = buf[py * (PHYS_W / 2) + px / 2];
  return (px & 1) ? (v >> 4) : (v & 0x0F);
}

void fillRect(int x, int y, int w, int h, uint8_t shade) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) pixel(xx, yy, shade);
}

void hline(int x, int y, int w, int t, uint8_t shade) { fillRect(x, y, w, t, shade); }
void vline(int x, int y, int h, int t, uint8_t shade) { fillRect(x, y, t, h, shade); }

void frame(int x, int y, int w, int h, int t, uint8_t shade) {
  hline(x, y, w, t, shade);
  hline(x, y + h - t, w, t, shade);
  vline(x, y, h, t, shade);
  vline(x + w - t, y, h, t, shade);
}

// --- text -------------------------------------------------------------------

static uint32_t nextCodepoint(const char *&s) {
  const uint8_t *u = (const uint8_t *)s;
  if (!*u) return 0;
  uint32_t cp;
  int len;
  if (u[0] < 0x80) { cp = u[0]; len = 1; }
  else if ((u[0] & 0xE0) == 0xC0) { cp = u[0] & 0x1F; len = 2; }
  else if ((u[0] & 0xF0) == 0xE0) { cp = u[0] & 0x0F; len = 3; }
  else if ((u[0] & 0xF8) == 0xF0) { cp = u[0] & 0x07; len = 4; }
  else { s++; return '?'; }
  for (int i = 1; i < len; i++) {
    if ((u[i] & 0xC0) != 0x80) { s += i; return '?'; }
    cp = (cp << 6) | (u[i] & 0x3F);
  }
  s += len;
  return cp;
}

static const GFXglyph *findGlyph(const GFXfont *font, uint32_t cp) {
  for (uint32_t i = 0; i < font->interval_count; i++) {
    const UnicodeInterval &iv = font->intervals[i];
    if (cp >= iv.first && cp <= iv.last) return &font->glyph[iv.offset + (cp - iv.first)];
  }
  return nullptr;
}

static const GFXglyph *glyphOrFallback(const GFXfont *font, uint32_t cp) {
  const GFXglyph *g = findGlyph(font, cp);
  return g ? g : findGlyph(font, '?');
}

int textWidth(const GFXfont *font, const char *s) {
  if (!s) return 0;
  int w = 0;
  uint32_t cp;
  while ((cp = nextCodepoint(s))) {
    const GFXglyph *g = glyphOrFallback(font, cp);
    if (g) w += g->advance_x;
  }
  return w;
}

static void drawGlyph(const GFXfont *font, const GFXglyph *g, int x, int y, uint8_t fg) {
  int bw = (g->width + 1) / 2;
  unsigned long size = (unsigned long)bw * g->height;
  if (size == 0) return;

  uint8_t *bitmap;
  bool allocated = false;
  if (font->compressed) {
    bitmap = (uint8_t *)malloc(size);
    if (!bitmap) return;
    allocated = true;
    uncompress(bitmap, &size, &font->bitmap[g->data_offset], g->compressed_size);
  } else {
    bitmap = &font->bitmap[g->data_offset];
  }

  for (int row = 0; row < g->height; row++) {
    for (int col = 0; col < g->width; col++) {
      uint8_t b = bitmap[row * bw + col / 2];
      uint8_t a = (col & 1) ? (b >> 4) : (b & 0x0F);  // coverage 0..15
      if (!a) continue;
      int px = x + g->left + col;
      int py = y - g->top + row;
      int old = getPixel(px, py);
      int v = old + ((int)fg - old) * a / 15;  // blend onto what's there
      pixel(px, py, (uint8_t)v);
    }
  }
  if (allocated) free(bitmap);
}

void text(const GFXfont *font, const char *s, int x, int y, uint8_t fg) {
  if (!s) return;
  uint32_t cp;
  while ((cp = nextCodepoint(s))) {
    const GFXglyph *g = glyphOrFallback(font, cp);
    if (!g) continue;
    drawGlyph(font, g, x, y, fg);
    x += g->advance_x;
  }
}

void textRight(const GFXfont *font, const char *s, int rightX, int y, uint8_t fg) {
  text(font, s, rightX - textWidth(font, s), y, fg);
}

void textCentered(const GFXfont *font, const char *s, int centerX, int y, uint8_t fg) {
  text(font, s, centerX - textWidth(font, s) / 2, y, fg);
}

void fit(const GFXfont *font, const char *s, int maxW, char *out, size_t outSize) {
  if (!outSize) return;
  strncpy(out, s ? s : "", outSize - 1);
  out[outSize - 1] = 0;
  if (textWidth(font, out) <= maxW) return;
  size_t n = strlen(out);
  char tmp[256];
  while (n > 0) {
    n--;
    while (n > 0 && (((uint8_t)out[n]) & 0xC0) == 0x80) n--;  // keep UTF-8 whole
    snprintf(tmp, sizeof(tmp), "%.*s...", (int)n, out);
    if (textWidth(font, tmp) <= maxW) break;
  }
  snprintf(out, outSize, "%.*s...", (int)n, s);
}

int centredBaseline(const GFXfont *font, int top, int height) {
  // Digits and capitals are roughly 0.72 of the ascender tall.
  int capH = font->ascender * 72 / 100;
  return top + (height + capH) / 2;
}

}  // namespace gfx
