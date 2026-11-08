# Stackable cross mount

A printed mount for the 13 mm silicone LED tube. Each level is two crosses that half-lap at the centre. A spacer on that lap carries the next level. The base bolts to the lamp's existing top-deck holes.

This replaces the loose saddle clips. Those clips printed with the layer lines running along the tube, and the arms broke. Printed on their side, the same clip felt strong, and nothing held a loose clip to the centre. A one-piece level with a pin on top cannot put all four clips in that strong orientation: the four C shapes sit in four different planes. Two crosses can. Each cross is a diameter, two clips opposite each other, and that whole C lies in the layer plane when the cross is printed flat.

## Size

| | |
|---|---|
| Tube | 13 mm OD |
| Centre-line radius | 43.5 mm (was 33.5). Outside of the coil is 100 mm. |
| Pitch | 23 mm. Gap between turns is 10 mm. |
| Clips on a level | 4, a quarter turn and 5.75 mm apart |
| Levels for 1 m of strip | 4 (15 clip stations on the strip, 16 molded) |
| Parts for 1 m | 1 base, 4 low crosses, 4 high crosses, 3 spacers, 1 cap |
| Extra tube | `extra_levels` on the assembly adds one low cross, one high cross, and one spacer per level. The cap stays on top. |

The lowest clip is 40° of helix past the light-cable hole, so the lead-in still faces that hole. The next level starts one pitch higher. The helix does not jump at the joint.

## Cable hole

Tom asked for 5 mm more diameter on the hole the light cable passes through.

| Part | Before | After |
|---|---|---|
| Plywood top, hand-cut templates, finger-joint top, sheet-metal hood | Ø10 | Ø15 |
| Printed `top_backer_plate` | Ø10 | Ø15 |
| This mount's base window | Ø14 | Ø19 |

The window stays 4 mm larger on the diameter than the deck hole, which is what the old mount did (Ø14 over Ø10). The hole is still at (+11.71, +32.24) mm. The nearest M3 bolt is about 16 mm clear of the Ø19 edge.

**A board or hood that is already cut at Ø10 is still Ø10.** Changing the file does not enlarge a hole that has been cut. Open it to Ø15 with a drill or a file. The original one-piece cage's internal Ø8 channel is a different hole and was not changed. USB, the DC jack, and the button cutouts were not changed. The purchased STLs in `original-plans/` were not changed.

Updated in this change: the v2 finger-joint top DXF and SVG, the v2 sheet layout and cut list, both sheet-metal hood DXFs and SVGs, the hand-cut top templates (3 mm and 6 mm SVG and letter PDF), and `top_backer_plate.stl`. The as-ordered SendCutSend set (`as_ordered_S7872337`) is left at Ø10. That set is the record of what was ordered. It is not a new cut file.

## Print

Creality CR-10 (300 × 300 × 400), PLA, 0.4 mm nozzle. Tom slices in Cura. Nothing here was sliced: this machine has no slicer installed. `nurb export` writes geometry-only 3MF and says so.

| Part | On the bed | Notes |
|---|---|---|
| `spiral_cross_low`, `spiral_cross_high` | Flat. The C is the layer plane. | 105 × 30 × 14 mm. The bore follows the helix, and the walls of that bore tilt only a few degrees, so they do not need support. |
| `spiral_base` | Flange down | 100 × 100 × 35.5 mm. Pin up. Ø19 window, 4 × Ø3.4 on the 76.2 mm square. |
| `spiral_spacer` | Socket down, pin up | The socket closes with a 45° cone, so there is no bridged ceiling. 28 mm across, 27.5 mm tall. |
| `spiral_cap` | Either way | Through hole. 28 × 9 mm. |
| `spiral_fit_coupon` | As exported, tabs on the bed | Print this first. |

The weak direction of a cross is splitting between layers through the 14 mm thickness. The half-lap tongue is 7 mm of the 14 mm hub, and the pin groove takes 3.1 mm of that, leaving about 3.9 mm. The spacer clamps the joint so the lap is not the only thing resisting wobble.

## Fits — not measured

These are design choices. Nothing has been printed or fitted. Print `spiral_fit_coupon`, snap the tabs, and try the pin in the socket and the two tongues together before printing all four levels. Then print one low cross, one high cross, and the base before the rest.

| Fit | Allowance |
|---|---|
| Half-lap slot | 0.3 mm wider than the 14 mm thickness |
| Pin Ø6 into the hole | 0.2 mm on the diameter (hole Ø6.2) |
| Clocking flat | 0.15 mm past the pin flat |

The flat is on the low cross only. It faces the lowest clip, so the level cannot be turned 180°. The high cross is clocked by the lap, not by the flat. A round hole in the high cross lets it drop over the flat.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The six parts. The assembly is not a separate STL. |
| `render/assembled_iso.png` | Stack with the tube |
| `render/exploded.png` | Levels pulled apart |
| `render/lap_and_pin.png` | One joint |
| `render/print_orientation.png` | Bed poses |
| `system.py` | Shared geometry |

## What changed relative to the loose-clip mount

The loose clips and the 80 mm coil are gone. Centre-line radius is 43.5 mm. The crosses and the spacers are the structure. Counts above are for this coil, not the old eight saddle clips.
