// Shapes and renders a UTF-8 string to a 1x PGM on stdout.
// Build: cc -std=c11 -I src examples/render_line.c src/ArabicFont.c \
//        src/ArabicShaper.c src/ArabicRenderer.c -o render_line
#include <stdio.h>
#include <stdlib.h>
#include "ArabicShaper.h"
#include "ArabicRenderer.h"
#include "generated/NaskhArabic28Font.h"

static uint8_t *fb;
static int fbw, fbh;
static void set(int16_t x, int16_t y, void *ctx) {
  (void)ctx;
  if (x >= 0 && y >= 0 && x < fbw && y < fbh) fb[y * fbw + x] = 1;
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: render_line <utf8 text>\n"); return 1; }
  const ArabicFont *f = &naskhArabic28Font;
  uint16_t shaped[512];
  size_t n = arabicShape(argv[1], shaped, 512, 0);
  const int margin = 4;
  fbw = arabicMeasure(f, shaped, n) + 2 * margin;
  fbh = f->lineHeight;
  fb = calloc((size_t)fbw * fbh, 1);
  arabicRenderAll(f, shaped, n, (int16_t)margin, 0, set, NULL);
  printf("P5\n%d %d\n255\n", fbw, fbh);
  for (int i = 0; i < fbw * fbh; i++) putchar(fb[i] ? 255 : 0);
  return 0;
}
