#include "ArabicFont.h"

#include <stddef.h>

const ArabicGlyph *arabicFontGlyph(const ArabicFont *font, uint16_t codepoint) {
  size_t lo = 0, hi = font->glyphCount;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    uint16_t c = font->glyphs[mid].codepoint;
    if (c == codepoint) return &font->glyphs[mid];
    if (c < codepoint) lo = mid + 1;
    else hi = mid;
  }
  return NULL;
}
