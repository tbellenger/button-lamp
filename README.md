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

`sketches/MatrixLamp_R4_Wifi/` drives a BTF-LIGHTING WS2812B 16×16 panel (256 LEDs) from an Arduino Uno R4 WiFi. The board's built-in 12×8 red matrix only scrolls the WiFi address. There is no animation upload and no stored frames. Firmware updates go over WiFi after one USB flash.

Copy `secrets.example.h` to `secrets.h` in that folder and put the network in `WIFI_SSID` / `WIFI_PASS`, and set `OTA_PASSWORD`. `secrets.h` is gitignored. Do not commit it. A USB build writes those values into the board's data flash when they differ from what is already stored. A build from GitHub Actions has no `secrets.h` and leaves the stored WiFi name and OTA password alone, so an over-the-air update does not knock the lamp off the network. With no stored network and no `secrets.h`, the board opens a setup WiFi named `MatrixLamp-Setup`.

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

Pass the port as the first argument. If you leave it off, the script looks for one Uno R4 WiFi in `arduino-cli board list`. None, or more than one, prints the ports it can see and asks (or exits, if nothing is waiting for input). `--monitor` opens the serial monitor at 115200 after a good USB upload. The `.bat` waits for a key at the end so a double-clicked window does not close. These scripts are for this sketch only.

The first flash has to be USB. The board can only accept a WiFi update once it is already running this firmware. After that:

```text
tools\upload_matrixlamp.bat ota
tools\upload_matrixlamp.bat ota 192.168.1.20
tools/upload_matrixlamp.sh --ota
tools/upload_matrixlamp.sh --ota 192.168.1.20
```

`ota` and `--ota` compile the sketch to a `.bin` and POST it to `http://<host>/update`. The host defaults to `matrixlamp.local`. An IP works the same way. The password is `OTA_PASSWORD` in the environment, or the same name in `secrets.h`, or a prompt. The script prints `SUCCESS` when the lamp comes back reporting the version that was just compiled, and `FAILED` when it does not. Windows uses the `curl.exe` that ships with Windows 10 and 11.

On Windows, `matrixlamp.local` often does not resolve. The lamp prints its IP on the serial monitor at boot and shows the same address at the top of the web page. Pass that IP as the host.

The phone page can do the same upload, and can also pull the GitHub release. See below.

#### What it plays

The page is a picture of the spiral lamp's **physical** control panel, with a Presets list under it (collapsed; tap a name for the steps). The panel itself: the printed `Switch Panel` layout (184.15 × 133.35 mm, fader along the front edge), with the same centres as `plywood/fingerjoint/src/controls.py`. Birch face, red and white arcade buttons, key switch, red mushroom, toggle, vertical rocker, red flip-cover over the wave toggle, speed knob, and the brightness fader. Buttons press in, the key turns, the mushroom latches, the rocker and toggles flip, the cover opens, and the knob and fader move by drag.

The mode is whatever those switches would select on the spiral lamp. Centres and pins, from the front edge up:

| Control | Pin | On the 16×16 |
|---|---|---|
| Fader | A0 | Brightness. Halfway is **20** of 255. It cannot pass the cap (40). |
| Red arcade, middle left | D8 | While Chaos is on, next lava bottom colour. With the rocker on, one step of the classic code (`l`). With every switch off, one step of the Metaballs code. In Metaballs, add a blob. |
| White arcade, top left | D4 | While Chaos is on, next lava top colour. With the rocker on, one step of the classic code (`r`). With every switch off, one step of the Metaballs code. In Metaballs, remove a blob. |
| Toggle, left of the rocker | D6 | With the rocker, Jumbo's colour cycle. Alone (rocker, key, wave, and chaos off), Music. |
| Rocker, centre | D7 | Power for Jumbo / Classic. Off, and nothing else selected, is the matrix fire. |
| Covered toggle, right of the rocker | D10 | Wave. Open the red cover, then flip the switch. |
| Speed knob | A1 | Speed for every moving mode. On Classic it is also the colour (orange → yellow → white). |
| Key, top middle | D5 | Rainbow. With Wave, Wave rainbow. With Wave and the toggle, Wave epic. |
| Mushroom, top right | D11 | Lava. It wins over the other switches. |

How each spiral mode sits on the grid (`y = 0` is the top of the picture):

