# Spiral Lamp / Button Box

Tom's Arduino spiral lamp: an Arduino Uno R3 (ATmega328P) on a screw-terminal shield, driving a BTF FCOB strip (5 mm, 160 LEDs/m, cut to the full metre = 160 LEDs) from a 5 V 3 A supply. The panel has arcade buttons, a key switch, a mushroom stop switch, a toggle, and slide and rotary pots. `sketches/SpiralLamp_R4_FireTrail/` is the same lamp for an Uno R4 WiFi and a 5 V 2 A supply (section 3). `sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI/` adds the web page and AppleMIDI (section 4). `sketches/MatrixLamp_R4_Wifi/` is the onboard 12×8 matrix only, with nothing wired to the board (section 5). Sketch names are `SpiralLamp_<Board>_<FeatureTags>` (see Sketches).

Upload the sketches in `sketches/`. `original-plans/` is the purchased set (STLs, four original `.ino` versions, and PDFs), kept for reference.

## Enclosure + tube spiral

There are three casings, all the same 250 × 210 × 120 mm wedge:

| Casing | Where | Controls mount in |
|---|---|---|
| Hand-cut 3 mm ply, butt joints + glue strips | [plywood/README.md](plywood/README.md), plan [plywood/PLAN.pdf](plywood/PLAN.pdf) | the printed Switch Panel, screwed on the slope |
| **Laser-cut 1/8" birch, finger joints (SendCutSend)** | [plywood/fingerjoint/README.md](plywood/fingerjoint/README.md) | **v2:** straight in the birch control slope (no printed panel). The set as ordered (S7872337) and a birch retrofit faceplate are there too |
| **Bent sheet metal, 5052 Al .063" or mild steel .059" (SendCutSend)** | [sheetmetal/README.md](sheetmetal/README.md) | straight in the hood's sloped face |

The two laser-cut versions share one control layout (`plywood/fingerjoint/src/controls.py`, measured from the printed Switch Panel). Both have a 12.2 mm DC jack hole on the back for a 5 V 10 A supply, and keep the USB window for the Uno R4 WiFi's USB-C. The spiral holder is unchanged for now.

A 3D-printable, support-free 16×16 WS2812B LED matrix and 162×162 mm polycarbonate diffuser attachment swaps onto the 3" 4-bolt top deck in place of the spiral holder: [enclosures/matrix_16x16_mount/](enclosures/matrix_16x16_mount/).

The strip is the BTF FCOB 160/m 5 mm strip, left at **1000 mm / 160 LEDs** (it was cut a bit long for the old 900 mm / 144 LED plan), in the 13 mm JARBIH silicone tube. `LED_COUNT` is 160. The coil is sized for that metre: 1037 mm of tube, 4.75 turns.

## Quick start

Run the bench test before the full sketch. The lamp sketches need the [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) library (Arduino IDE → Library Manager → Adafruit NeoPixel). The Uno R4 WiFi port needs 1.12.1 or newer.

Authoritative wiring is [docs/wiring.md](docs/wiring.md). `docs/wiring_diagram_OUTDATED.png` is outdated: it shows the strip powered from the Arduino. Do not follow it.

### 1. Bench test

Unplug USB and the wall adapter, then wire:

- Blue button: COM → GND, NO → D4 (cycles colour)
- Red button: COM → GND, NO → D8 (cycles brightness)
- Strip 3-pin plug green (DIN) → 330 Ω → D9
- Strip 3-pin plug white → Arduino GND
- Strip red/white power pigtail → the 5 V supply (red to +, white to −)
- 3-pin plug red → leave unconnected (tape it off)
- 1000 µF electrolytic across the supply (striped leg to −)

Open `sketches/bench_test/bench_test.ino` (the folder name must match the `.ino`), upload, plug in the 5 V supply, and open Serial Monitor at **115200** baud. Boot runs a red, then green, then blue wipe, then a dim moving rainbow. The blue button steps colour/pattern; the red button steps brightness (5 → 10 → 20 → 40).

### 1b. Bench test v2 (all switches and pots)

Once the strip and both buttons work, upload `sketches/bench_test_v2/bench_test_v2.ino` and add the pots and panel switches one at a time with [docs/bench_test_v2.md](docs/bench_test_v2.md). Serial at 115200 prints every pot and switch state 4x a second.

