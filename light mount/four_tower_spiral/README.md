# Four-tower spiral

A separate way to hold the 13 mm silicone tube. It does not replace the cross mount in [`../spiral_tube_mount/`](../spiral_tube_mount/).

The base is only a ring: the existing M3 bolts, a clear path for the light cable, and four attachment points. Four towers stand at 0°, 90°, 180° and 270°. Clips on the towers follow one helix. A cap drops onto the four tops and holds them on their circle.

## How thin the rib is

The first version of this mount was a slab: **26 mm radial by 14 mm tangential**, full height. That blocked the view through the coil. The running member is now a rib **6.0 mm radial by 3.2 mm tangential**. The foot, each clip, and the spoke into that clip are local pads. The cap is a hoop about 11 mm wide instead of a 22 mm band.

nurb has no stress solver, and there is no measured uncoil force for this tube. The 6.0 × 3.2 mm section is a printable default, not a calculated minimum.

What sets it:

- **3.2 mm tangential** is the width you see edge-on through the coil. It is eight perimeters on a 0.4 mm nozzle, solid, on the 3 mm wall floor. Thinner than 2.4 mm is a ribbon that splits when the tube is pushed into the clip.
- **6.0 mm radial** is the depth that takes the uncoil bend. That bend is in the layer plane, because the tower prints on its side. Six millimetres is enough to read as a beam over the span from the ring to the cap, and still much smaller than the 10 mm gap between turns. It is not derived from a tube force.
- The **foot is local**, about 12.4 mm by 12.4 mm and 16 mm tall, only so a 5.2 mm-across-flats diamond pin (the print-rule floor for a pin, diagonal 7.35 mm) has 2.4 mm of wall. That pad does not run up the tower.
- Each **clip stays 14 mm** along the tube, standing off one face of the rib. The spoke into the clip is 4 mm tall.

If a trial print flexes, raise `rib_radial` first (6, then 8). That adds depth where the coil pushes out, and it barely changes the view. Raise `rib_tangential` only if the rib twists. Raise `clip_arm` if a spoke at the clip root bends. All three are sliders.

## Why the towers print on their side

Clips printed upright, with the layer lines running along the tube, broke across those layers. A clip printed on its side, so the C is the layer plane, holds. Each tower is that second pose for every clip on it. The snap arms are loaded in the layers, not across them. Infill does not fix the upright version.

## Coil

| | |
|---|---|
| Tube | 13 mm OD |
| Centre-line radius | 43.5 mm. Outside of the coil is 100 mm. |
| Pitch | 23 mm. Quarter turn is 5.75 mm of rise. |
| Towers | 0° (+X, right), 90° (+Y, back), 180° (−X), 270° (−Y, front) |
| Where the helix starts | The **90° tower**, two notches. Lowest clip. |
| Lead-in | The cable hole is at (+11.71, +32.24), about 70° from +X. The first clip is at 90°, so the tube climbs about 20° from the hole to that clip, counterclockwise. |

A 1 m strip is 15 clip stations (k = 0…14). Station 15 is past the strip.

| Tower | Notches | Clips on a 1 m strip | Stations |
|---|---|---|---|
| `tower_0` | 1 | 3 | 3, 7, 11 |
| `tower_90` | 2 | 4, including the lowest | 0, 4, 8, 12 |
| `tower_180` | 3 | 4 | 1, 5, 9, 13 |
| `tower_270` | 4 | 4, including the highest | 2, 6, 10, 14 |

`strip_length` and `extra_stations` change the count and the tower height together. The four towers, the cap, and the ring have to use the same values.

## Print

Creality CR-10, PLA, 0.4 mm nozzle. Tom slices in Cura. No slicer is installed here. `nurb slice` says so, and the 3MF files are geometry only. These checks are the solids, not a sliced toolpath.

| Part | Qty | Size (mm) | On the bed |
|---|---|---|---|
| `tower_base` | 1 | 128 × 128 × 12.7 | Ring down. Pins up. |
| `tower_0` | 1 | 35 × 122 × 14 overall | On its side. The rib is 6.0 × 3.2. One notch. |
| `tower_90` | 1 | 35 × 122 × 14 overall | On its side. Two notches. Start of the coil. |
| `tower_180` | 1 | 35 × 122 × 14 overall | On its side. Three notches. |
| `tower_270` | 1 | 35 × 122 × 14 overall | On its side. Four notches. |
| `tower_cap` | 1 | 55 × 55 × 8 | Ring down. Hoop about 11 mm wide. |
| `tower_coupon` | 1 | 67 × 35 × 14 | On its side. Print this first. |

The diamond socket in the foot points up in that side pose, so the roof is 45°. The top of each tower is a tenon that is part of the same extrusion, full thickness, so it sits on the bed. A pin sticking out the end would have hung in the air.

## Fits — not measured

| Fit | Allowance |
|---|---|
| Diamond pin into the foot | 0.2 mm extra across the flats. Pin diagonals 8 × 8 mm (5.66 mm across the flats). Hole diagonals 8.28 × 8.28 mm (5.86 mm across the flats). |
| Lug into its pocket | 0.2 mm extra on the width. Each tower's lug is at a different radius. |
| Tenon into the cap | 0.2 mm extra on the width and on the thickness. |

Snug, from the fit table. Nothing here has been printed. The coupon's gauge should slide into the socket. If it binds, change `PIN_CLEAR` and reprint the coupon, not all four towers.

## Assembly

The ring's pins and the lugs stop two mistakes. A tower turned 180° puts its tooth into solid pin. A tower on the wrong pad puts its lug onto solid ring. The 90° tower, the one with two notches, goes at the back. Its lowest clip faces out, toward the back of the lamp.

The cap is inboard of the tube. Two orders both work:

1. Towers, then the cap, then snap the tube into the clips from the outside, starting at the lowest clip and winding counterclockwise.
2. Towers, snap the tube, then drop the cap on from above. The cap does not have to pass the tube.

The clips stay reachable either way. The cap is what keeps the tops from spreading. The diamond pins keep the feet from twisting.

## Cable

The ring leaves a Ø19 window at (+11.71, +32.24). The deck and the backer are Ø15. The nearest pin is the back tower's, and it clears that window. USB, the DC jack, and the button holes are not part of this mount.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The seven parts |
| `render/four_tower_front.png` | Through the front, far coil visible |
| `render/four_tower_side.png` | Through the side |
| `render/exploded.png` | Ring, towers, cap |
| `render/clip_closeup.png` | Lowest clip, in the print pose |
| `render/print_orientation.png` | Bed poses |