| Mode | Switches | On the 16×16 |
|---|---|---|
| Fire | all off | The matrix fire: deep red, orange, yellow, white only in the hottest cores. Not a spiral-lamp mode. This is what the strip lamp leaves dark. |
| Jumbo | rocker, toggle optional | A wipe from the bottom row upward in the jumbo colour (white, then red, purple, blue, green, yellow, orange). The toggle steps that colour. |
| Classic | rocker, after `rlrrlrll` on the arcade buttons | The whole panel one colour from the knob. Turning the toggle on leaves Classic. |
| Rainbow | key | A hue runs along a serpentine path that starts at the bottom. The toggle reverses it. |
| Wave | wave | A bright band ripples up the rows. |
| Wave modifier | wave + toggle | That band crossfades a colour pair. |
| Wave rainbow | wave + key | Each row is a hue; the band still travels. |
| Wave epic | wave + key + toggle | One row flashes through the epic colours. |
| Secret | rocker + toggle + key + wave | The knob fills LEDs from the bottom along the path. The fader picks the solid colour, and the bottom of the fader mixes in the violet-to-red gradient. |
| Lava | mushroom | Two balls toss across the grid under gravity. Bottom ball uses the red button's colour, top ball the white button's. The fire trail (on at boot, no switch on the real panel) sits behind each ball. |
| Music | toggle only | A dim pulse walks the path, and a short built-in phrase sends brighter notes. This lamp has no AppleMIDI socket. |
| Metaballs | arcade code `rrllrrll` while every switch is off (white, white, red, red, white, white, red, red) | Glowing blobs drift and bounce. Their fields add, so they merge and split. The hue drifts. Red (D8) adds a blob, up to 8. White (D4) removes one, down to 1. The knob is their speed. The fader stays brightness. Those presses are not written into the Classic code: Classic only listens while the rocker is on, and this code only listens while every switch is off and Metaballs is not already showing. Turning the rocker, toggle, key, wave switch, or mushroom on leaves Metaballs. |
| Plasma | arcade code `llrrllrr` while every switch is off (red, red, white, white, red, red, white, white) | A plasma field rotates and zooms in and out. The knob is the animation speed. The fader stays brightness. Red zooms in one step (five steps, held at the tight end). White zooms out. Same as Metaballs: once the mode is on, the arcade buttons are that control and are not another code. Either code clears the other. A switch turning on leaves Plasma. |
| Sun | arcade code `rrrllrrr` while every switch is off (white, white, white, red, red, white, white, white) | A pulsing sun: yellow-white core, orange body, noisy rays in red. The knob is the shimmer and pulse speed. The fader stays brightness. Red grows the sun one step. White shrinks it. Five steps, held at the ends. Starts at 2. Same rule: once it is showing, red and white are size, not another code. Any of the three idle codes clears the other two. A switch turning on leaves Sun. |

The 12×8 matrix scrolls the IP while WiFi is online. Until DHCP has a non-zero address it shows a waiting mark. It does not scroll `0.0.0.0`.

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
| `mode` | `fire`, `jumbo`, `classic`, `rainbow`, `wave`, `wave-mod`, `wave-rainbow`, `wave-epic`, `secret`, `lava`, `music`. Derived from the switches. |
| `rocker`, `toggle`, `key`, `wave`, `chaos` | `true` when that switch is on |
| `classic` | The arcade sequence has armed Classic |
| `balls` | Metaballs blob count, 1–8. Starts at 3 |
| `zoom` | Plasma zoom step, 0–4. Starts at 2. Higher is tighter |
| `sun` | Sun size step, 0–4. Starts at 2. Higher is a larger core and stronger rays |
| `fireTrail` | Lava tail. On at boot. No switch on the panel. |
| `fader` | 0–100, the slider position. 50 is the boot position. |
| `brightness` | NeoPixel level 0–255 after the cap. Boot value 20. |
| `brightnessCap` | The cap, 40 unless the power budget is lower |
| `speed` | 0–100, the knob |
| `top`, `topName`, `bottom`, `bottomName` | Lava colours, `red` `purple` `blue` `green` `yellow` `orange` `off` |
| `wifi` | `off`, `joining`, or `connected`. `connected` means a non-zero DHCP address |
| `ip` | Dotted address, or `0.0.0.0` while DHCP has not finished |
| `rssi` | dBm, or 0 when offline |
| `ledCount`, `width`, `height` | 256, 16, 16 |
| `serpentine`, `flipX`, `flipY`, `rotate` | The orientation settings compiled into the sketch |
| `powerBudgetMa` | `POWER_BUDGET_MA` |
| `fw` | Firmware version. `local` for a USB build. GitHub builds use `<short sha>-<run>` |
| `built` | Compile date and time |
| `ota` | `idle`, `upload`, `check`, `checked`, `download`, `applying`, or `error` |
| `otaPct` | Progress while a new image is being stored, 0–100 |
| `otaMsg` | Short status, for example `up to date` or `bad image` |
| `ghVersion`, `ghSize` | The release the lamp last checked |

