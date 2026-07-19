// Shapes and renders a UTF-8 string to a 1x PGM on stdout, word-wrapping
// to maxWidth px if given (lines are right-aligned, RTL reading order).
// Build: cc -std=c11 -I src examples/render_line.c src/ArabicFont.c \
//        src/ArabicShaper.c src/ArabicRenderer.c -o render_line
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ArabicShaper.h"
#include "ArabicRenderer.h"
#include "generated/NaskhArabic28Font.h"

enum { MAX_LINES = 16 };

static uint8_t *fb;
static int fbw, fbh;
static void set(int16_t x, int16_t y, void *ctx) {
  (void)ctx;
  if (x >= 0 && y >= 0 && x < fbw && y < fbh) fb[y * fbw + x] = 1;
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: render_line <utf8 text> [maxWidth]\n"); return 1; }
  const ArabicFont *f = &naskhArabic28Font;
  const int margin = 4;
  const int maxWidth = argc > 2 ? atoi(argv[2]) : 0;

  char text[2048];
  snprintf(text, sizeof text, "%s", argv[1]);
  char *lines[MAX_LINES];
  int lineCount = 0;
  if (maxWidth > 0) {
    // Greedy wrap: extend the line word by word while it still fits.
    char *cursor = text;
    while (*cursor && lineCount < MAX_LINES) {
      lines[lineCount] = cursor;
      char *lastFit = NULL;
      for (char *p = cursor;; p++) {
        if (*p != ' ' && *p != '\0') continue;
        const char saved = *p;
        *p = '\0';
        const int w = arabicMeasureUtf8(f, cursor);
        if (w <= maxWidth || lastFit == NULL) {
          lastFit = p;
          *p = saved;
          if (saved == '\0') { cursor = p; break; }
        } else {
          *p = saved;
          *lastFit = '\0';
          cursor = lastFit + 1;
          break;
        }
      }
      lineCount++;
    }
  } else {
    lines[lineCount++] = text;
  }

  int widths[MAX_LINES];
  fbw = 0;
  for (int i = 0; i < lineCount; i++) {
    widths[i] = arabicMeasureUtf8(f, lines[i]);
    if (widths[i] > fbw) fbw = widths[i];
  }
  fbw += 2 * margin;
  fbh = lineCount * f->lineHeight;
  fb = calloc((size_t)fbw * fbh, 1);

  for (int i = 0; i < lineCount; i++) {
    uint16_t shaped[512];
    const size_t n = arabicShape(lines[i], shaped, 512, 0);
    arabicRenderAll(f, shaped, n, (int16_t)(fbw - margin - widths[i]),
                    (int16_t)(i * f->lineHeight), set, NULL);
  }
  printf("P5\n%d %d\n255\n", fbw, fbh);
  for (int i = 0; i < fbw * fbh; i++) putchar(fb[i] ? 255 : 0);
  return 0;
}
