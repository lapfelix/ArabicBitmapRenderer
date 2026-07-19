#ifndef ARABIC_SHAPER_H
#define ARABIC_SHAPER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  ARABIC_SHAPE_KEEP_HARAKAT = 0x01,  // Emit harakat as zero-advance marks
};

// UTF-8 logical order -> presentation-form codepoints in visual order
// (left-to-right for the renderer, RTL base direction). Invalid UTF-8
// bytes decode as '?'. Returns item count, truncated at capacity.
size_t arabicShape(const char *utf8, uint16_t *out, size_t capacity,
                   uint8_t flags);

// True for combining marks (harakat) — rendered with zero advance.
int arabicIsMark(uint16_t codepoint);

#ifdef __cplusplus
}
#endif

#endif  // ARABIC_SHAPER_H
