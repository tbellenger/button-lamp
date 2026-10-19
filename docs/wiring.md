# Spiral Lamp: breadboard bench test wiring

Goal: prove that the **LED strip**, **button 1** and **button 2** work before anything goes in the box.
Sketch: `bench_test/bench_test.ino`. Diagram: `wiring_diagram.png`.

## What you need
- Arduino Uno or Nano (ATmega328P), USB cable
- WS2812B-compatible / NeoPixel strip, GRB colour order. **Tom's strip: BTF FCOB 5 mm, 160 LEDs/m, full metre = 160 LEDs** (the purchased plans assume 60; the sketches in `sketches/` are set to `LED_COUNT 160`)
- 2 push buttons (4-pin tactile switches or the arcade buttons)
- 1 resistor, 330 Ω (any 220–470 Ω works). 5-band blue resistors: orange-orange-black-black = 330 Ω
- 1 electrolytic capacitor, 1000 µF, 6.3 V or higher (the kit's 16 V one is fine)
- Jumper wires and a breadboard
- **5 V 3 A supply** with a barrel-jack-to-screw-terminal adapter, to power the strip (must say **5 V**; a 12 V adapter destroys the strip)

## Pin-by-pin table (Tom's build: strip on the 5 V 3 A supply, Arduino on USB)

The strip's input end has **two** sets of wires: a loose **red/white power pigtail** and a **3-pin plug** (red / green / white).

| From | To | Notes |
|---|---|---|
| Strip power pigtail **red** | barrel adapter screw terminal **+** | Strip power comes from the 5 V 3 A supply. |
| Strip power pigtail **white** | barrel adapter screw terminal **−** | |
| 1000 µF cap | across adapter **+ / −**, at the screw terminal | Striped leg (−) to the white/− side. Backwards = it can pop. |
| 3-pin plug **green** (DIN) | 330 Ω resistor → Arduino **D9** | Data line. Keep it short. |
| 3-pin plug **white** (GND) | Arduino **GND** (shield GND terminal) | Common ground: strip and Arduino **must** share GND or data won't work. |
| 3-pin plug **red** (+5 V) | **nothing: leave disconnected** (tape it off) | Stops the Arduino's USB 5 V and the adapter's 5 V fighting each other. |
| Arduino USB | computer | The Arduino runs from USB; the strip from the adapter. |
| Button 1 (arcade microswitch **NO**) | Arduino **D4** | "White" button in the plans (blue on Tom's panel). Cycles colour/pattern. |
| Button 1 microswitch **COM** | Arduino **GND** | Leave **NC** unconnected. |
| Button 2 (arcade microswitch **NO**) | Arduino **D8** | "Red" button in the plans. Cycles brightness. |
| Button 2 microswitch **COM** | Arduino **GND** | Leave **NC** unconnected. |

Breadboard tactile switches work too: one leg to the pin, the **diagonally opposite** leg to GND.

No resistors on the buttons. The sketch uses `INPUT_PULLUP`, so a pin reads HIGH when the button is released and LOW when it's pressed.

To use different pins, change `PIN_LED`, `PIN_BTN1` and `PIN_BTN2` at the top of `bench_test.ino`.

## Step by step
1. **Unplug USB and the wall adapter.** Wire with no power connected.
2. Check the adapter label says **5 V** output (Tom's: 5 V 3 A).
3. Find the strip's **input end**: the printed arrows point *away* from it (pads say DIN/DI, not DOUT/DO).
4. Pigtail red → adapter **+**, pigtail white → adapter **−**.
5. 1000 µF cap across the adapter terminals: long leg to +, striped leg to −.
6. 3-pin plug green → 330 Ω resistor → **D9**.
7. 3-pin plug white → Arduino **GND**.
8. 3-pin plug red → **not connected**. Tape the end.
9. Buttons: COM → GND, NO → D4 (button 1) / D8 (button 2). NC unused.
10. Install the **Adafruit NeoPixel** library (Arduino IDE → Library Manager). Open `bench_test/bench_test.ino` (folder name must match the `.ino`). `LED_COUNT` is already 160. Upload, then plug in the 5 V supply.
11. Open **Serial Monitor at 115200 baud**.

What you should see:
- On boot, a red, then green, then blue wipe runs to the last LED, then a moving rainbow at brightness 10/255 (about 4%).
- **Button 1** moves to the next mode: red → green → blue → white → rainbow → chase. Serial prints `Button 1 -> mode: ...`.
- **Button 2** moves to the next brightness step: 5 → 10 → 20 → 40 → 5. Serial prints `Button 2 -> brightness ...`.
- If a button is on a different pin from the one you expect, Serial prints `NOTE: something on D7 went LOW`. That tells you where the wire actually is.

## Power notes (read these)
- **Common ground is mandatory.** The Arduino, the strip and any supply must share GND. If they don't, the data signal has no reference, and you'll get nothing or random colours.
- **Each LED can draw about 60 mA at full white, so 160 LEDs can draw about 9.6 A at brightness 255.** A USB port gives about 0.5 A, and the Arduino's 5V pin isn't made for more. That's why the strip runs from its own supply and the 3-pin plug's red wire stays disconnected.
- **Bench test on the 5 V 3 A supply:** all four steps (5/10/20/40 of 255) are safe; full white at 40 is about 1.5 A. If you ever run the strip from USB instead, stay at 5 or 10 (10 is about 0.4 A).
- **Full sketch:** `BRIGHTNESS_CAP` is **50**, so full white stays near 1.9 A on the 3 A supply. The original sketch starts at brightness 255 (rocker ON + toggle OFF = solid white), which browns out any small supply and *looks* like broken code.
- To go brighter later, raise `BRIGHTNESS_CAP` but keep it at or below **80** on this 5 V 3 A supply (80 is about 3.0 A of full white). A higher cap needs a bigger 5 V supply: brightness 255 is about 9.6 A. Don't use VIN for a 5 V supply (VIN needs 7–12 V). Only feed the Arduino's 5V pin from the supply when USB is unplugged.
- The 330 Ω (220–470 Ω) data resistor prevents ringing and protects the first LED. The 1000 µF cap absorbs the switch-on surge.

## Troubleshooting checklist
**Strip does nothing**
- [ ] Data goes into the **DIN end**: arrows point away from the wire. Wiring the DOUT end is the #1 mistake.
- [ ] Data is on the pin the sketch uses (D9 by default). Check that `PIN_LED` matches.
- [ ] Strip GND (3-pin plug white) is joined to Arduino GND.
- [ ] Strip gets 5 V: measure across the adapter's screw terminals / the strip's +5V/GND pads with a meter, with the adapter plugged in.
- [ ] The resistor is in series (D9 → resistor → DIN), not from data to GND.
- [ ] Solder joints on the strip pads are solid. A cold joint on DIN kills everything after it.
- [ ] The first LED may be dead. Try cutting it off, or feed DIN of LED 2.

**Only some LEDs light / the rest glitch**
- [ ] `LED_COUNT` matches the strip, and the first LEDs show up correctly.
- [ ] Power drops along a long strip. Feed 5 V/GND to both ends, and lower the brightness.

**Wrong colours**
- [ ] Red and green swapped? Change `NEO_GRB` to `NEO_RGB`. The boot wipe should run **red → green → blue**.
- [ ] Random colours or flicker usually means a missing common ground, no data resistor, a long data wire, or a weak supply.
- [ ] Make sure it's really a WS2812B/SK6812 (3 pads: 5V, DIN, GND). A 4-pad strip with CLK is APA102/SK9822 and needs different code. A 12 V strip (WS2811/WS2815) needs a 12 V supply.

**Button does nothing**
- [ ] On a 4-pin tactile switch, the two legs on each *long side* are always connected. Wiring them gives a button that's always "pressed" or never changes. Use **diagonal** legs. The sketch warns at boot if a button reads pressed.
- [ ] The button goes to **GND**, not 5V. With INPUT_PULLUP, a press must pull the pin LOW.
- [ ] The switch straddles the breadboard centre gap, so its legs aren't shorted by one breadboard row.
- [ ] Arcade buttons with a microswitch: use **COM** and **NO**, not NC (NC = works backwards).
- [ ] Watch Serial at 115200: every press and release on D4/D8 prints, and stray pins show up as `NOTE: ... D? went LOW`.

**Arduino resets, USB disconnects, or LEDs dim when turned up**
- [ ] Too much current from USB. Lower the brightness, or use an external 5 V supply with a common ground.

**Moving to the full sketch (`SpiralLamp_clean`)**
- [ ] The lamp stays dark until the **rocker on D7 is ON** (closed to GND). For a quick test, jumper D7 to GND.
- [ ] In the original code, an **unconnected D11 (Chaos) counts as ON**, so Lava/bouncy-ball mode takes over and the buttons don't do what you expect. The clean sketch fixes this: Chaos is ON only when D11 is grounded.
- [ ] If the fader/knob aren't wired yet, set `HAVE_FADER 0` / `HAVE_SPEED_KNOB 0`. Otherwise the floating A0/A1 inputs give random brightness/speed (often near 0 = "nothing lights").
- [ ] Use the **Adafruit NeoPixel** library. FastLED is not used by these sketches.
- [ ] RAM: Uno compile of `SpiralLamp_clean` reports **1293 B** of globals (63%). NeoPixel then mallocs **480 B** for 160 LEDs, leaving about **275 B** for the stack (2048 − 1293 − 480). That is under 400 B. Don't add per-LED arrays; the lamp can crash or reset if the stack runs into the LED buffer.
