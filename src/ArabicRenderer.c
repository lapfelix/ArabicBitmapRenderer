#include "ArabicRenderer.h"

#include "ArabicShaper.h"

static uint16_t fallbackAdvance(const ArabicFont *font, uint16_t cp) {
  return arabicIsMark(cp) ? 0 : (uint16_t)(font->lineHeight / 2);
}

uint16_t arabicMeasure(const ArabicFont *font, const uint16_t *shaped,
                       size_t count) {
  uint16_t w = 0;
  for (size_t i = 0; i < count; i++) {
    const ArabicGlyph *g = arabicFontGlyph(font, shaped[i]);
    w = (uint16_t)(w + (g ? g->advance : fallbackAdvance(font, shaped[i])));
  }
  return w;
}

uint16_t arabicMeasureUtf8(const ArabicFont *font, const char *utf8) {
  uint16_t buf[96];
  size_t n = arabicShape(utf8, buf, sizeof buf / sizeof buf[0], 0);
  return arabicMeasure(font, buf, n);
}

void arabicRenderRow(const ArabicFont *font, const uint16_t *shaped,
                     size_t count, int16_t x, uint16_t lineTop, uint16_t row,
                     void (*setPixel)(int16_t x, void *ctx), void *ctx) {
  if (row < lineTop || row >= lineTop + font->lineHeight) return;
  int16_t rel = (int16_t)(row - lineTop);
  int16_t pen = x;
  for (size_t i = 0; i < count; i++) {
    const ArabicGlyph *g = arabicFontGlyph(font, shaped[i]);
    if (!g) {
      pen = (int16_t)(pen + fallbackAdvance(font, shaped[i]));
      continue;
    }
    int16_t gr = (int16_t)(rel - g->yOffset);
    if (gr >= 0 && gr < g->height) {
      uint16_t rowBytes = (uint16_t)((g->width + 7) >> 3);
      const uint8_t *src =
          font->bitmapData + g->offset + (uint32_t)gr * rowBytes;
      int16_t gx = (int16_t)(pen + g->xOffset);
      for (uint16_t bx = 0; bx < rowBytes; bx++) {
        uint8_t bits = src[bx];
        if (!bits) continue;
        uint16_t base = (uint16_t)(bx << 3);
        uint8_t span = (uint8_t)(g->width - base > 8 ? 8 : g->width - base);
        for (uint8_t b = 0; b < span; b++)
          if (bits & (0x80u >> b)) setPixel((int16_t)(gx + base + b), ctx);
      }
    }
    pen = (int16_t)(pen + g->advance);
  }
}

typedef struct {
  void (*fn)(int16_t x, int16_t y, void *ctx);
  void *ctx;
  int16_t y;
} AllCtx;

static void rowToXy(int16_t x, void *ctx) {
  AllCtx *a = (AllCtx *)ctx;
  a->fn(x, a->y, a->ctx);
}

void arabicRenderAll(const ArabicFont *font, const uint16_t *shaped,
                     size_t count, int16_t x, int16_t y,
                     void (*setPixel)(int16_t x, int16_t y, void *ctx),
                     void *ctx) {
  AllCtx a = {setPixel, ctx, 0};
  for (uint16_t r = 0; r < font->lineHeight; r++) {
    a.y = (int16_t)(y + r);
    arabicRenderRow(font, shaped, count, x, 0, r, rowToXy, &a);
  }
}
