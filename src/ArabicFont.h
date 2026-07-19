#ifndef ARABIC_FONT_H
#define ARABIC_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t codepoint;  // Presentation form (FB50-FEFF) or base codepoint
  uint16_t offset;     // Byte offset into bitmapData
  uint8_t width;       // Tight bbox width in px
  uint8_t height;      // Tight bbox height in px
  int8_t xOffset;      // Left bearing: pen + xOffset = first bbox column
  int8_t yOffset;      // Bbox top relative to line-box top
  uint8_t advance;     // Pen advance in px
} ArabicGlyph;

typedef struct {
  uint8_t lineHeight;  // Line box height in px
  uint8_t baseline;    // Baseline row from line-box top
  uint16_t glyphCount;
  const ArabicGlyph *glyphs;  // Sorted by codepoint (binary search)
  // 1bpp, MSB-first, each glyph row padded to a byte boundary.
  const uint8_t *bitmapData;
} ArabicFont;

// NULL if the codepoint has no glyph.
const ArabicGlyph *arabicFontGlyph(const ArabicFont *font, uint16_t codepoint);

#ifdef __cplusplus
}
#endif

#endif  // ARABIC_FONT_H