### 2. Full sketch

When the strip and both buttons behave, open `sketches/SpiralLamp_R3_Base/SpiralLamp_R3_Base.ino` and upload that. Same library. The lamp stays dark until the rocker on D7 is closed to GND. `BRIGHTNESS_CAP` is 50 so full white on 160 LEDs stays near 1.9 A on the 5 V 3 A supply. That sketch is the known-good lamp.

`sketches/SpiralLamp_R3_FireTrail/SpiralLamp_R3_FireTrail.ino` is the same lamp plus a fire tail on the lava balls. Lava is the mushroom on D11 (Chaos), after that pin has been seen low once. Two balls bounce in from the ends; the strongest toss reaches the middle of the strip. Each ball leaves a fire tail opposite the way it is moving: orange beside the ball, then yellow, then white at the far end, dimming so the tip does not stop hard, and nothing at the top of the bounce. The tail is about a dozen LEDs at full speed and shrinks as the ball slows. It uses the same fader, colour calibration and brightness cap as the balls. Set `LAVA_FIRE_TRAIL` to 0 for plain balls. `tools/lava_harness/` builds that sketch by default (`v2` or `firetrail`). Pass `clean` or `base` for `SpiralLamp_R3_Base`, or `r4` / `r4-firetrail` for the R4 port. It prints the peak of each toss plus a few ASCII frames of the tail.

### 3. Uno R4 WiFi

`sketches/SpiralLamp_R4_FireTrail/SpiralLamp_R4_FireTrail.ino` is the FireTrail lamp built for the Arduino Uno R4 WiFi. Same pins, same 160 LEDs, same modes and the same fire trail. It does not use the radio. The web page and AppleMIDI are `SpiralLamp_R4_FireTrail_WiFi_MIDI` (section 4).

In the Arduino IDE:

- Boards Manager → install **Arduino UNO R4 Boards** (this port was compiled with 1.6.0). Tools → Board → Arduino UNO R4 WiFi.
- Library Manager → **Adafruit NeoPixel 1.12.1 or newer**. Renesas RA4M1 / Uno R4 `show()` support landed in 1.12.1. This port was compiled with 1.15.5.
- Upload over the USB-C port. Serial Monitor at 115200. The boot line is `Spiral Lamp R4 v0 starting`. Open the monitor and press reset if that line has already scrolled past; the R4 does not wait for the monitor.

The R4 is a 5 V board and its digital pins are 5 V tolerant, so the strip data line is the same 5 V signal as on the Uno R3. Wiring matches the R3 lamp:

- Strip DIN (green) → 330–470 Ω → D9
- Strip ground (white) and the supply ground → R4 GND
- Strip 5 V pigtail → a **5 V 2 A** supply, not the board's 5 V pin and not USB-C
- 1000 µF across that supply (striped leg to −)
- Pots on A0 (brightness) and A1 (speed). `analogRead` stays 10-bit, 0..1023
- Switches to GND, same pins as [docs/wiring.md](docs/wiring.md) (rocker D7, mushroom D11, and the rest)

`BRIGHTNESS_CAP` is still 50, so full white on 160 LEDs stays near 1.9 A. That fits the 2 A supply with little spare. Leave the cap there.

### 4. R4 WiFi page and Music

`sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI/` is the FireTrail lamp plus a phone page and an AppleMIDI (rtpMIDI) Music mode. Same pins, same `BRIGHTNESS_CAP` of 50. `LED_COUNT` defaults to 160. A 320-LED strip is `-DLED_COUNT=320` (about 3.8 A at cap 50; a 5 V 10 A supply has room to raise the cap later).

Copy `secrets.example.h` to `secrets.h` in that folder and put the home network in `WIFI_SSID` / `WIFI_PASS`. `secrets.h` is gitignored. `MAC_SESSION_IP` is optional (below). An empty SSID skips WiFi and the lamp still runs from the panel.

Libraries, Arduino IDE:

- Boards Manager → **Arduino UNO R4 Boards** (compiled with 1.6.0). Board: Arduino UNO R4 WiFi.
- **Adafruit NeoPixel 1.12.1 or newer** (compiled with 1.15.5). `WiFiS3` ships with the core. Do not add a separate WiFi library.
- **AppleMIDI** (lathoub/Arduino-AppleMIDI-Library) **3.5.x** (compiled with 3.5.0) and its dependency **MIDI Library** (compiled with 5.0.2).

Serial Monitor at 115200. The boot line is `Spiral Lamp R4 FireTrail WiFi MIDI starting`. On connect it prints the IP. Open `http://spirallamp.local/` or `http://spiral-lamp.local/` (or `http://<that-ip>/`). The DHCP hostname is `spirallamp`, so `http://spirallamp/` works on a router that publishes DHCP names. `spiral-lamp` without `.local` is not a second DHCP name. The AppleMIDI session is still not advertised over Bonjour.

The page selects the mode (including Music and Lava), master brightness, knob position (speed), lava top and bottom colour, chaos, and the fire trail. Live status includes WiFi and MIDI. `GET /api/state` returns the same fields as JSON. `GET /api/set?key=value&...` applies keys left to right and returns that state (`ok` false on the first bad key; earlier keys stick).

| Key | Value |
|---|---|
| `mode` | `off`, `jumbo`, `classic`, `rainbow`, `wave`, `wave-mod`, `wave-rainbow`, `wave-epic`, `secret`, `lava`, `music` |
| `brightness` | `0`–`100`. Linear. The pot's top-end stretch is only on the physical fader. |
| `speed` | `0`–`100` knob position. Centre is 1×. Modes keep their own curves. |
| `chaos` | `0`, `1`, or `toggle` |
| `fire` | `0`, `1`, or `toggle` |
| `top`, `bottom` | `0`–`6` or `next` (same order as the white and red buttons) |

Precedence: web brightness holds until the A0 fader moves about 40 counts (~4% of the travel) from the reading taken when the web value was set. Web speed holds until A1 moves that far. The two pots are independent. Web mode holds until the rocker, toggle, rainbow key, wave switch, or chaos mushroom changes; then the whole mode follows the panel together. Lava colours apply immediately and do not lock D4/D8. Fire trail has no switch, so the page keeps it. `LAVA_FIRE_TRAIL` is only the value at boot.

Music on the panel: toggle ON (D6), rocker OFF (D7), rainbow key OFF (D5), wave OFF (D10), chaos OFF (D11). That combination used to leave the strip dark. A note (or CC1 / CC7) also enters Music from Off when the page is not already holding a mode, and the page then owns the mode until a mode switch moves. It does not steal Jumbo, Rainbow, Wave, Secret, Lava, or an explicit web mode. After about 10 seconds with no note-on or note-off, Music draws a dim idle pulse and stays in Music.

Each note-on starts a glow at the bottom LED and travels up the coil (32 slots, no per-note heap). Pitch class picks one of 12 hues. Velocity sets brightness and how long the tail is (about 12% of the strip at velocity 127). Note-off fades that pulse over about 400 ms, so a held note is longer. Channel 10 flashes the whole coil and does not travel. CC1 scales travel speed (0 ≈ 0.5×, 64 = 1×, 127 = 2×) on top of the A1 knob. CC7 scales Music brightness on top of the A0 fader. Neither CC takes the pot away from the panel or the page.

AppleMIDI listens on UDP **5004** (data port **5005**). The session name is `Spiral Lamp`. The hostname resolves over mDNS, but the MIDI session itself is not advertised over Bonjour.