`GET /api/set?key=value&...` applies keys left to right and returns that state. `ok` is false on the first bad key; earlier keys stick. `error` is the key that failed, or `method`.

| Key | Values |
|---|---|
| `rocker`, `toggle`, `key`, `wave`, `chaos` | `0` / `off`, `1` / `on`, `toggle` |
| `fader`, `speed` | `0`–`100` |
| `brightness` | `0`–`255`, clamped to the cap. Moves the fader so the level matches. |
| `white`, `top` | `press` or `next` cycles the top lava colour while Chaos is on. A number `0`–`6` sets it. `press` is also the white button in the classic code. |
| `red`, `bottom` | Same, for the bottom colour and the red button. |
| `fire` | `0`, `1`, or `toggle`. The lava tail. Not on the panel. |

```text
curl -s http://<ip>/api/state
curl -s 'http://<ip>/api/set?rocker=1&toggle=1'
curl -s 'http://<ip>/api/set?chaos=1&white=press'
curl -s 'http://<ip>/api/set?brightness=20'
curl -s 'http://<ip>/api/set?white=press&white=press&red=press&red=press&white=press&white=press&red=press&red=press'
curl -s 'http://<ip>/api/set?red=press'
```

Host check for the fire, from this folder's parent:

```text
g++ -std=c++17 -DMATRIX_FIRE_TEST -x c++ sketches/MatrixLamp_R4_Wifi/matrix_fire.h -o /tmp/matrix_fire_test && /tmp/matrix_fire_test
g++ -std=c++17 -DMDNS_TEST -x c++ sketches/common/mdns_responder.h -o /tmp/mdns_test && /tmp/mdns_test
```

```text
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi sketches/MatrixLamp_R4_Wifi
```

Core 1.6.0, Adafruit NeoPixel 1.15.5, ArduinoOTA 1.1.1, no `secrets.h`: 122632 B flash (46% of 262144), 15204 B globals (46% of 32768), 17564 B left for locals. The exported `.bin` is 122640 B. NeoPixel then mallocs 768 B (`256 * 3`) on the heap. The HTTP request buffer is 1024 B. The mDNS packet buffer is 512 B. WiFiClient adds a 1024 B receive FIFO on the heap while a client is connected. The mDNS `WiFiUDP` (about 1.5 KB) is malloc'd while WiFi is up.

#### WiFi firmware update

The upload path is an HTTP POST on the lamp's existing web server, not the Arduino IDE network port. ArduinoOTA 1.1.1 does support this board: its Renesas storage writes the new image into the top half of code flash and, only when asked, copies it over the running sketch and resets. The sketch defines `NO_OTA_NETWORK` so the library does not open a second TCP port or a second mDNS responder. The lamp already answers `matrixlamp.local`. Curl and the phone both speak HTTP, so one endpoint covers the Windows script and the iPhone. The IDE recipe (`platform.local.txt` copied into the core) is not used.

Checked against `arduino:renesas_uno` 1.6.0. Code flash is 256 KB with a 16 KB bootloader, so the sketch region is 240 KB. `InternalStorage` allows half of that, aligned down to the 2 KB block: **122880 bytes**. This build's `.bin` is 122640 bytes, 240 bytes under that cap. A larger image is refused and not applied. The 8 KB data flash holds the WiFi name and the OTA password. It is not big enough for a second sketch, and a code-flash update does not erase it.

`POST /update` requires the header `X-OTA-Password`. An empty stored password refuses every upload. Optional header `X-OTA-Crc32` is 8 hex digits; a mismatch is not applied. The image also has to start like a Cortex-M sketch (stack pointer in RAM, reset vector inside the image, Thumb bit set). A short body, a dropped connection, or a failed flash write calls `close` and does not call `apply`. During the transfer the 24 kHz matrix scan is stopped, and the 16×16 fills with orange as the bytes land. `apply` reboots into the new firmware.

The web page shows `firmware` and the build time from `/api/state`. The Firmware section asks for the OTA password and stays in that browser tab only. **Upload .bin** picks a file (Safari on iPhone can do this) and POSTs it with a progress readout. **Check for update** and **Update from GitHub** make the lamp fetch `https://github.com/tbellenger/button-lamp/releases/download/matrixlamp-latest/manifest.json` and the `.bin` named there, over `WiFiSSLClient`. It follows one HTTPS redirect (GitHub sends the file to `objects.githubusercontent.com`), checks the size and the `crc32` in the manifest, then applies. The panel shows the same orange bar. The page polls until the version changes.

