#include <stdio.h>
#include <string.h>

#include "../src/ArabicFont.h"
#include "../src/ArabicRenderer.h"
#include "../src/ArabicShaper.h"

static int failures, checks;

#define CHECK(cond)                                              \
  do {                                                           \
    checks++;                                                    \
    if (!(cond)) {                                               \
      failures++;                                                \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
    }                                                            \
  } while (0)

static void checkShape(const char *label, const char *utf8, uint8_t flags,
                       const uint16_t *want, size_t wantN) {
  uint16_t got[96];
  size_t n = arabicShape(utf8, got, 96, flags);
  checks++;
  if (n != wantN || memcmp(got, want, wantN * sizeof *want) != 0) {
    failures++;
    printf("FAIL shape %s\n  want:", label);
    for (size_t i = 0; i < wantN; i++) printf(" %04X", want[i]);
    printf("\n  got: ");
    for (size_t i = 0; i < n; i++) printf(" %04X", got[i]);
    printf("\n");
  }
}

// ---- fake font fixture ----

static const uint8_t kBitmap[] = {
    0xA0,        // 'A' row 0: cols 0,2
    0x40,        // 'A' row 1: col 1
    0xFF, 0x80,  // FE90 row 0: cols 0..8 (2-byte row)
};

static const ArabicGlyph kGlyphs[] = {
    {0x0041, 0, 3, 2, 0, 1, 4},   // 'A'
    {0xFE90, 2, 9, 1, 1, 0, 10},  // beh final, 9px wide
};

static const ArabicFont kFont = {8, 6, 2, kGlyphs, kBitmap};

typedef struct {
  uint8_t px[8][32];
} Grid;

static void gridSet(int16_t x, int16_t y, void *ctx) {
  Grid *g = (Grid *)ctx;
  if (x >= 0 && x < 32 && y >= 0 && y < 8) g->px[y][x] = 1;
}

typedef struct {
  int16_t xs[32];
  int n;
} RowHits;

static void rowSet(int16_t x, void *ctx) {
  RowHits *r = (RowHits *)ctx;
  if (r->n < 32) r->xs[r->n++] = x;
}

