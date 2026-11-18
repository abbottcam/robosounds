#!/usr/bin/env bash
# Builds the screen layouts on a computer and writes PNG previews to out/.
#   ./build.sh <path to an ArduinoJson checkout> [config.json]
# Needs g++, zlib and Python with Pillow.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
AJSON="$1"; CONFIG="${2:-$HERE/sample.json}"
OUT="$HERE/out"; mkdir -p "$OUT"
g++ -std=c++17 -O2 -w -DSLATE_HOST_PREVIEW \
  -I"$AJSON/src" -I"$HERE/../../src" \
  "$HERE/preview.cpp" "$HERE/../../src/gfx.cpp" "$HERE/../../src/render.cpp" \
  -lz -o "$OUT/preview"
"$OUT/preview" "$CONFIG" "$OUT"
python3 "$HERE/to_png.py" "$OUT"
