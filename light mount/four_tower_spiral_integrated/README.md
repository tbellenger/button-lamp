# Four-tower spiral, openings in the bar

Same ring and cap as [`../four_tower_spiral/`](../four_tower_spiral/). The tower bar stands on the tube. Each C is cut into the outer face of that bar. There is no neck and no separate clip.

The foot is a short arm back to the original slot, and the top is a short arm back to the original cap pocket. A ring and a cap already printed from the earlier set still fit.

## The coil, unchanged from the count fix

| | |
|---|---|
| Tube | 13 mm OD, 965.2 mm cut length (38 in) |
| Straight entry | 46.94 mm, tangent from the ring's outer edge (radius 64 mm) to the first opening |
| Helix | 918.26 mm, **3.345 turns** |
| One turn | 274.55 mm at the 43.5 mm centreline |
| Pitch | 26 mm. A quarter turn is 6.5 mm. The bore climbs at **5.4°** |
| Openings | **14** (4 + 4 + 3 + 3) |

The tube ends 26 mm past the top opening, on yellow, short of the next tower.

## Where the openings are

Same 45° rotation. Yellow 2 and green 3 are the front corners. +Y is the back. −Y is the buttons.

| Number | Colour | File | Place | Openings | World Z (mm) |
|---|---|---|---|---|---|
| 1 | blue | `tower_90` | back-left, 135° | 4, the lowest | 20.5, 46.5, 72.5, 98.5 |
| 2 | yellow | `tower_180` | front-left, 225° | 4, the highest | 27.0, 53.0, 79.0, 105.0 |
| 3 | green | `tower_270` | front-right, 315° | 3 | 33.5, 59.5, 85.5 |
| 4 | orange | `tower_0` | back-right, 45° | 3 | 40.0, 66.0, 92.0 |

## The bar and the C

The bar is centred on the tube. It is 18.2 mm radially and 10 mm tangentially. The 10 mm is the grip along the tube: the bore goes through the bar.

| | |
|---|---|
| Tube | 13 mm |
| Bore | 13.4 mm (0.2 mm clearance) |
| Wall | 2.4 mm on the closed sides |
| Mouth | 11.5 mm, on the **outer** face |
| Along the tube | 10 mm, the bar thickness |
| Tangent | the bore rises 5.4°, the helix angle at 26 mm pitch |

The mouth faces away from the lamp. You press the tube inward, from outside, and the 11.5 mm lips are narrower than the 13 mm tube, so it snaps and stays. The inner face of the bar is solid. The foot and cap arms stay on that inner side and stop at the bar, so nothing stands outside the mouth. The cap can be on already: it sits at radius 27.4 mm, inboard of the bar, and does not cross the openings.

The bore is not square to the bar. Across the 10 mm it follows the helix, so the tube is not kinked at the opening.

The bar lies on the bed, 10 mm thick. That puts the bore centre 5 mm off the tower's radial plane. The bar is shifted in until the centreline is still at 43.5 mm. The helix phase is −6.6° so every bore is on the tube. The towers themselves stay on the 45° lines, and the heights above do not move.

## Base and cap

They did not change. The slot and the pocket are still 6.2 × 3.4 mm. The foot tab is still 6.0 × 3.2 mm, 4.5 mm through the ring. The ring solid is the same 55174.3 mm³. The cap is the same 11268.5 mm³ hoop.

The cap sits at 117.6 mm above the ring, 4 mm higher than the necked towers, because the arm needs room above the top opening. The pocket is the same pocket.

## Print

Creality CR-10 V2, 300 × 300 × 400 mm, PLA, 0.4 mm nozzle. Tom slices in Cura. The STL and 3MF are already lying down. Flat face on the bed. The C is the layer outline. No supports.

| Part | Qty | On the bed (mm) | Against the 300 × 300 bed |
|---|---|---|---|
| `tower_90` | 1 | 33.3 × 130.1 × 10 | 130 mm on the 300 mm axis. All four bars are this size. |
| `tower_180` | 1 | 33.3 × 130.1 × 10 | same |
| `tower_270` | 1 | 33.3 × 130.1 × 10 | same |
| `tower_0` | 1 | 33.3 × 130.1 × 10 | same |
| `tower_coupon` | 1 | 60.3 × 40.5 × 10 | print this first |
| `tower_base` | 1 | 128 × 128 × 4.5 | reuse the earlier ring |
| `tower_cap` | 1 | 54.8 × 54.8 × 8 | reuse the earlier cap |

Stood up, the assembly is 128 × 128 × 130.1 mm.

`nurb check` reports nothing to fix. It warns that the helix bore meets the bed at a few degrees, which polishes the first layer, and that the recessed digit is a thin stroke. Neither needs a support.

## Reprint

Reprint the coupon, then the four towers. Leave the ring and the cap.

## Assembly

1. Bolt the ring down. +Y, the cable window, points at the back. Number 1 is back-left, then 2, 3, and 4 counterclockwise.
2. Drop each tower into the slot with the same number. The bar is out at the tube. The arm reaches in to the slot.
3. Drop the cap on. It does not pass the tube, and it does not cover the mouths.
4. Bring the tube in from the side along the tangent, and press it inward into the lowest opening. Wind counterclockwise and up. The cut end stops a little past the top opening on yellow.
5. Glue the feet and the cap once the coil sits where you want it.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The seven parts, in the print pose |
| `render/assembled_iso.png` | Assembly and the tube, straight entry included |
| `render/hook_closeup.png` | One opening, from the outside, on the bed |
| `render/print_orientation.png` | Bed poses |
| `system.py` | Shared geometry |
