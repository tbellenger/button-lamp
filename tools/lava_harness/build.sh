#!/bin/bash
# Compile a SpiralLamp sketch as a host program with Arduino/NeoPixel stubs.
# Usage: build.sh [out_dir] [v2|clean|r4|path/to/sketch.ino]
# Default sketch is sketches/SpiralLamp_R3_FireTrail/SpiralLamp_R3_FireTrail.ino (the fire trail).
# Short aliases: v2 / firetrail, clean / base, r4 / r4-firetrail.
# "r4" builds sketches/SpiralLamp_R4_FireTrail (Uno R4 port, same lava code).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-/tmp/lava-harness}"
WHICH="${2:-v2}"
case "$WHICH" in
  v2|firetrail|SpiralLamp_R3_FireTrail) SKETCH="$ROOT/sketches/SpiralLamp_R3_FireTrail/SpiralLamp_R3_FireTrail.ino" ;;
  clean|base|SpiralLamp_R3_Base) SKETCH="$ROOT/sketches/SpiralLamp_R3_Base/SpiralLamp_R3_Base.ino" ;;
  r4|r4-firetrail|SpiralLamp_R4_FireTrail) SKETCH="$ROOT/sketches/SpiralLamp_R4_FireTrail/SpiralLamp_R4_FireTrail.ino" ;;
  *) SKETCH="$WHICH" ;;
esac
if [[ "$SKETCH" != /* ]]; then
  SKETCH="$ROOT/$SKETCH"
fi
if [[ ! -f "$SKETCH" ]]; then
  echo "sketch not found: $SKETCH" >&2
  exit 1
fi
mkdir -p "$OUT"
echo "sketch $SKETCH"
arduino-cli compile --preprocess -b arduino:avr:uno \
  "$SKETCH" > "$OUT/sketch_pre.cpp"
g++ -std=gnu++17 -O2 -Wall -Wextra -Wno-unused-parameter -DHOST_LAVA_HARNESS \
  -I "$ROOT/tools/lava_harness/stub" \
  -I "$OUT" \
  -o "$OUT/lava_harness" \
  "$ROOT/tools/lava_harness/harness.cpp"
echo "built $OUT/lava_harness"
