# Matrix Lamp preview

A local browser view of the Matrix Lamp presets. The picture is the firmware's C++ effect core (`sketches/MatrixLamp_R4_Wifi/lamp_core.cpp`), compiled to WebAssembly. It is not a JavaScript rewrite. Fire, Rainbow, Meatballs, Plasma, Sun, Spiral, Fluid, Heart, and Matrix rain are the same functions the Uno runs. The view is after rotate 3, so the ember row sits on the bottom the way it does on the desk.

Nothing here is deployed. `npm start` only listens on `127.0.0.1`.

## One-time: Emscripten

`emcc` has to be on your PATH. The [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) is the install:

```text
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

`source ./emsdk_env.sh` is per shell. Node 18 or newer is enough for the server. No other npm packages are required.

## Build the WASM

From `web/simulator`:

```text
npm run build
```

That runs `build.sh`, which is:

```text
emcc lamp_wasm.cpp ../../sketches/MatrixLamp_R4_Wifi/lamp_core.cpp \
  -I ../../sketches/MatrixLamp_R4_Wifi \
  -O2 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=LampModule \
  -s ENVIRONMENT=web \
  -o lamp.js
```

The full exported-function list is in `build.sh`. Rebuild after any change under `sketches/MatrixLamp_R4_Wifi/lamp_core.cpp`, `matrix_modes.h`, or `matrix_fire.h`.

## Run it

From `web/simulator`, after the build:

```text
npm start
```

Open http://127.0.0.1:8080/

`PORT=8765 npm start` uses another port. The server sends `application/wasm` for `.wasm`.

The page has the same controls as the lamp: preset buttons, brightness, speed, and + Ball / − Ball while Meatballs is showing. Brightness uses the NeoPixel show scale, `(channel * (level + 1)) >> 8`, with the same 0–100 fader and cap of 40.

## Caveats

- The fire's random sequence is not the Arduino `random()` sequence, so a given flame will not match a given board frame for frame. The step math is the same function.
- The preview does not open WiFi, save presets, or run OTA. It only plays the picture.
- `npm start` before `npm run build` loads the page and says `lamp.js` is missing.
