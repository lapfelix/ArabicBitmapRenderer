#!/bin/sh
set -e
cd "$(dirname "$0")"
cc -std=c99 -Wall -Wextra -Werror -o /tmp/arabic_test_shaper \
  test_shaper.c ../src/ArabicShaper.c ../src/ArabicFont.c ../src/ArabicRenderer.c
/tmp/arabic_test_shaper
