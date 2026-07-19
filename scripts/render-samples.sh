#!/bin/bash
# Renders README/demo images through the real bitmap pipeline:
#   1. alphabet-table.png — every letter in isolated/final/medial/initial form
#   2. pangram.png       — Arabic pangram (all 28 letters in one sentence)
# Usage: scripts/render-samples.sh [output-dir]   (default: ~/Desktop)
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-$HOME/Desktop}"
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT

SRC="src/ArabicFont.c src/ArabicShaper.c src/ArabicRenderer.c"
cc -std=c11 -I src examples/glyph_table.c $SRC -o "$BUILD/glyph_table"
cc -std=c11 -I src examples/render_line.c $SRC -o "$BUILD/render_line"

PANGRAM="نص حكيم له سر قاطع وذو شأن عظيم مكتوب على ثوب أخضر ومغلف بجلد أزرق"

"$BUILD/glyph_table" > "$BUILD/table.pgm"
swift tools/ComposePng.swift "$BUILD/table.pgm" "$OUT/alphabet-table.png" 5

"$BUILD/render_line" "$PANGRAM" > "$BUILD/pangram.pgm"
swift tools/ComposePng.swift "$BUILD/pangram.pgm" "$OUT/pangram.png" 4