1. Audio MIDI Setup → MIDI Studio → double-click **Network**.
2. Enable **Session 1** (this is Logic's **Network Session 1**). Its port is 5004.
3. The lamp will not show up by itself. Under the session, add a remote participant at the lamp's IP, port **5004**, and connect. If that macOS build has no manual address field, put the Mac's address in `MAC_SESSION_IP` in `secrets.h` (not `0.0.0.0`). The lamp then calls `sendInvite()` and **Spiral Lamp** appears in the session to connect. One session at a time.
4. In Logic, use an External MIDI track, or a software instrument with the External Instrument plugin, and set the MIDI destination to **Network Session 1**. A keyboard or OP-1 plugs into the Mac; the Mac forwards it to the lamp.

The loop polls AppleMIDI and the web server every pass. Music redraws at about 60 fps. `/api/state` includes `midi` (`off`, `waiting`, `connected`), `midiSession`, `midiLast` (−1 if none), and `midiLastName` (MIDI 60 is `C4`).

```text
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi \
  --build-property compiler.cpp.extra_flags=-DLED_COUNT=320 \
  sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI
```

Core 1.6.0, NeoPixel 1.15.5, AppleMIDI 3.5.0, MIDI Library 5.0.2:

| `LED_COUNT` | Flash | Globals |
|---|---|---|
| 160 | 118816 B (45% of 262144) | 15892 B (48% of 32768), 16876 B left for locals |
| 320 | 118768 B (45% of 262144) | 16452 B (50% of 32768), 16316 B left for locals |

NeoPixel also mallocs `LED_COUNT * 3` (480 B or 960 B). The HTTP client adds a 1024 B receive FIFO on the heap. The mDNS socket is a `WiFiUDP` (about 1.5 KB) malloc'd while WiFi is up; the 512 B packet buffer is in the globals above. The two AppleMIDI UDP sockets are inside the session object (counted in the globals above). Joining WiFi does not block the frame loop (`WiFi.setTimeout(0)`, status polled). The same mDNS responder as the matrix lamp (`sketches/common/mdns_responder.h`) answers `spirallamp.local` and `spiral-lamp.local` without blocking the LED loop. In core 1.6.0, `WiFi.status()` can be `WL_CONNECTED` as soon as the radio associates, while `WiFi.localIP()` is still `0.0.0.0`. The lamp stays in joining, and does not start the page or AppleMIDI, until that address is non-zero. It re-reads the address every few seconds and updates it if DHCP changes it. A new `WiFi.begin()` waits 22 seconds, so a slow lease is not aborted. One HTTP client; accepts are paced at 30 ms; each loop does one short read or one short write. A wedged coprocessor is capped at 800 ms per modem command after the radio has started.

### 5. Onboard matrix lamp

`sketches/MatrixLamp_R4_Wifi/` drives only the Uno R4 WiFi's built-in 12×8 red LED matrix. Nothing is wired to the board: no strip, no buttons, no pots. Copy `secrets.example.h` to `secrets.h` in that folder and put the network in `WIFI_SSID` / `WIFI_PASS`. `secrets.h` is gitignored. A missing `secrets.h`, or an empty `WIFI_SSID`, skips WiFi. The matrix still runs.

Libraries: Boards Manager → **Arduino UNO R4 Boards** (compiled with 1.6.0). Board: Arduino UNO R4 WiFi. `Arduino_LED_Matrix` and `WiFiS3` ship with the core. Do not add a separate WiFi library. Serial Monitor at 115200. The boot line is `Matrix Lamp R4 WiFi starting`. On connect it prints the IP. Open `http://matrixlamp.local/` or `http://matrix-lamp.local/` (or `http://<that-ip>/`). The DHCP hostname is `matrixlamp`, so `http://matrixlamp/` works on a router that publishes DHCP names. `matrix-lamp` without `.local` is mDNS only.

The loop never blocks on the radio after the first join. `setup` calls `WiFi.begin()` with the library defaults (10 s connect wait, 10 s modem command timeout) **before** the matrix timer starts. Core 1.6.0's `begin()` returns as soon as `WiFi.status()` is `WL_CONNECTED`, which can be the moment the radio associates, while `WiFi.localIP()` is still `0.0.0.0`. The sketch does not treat that as online. It stays on the joining animation, keeps polling `localIP()`, and only then scrolls the address and starts the page. A `0.0.0.0` read after a real address is already stored is ignored, because a modem timeout returns the same value. Another `WiFi.begin()` waits 22 seconds so DHCP is not reset mid-handshake. Once an address is stored, `setTimeout(0)` and `modem.timeout(800)` bound later polls. One HTTP client on port 80. Accepts are paced at 30 ms. Each pass does one short read or one short write, so the matrix and the page stay live together. `sketches/common/mdns_responder.h` then joins `224.0.0.251:5353` and answers A queries for both `.local` names (cache-flush, TTL 120 seconds). An AAAA question gets an NSEC record and no IPv6 address, so macOS and iOS do not wait on AAAA. Replies go to the multicast group, or unicast when the query's QU bit is set or the source port is not 5353. The poll is every 50 ms and does not run during an HTTP read or write. Connecting sends a gratuitous announcement, then one more a second later. The spiral lamp uses the same header.

What the matrix plays:

- Before any frame is posted, and after a return to fire: scroll the WiFi IP, then **10 seconds** of fire, then the IP, then the fire, and so on.
- Until DHCP has given a non-zero address, that scroll slot is a waiting animation instead of an IP. It does not scroll `0.0.0.0`. The 10-second slot still runs.
- A posted frame does not stop the loop. It **replaces the fire** in the 10-second slot: IP scroll, then 10 seconds of that frame, then the IP, then the frame, and so on.
- A posted animation does the same, stepping `frames[0 .. N-1]` every `interval` milliseconds and wrapping until the 10 seconds end.
- Each new POST of LED data or an animation shows that picture immediately and restarts the 10 seconds. When they end, the IP scrolls, then the picture shows again.

Core 1.6.0's `Arduino_LED_Matrix` is 1-bit (`loadFrame` / `renderBitmap`) and its own timer is 10 kHz. It has no grayscale `draw()`. The sketch includes that header for the matrix pin map and runs its own timer at **24 kHz**: one LED at a time, Bresenham duty over levels 0..15. That is a 250 Hz scan of the 96 LEDs. The old 96 kHz tick did not leave a full UART byte time (115200 baud on the link to the ESP32-S3) between interrupts, which is enough to make `localIP()` and `begin()` time out.

`y` is the row, 0 at the top, 7 at the bottom. `x` is the column, 0 at the left. That is the same order as `renderBitmap`. A level is 0..15. A value 16..255 is an 8-bit brightness mapped with `(v * 15 + 127) / 255`. `on: false` forces that LED off. `on: true` with no brightness is level 15.

The page is a 12×8 grid. Click or drag to paint the current frame with the 0–15 brush (click a lit pixel at the current brush level to turn it off). Clear and Fill post that one frame. Under the grid is a frame strip of up to 128 frames: Previous, Next, Add frame, Duplicate, Delete, and Play / Pause (the preview stays in the browser). Interval is 20–5000 ms, default 100. **Send animation** posts the strip as a packed binary body (`Content-Type: application/octet-stream`, format A below). **Resume fire** posts `POST /api/reset` and puts the 10-second slot back on the fire. The IP scroll keeps going. Reset keeps the stored picture, so the next paint or Send animation can send it again. Loading `GET /api/state` reads `frames` as 96-character hex strings (or 8 strings of 12 hex digits) back into the strip. `leds` stays an 8×12 array of integers, so older clients that only read `leds` still work.

The fire in that slot is a solid hot ember bed on the bottom row (every pixel stays in 12–15) with two or three wide flames rising from it. Rows 5–6 are full and bright, rows 3–4 are the tapering bodies, rows 1–2 are tips, and the top row stays dark except for a rare lick that reaches y = 0.

`GET /api/state` returns JSON:

| Field | Meaning |
|---|---|
| `mode` | `fire`, `custom`, or `animation`. This is the 10-second slot, not a mode that stops the IP scroll. |
| `phase` | `scroll` or `show` |
| `wifi` | `off`, `joining`, or `connected`. `connected` means a non-zero DHCP address, not just association. |
| `ip` | Dotted address, or `0.0.0.0` while DHCP has not finished |
| `rssi` | dBm, or 0 when offline |
| `remainingMs` | Milliseconds left in the 10-second slot, otherwise 0 |
| `frameCount` | Stored frames. 0 after boot and after DELETE. 1 for a single posted frame. |
| `maxFrames` | 128. The most frames one animation can store. |
| `interval` | Frame interval in ms. Default 100. Clamped to 20–5000. |
| `frame` | Index of the animation frame in the 10-second slot, otherwise 0 |
| `leds` | Current frame, 8 rows of 12 levels, 0..15. During fire this is the stored frame, not a live copy of the fire. During an animation it is the frame on screen. |
| `frames` | Stored frames, one 96-character lowercase hex string each. Each digit is a level 0..15, 12 columns × 8 rows, same order as `leds`. Empty when nothing is stored. |
| `durations` | Per-frame interval in ms, one entry per stored frame. |

`mode` value `loop` is accepted on input and means `fire`. `animation` resumes a stored animation from frame 0. It is an error if none is stored.

| Method | Path | What it does |
|---|---|---|
| `GET` | `/` | The page |
| `GET` | `/api/state` | The JSON above |
| `POST` | `/api/frame`, `/api/leds`, `/api/animation` | Store LED data or an animation and show it now, restarting the 10 seconds. A JSON body with `frames` is an animation on any of these paths. A single frame (no `frames` key) is mode `custom`. A body that is only `{"mode":"fire"}` or `{"mode":"loop"}` returns the 10-second slot to the fire. `{"mode":"custom"}` shows the stored frame now. `{"mode":"animation"}` restarts the stored animation. If the body also has `frames` or pixels, those win. `POST /api/animation` also accepts the raw binary packet below. |
| `POST` | `/api/animation.bin` | Same as a binary `POST /api/animation`. |
| `POST` | `/api/mode` | `{"mode":"fire"}`, `{"mode":"loop"}`, `{"mode":"custom"}`, `{"mode":"animation"}`, or `?mode=` with the same tokens. `fire` / `loop` show the fire now and keep the stored picture. |
| `POST` | `/api/reset` | Same as mode `fire`. No body. |
| `DELETE` | `/api/frame`, `/api/animation` | Clear the stored picture and show the fire now. |

Frame bodies (any one of these). A number 0..15 is the level. A number 16..255 still maps with `(v * 15 + 127) / 255`. A hex digit is the level itself, 0..15, and is not scaled.

- `{"x":0,"y":7,"on":true,"brightness":15}` one pixel. `x` is 0..11, `y` is 0..7.
- `{"pixels":[{"x":1,"y":2,"on":true,"brightness":8}]}` sparse updates, merged into the stored frame.
- `{"leds":[...]}` or `{"frame":[...]}` or a bare array. Either 96 levels, or 8 rows of 12. Numbers only, or the hex shapes below.
- `{"leds":[{"on":true,"brightness":4}, ...]}` objects without `x`/`y`, applied in order from LED 0. Up to 96.
- Eight row strings, each exactly 12 hex digits (`0`-`9`, `a`-`f`, `A`-`F`): `{"rows":["000ff0000ff0","00f55f00f55f", ...]}` or a bare array of those eight strings. `leds`, `frame`, or one element of `frames` can be that array too.
- One 96-character hex string, the whole frame in row order: `{"hex":"000ff0000ff000f55f00f55f..."}` or a bare JSON string. Spaces, tabs, newlines, and the separators `:`, `-`, and `,` are skipped. The string must contain exactly 96 hex digits. A `\` is rejected.

Animation bodies (JSON, on `POST /api/frame`, `/api/leds`, or `/api/animation`):

- `frames`: 1 to 128 frames. Each frame is any single-frame shape above: 96 numbers, 8 rows of 12 numbers, pixel objects, 8 hex row strings, a 96-character hex string, or an object `{"leds":[...]}`, `{"frame":[...]}`, `{"pixels":[...]}`, `{"hex":"..."}`, or `{"rows":[...]}`. A frame object may also set `duration`, `interval`, or `intervalMs`.
- `interval` or `intervalMs`: milliseconds between frames. Default 100. Values are clamped to 20–5000 (`intervalMs` wins if both are sent). A missing per-frame duration uses that interval. One frame plays as a still image for the whole 10 seconds.
- Example: `{"interval":80,"frames":["<96 hex>","<96 hex>"]}`.

A 129th frame is rejected and nothing is stored. About 60 frames of 96-digit hex fit in one JSON body. A frame made of 96 `{on, brightness}` objects is much larger, so only a few of those fit; the board answers 413 `full` when the body does not.

Raw binary (on `POST /api/animation` or `POST /api/animation.bin`). The body is binary when the path is `/api/animation.bin`, the `Content-Type` is `application/octet-stream`, or the first byte is not `{`, `[`, `"`, or whitespace (a bare hex string and JSON that starts with a space stay JSON).

Both formats start with 4 bytes: magic, `frameCount` (1..128), then `intervalMs` as a little-endian uint16, clamped to 20–5000. Pixels are packed two per byte, high nibble = pixel `2*i`, low nibble = pixel `2*i+1` (48 bytes per frame).

- Format A, magic `'M'` (`0x4D`) or version `1` (`0x01`): the 4-byte header, then `frameCount * 48` packed bytes. Every frame uses `intervalMs`.
- Format B, magic `'N'` (`0x4E`) or version `2` (`0x02`): the same header, then each frame is a little-endian uint16 `durationMs` (clamped to 20–5000) plus 48 packed bytes. 128 frames are 6404 bytes.

Bytes after the packet are ignored. A short packet, a count of 0 or above 128, or a bad magic returns 400 and stores nothing. Request headers are dropped before the body is kept, so the 6404-byte packet fits in the 6720-byte buffer. The page sends format A.

A bad body returns 400 and changes nothing. `error` is `json`, `pixel`, `mode`, `frames`, `interval`, or `method`. A body that does not fit returns 413 `full`.

Host checks, from this folder's parent:

```text
g++ -std=c++17 -DFRAME_JSON_TEST -x c++ sketches/MatrixLamp_R4_Wifi/frame_json.h -o /tmp/frame_json_test && /tmp/frame_json_test
g++ -std=c++17 -DMATRIX_FIRE_TEST -x c++ sketches/MatrixLamp_R4_Wifi/matrix_fire.h -o /tmp/matrix_fire_test && /tmp/matrix_fire_test
g++ -std=c++17 -DMDNS_TEST -x c++ sketches/common/mdns_responder.h -o /tmp/mdns_test && /tmp/mdns_test
```

```text
curl -s http://<ip>/api/state
curl -s -X POST http://<ip>/api/frame -H 'Content-Type: application/json' \
  -d '{"x":0,"y":7,"on":true,"brightness":15}'
curl -s -X POST http://<ip>/api/leds -H 'Content-Type: application/json' \
  -d '{"pixels":[{"x":1,"y":1,"on":false}]}'
curl -s -X POST http://<ip>/api/mode -H 'Content-Type: application/json' -d '{"mode":"fire"}'
curl -s -X POST 'http://<ip>/api/mode?mode=loop'
curl -s -X POST http://<ip>/api/animation -H 'Content-Type: application/json' \
  -d '{"interval":120,"frames":[[[15,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0]],[[0,0,0,0,0,0,0,0,0,0,0,15],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0,0,0,0]]]}'
curl -s -X POST http://<ip>/api/animation -H 'Content-Type: application/json' \
  -d '{"interval":80,"frames":["f00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000","00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000f"]}'
python3 -c 'open("/tmp/anim.bin","wb").write(bytes([0x4D,1,100,0])+bytes([0xF0])+bytes(47))'
curl -s -X POST http://<ip>/api/animation.bin -H 'Content-Type: application/octet-stream' \
  --data-binary @/tmp/anim.bin
curl -s -X POST http://<ip>/api/reset
curl -s -X DELETE http://<ip>/api/frame
curl -s -X DELETE http://<ip>/api/animation
```

```text
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi sketches/MatrixLamp_R4_Wifi
```

Core 1.6.0, empty `WIFI_SSID` (no `secrets.h`): 97416 B flash (37% of 262144), 22404 B globals (68% of 32768), 10364 B left for locals. Frames are nibble-packed, 48 bytes each, so 128 frames are 6144 B plus 256 B of durations. One 6720 B buffer holds the HTTP request and the start of the state response; `GET /api/state` then streams `frames` and `durations`, so those hex strings are not a second copy in SRAM. The mDNS packet buffer is 512 B.

The core places an 8 KB heap immediately after BSS and a 1 KB main stack at the top of RAM. There is no linker check if they meet. Before this change the gap was about 200 bytes, with the request buffer and the state JSON each 6144 B (23048 B globals). Growing the shared buffer to 8192 B on top of the packed frames would close that gap and leave fewer than 10 KB in the "left for locals" figure. At 6720 B the gap is 864 bytes, globals are 22404 B, and 10364 B remain. WiFiClient adds a 1024 B receive FIFO on the heap while a client is connected. The mDNS `WiFiUDP` (about 1.5 KB) is malloc'd while WiFi is up. Flash moves a little with the length of the SSID and password.

## Sketches

Names are `SpiralLamp_<Board>_<FeatureTags>`. Feature tags stay in this order: **FireTrail**, **WiFi**, **MIDI** (a later tag such as Mic would go after those). Fixes and improvements land in the existing sketch; git history is the version. A different feature set is a new folder, and the folder name matches the `.ino`. `_Base` is the known-good fallback with no feature tags. `MatrixLamp_R4_Wifi` is the onboard matrix and is not part of that chain.

| Sketch | Board | LEDs default | Lava | FireTrail | WiFi web UI | MIDI (AppleMIDI) | Notes |
|---|---|---|---|---|---|---|---|
| `SpiralLamp_R3_Base` | Uno R3 | 160 | yes | no | no | no | Known-good fallback. Stays dark until the rocker (D7) is on. |
| `SpiralLamp_R3_FireTrail` | Uno R3 | 160 | yes | yes | no | no | Base plus the lava fire tail. |
| `SpiralLamp_R4_FireTrail` | Uno R4 WiFi | 160 | yes | yes | no | no | Same lamp as R3 FireTrail. Radio unused. Boot line still says `R4 v0`. |
| `SpiralLamp_R4_FireTrail_WiFi_MIDI` | Uno R4 WiFi | 160 (320 compiles) | yes | yes | yes | yes | Page on port 80, Music mode on UDP 5004. mDNS `spirallamp.local` and `spiral-lamp.local`. |
| `MatrixLamp_R4_Wifi` | Uno R4 WiFi | 12×8 onboard | no | no | yes | no | No strip and no panel. IP scroll, then 10 s of fire, a posted frame, or an animation of up to 128 frames. mDNS `matrixlamp.local` and `matrix-lamp.local`. |

## Layout

| Path | What it is |
|---|---|
| `sketches/bench_test/` | Breadboard check: strip, colour button, brightness button |
| `sketches/bench_test_v2/` | Bench test v2: both pots and every panel switch, one at a time ([guide](docs/bench_test_v2.md)) |
| `sketches/SpiralLamp_R3_Base/` | Known-good lamp (was `SpiralLamp_clean`) |
| `sketches/SpiralLamp_R3_FireTrail/` | Same lamp, plus a fire tail on the lava balls (was `SpiralLamp_v2`) |
| `sketches/SpiralLamp_R4_FireTrail/` | FireTrail lamp on the Uno R4 WiFi, radio unused (was `SpiralLamp_R4_v0`) |
| `sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI/` | R4 lamp plus the web page and AppleMIDI Music mode |
| `sketches/MatrixLamp_R4_Wifi/` | Onboard 12×8 matrix: IP scroll, 10 s fire, frame, or animation, page on port 80 |
| `tools/lava_harness/` | Host build of the lava effect (`v2`/`firetrail` default, `clean`/`base`, `r4`/`r4-firetrail`) |
| `docs/wiring.md` | Authoritative wiring |
| `docs/issues.md` | What was wrong in the purchased plans |
| `docs/wiring_diagram_OUTDATED.png` | Old diagram. Strip is shown powered from the Arduino. Do not follow it. |
| `docs/draw_wiring.py` | Script that drew that diagram |
| `original-plans/` | Purchased originals, kept for reference |
| `plywood/` | Plywood enclosure and tube spiral ([readme](plywood/README.md), [plan](plywood/PLAN.pdf)) |
| `plywood/fingerjoint/` | Laser-cut finger-joint birch casing: v2 control slope, as-ordered set, retrofit faceplate ([readme](plywood/fingerjoint/README.md)) |
| `sheetmetal/` | Bent sheet-metal casing, aluminium and steel DXFs ([readme](sheetmetal/README.md)) |
| `enclosures/matrix_16x16_mount/` | 3D-printable support-free 16×16 WS2812B matrix and 162×162 mm polycarbonate diffuser on the 3" 4-bolt top deck ([readme](enclosures/matrix_16x16_mount/README.md)) |
