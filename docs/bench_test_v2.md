# Bench test v2: add the panel parts one at a time

Sketch: `sketches/bench_test_v2/bench_test_v2.ino`. It uses the same pins as the full lamp (`SpiralLamp_R3_Base.ino`), so whatever works here will work there.

Start from the working v1 bench test: strip on D9 through the 330 Ω resistor, strip on the 5 V 3 A supply, strip white tied to Arduino GND, buttons on D4 and D8.

**Rules for every step**
- Unplug USB and the 5 V adapter before you move a wire.
- Every switch goes **pin → switch → GND**. No resistors: the sketch turns on the internal pull-ups.
- Add **one** part, upload or reset, and check the Serial line before adding the next part.
- Open Serial Monitor at **115200**. About 4 times a second you'll see a line like this:

```
A0=512 A1=1023(not seen) | D4=up D8=up D5key=off D6mod=off D7pwr=unwired D10wave=off D11chaos=unwired | mode=RAINBOW bright=20(preset) speed=1.00x
```

Every change on a pin also prints a line of its own, for example `[D10] LOW (closed to GND)`.

Parts that aren't wired yet are safe: an unwired switch reads as off, and the power switch and mushroom are ignored until they've been wired. Unwired pots read a steady `1023(not seen)` and are ignored. To switch an input off completely, set its `USE_xxx` to 0 at the top of the sketch.

---

## Step 0: upload with nothing new wired
Expected: boot wipe red → green → blue, then a rainbow at brightness 20. Serial shows `D7pwr=unwired D11chaos=unwired`, and both pots show `(not seen)`.
- **D4** steps through the patterns SOLID → RAINBOW → COMET → BREATHE → THEATER.
- **D8** steps through brightness 5 → 10 → 20 → 35 → 50.

## Step 1: slide pot (brightness) on A0
| Pot leg | Goes to |
|---|---|
| one outer leg | 5V |
| other outer leg | GND |
| middle leg (wiper) | A0 |

Move the slider off the top end once. `A0=` drops below 1000, `(not seen)` disappears, and the status changes to `bright=..(pot)`. The slider now sets brightness from 0 to 50 (the cap for 160 LEDs on the 5 V 3 A supply, about 1.9 A at full white). If it's brightest at the wrong end, swap the two outer legs. Once the slide pot is live, **D8 shifts the colour** instead of setting brightness.

## Step 2: rotary pot (speed / hue) on A1
Wire it the same way: outer legs to 5V and GND, middle leg to **A1**. Turn it off the end once.
- In RAINBOW, COMET, BREATHE and THEATER, it sets the speed from `0.25x` at one end, through `1.00x` in the middle, to `4.00x` at the other end.
- In SOLID, it sets the colour around the whole colour wheel, and speed stays `1.00x`.

## Step 3: power switch on D7
For a 2-pin toggle or rocker, one pin goes to **D7** and the other to **GND**. Either way round works.

For a 3-pin lit toggle (marked +/power, ACC/load and GND/lamp), use only the two **switch** terminals. Check with a multimeter which pair closes when it's ON (usually + and ACC). Leave the lamp terminal unconnected. Its built-in light won't light at 5 V, which is fine.

Flick it ON: `D7pwr=ON`. Flick it OFF: `D7pwr=OFF`, the strip blacks out, and LED 0 glows faint red to show the board is still alive. Until D7 has been seen ON once, it shows `unwired` and the lamp stays lit.

## Step 4: modifier switch on D6
Wire it like the power switch: 2 pins, one to **D6** and one to **GND**. With it ON (`D6mod=ON`), the pattern mirrors about the middle of the strip and runs in the opposite direction. This is easiest to see in COMET or RAINBOW.

## Step 5: wave switch on D10
Wire it like the power switch: one pin to **D10** and one to **GND**. With it ON (`D10wave=ON`), a bright/dim wave travels along the strip on top of whatever pattern is running.

## Step 6: key switch on D5 (use terminals 3–4, the NO pair)
| Key switch terminal | Goes to |
|---|---|
| 3 | D5 |
| 4 | GND |

Turn the key ON: `D5key=ON` and the strip goes to RAINBOW whatever D4 is set to. Turn it OFF and you're back to the D4 pattern. If it's backwards (rainbow with the key OFF), you're on the NC pair. Move to the other two terminals.

## Step 7: mushroom stop on D11 (use terminals 1–2, the NC pair)
| Mushroom terminal | Goes to |
|---|---|
| 1 | D11 |
| 2 | GND |

NC means the contact is **closed when released** and **opens when you press** the mushroom. Released, D11 reads LOW, the status says `D11chaos=off`, and Chaos is armed. Press or latch it down: `D11chaos=ON`, and the strip sparkles randomly. It's single pixels at about 9 Hz, capped at the same brightness, not a full-strip strobe. Twist or pull to release, and it goes back to the normal pattern.

If it shows `unwired` after you've wired it, D11 has never read closed. Check that you're on 1–2 and not the NO pair, and that the mushroom isn't latched down.

## Arcade buttons (already on D4 and D8)
Each microswitch has **COM**, **NO** and **NC**. COM goes to GND, NO goes to the pin, and NC stays unconnected. A press prints `D4=DOWN` / `D8=DOWN`. If the sketch warns at boot that a button reads pressed, it's wired on NC.

---

## Quick reference
| Control | Pin | Does |
|---|---|---|
| Arcade button 1 | D4 | next pattern |
| Arcade button 2 | D8 | brightness preset; colour shift once the slide pot is live |
| Slide pot | A0 | brightness 0–50 |
| Rotary pot | A1 | speed 0.25×–4× (hue in SOLID) |
| Key switch NO (3–4) | D5 | rainbow override |
| Modifier switch | D6 | mirror + reverse |
| Power switch | D7 | OFF = blackout |
| Wave switch | D10 | travelling wave |
| Mushroom NC (1–2) | D11 | chaos sparkle while pressed |

Priority, highest first: power OFF, then Chaos, then key (rainbow), then the D4 pattern. Wave and the modifier apply on top of the pattern.

**Note for the full sketch:** `SpiralLamp_R3_Base.ino` has `CHAOS_ACTIVE_HIGH 1` (mushroom NC: pressed = pin HIGH). Chaos arms only after D11 has been seen LOW, so an unwired pin does not force Lava.

Compile check (arduino-cli, `arduino:avr:uno`): 12,048 B flash (37%), 330 B globals (16%). The 480 B strip buffer is allocated at runtime, leaving 1,238 B for the stack.
