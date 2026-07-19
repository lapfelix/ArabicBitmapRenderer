# ArabicBitmapRenderer

<img alt="Hello, world — rendered by the bitmap pipeline" src="https://github.com/user-attachments/assets/ff899b7a-fbff-4929-8831-0bd013b4cce2" width="680" />

A small toolkit for rendering Arabic text on tiny microcontroller displays
(RP2040-class). Glyphs are pre-rendered from [Noto Naskh
Arabic](https://fonts.google.com/noto/specimen/Noto+Naskh+Arabic) on a Mac;
the runtime is plain C99 with no allocation, no floats, and no curve math —
just table lookups and bit-blitting.

- **Generator** (`tools/GenerateArabicFont.swift`) — CoreText renders each
  contextual form at a fixed pixel size and weight, packed into a C header:
  1 bit/pixel bitmaps, tight bounding boxes, integer advances. The full
  alphabet in every form fits in a few KB of flash (~5.5 KB at 28 px).
- **Shaper** (`src/ArabicShaper`) — isolated/initial/medial/final forms,
  lam-alef ligatures, harakat, ZWJ/ZWNJ, and enough bidi to lay out mixed
  RTL/LTR lines in visual order.
- **Renderer** (`src/ArabicRenderer`) — one-pass width measurement and
  scanline or framebuffer blitting, suited to row-streaming LCD drivers.

## Usage

```sh
swift tools/GenerateArabicFont.swift \
  fonts/noto-naskh-arabic/NotoNaskhArabic-VariableFont_wght.ttf \
  --size 28 --weight 400 --out src/generated
```

Compile `src/*.c` plus the generated header into your firmware. Shape once
per content change, reuse the shaped array every frame; use
`arabicMeasureUtf8()` for quick layout decisions. `DESIGN.md` has the data
format, API, and edge-case notes.

## Samples

Every letter in its contextual forms, and the classic Arabic pangram —
both drawn by the actual runtime (`scripts/render-samples.sh` regenerates
them):

<img alt="Alphabet table: every letter in isolated, final, medial, and initial form" src="https://github.com/user-attachments/assets/048f7cce-6dff-477a-a884-187dcef64bc7" width="680" />
<img alt="Arabic pangram rendered across three lines" src="https://github.com/user-attachments/assets/60b42ef9-2d86-469f-ba3e-9887b7d8414d" width="480" />

## Demo app

```sh
cd demo && swift run
```

A little macOS app for eyeballing spacing and joins: type Arabic text and
compare the bitmap pipeline against CoreText rendering the vector font at
the same size, zoomed in with a pixel grid and baseline guide.

## License

Code: MIT (see `LICENSE`).

Font: Noto Naskh Arabic, © Google, under the SIL Open Font License 1.1
(`fonts/noto-naskh-arabic/OFL.txt`). Generated bitmap data is derived from
the font and remains under the OFL; keep the license text alongside any
distribution of the glyph data.
