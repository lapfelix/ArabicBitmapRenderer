#ifndef ARABIC_RENDERER_H
#define ARABIC_RENDERER_H

#include <stddef.h>
#include <stdint.h>

#include "ArabicFont.h"

#ifdef __cplusplus
extern "C" {
#endif

// Sum of advances. Missing glyph -> lineHeight/2 (marks -> 0).
uint16_t arabicMeasure(const ArabicFont *font, const uint16_t *shaped,
                       size_t count);

// Shape (default flags) into a stack buffer + measure, for layout decisions.
uint16_t arabicMeasureUtf8(const ArabicFont *font, const char *utf8);

// Blit one absolute scanline; setPixel(x) called per ink pixel.
void arabicRenderRow(const ArabicFont *font, const uint16_t *shaped,
                     size_t count, int16_t x, uint16_t lineTop, uint16_t row,
                     void (*setPixel)(int16_t x, void *ctx), void *ctx);

// Full line box at (x, y) for framebuffer targets.
void arabicRenderAll(const ArabicFont *font, const uint16_t *shaped,
                     size_t count, int16_t x, int16_t y,
                     void (*setPixel)(int16_t x, int16_t y, void *ctx),
                     void *ctx);

#ifdef __cplusplus
}
#endif

#endif  // ARABIC_RENDERER_H
