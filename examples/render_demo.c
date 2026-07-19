// Usage: render_demo "<utf-8 text>"
// Shapes the text; with generated font data present, renders ASCII art.
#include <stdio.h>
#include <string.h>

#include "../src/ArabicRenderer.h"
#include "../src/ArabicShaper.h"

#if defined(__has_include)
#if __has_include("../src/generated/ArabicFontData.h")
#include "../src/generated/ArabicFontData.h"
#define HAVE_FONT 1
#endif
#endif

#define MAX_W 256

#ifdef HAVE_FONT
static char grid[64][MAX_W];

static void plot(int16_t x, int16_t y, void *ctx) {
  (void)ctx;
  if (x >= 0 && x < MAX_W && y >= 0 && y < 64) grid[y][x] = '#';
}
#endif

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s \"text\"\n", argv[0]);
    return 2;
  }
  uint16_t shaped[128];
  size_t n = arabicShape(argv[1], shaped, 128, 0);
  printf("shaped (%zu, visual L-to-R):", n);
  for (size_t i = 0; i < n; i++) printf(" %04X", shaped[i]);
  printf("\n");

#ifdef HAVE_FONT
#ifndef ARABIC_DEMO_FONT
#define ARABIC_DEMO_FONT arabicFont  // expected generated symbol
#endif
  const ArabicFont *font = &ARABIC_DEMO_FONT;
  uint16_t w = arabicMeasure(font, shaped, n);
  if (w > MAX_W) w = MAX_W;
  memset(grid, '.', sizeof grid);
  arabicRenderAll(font, shaped, n, 0, 0, plot, NULL);
  for (uint16_t y = 0; y < font->lineHeight && y < 64; y++)
    printf("%.*s\n", w, grid[y]);
#else
  printf("(no generated font data; shaped output only)\n");
#endif
  return 0;
}
