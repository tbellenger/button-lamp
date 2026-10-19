# Spiral Lamp / Button Box

Tom's Arduino spiral lamp: an Arduino Uno R3 (ATmega328P) on a screw-terminal shield, driving a BTF FCOB strip (5 mm, 160 LEDs/m, cut to the full metre = 160 LEDs) from a 5 V 3 A supply. The panel has arcade buttons, a key switch, a mushroom stop switch, a toggle, and slide and rotary pots. `sketches/SpiralLamp_R4_v0/` is the same lamp for an Uno R4 WiFi and a 5 V 2 A supply (section 3).

Upload the sketches in `sketches/`. `original-plans/` is the purchased set (STLs, four original `.ino` versions, and PDFs), kept for reference.

## Plywood enclosure + tube spiral

Hand-cut plywood base and printed spiral holder: [plywood/README.md](plywood/README.md). Full build plan: [plywood/PLAN.pdf](plywood/PLAN.pdf).

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

When the strip and both buttons behave, open `sketches/SpiralLamp_clean/SpiralLamp_clean.ino` and upload that. Same library. The lamp stays dark until the rocker on D7 is closed to GND. `BRIGHTNESS_CAP` is 50 so full white on 160 LEDs stays near 1.9 A on the 5 V 3 A supply. That sketch is the known-good lamp.

`sketches/SpiralLamp_v2/SpiralLamp_v2.ino` is the same lamp plus a fire tail on the lava balls. Lava is the mushroom on D11 (Chaos), after that pin has been seen low once. Two balls bounce in from the ends; the strongest toss reaches the middle of the strip. Each ball leaves a fire tail opposite the way it is moving: orange beside the ball, then yellow, then white at the far end, dimming so the tip does not stop hard, and nothing at the top of the bounce. The tail is about a dozen LEDs at full speed and shrinks as the ball slows. It uses the same fader, colour calibration and brightness cap as the balls. Set `LAVA_FIRE_TRAIL` to 0 for plain balls. `tools/lava_harness/` builds v2 by default (pass `clean` to build the known-good sketch, or `r4` for the R4 port) and prints the peak of each toss plus a few ASCII frames of the tail.

### 3. Uno R4 WiFi

`sketches/SpiralLamp_R4_v0/SpiralLamp_R4_v0.ino` is that v2 lamp built for the Arduino Uno R4 WiFi. Same pins, same 160 LEDs, same modes and the same fire trail. It does not use WiFi yet.

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

## Layout

| Path | What it is |
|---|---|
| `sketches/bench_test/` | Breadboard check: strip, colour button, brightness button |
| `sketches/bench_test_v2/` | Bench test v2: both pots and every panel switch, one at a time ([guide](docs/bench_test_v2.md)) |
| `sketches/SpiralLamp_clean/` | Full lamp (cleaned Version 4), the known-good build |
| `sketches/SpiralLamp_v2/` | Same lamp, plus a fire tail on the lava balls |
| `sketches/SpiralLamp_R4_v0/` | v2 ported to the Uno R4 WiFi (same behaviour, no WiFi yet) |
| `tools/lava_harness/` | Host build of the lava effect (`v2` default, or `clean` / `r4`) |
| `docs/wiring.md` | Authoritative wiring |
| `docs/issues.md` | What was wrong in the purchased plans |
| `docs/wiring_diagram_OUTDATED.png` | Old diagram. Strip is shown powered from the Arduino. Do not follow it. |
| `docs/draw_wiring.py` | Script that drew that diagram |
| `original-plans/` | Purchased originals, kept for reference |
| `plywood/` | Plywood enclosure and tube spiral ([readme](plywood/README.md), [plan](plywood/PLAN.pdf)) |
