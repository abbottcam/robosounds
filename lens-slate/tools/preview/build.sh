#!/usr/bin/env bash
# Builds the screen layouts on a computer and writes PNG previews.
#   ./build.sh <path to LilyGo-EPD47 checkout> <path to ArduinoJson checkout> [config.json]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LILYGO="$1"; AJSON="$2"; CONFIG="${3:-$HERE/sample.json}"
OUT="$HERE/out"; mkdir -p "$OUT/fonts"
# The font files include "epd_driver.h" from their own folder, so copy them next
# to the stand-in header instead of using the real (ESP32-only) one.
cp "$HERE/host/epd_driver.h" "$OUT/fonts/"
cp "$LILYGO"/src/{firasans,roboto12,roboto18,roboto32}.h "$OUT/fonts/"
g++ -std=c++17 -O2 -w -DSLATE_HOST_PREVIEW \
  -I"$OUT/fonts" -I"$AJSON/src" -I"$HERE/../../src" \
  "$HERE/preview.cpp" "$HERE/../../src/gfx.cpp" "$HERE/../../src/render.cpp" \
  -lz -o "$OUT/preview"
"$OUT/preview" "$CONFIG" "$OUT"
python3 "$HERE/to_png.py" "$OUT"
