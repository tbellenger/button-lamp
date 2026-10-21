# What's wrong or inconsistent in the original Spiral Lamp plans

## Why a breadboard test "doesn't work" with the original code
1. **An unconnected Chaos pin = Chaos ON.** D11 uses `INPUT_PULLUP` but treats **HIGH as ON** (all 4 versions do: V1/V3 via `!pressed()`, V2/V4 via `digitalRead()==HIGH`). With nothing on D11, the lamp boots into Lava/"bouncy ball" mode, which overrides every other mode, and the buttons only change ball colours.
2. **Nothing lights without the rocker.** D7 must be closed to GND before the base lamp turns on. The Readme mentions this only in troubleshooting.
3. **Floating fader/knob.** Brightness comes from A0 and speed from A1. With no pots wired, those pins float, so brightness is random and often about 0 (dark strip) and speed is erratic. There's no "no pot" option.
4. **Full brightness by default.** `strip.setBrightness(255)`, and Jumbo starts at full **white**. 60 LEDs at about 3.6 A can't run from USB / Arduino 5V, so the board browns out and resets, or the strip glitches. (Tom's strip is **160 LEDs**, up to about 9.6 A at full white; the clean sketch caps brightness at **50** so it runs on his **5 V 3 A** supply, about 1.9 A max.)
5. **No Serial output at all**, so there's no way to see which inputs the board actually reads.

## Code bugs / weaknesses (V4, "Exact YouTube Version")
- **Debounce is a lockout, not a debounce.** The first raw change is accepted *immediately*, then ignored for 15 ms, so a noise spike counts as a press and a bounce 15 ms later can count as a release. Replaced with "stable for 20 ms".
- **Chaos read raw.** `chaosOn()` uses an undebounced `digitalRead`, while the "chaos turned off" edge uses the debounced input. Also, `inChaos.begin()` is followed by a redundant `pinMode()`.
- **Regression of the author's own fix.** V2 changed the Wave zero-window check to use each LED's *current* mode ("FIXED"). V4 reverted it to the *desired* mode, which brings back glitchy mode switches in Wave/Epic.
- **Two separate `static bool prev`** edge trackers (Rainbow-handover path and Jumbo path) for the same colour-cycle toggle, so the toggle edge can be missed or doubled when switching paths.
- **Inconsistent colour calibration.** Jumbo/handover/Dots paths skip the `CAL_*_GAIN` correction that Wave/Rainbow/Lava apply, so the same colour looks slightly different between modes. (Noted, not changed.)
- Unused variables/warnings (`Acol`, `O_R`, `O_G`, `eJ`) and a confusing `O_Gb` "shadowing" workaround.
- Pins, LED count and colour order are scattered `#define`s with no single config block, and the brightness cap is missing.
- **Hard-coded for 60 LEDs.** The centre-out fades use LED 29/30 as the centre and an 8-bit stagger that overflows on longer strips. Fixed: the centre follows `LED_COUNT`.
- **Doesn't fit in RAM with a long strip.** The plans' code uses ~1.7 KB of globals (several `LED_COUNT`-sized arrays, colour tables and strings in RAM); NeoPixel then mallocs 3 bytes per LED at runtime (432 B at 144, 480 B at 160), leaving almost nothing for the stack, so it crashes. Fixed in the clean sketch: tables/strings in flash, on-the-fly Jumbo brightness, 2-bit Wave modes. That diet took a 144-LED build from 1728 B of globals to 1236 B, about 380 B free after the strip buffer. `LED_COUNT` is now 160; the Uno compile line for that build is in `sketches/SpiralLamp_R3_Base/SpiralLamp_R3_Base.ino`. Free RAM after the 480 B buffer is under 400 B, so don't add per-LED arrays.

## Version confusion
- **4 versions, and the Readme says "paste this full sketch" without saying which one.** V1 is a different, older program (Chaos = scramble effect; pin names `PIN_ARCADE`/`PIN_GREEN`). V3 "Chaos Button Edition" splits the strip into 4 zones for Chaos. V2 and V4 are nearly identical: V2 has the Wave fix and a Lava brightness boost, V4 has the fader-top fix.
- **V3 doesn't fit on an Arduino Nano with the old bootloader** (30,980 bytes > 30,720). It compiles only for the Uno / new-bootloader Nano. V1, V2 and V4 fit everywhere.
- The folder names contain curly quotes and en-dashes ("Version 4 – “Exact YouTube Version”"), and the Arduino IDE wants the folder to be named exactly like the `.ino` (`V4`). Opening the file prompts you to move it.
- All versions compile cleanly for the Uno (checked with arduino-cli, Adafruit NeoPixel 1.15.5). So "doesn't work" is wiring/defaults, not a compile error.

## PDFs vs code vs video
- **Chaos wiring is contradictory.** The Pin Map says an SPDT with common → D11 and throws to 5V/GND. The Readme says "5V → switch → D11 plus a pulldown", while the code *also* enables the internal pull-up. Every other switch goes to GND. In the video it's "the big red button… press at any point", which suggests a latching push button, not a 2-throw switch.
- The PDFs have **no diagram at all**: text tables only, and "Wiring Tips (High-Level, Not Step-by-Step)".
- Missing details: strip input-end/arrow orientation, the 4-pin tactile diagonal-leg gotcha, which arcade microswitch terminals to use (COM/NO), Arduino power while the strip is on an external supply, the enclosure parts list (rocker, toggles, key switch, covered toggle, latching button, fader, knob), and the Arduino model (just "Uno-class").
- **Mode names differ.** The video calls wave + toggle + key "**comet mode**". The code/Readme call it "**Epic**" wave. The video calls Chaos "**bouncy ball mode**". In an earlier short (ZQ1QPfyrVCk) Chaos "scrambles" the LEDs, which is V1/V3 behaviour, not V4's Lava.
- **Button colours changed over time.** An early short (LsDJrKt6cIY) has a *red* arcade button for dots and a *green* one for colour-wheel dots (V1's `PIN_GREEN`). The final video and V4 use *white* = spawn dot and *red* = delete.
- The Readme says wave starts in the Jumbo colour. An early short says wave is "just purple and blue" at first. That matches older code, not V4.
- The plans claim "the code debounces these inputs internally". It does, but badly (see above).
- **FastLED isn't used anywhere.** Only Adafruit NeoPixel is needed.
