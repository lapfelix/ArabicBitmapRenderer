# ArabicBitmapRenderer

Pre-cooked Arabic bitmap font rendering for tiny microcontrollers (RP2040-class),
designed to slot into the `BitmapFont` rendering model used by PicoOled.

- **Offline generator** (`tools/GenerateArabicFont.swift`): renders Noto Naskh
  Arabic at a fixed pixel size and weight (e.g. 18 px @ wght 450) via CoreText
  on macOS, emitting a compact C header — 1 bit/pixel packed bitmaps, tight
  bounding boxes, integer advances/bearings.
- **Runtime** (`src/`): C99, no allocation, no floats, no curve math.
  - `ArabicShaper` — contextual shaping (isolated/initial/medial/final),
    lam-alef ligatures, harakat handling, ZWJ/ZWNJ, bidi-lite run reordering
    into visual order.
  - `ArabicRenderer` — fast width measurement (one pass, sum of advances) and
    scanline/framebuffer blitting.

See `DESIGN.md` for the data format, API, and edge-case notes.

## Usage

```sh
swift tools/GenerateArabicFont.swift \
  fonts/noto-naskh-arabic/NotoNaskhArabic-VariableFont_wght.ttf \
  --size 18 --weight 450 --out src/generated
```

Then compile `src/*.c` + the generated header into your firmware. Shape once
per content change, reuse the shaped array every frame; measure with
`arabicMeasureUtf8()` when laying out mixed RTL/LTR lines.

## License

Code: MIT (see `LICENSE`).

Font: Noto Naskh Arabic, © Google, licensed under the SIL Open Font License 1.1
(`fonts/noto-naskh-arabic/OFL.txt`). Generated bitmap data is a derivative of
the font and remains under the OFL; the license text must accompany any
distribution that includes the generated glyph data.
