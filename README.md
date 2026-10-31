# Spiral Lamp / Button Box

Tom's Arduino spiral lamp: an Arduino Uno R3 (ATmega328P) on a screw-terminal shield, driving a BTF FCOB strip (5 mm, 160 LEDs/m, cut to the full metre = 160 LEDs) from a 5 V 3 A supply. The panel has arcade buttons, a key switch, a mushroom stop switch, a toggle, and slide and rotary pots. `sketches/SpiralLamp_R4_FireTrail/` is the same lamp for an Uno R4 WiFi and a 5 V 2 A supply (section 3). `sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI/` adds the web page and AppleMIDI (section 4). `sketches/MatrixLamp_R4_Wifi/` is the 16×16 WS2812B panel on an Uno R4 WiFi, with the onboard 12×8 matrix scrolling the IP (section 5). Sketch names are `SpiralLamp_<Board>_<FeatureTags>` (see Sketches).

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

### 5. Matrix lamp (16×16 panel)

`sketches/MatrixLamp_R4_Wifi/` drives a BTF-LIGHTING WS2812B 16×16 panel (256 LEDs) from an Arduino Uno R4 WiFi. The board's built-in 12×8 red matrix only scrolls the WiFi address. There is no animation upload, no stored frames, and no OTA.

Copy `secrets.example.h` to `secrets.h` in that folder and put the network in `WIFI_SSID` / `WIFI_PASS`. `secrets.h` is gitignored. A missing `secrets.h`, or an empty `WIFI_SSID`, skips WiFi. The panel still runs.

#### Wiring

Data is **D6**, through a series resistor (330 Ω; anything 220–470 Ω is fine). Tom has confirmed the panel with Adafruit NeoPixel `NEO_GRB + NEO_KHZ800` on D6. The panel is powered from its own 5 V supply. The Arduino and the panel share ground. A 1000 µF capacitor (6.3 V or higher) goes across the panel 5 V and GND, at the panel, striped leg to GND.

| From | To |
|---|---|
| Panel DIN | 330 Ω resistor → Arduino **D6** |
| Panel GND | Arduino **GND** and the 5 V supply **−** |
| Panel 5 V | 5 V supply **+** (not the Arduino 5 V pin while USB is plugged in) |
| 1000 µF capacitor | across the supply **+ / −**, at the panel |

Library: **Adafruit NeoPixel 1.12 or newer** (compiled with 1.15.5). Boards Manager → **Arduino UNO R4 Boards** (compiled with 1.6.0). Board: Arduino UNO R4 WiFi. `Arduino_LED_Matrix` and `WiFiS3` ship with the core. Do not add a separate WiFi library. Serial Monitor at 115200. The boot line is `Matrix Lamp R4 WiFi starting`. On connect it prints the IP. Open `http://matrixlamp.local/` or `http://matrix-lamp.local/` (or `http://<that-ip>/`). The DHCP hostname is `matrixlamp`, so `http://matrixlamp/` works on a router that publishes DHCP names. `matrix-lamp` without `.local` is mDNS only.

#### One-click upload

Install [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/) first (`brew install arduino-cli` on macOS, the install script on Linux, `winget install ArduinoSA.CLI` on Windows). Then, from the repo:

| | |
|---|---|
| Windows | double-click `tools/upload_matrixlamp.bat`, or `tools\upload_matrixlamp.bat COM5` |
| macOS / Linux | `tools/upload_matrixlamp.sh` or `tools/upload_matrixlamp.sh /dev/cu.usbmodem1101` |

The script pulls the repo (a failed pull prints a warning and continues), checks that `arduino-cli` is installed, installs the `arduino:renesas_uno` core and the Adafruit NeoPixel library if they are missing, and warns if `secrets.h` is missing. It uploads with:

```text
arduino-cli compile --upload -b arduino:renesas_uno:unor4wifi -p <port> sketches/MatrixLamp_R4_Wifi
```

Pass the port as the first argument. If you leave it off, the script looks for one Uno R4 WiFi in `arduino-cli board list`. None, or more than one, prints the ports it can see and asks (or exits, if nothing is waiting for input). `--monitor` opens the serial monitor at 115200 after a good upload. The `.bat` waits for a key at the end so a double-clicked window does not close. These scripts are for this sketch only. There is no OTA.

#### What it plays

The page is the control panel. It follows the spiral lamp page in `sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI/` (dark page, mode menu, sliders, a row of buttons, a status block).

- **Off**, **Fire**, **Solid colour**, **Rainbow**. Off is remembered: Turn on comes back to the last mode.
- **Brightness** is the NeoPixel 0–255 value. It starts at **20**. The slider cannot go above the cap (40, or lower if the power budget is tighter). 20 is the same level as Tom's rainbow test and is plenty on this panel.
- **Speed** is the fire step and the rainbow (0 slow, 100 fast). **Fire intensity** is how tall the flames are. 50 is the tuned flame.
- The colour buttons switch to that solid colour: red, orange, yellow, white, green, blue, purple.
- The 12×8 matrix scrolls the IP while WiFi is online. Until DHCP has a non-zero address it shows a waiting mark. It does not scroll `0.0.0.0`.

