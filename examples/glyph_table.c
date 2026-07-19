// Renders the full alphabet as a table: one row per letter, columns =
// isolated / final / medial / initial (joined against tatweel so the
// connecting strokes are visible). Output: PGM on stdout, 1x scale.
// Build: cc -std=c11 -I src examples/glyph_table.c src/ArabicFont.c \
//        src/ArabicShaper.c src/ArabicRenderer.c -o glyph_table
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ArabicShaper.h"
#include "ArabicRenderer.h"
#include "generated/NaskhArabic28Font.h"

enum { CELL_W = 36, CELL_GAP = 2, GROUP_GAP = 14, GROUPS = 3 };

static const uint16_t letters[] = {
    0x0621, 0x0622, 0x0623, 0x0624, 0x0625, 0x0626, 0x0627, 0x0628, 0x0629,
    0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x062F, 0x0630, 0x0631, 0x0632,
    0x0633, 0x0634, 0x0635, 0x0636, 0x0637, 0x0638, 0x0639, 0x063A, 0x0641,
    0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647, 0x0648, 0x0649, 0x064A,
};

static uint8_t *fb;
static int fbw, fbh;
static void set(int16_t x, int16_t y, void *ctx) {
  (void)ctx;
  if (x >= 0 && y >= 0 && x < fbw && y < fbh) fb[y * fbw + x] = 1;
}

static size_t utf8(uint16_t cp, char *out) {
  if (cp < 0x80) { out[0] = (char)cp; return 1; }
  if (cp < 0x800) {
    out[0] = (char)(0xC0 | cp >> 6);
    out[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  }
  out[0] = (char)(0xE0 | cp >> 12);
  out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
  out[2] = (char)(0x80 | (cp & 0x3F));
  return 3;
}

int main(void) {
  const ArabicFont *f = &naskhArabic28Font;
  const int letterCount = (int)(sizeof letters / sizeof *letters);
  const int rowsPerGroup = (letterCount + GROUPS - 1) / GROUPS;
  const int rowH = f->lineHeight - 10;  // Naskh line box has generous slack
  const int groupW = 4 * CELL_W + GROUP_GAP;
  fbw = GROUPS * groupW - GROUP_GAP + 2 * CELL_GAP;
  fbh = rowsPerGroup * rowH + CELL_GAP;
  fb = calloc((size_t)fbw * fbh, 1);

  for (int i = 0; i < letterCount; i++) {
    const uint16_t tatweel = 0x0640;
    // Column order (right-to-left like an alphabet chart): iso, final, medial, initial.
    const uint16_t samples[4][3] = {
        {letters[i], 0, 0},
        {tatweel, letters[i], 0},
        {tatweel, letters[i], tatweel},
        {letters[i], tatweel, 0},
    };
    const int group = i / rowsPerGroup;
    const int row = i % rowsPerGroup;
    for (int form = 0; form < 4; form++) {
      char text[16];
      size_t n = 0;
      for (int c = 0; c < 3 && samples[form][c]; c++) n += utf8(samples[form][c], text + n);
      text[n] = '\0';
      uint16_t shaped[8];
      size_t count = arabicShape(text, shaped, 8, 0);
      const int x = CELL_GAP + group * groupW + form * CELL_W +
                    (CELL_W - arabicMeasure(f, shaped, count)) / 2;
      arabicRenderAll(f, shaped, count, (int16_t)x, (int16_t)(CELL_GAP + row * rowH), set, NULL);
    }
  }

  printf("P5\n%d %d\n255\n", fbw, fbh);
  for (int i = 0; i < fbw * fbh; i++) putchar(fb[i] ? 255 : 0);
  return 0;
}
