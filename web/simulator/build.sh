#!/usr/bin/env bash
# Build the shared effect core to WebAssembly.
# Requires Emscripten (emcc) on PATH. See README.md for the one-time install.
set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
src="$root/sketches/MatrixLamp_R4_Wifi"

if ! command -v emcc >/dev/null 2>&1; then
  echo "emcc was not found on PATH." >&2
  echo "Install Emscripten, then run: source emsdk_env.sh" >&2
  echo "https://emscripten.org/docs/getting_started/downloads.html" >&2
  exit 1
fi

emcc "$here/lamp_wasm.cpp" "$src/lamp_core.cpp" \
  -I "$src" \
  -O2 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=LampModule \
  -s ENVIRONMENT=web \
  -s EXPORTED_FUNCTIONS='["_lamp_init","_lamp_set","_lamp_mark_dirty","_lamp_mode_count","_lamp_mode_name","_lamp_balls_add","_lamp_balls_remove","_lamp_balls_set","_lamp_balls","_lamp_step","_lamp_frame","_lamp_applied","_malloc","_free"]' \
  -s EXPORTED_RUNTIME_METHODS='["cwrap","UTF8ToString","HEAPU8"]' \
  -o "$here/lamp.js"

echo "Wrote $here/lamp.js and $here/lamp.wasm"