Fire is full colour on the 16×16: deep red at the cool edge, through orange and yellow, to white only on the hottest cells (the bottom-row plume cores). The bottom row stays hot. Three wide plumes rise from it, and a rare draft licks the top row without going white.

If the picture is mirrored or turned, edit the `PANEL_*` settings at the top of the sketch. Nothing else has to change.

| Setting | What to try |
|---|---|
| `PANEL_SERPENTINE` | `true` if rows alternate direction (how these panels are built). `false` if every row is left to right. |
| `PANEL_SERPENTINE_ODD` | `true`: row 0 is left to right and row 1 is right to left. `false`: the other way around. |
| `PANEL_FLIP_X` / `PANEL_FLIP_Y` | Mirror left-right or top-bottom. |
| `PANEL_ROTATE` | Clockwise quarter-turns: 0, 1, 2, or 3. |

Rotation, then flip, then the serpentine index. `y = 0` is the top of the picture, `x = 0` is the left.

#### Power

`BRIGHTNESS_DEFAULT` is 20. `BRIGHTNESS_CAP` is 40. `LED_MA_FULL` is 60 mA (one LED, full white, brightness 255). `POWER_BUDGET_MA` is 2500. The firmware uses the lower of the cap and `POWER_BUDGET_MA * 255 / (256 * 60)`. At 2500 mA that budget allows about 41, so the cap of 40 wins. Full white at 40 is about 2.4 A. Full white at 20 is about 1.2 A. Fire is well under full white. A USB port may still cope at 20; a small 5 V supply is the intended source. Do not feed the panel from the Arduino 5 V pin while USB is plugged in.

#### WiFi

The loop never blocks on the radio after the first join. `setup` calls `WiFi.begin()` with the library defaults (10 s connect wait, 10 s modem command timeout) **before** the matrix timer starts. Core 1.6.0's `begin()` returns as soon as `WiFi.status()` is `WL_CONNECTED`, which can be the moment the radio associates, while `WiFi.localIP()` is still `0.0.0.0`. The sketch does not treat that as online. It keeps polling `localIP()`, and only then scrolls the address and starts the page. A `0.0.0.0` read after a real address is already stored is ignored, because a modem timeout returns the same value. Another `WiFi.begin()` waits 22 seconds so DHCP is not reset mid-handshake. Once an address is stored, `setTimeout(0)` and `modem.timeout(800)` bound later polls. One HTTP client on port 80. Accepts are paced at 30 ms. Each pass does one short read or one short write. `sketches/common/mdns_responder.h` joins `224.0.0.251:5353` and answers A queries for both `.local` names (cache-flush, TTL 120 seconds). An AAAA question gets an NSEC record and no IPv6 address. Replies go to the multicast group, or unicast when the query's QU bit is set or the source port is not 5353. The poll does not run during an HTTP read or write. Connecting sends a gratuitous announcement, then one more a second later.

`strip.show()` pauses the 12×8 scan. NeoPixel bit-bangs the 256 LEDs with interrupts off for a few milliseconds, and leaving the matrix timer running would freeze one onboard LED on.

Core 1.6.0's `Arduino_LED_Matrix` is 1-bit. The sketch includes that header for the matrix pin map and runs its own timer at **24 kHz**: one LED at a time, Bresenham duty over levels 0..15. That is a 250 Hz scan of the 96 LEDs.

`GET /api/state` returns JSON:

| Field | Meaning |
|---|---|
| `mode` | `off`, `fire`, `solid`, or `rainbow` |
| `power` | `false` when mode is `off` |
| `brightness` | 0–255, already clamped to the cap. Boot value 20 |
| `brightnessCap` | The cap the slider is allowed to reach |
| `speed` | 0–100. Fire step and rainbow |
| `intensity` | 0–100. Fire height. 50 is the tuned flame |
| `color` | Solid colour: `red`, `orange`, `yellow`, `white`, `green`, `blue`, `purple` |
| `wifi` | `off`, `joining`, or `connected`. `connected` means a non-zero DHCP address |
| `ip` | Dotted address, or `0.0.0.0` while DHCP has not finished |
| `rssi` | dBm, or 0 when offline |
| `ledCount`, `width`, `height` | 256, 16, 16 |
| `serpentine`, `flipX`, `flipY`, `rotate` | The orientation settings compiled into the sketch |
| `powerBudgetMa` | `POWER_BUDGET_MA` |