int main(void) {
  // Isolated word: kaf-teh-beh, visual order = final..initial
  checkShape("kataba", "كتب", 0, (const uint16_t[]){0xFE90, 0xFE98, 0xFEDB}, 3);

  // Right-joiners break joins: dal-reh-seen all isolated
  checkShape("dars", "درس", 0, (const uint16_t[]){0xFEB1, 0xFEAD, 0xFEA9}, 3);

  // Lam-alef isolated + final
  checkShape("laa", "لا", 0, (const uint16_t[]){0xFEFB}, 1);
  checkShape("bilaa", "بلا", 0, (const uint16_t[]){0xFEFC, 0xFE91}, 2);

  // Alef-lam-alef: alef breaks join, lam-alef isolated
  checkShape("alef-lam-alef", "الا", 0, (const uint16_t[]){0xFEFB, 0xFE8D}, 2);

  // Lam-alef madda / hamza-above / hamza-below variants
  checkShape("lam-madda", "لآ", 0, (const uint16_t[]){0xFEF5}, 1);
  checkShape("lam-hamza-above", "بلأ", 0, (const uint16_t[]){0xFEF8, 0xFE91}, 2);
  checkShape("lam-hamza-below", "لإ", 0, (const uint16_t[]){0xFEF9}, 1);

  // Harakat stripped by default; joining unaffected across them
  checkShape("harakat-strip", "كَتَبَ", 0,
             (const uint16_t[]){0xFE90, 0xFE98, 0xFEDB}, 3);
  // Kept with flag as marks (RTL run, so fully reversed)
  checkShape("harakat-keep", "كَتَبَ", ARABIC_SHAPE_KEEP_HARAKAT,
             (const uint16_t[]){0x064E, 0xFE90, 0x064E, 0xFE98, 0x064E, 0xFEDB},
             6);

  // Mixed direction: RTL base -> first logical run rightmost
  checkShape("mixed", "ABC مرحبا 123", 0,
             (const uint16_t[]){'1', '2', '3', ' ', 0xFE8E, 0xFE92, 0xFEA3,
                                0xFEAE, 0xFEE3, ' ', 'A', 'B', 'C'},
             13);

  // ZWNJ breaks a join, is not output
  checkShape("zwnj", "ب\xE2\x80\x8Cب", 0,
             (const uint16_t[]){0xFE8F, 0xFE8F}, 2);
  checkShape("no-zwnj", "بب", 0, (const uint16_t[]){0xFE90, 0xFE91}, 2);

  // ZWJ forces joining forms, is not output
  checkShape("zwj-after", "ب\xE2\x80\x8D", 0, (const uint16_t[]){0xFE91}, 1);
  checkShape("zwj-before", "\xE2\x80\x8Dب", 0, (const uint16_t[]){0xFE90}, 1);

  // Neutrals: trailing punct goes to base RTL (left edge visually)
  checkShape("trailing-punct", "مرحبا!", 0,
             (const uint16_t[]){'!', 0xFE8E, 0xFE92, 0xFEA3, 0xFEAE, 0xFEE3},
             6);

  // Paren mirroring in RTL context
  checkShape("parens", "(مرحبا)", 0,
             (const uint16_t[]){'(', 0xFE8E, 0xFE92, 0xFEA3, 0xFEAE, 0xFEE3,
                                ')'},
             7);
  // Parens inside an LTR run stay unmirrored, order kept
  checkShape("ltr-parens", "a(b)c", 0,
             (const uint16_t[]){'a', '(', 'b', ')', 'c'}, 5);

  // Arabic punctuation passes through in the RTL run
  checkShape("arabic-comma", "لا،لا", 0,
             (const uint16_t[]){0xFEFB, 0x060C, 0xFEFB}, 3);

  // Arabic-Indic digits render LTR
  checkShape("indic-digits", "٠١٢", 0,
             (const uint16_t[]){0x0660, 0x0661, 0x0662}, 3);

  // Unknown codepoints pass through; hamza is non-joining
  checkShape("unknown", "é", 0, (const uint16_t[]){0x00E9}, 1);
  checkShape("hamza", "بءب", 0,
             (const uint16_t[]){0xFE8F, 0xFE80, 0xFE8F}, 3);

  // Invalid UTF-8 -> '?'
  checkShape("bad-cont", "\x80", 0, (const uint16_t[]){'?'}, 1);
  checkShape("truncated", "a\xC3", 0, (const uint16_t[]){'?', 'a'}, 2);
  checkShape("bad-lead", "\xFFz", 0, (const uint16_t[]){'z', '?'}, 2);
  checkShape("overlong", "\xC0\xAF", 0, (const uint16_t[]){'?'}, 1);

  // Capacity truncation
  {
    uint16_t small[2];
    size_t n = arabicShape("كتب", small, 2, 0);
    CHECK(n == 2);
  }

  // ---- font lookup ----
  CHECK(arabicFontGlyph(&kFont, 0x0041) == &kGlyphs[0]);
  CHECK(arabicFontGlyph(&kFont, 0xFE90) == &kGlyphs[1]);
  CHECK(arabicFontGlyph(&kFont, 0x0042) == NULL);

  // ---- measure ----
  {
    const uint16_t sh[] = {0xFE90, 0x0041};
    CHECK(arabicMeasure(&kFont, sh, 2) == 14);
    const uint16_t missing[] = {0x0042};
    CHECK(arabicMeasure(&kFont, missing, 1) == 4);  // lineHeight/2
    const uint16_t mark[] = {0x064E};
    CHECK(arabicMeasure(&kFont, mark, 1) == 0);  // marks zero-advance
    CHECK(arabicMeasureUtf8(&kFont, "A") == 4);
    CHECK(arabicMeasureUtf8(&kFont, "AB") == 8);  // B falls back
  }

  // ---- renderAll ----
  {
    Grid g;
    memset(&g, 0, sizeof g);
    const uint16_t sh[] = {0xFE90, 0x0041};
    arabicRenderAll(&kFont, sh, 2, 0, 0, gridSet, &g);
    for (int xx = 1; xx <= 9; xx++) CHECK(g.px[0][xx] == 1);  // FE90 xOffset 1
    CHECK(g.px[0][0] == 0 && g.px[0][10] == 0);
    CHECK(g.px[1][10] == 1 && g.px[1][11] == 0 && g.px[1][12] == 1);  // A row0
    CHECK(g.px[2][11] == 1 && g.px[2][10] == 0);                      // A row1
    int total = 0;
    for (int yy = 0; yy < 8; yy++)
      for (int xx = 0; xx < 32; xx++) total += g.px[yy][xx];
    CHECK(total == 9 + 3);
  }

  // ---- renderRow ----
  {
    RowHits r = {{0}, 0};
    const uint16_t sh[] = {0xFE90, 0x0041};
    arabicRenderRow(&kFont, sh, 2, 5, 10, 11, rowSet, &r);  // A row 0 only
    CHECK(r.n == 2 && r.xs[0] == 15 && r.xs[1] == 17);
    r.n = 0;
    arabicRenderRow(&kFont, sh, 2, 5, 10, 30, rowSet, &r);  // outside line box
    CHECK(r.n == 0);
    r.n = 0;
    const uint16_t miss[] = {0x0042, 0x0041};  // fallback advance shifts pen
    arabicRenderRow(&kFont, miss, 2, 0, 0, 1, rowSet, &r);
    CHECK(r.n == 2 && r.xs[0] == 4 && r.xs[1] == 6);
  }

  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
