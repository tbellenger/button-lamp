# Four-tower spiral, openings in the bar

Same coil as before. The bar stands on the tube and the C is cut into its outer face. Each end of the bar is the plug. It drops straight into a plain slot in the ring and a pocket in the cap. There is no arm, and no key tooth. The recessed numbers say which tower is which.

The ring and the cap in [`../four_tower_spiral/`](../four_tower_spiral/) do not fit these bars. Reprint both.

## The coil

| | |
|---|---|
| Tube | 13 mm OD, 965.2 mm cut length (38 in) |
| Straight entry | **72.90 mm**. The first 46.94 mm is the tangent from the ring edge in to the coil. The rest is straight tube outside the ring. |
| Helix | **892.30 mm, 3.25 turns**. Exactly 13 quarter-turns. One turn is 274.55 mm |
| Pitch | 26 mm, unchanged. The bore climbs at **5.4°** |
| Openings | **14** (4 + 4 + 3 + 3) |

| Number | Colour | File | Openings, world Z (mm) |
|---|---|---|---|
| 1 blue, back-left | `tower_90` | 20.5, 46.5, 72.5, 98.5 |
| 2 yellow, front-left | `tower_180` | 27.0, 53.0, 79.0, **105.0** |
| 3 green, front-right | `tower_270` | 33.5, 59.5, 85.5 |
| 4 orange, back-right | `tower_0` | 40.0, 66.0, 92.0 |

The helix starts in the lowest opening (blue, 20.5 mm) and the cut end is the centre of the top opening (yellow, 105.0 mm). Both ends of the coiled tube sit in a clip.

The place where the straight tube crosses the ring rim is not in a clip. It is 46.94 mm before the first opening, on the tangent. Putting that crossing in a clip as well, while keeping the cut end in a clip, needs a pitch of about 72 mm. The coil would be more than twice as tall, and 14 quarter-turns will not fit on a 43.5 mm radius at all. Turning the four towers together does not help: the start and the end are 124° apart, which is not a multiple of 90°. The end of the tube was the one that had to land in a clip. The pitch, the 45° tower angles, and the bolt pattern stayed.

## The bar and the plug

The bar is **16.6 mm** radially and **8 mm** tangentially. The bore is centred in it.

| | |
|---|---|
| Bore | 13.4 mm (13 mm tube, 0.2 mm clearance) |
| Wall | **1.6 mm**. Four perimeters on a 0.4 mm nozzle. The lips only spring 0.75 mm each to pass the tube, then sit slack in the bore. Three perimeters (1.2 mm) would split. |
| Mouth | 11.5 mm, on the **outer** face |
| Along the tube | **8 mm**, the bar thickness |
| Plug | the bar end itself, 16.6 × 8.0 mm, 4.5 mm into the ring and 4.5 mm into the cap |
| Slot and pocket | **16.8 × 8.2 mm**. The extra 0.2 mm is `joint_clear` on the full size, the same style as the old 6.2 × 3.4 over a 6.0 × 3.2 tab |

The 11.5 mm mouth is narrower than the 13 mm tube, so the lips snap and hold. Press the tube in from outside. The cap sits inboard of the mouths, so it can be on already. Nothing stands outside the bar.

The slots are plain rectangles plus a 2 mm 45° chamfer on the outer bed corner. The chamfer is only there because the towers sit on the same diagonals as the M3 bolts. A plain corner would leave 0.5 mm of ring; the chamfer leaves about 1.2 mm and stays clear of the bore. It is not a key. The bolt pattern is unchanged.

The recessed 1–4 on the bar and on the ring are the only marks. Any tower fits any slot. Match the numbers.

## Centre holes

| Part | Hole | What stops it |
|---|---|---|
| Ring | **Ø44.8 mm** | The Ø19 cable window. Its inner edge is 24.8 mm from the centre. The hole stops 2.4 mm short of that. The thinner slots would have allowed a larger hole. The window still wins. |
| Cap | **Ø65.2 mm** | The pockets. 2.4 mm of hoop inside them. The cap has no bolts and no cable window. |

The ring is still Ø128 mm, 4.5 mm thick. The four M3 holes stay on the 76.2 mm square. The cable window stays Ø19 at (+11.71, +32.24). That is the deck joint, and it did not move.

## The cap

| | |
|---|---|
| Thickness | **4.5 mm**, the same glue length as the plug in the ring |
| Pocket depth | **4.5 mm**. The pocket goes through the plate |
| Outside | Ø109.3 mm |
| Height | the underside is **117.3 mm** above the deck. The top is **121.8 mm** above the deck |

## Print

Creality CR-10 V2, 300 × 300 × 400, PLA. The STL and 3MF are in the print pose. Towers lie on the flat face, C in the layers. Ring and cap are flat. No supports.

| Part | On the bed (mm) |
|---|---|
| each tower | **16.6 × 121.8 × 8**. The 122 mm side is on the 300 mm axis. |
| `tower_coupon` | **45.4 × 38.5 × 8.2**. Print this first. |
| `tower_base` | **128 × 128 × 4.5**, hole Ø44.8 |
| `tower_cap` | **109.3 × 109.3 × 4.5**, hole Ø65.2 |

Stood up, the assembly is 128 × 128 × 121.8 mm.

`nurb check` has nothing to fix. The assembly and the cap are clean. The towers warn that the helix bore meets the bed at a few degrees, which polishes the first layer, and that the digit is a thin stroke.

## Reprint

Reprint the ring, the cap, the coupon, and all four towers. The slots, the bar, and the cap thickness all changed.

## Assembly

1. Bolt the ring down. +Y, the cable window, points at the back. Match 1 to 1, then 2, 3, and 4.
2. Drop the cap on.
3. Press the tube in from the outside, starting at the lowest opening, and wind counterclockwise. The cut end stops in the top opening on yellow.
4. Glue the plugs once the coil sits where you want it.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The seven parts, in the print pose |
| `render/assembled_iso.png` | Assembly and the tube |
| `render/hook_closeup.png` | One outward opening, on the bed |
| `render/print_orientation.png` | Bed poses |
| `render/base_top.png` | Ring, looking down |
| `render/cap_top.png` | Cap, looking down |
