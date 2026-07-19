# Arabic Bitmap Renderer — Design

Target: RP2040-class microcontrollers driving small OLED/LCD panels (see
PicoOled's `BitmapFont` model). Everything below is integer-only, no floats,
no curve math at runtime. Glyph bitmaps are pre-rendered ("pre-cooked") on a
Mac from Noto Naskh Arabic at a fixed pixel size and weight (default 450) by
a Swift + CoreText generator.

## Components

1. `tools/GenerateArabicFont.swift` — offline generator (macOS, CoreText).
2. `src/ArabicFont.h` — data-format structs shared by generator output and runtime.
3. `src/ArabicShaper.{h,c}` — Unicode Arabic → shaped glyph sequence (visual order).
4. `src/ArabicRenderer.{h,c}` — measurement + per-row blitting compatible with
   scanline renderers like PicoOled's LCD path.
5. `examples/` + host tests — render to ASCII/PGM on the host to verify joins.

## Data format (generated .h/.c)

```c
typedef struct {
  uint16_t codepoint;   // Arabic Presentation Form (FB50–FEFF) or base char
  uint16_t offset;      // byte offset into packed bitmap blob
  uint8_t  width;       // tight bbox width in px
  uint8_t  height;      // tight bbox height in px
  int8_t   xOffset;     // left bearing: pen + xOffset = first bbox column
  int8_t   yOffset;     // bbox top relative to line top (row = yOffset + n)
  uint8_t  advance;     // pen advance in px (integer, from CoreText, rounded)
} ArabicGlyph;

typedef struct {
  uint8_t  lineHeight;  // total line box height in px
  uint8_t  baseline;    // baseline row from top (for aligning with Latin font)
  uint16_t glyphCount;
  const ArabicGlyph *glyphs;   // sorted by codepoint → binary search
  const uint8_t *bitmapData;   // 1bpp, row-major, each row padded to whole bytes
} ArabicFont;
```

- Bitmaps are 1 bit per pixel, MSB-first within a byte, each glyph row starts
  on a byte boundary (`rowBytes = (width + 7) / 8`). Bit set = ink.
- Tight bounding boxes (empty rows/columns cropped) + `xOffset`/`yOffset`
  keep the blob minimal; `offset` is `uint16_t` — the generator asserts the
  blob stays under 64 KB (expected ~4–6 KB at 18 px).
- Everything goes in one `.h` (or `.h` + `.c`) with `const` arrays so it
  lives in flash, not RAM.

## Glyph set

- All Arabic letters U+0621–U+064A in their valid contextual forms, keyed by
  Arabic Presentation Forms-B codepoints (U+FE70–U+FEFF).
- Lam-alef ligatures (mandatory): U+FEF5–U+FEFC (lam + alef madda/hamza
  above/hamza below/plain, isolated + final).
- Tatweel U+0640, Arabic-Indic digits U+0660–U+0669, Arabic punctuation
  U+060C ، U+061B ؛ U+061F ؟, space.
- Optional (flag): harakat as zero-advance marks. Default: shaper strips them.
- No Latin — mixed text uses the host project's existing Latin font; `baseline`
  lets both sit on the same line.

## Generator (Swift + CoreText)

CLI: `swift tools/GenerateArabicFont.swift <font.ttf> --size 18 --weight 450 --out src/generated/`

- Instantiates the variable font at the requested `wght` via
  `CTFontCreateCopyWithAttributes` + variation axis dictionary.
- For each contextual form, builds a context string with ZWJ (U+200D):
  initial = `X + ZWJ`, medial = `ZWJ + X + ZWJ`, final = `ZWJ + X`,
  isolated = `X`; shapes it with CTLine, then extracts *only the target
  glyph's* position/advance and renders it into an 8-bit grayscale context.
- Threshold at 50% to 1bpp (Naskh at small sizes needs a checked threshold;
  generator also emits a `preview.png`/ASCII proof sheet for eyeballing).
- Records exact integer advance (rounded), bearings from the rendered origin.
- Emits the OFL license header + attribution in generated files.

## Shaper (runtime, C)

`ArabicShaper` converts UTF-8 logical order → glyph indices in visual order:

1. Decode UTF-8 (reuse-compatible with PicoOled's `nextUtf8Codepoint`).
2. Joining-class table (right-joining vs dual-joining) drives a simple state
   machine choosing isolated/initial/medial/final — pure table lookups.
3. Lam + alef pairs collapse to the ligature form.
4. Harakat (U+064B–U+0652) stripped (or emitted as zero-advance if enabled).
5. Bidi-lite: split into directional runs — Arabic runs are reversed into
   visual order; LTR runs (Latin, digits incl. Arabic-Indic) keep their order.
   Output is a flat visual-order array the renderer draws left-to-right.
   (Full UBA is out of scope; run-splitting covers titles/artist strings.)

API sketch:

```c
// Shape into caller's buffer; returns glyph count. No allocation.
size_t arabic_shape(const ArabicFont *font, const char *utf8,
                    const ArabicGlyph **out, size_t capacity);
// Fast width: one shaping pass, sum of advances. For layout decisions.
uint16_t arabic_measure(const ArabicFont *font, const char *utf8);
// Blit one scanline: for row-streaming renderers (PicoOled LCD path).
// setPixel(x) is called for each ink pixel in [x0, ...) on this row.
void arabic_render_row(const ArabicFont *font, const ArabicGlyph *const *glyphs,
                       size_t count, int16_t x0, uint16_t lineTop, uint16_t row,
                       void (*setPixel)(int16_t x, void *ctx), void *ctx);
```

Shaping a marquee line once per content change (then reusing the shaped
array every frame, like PicoOled's `TextLine`) keeps per-frame cost to pure
blitting.

## Edge cases tracked

- Non-joining chars (space, punctuation, digits) break joining context.
- Alef & other right-joining-only letters never take initial/medial forms.
- Lam-alef inside a joined word takes the *final* ligature form.
- ZWJ/ZWNJ in input honored as join/non-join hints; other controls dropped.
- Unknown codepoints → fallback box glyph (index 0), never desync joining.
- Mirrored punctuation (parens) swapped in RTL runs.

## Licensing

Noto Naskh Arabic is © Google, SIL Open Font License 1.1. `fonts/.../OFL.txt`
ships in-repo; generated files embed the OFL notice. Bitmap renderings of the
font are derivative works — permitted under OFL including embedding, provided
the license accompanies them. Generated font data keeps the Reserved Font
Name rules by not calling itself "Noto".
