#!/bin/bash
# Compile SpiralLamp_clean.ino as a host program with Arduino/NeoPixel stubs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-/tmp/lava-harness}"
mkdir -p "$OUT"
arduino-cli compile --preprocess -b arduino:avr:uno \
  "$ROOT/sketches/SpiralLamp_clean/SpiralLamp_clean.ino" > "$OUT/sketch_pre.cpp"
g++ -std=gnu++17 -O2 -Wall -Wextra -Wno-unused-parameter -DHOST_LAVA_HARNESS \
  -I "$ROOT/tools/lava_harness/stub" \
  -I "$OUT" \
  -o "$OUT/lava_harness" \
  "$ROOT/tools/lava_harness/harness.cpp"
echo "built $OUT/lava_harness"
