// Baseline alignment proof: renders a Latin line (NewYork bitmap font) and an
// Arabic line (generated Naskh font) on one common baseline, as ASCII art and
// as a PGM image (with a gray guide line at the baseline row).
//
// Needs the NewYork font headers from the NiceBitmapFonts repo (not vendored
// here). Compile with -I pointing at it:
//
//   cc -o baseline_proof examples/baseline_proof.c \
//      src/ArabicShaper.c src/ArabicRenderer.c src/ArabicFont.c \
//      -I /Users/felix/Gits/NiceBitmapFonts
//
// Usage: baseline_proof [out.pgm]
//
// NewYork format: 1 byte per pixel, 0 = ink, 255 = blank; all glyphs are
// 25 rows tall (the struct's declared height field is wrong — ignore it).
#include <stdio.h>
#include <string.h>

#include "../src/ArabicRenderer.h"
#include "../src/ArabicShaper.h"
#include "../src/generated/NaskhArabic18Font.h"
#ifdef ARABIC_PROOF_FONT2_HEADER
#include ARABIC_PROOF_FONT2_HEADER
#endif

#include "NewYork25Font.h"

#define NY_ROWS 25
#define FB_W 360
#define FB_H 48
#define SCALE 4

static uint8_t fb[FB_H][FB_W];  // 0 = blank, 1 = ink, 2 = guide

static const FontCharacter *nyChar(unsigned int c) {
  for (int i = 0; i < bitmap_newyork24Font.characterCount; i++)
    if (bitmap_newyork24CharactersData[i].representedCharacter == c)
      return &bitmap_newyork24CharactersData[i];
  return NULL;
}

static int nyInk(const FontCharacter *fc, unsigned int row, unsigned int col) {
  return bitmap_newyork24FontData[fc->offset + row * fc->width + col] == 0;
}

// Lowest/highest ink rows across the 25-row box; -1 if no ink.
static void nyInkRows(const FontCharacter *fc, int *top, int *bottom) {
  *top = -1;
  *bottom = -1;
  for (unsigned int r = 0; r < NY_ROWS; r++)
    for (unsigned int c = 0; c < fc->width; c++)
      if (nyInk(fc, r, c)) {
        if (*top < 0) *top = (int)r;
        *bottom = (int)r;
        c = fc->width;  // next row
      }
}

static int nyDraw(const char *text, int x, int top) {
  for (const char *p = text; *p; p++) {
    const FontCharacter *fc = nyChar((unsigned char)*p);
    if (!fc) continue;
    if (*p == ' ') {
      x += 6;
      continue;
    }
    for (unsigned int r = 0; r < NY_ROWS; r++)
      for (unsigned int c = 0; c < fc->width; c++)
        if (nyInk(fc, r, c)) {
          int px = x + (int)c, py = top + (int)r;
          if (px >= 0 && px < FB_W && py >= 0 && py < FB_H) fb[py][px] = 1;
        }
    x += (int)fc->width + 1;
  }
  return x;
}

static void plot(int16_t x, int16_t y, void *ctx) {
  (void)ctx;
  if (x >= 0 && x < FB_W && y >= 0 && y < FB_H) fb[y][x] = 1;
}

static void arabicInkExtents(const ArabicFont *font, uint16_t cp, int *ascent,
                             int *descent) {
  const ArabicGlyph *g = arabicFontGlyph(font, cp);
  *ascent = *descent = 0;
  if (!g) return;
  *ascent = font->baseline - g->yOffset;
  *descent = g->yOffset + g->height - font->baseline;
}

static void report(const ArabicFont *font, const char *label) {
  int a, d;
  printf("%s: lineHeight=%d baseline=%d\n", label, font->lineHeight,
         font->baseline);
  arabicInkExtents(font, 0xFE8D, &a, &d);
  printf("  alef isolated (FE8D): ascent=%d descent=%d\n", a, d);
  arabicInkExtents(font, 0xFE8F, &a, &d);
  printf("  beh isolated (FE8F): ascent=%d descent=%d\n", a, d);
}

int main(int argc, char **argv) {
  const FontCharacter *H = nyChar('H'), *g = nyChar('g'), *x = nyChar('x');
  int hTop, hBot, gTop, gBot, xTop, xBot;
  nyInkRows(H, &hTop, &hBot);
  nyInkRows(g, &gTop, &gBot);
  nyInkRows(x, &xTop, &xBot);
  // ArabicFont.baseline counts rows above the baseline coordinate (alef's
  // bottom ink row is baseline-1), so NY's equivalent is hBot + 1.
  int nyBaseline = hBot + 1;
  printf("NewYork24 (25-row box): H ink rows %d..%d -> baseline %d, "
         "cap height %d\n", hTop, hBot, nyBaseline, hBot - hTop + 1);
  printf("  g ink rows %d..%d -> descender depth %d; x-height %d\n",
         gTop, gBot, gBot - nyBaseline, xBot - xTop + 1);

#ifdef ARABIC_PROOF_FONT
  const ArabicFont *afont = &ARABIC_PROOF_FONT;
#else
  const ArabicFont *afont = &naskhArabic18Font;
#endif
  report(afont, "Arabic font");

  // Common baseline: both boxes must fit in FB_H.
  int commonBaseline = nyBaseline > afont->baseline ? nyBaseline
                                                    : afont->baseline;
  int nyTop = commonBaseline - nyBaseline;
  int arTop = commonBaseline - afont->baseline;
  printf("common baseline row %d (NY top %d, Arabic lineTop %d)\n\n",
         commonBaseline, nyTop, arTop);

  memset(fb, 0, sizeof fb);
  int penX = nyDraw("Hxg 123", 2, nyTop);

  uint16_t shaped[64];
  size_t n = arabicShape("\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 "
                         "\xd8\xa8\xd8\xa7\xd9\x84\xd8\xb9\xd8\xa7\xd9\x84"
                         "\xd9\x85",  // مرحبا بالعالم
                         shaped, 64, 0);
  arabicRenderAll(afont, shaped, n, (int16_t)(penX + 8), (int16_t)arTop, plot,
                  NULL);
  int width = penX + 8 + arabicMeasure(afont, shaped, n) + 2;
  if (width > FB_W) width = FB_W;
  int height = commonBaseline +
               ((NY_ROWS - nyBaseline) > (afont->lineHeight - afont->baseline)
                    ? (NY_ROWS - nyBaseline)
                    : (afont->lineHeight - afont->baseline));
  if (height > FB_H) height = FB_H;

  // Guide line at the common baseline (only where there's no ink).
  for (int c = 0; c < width; c++)
    if (fb[commonBaseline][c] == 0) fb[commonBaseline][c] = 2;

  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++)
      putchar(fb[r][c] == 1 ? '#' : (fb[r][c] == 2 ? '-' : '.'));
    putchar('\n');
  }

  if (argc > 1) {
    FILE *f = fopen(argv[1], "wb");
    if (!f) {
      perror("fopen");
      return 1;
    }
    fprintf(f, "P5\n%d %d\n255\n", width * SCALE, height * SCALE);
    for (int r = 0; r < height * SCALE; r++)
      for (int c = 0; c < width * SCALE; c++) {
        uint8_t v = fb[r / SCALE][c / SCALE];
        fputc(v == 1 ? 0 : (v == 2 ? 170 : 255), f);
      }
    fclose(f);
    printf("\nwrote %s\n", argv[1]);
  }
  return 0;
}