`GET /api/set?key=value&...` applies keys left to right and returns that state. `ok` is false on the first bad key; earlier keys stick. `error` is `mode`, `brightness`, `speed`, `intensity`, `color`, `power`, `key`, or `method`.

| Key | Values |
|---|---|
| `mode` | `off`, `fire`, `solid`, `rainbow` |
| `power` | `0` / `off`, `1` / `on`, `toggle`. On restores the last mode |
| `brightness` | `0`–`255`, clamped to the cap |
| `speed`, `intensity` | `0`–`100` |
| `color` (or `colour`) | a colour name, or `next` |

```text
curl -s http://<ip>/api/state
curl -s 'http://<ip>/api/set?mode=fire&brightness=20&intensity=50'
curl -s 'http://<ip>/api/set?color=orange&mode=solid'
curl -s 'http://<ip>/api/set?power=toggle'
```

Host check for the fire, from this folder's parent:

```text
g++ -std=c++17 -DMATRIX_FIRE_TEST -x c++ sketches/MatrixLamp_R4_Wifi/matrix_fire.h -o /tmp/matrix_fire_test && /tmp/matrix_fire_test
g++ -std=c++17 -DMDNS_TEST -x c++ sketches/common/mdns_responder.h -o /tmp/mdns_test && /tmp/mdns_test
```

```text
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi sketches/MatrixLamp_R4_Wifi
```

Core 1.6.0, Adafruit NeoPixel 1.15.5, empty `WIFI_SSID` (no `secrets.h`): 85992 B flash (32% of 262144), 10900 B globals (33% of 32768), 21868 B left for locals. NeoPixel then mallocs 768 B (`256 * 3`) on the heap. The HTTP request buffer is 1024 B. The mDNS packet buffer is 512 B. The old 128-frame nibble buffer is gone. WiFiClient adds a 1024 B receive FIFO on the heap while a client is connected. The mDNS `WiFiUDP` (about 1.5 KB) is malloc'd while WiFi is up. Flash moves a little with the length of the SSID and password.

## Sketches

Names are `SpiralLamp_<Board>_<FeatureTags>`. Feature tags stay in this order: **FireTrail**, **WiFi**, **MIDI** (a later tag such as Mic would go after those). Fixes and improvements land in the existing sketch; git history is the version. A different feature set is a new folder, and the folder name matches the `.ino`. `_Base` is the known-good fallback with no feature tags. `MatrixLamp_R4_Wifi` is the 16×16 panel and is not part of that chain.

| Sketch | Board | LEDs default | Lava | FireTrail | WiFi web UI | MIDI (AppleMIDI) | Notes |
|---|---|---|---|---|---|---|---|
| `SpiralLamp_R3_Base` | Uno R3 | 160 | yes | no | no | no | Known-good fallback. Stays dark until the rocker (D7) is on. |
| `SpiralLamp_R3_FireTrail` | Uno R3 | 160 | yes | yes | no | no | Base plus the lava fire tail. |
| `SpiralLamp_R4_FireTrail` | Uno R4 WiFi | 160 | yes | yes | no | no | Same lamp as R3 FireTrail. Radio unused. Boot line still says `R4 v0`. |
| `SpiralLamp_R4_FireTrail_WiFi_MIDI` | Uno R4 WiFi | 160 (320 compiles) | yes | yes | yes | yes | Page on port 80, Music mode on UDP 5004. mDNS `spirallamp.local` and `spiral-lamp.local`. |
| `MatrixLamp_R4_Wifi` | Uno R4 WiFi | 16×16 WS2812B on D6 | no | no | yes | no | Fire, solid, or rainbow on the panel. Onboard 12×8 scrolls the IP. mDNS `matrixlamp.local` and `matrix-lamp.local`. No animation upload. |

## Layout

| Path | What it is |
|---|---|
| `sketches/bench_test/` | Breadboard check: strip, colour button, brightness button |
| `sketches/bench_test_v2/` | Bench test v2: both pots and every panel switch, one at a time ([guide](docs/bench_test_v2.md)) |
| `sketches/SpiralLamp_R3_Base/` | Known-good lamp (was `SpiralLamp_clean`) |
| `sketches/SpiralLamp_R3_FireTrail/` | Same lamp, plus a fire tail on the lava balls (was `SpiralLamp_v2`) |
| `sketches/SpiralLamp_R4_FireTrail/` | FireTrail lamp on the Uno R4 WiFi, radio unused (was `SpiralLamp_R4_v0`) |
| `sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI/` | R4 lamp plus the web page and AppleMIDI Music mode |
| `sketches/MatrixLamp_R4_Wifi/` | 16×16 WS2812B panel: fire, solid, rainbow, page on port 80. Onboard 12×8 scrolls the IP |
| `tools/upload_matrixlamp.sh`, `tools/upload_matrixlamp.bat` | Compile and upload MatrixLamp only (no OTA). `--monitor` opens serial at 115200 |
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