The repository is public, so those release files need no token. Nothing in the firmware is a GitHub credential. If the repo were private, the lamp could not download the release; the choices would be to keep the repo public, or to download the `.bin` on a computer and upload it from the phone. A token is not stored in the source.

GitHub Actions (`.github/workflows/matrixlamp-build.yml`) runs on a push to `main` that touches this sketch, and from the Actions tab (Run workflow). It installs the core and libraries, compiles with no `secrets.h`, and fails the job if `secrets.h` is in the tree or the `.bin` is over 122880 bytes. The `.bin` and `manifest.json` are a workflow artifact and two releases: the moving tag `matrixlamp-latest`, and `matrixlamp-<version>`. Version is the short git SHA and the run number. Actions has to be enabled for the repository. The release appears after this firmware is on `main`, not from an unmerged pull request.

Before any of that works, flash once over USB so the board is running this sketch. If that USB build included `secrets.h`, the lamp joins the network by itself. If the chip has never stored a network (a GitHub `.bin` on a blank board), join `MatrixLamp-Setup`, open the page it serves, and save the WiFi name, WiFi password, and OTA password. Hold the red and white arcade buttons while the board resets to open that setup page again. After the lamp is on the home network, WiFi updates do not need USB.

## Sketches

Names are `SpiralLamp_<Board>_<FeatureTags>`. Feature tags stay in this order: **FireTrail**, **WiFi**, **MIDI** (a later tag such as Mic would go after those). Fixes and improvements land in the existing sketch; git history is the version. A different feature set is a new folder, and the folder name matches the `.ino`. `_Base` is the known-good fallback with no feature tags. `MatrixLamp_R4_Wifi` is the 16×16 panel and is not part of that chain.

| Sketch | Board | LEDs default | Lava | FireTrail | WiFi web UI | MIDI (AppleMIDI) | Notes |
|---|---|---|---|---|---|---|---|
| `SpiralLamp_R3_Base` | Uno R3 | 160 | yes | no | no | no | Known-good fallback. Stays dark until the rocker (D7) is on. |
| `SpiralLamp_R3_FireTrail` | Uno R3 | 160 | yes | yes | no | no | Base plus the lava fire tail. |
| `SpiralLamp_R4_FireTrail` | Uno R4 WiFi | 160 | yes | yes | no | no | Same lamp as R3 FireTrail. Radio unused. Boot line still says `R4 v0`. |
| `SpiralLamp_R4_FireTrail_WiFi_MIDI` | Uno R4 WiFi | 160 (320 compiles) | yes | yes | yes | yes | Page on port 80, Music mode on UDP 5004. mDNS `spirallamp.local` and `spiral-lamp.local`. |
| `MatrixLamp_R4_Wifi` | Uno R4 WiFi | 16×16 WS2812B on D6 | yes | yes | yes | no | Physical-panel page. Spiral modes, plus Metaballs, Plasma, and Sun from arcade codes while idle. mDNS `matrixlamp.local` and `matrix-lamp.local`. WiFi firmware update after one USB flash. |

## Layout

| Path | What it is |
|---|---|
| `sketches/bench_test/` | Breadboard check: strip, colour button, brightness button |
| `sketches/bench_test_v2/` | Bench test v2: both pots and every panel switch, one at a time ([guide](docs/bench_test_v2.md)) |
| `sketches/SpiralLamp_R3_Base/` | Known-good lamp (was `SpiralLamp_clean`) |
| `sketches/SpiralLamp_R3_FireTrail/` | Same lamp, plus a fire tail on the lava balls (was `SpiralLamp_v2`) |
| `sketches/SpiralLamp_R4_FireTrail/` | FireTrail lamp on the Uno R4 WiFi, radio unused (was `SpiralLamp_R4_v0`) |
| `sketches/SpiralLamp_R4_FireTrail_WiFi_MIDI/` | R4 lamp plus the web page and AppleMIDI Music mode |
| `sketches/MatrixLamp_R4_Wifi/` | 16×16 panel, spiral modes, physical-panel page. Onboard 12×8 scrolls the IP |
| `tools/upload_matrixlamp.sh`, `tools/upload_matrixlamp.bat` | Compile and upload MatrixLamp. USB by default. `ota` / `--ota [host]` sends the `.bin` over WiFi. `--monitor` opens serial at 115200 after USB |
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
