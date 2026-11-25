# Four-tower spiral, openings in the bar

Same coil as before. The bar stands on the tube and the C is cut into its outer face. Each end of the bar is the plug. It drops straight into a slot in the ring and a pocket in the cap. There is no arm.

The ring and the cap in [`../four_tower_spiral/`](../four_tower_spiral/) do not fit these bars. Reprint both.

## The coil

| | |
|---|---|
| Tube | 13 mm OD, 965.2 mm cut length (38 in) |
| Straight entry | 46.94 mm, tangent from the ring edge to the first opening |
| Helix | 918.26 mm, **3.345 turns**. One turn is 274.55 mm |
| Pitch | 26 mm. The bore climbs at **5.4°** |
| Openings | **14** (4 + 4 + 3 + 3) |

| Number | Colour | File | Openings, world Z (mm) |
|---|---|---|---|
| 1 blue, back-left | `tower_90` | 20.5, 46.5, 72.5, 98.5 |
| 2 yellow, front-left | `tower_180` | 27.0, 53.0, 79.0, 105.0 |
| 3 green, front-right | `tower_270` | 33.5, 59.5, 85.5 |
| 4 orange, back-right | `tower_0` | 40.0, 66.0, 92.0 |

The tube ends 26 mm past the top yellow opening.

## The bar and the plug

The bar is 18.2 mm radially and 10 mm tangentially. The bore is centred in it.

| | |
|---|---|
| Bore | 13.4 mm (13 mm tube, 0.2 mm clearance) |
| Wall | 2.4 mm |
| Mouth | 11.5 mm, on the **outer** face |
| Along the tube | 10 mm, the bar thickness |
| Plug | the bar end itself, 18.2 × 10.0 mm, 4.5 mm into the ring and 8 mm into the cap |
| Slot | 18.4 × 10.2 mm. The extra 0.2 mm is `joint_clear`, the same style as the old 6.2 × 3.4 over a 6.0 × 3.2 tab |

The 11.5 mm mouth is narrower than the 13 mm tube, so the lips snap and hold. Press the tube in from outside. The cap sits inboard of the mouths, so it can be on already. Nothing stands outside the bar.

A 3 mm 45° chamfer takes the outer bed corner off the bar and out of the slot. Without it the slot breaks the nearest M3 hole: the towers are on the same diagonals as the four deck bolts. The chamfer leaves about 1.5 mm of ring between the slot and the hole. The bolt pattern is unchanged.

Each plug has one notch, 2.4 mm wide and 3.2 mm deep, in the print-top face. From the inside of the bar outward the notches are towers 1, 2, 3, 4. The slot keeps a tooth 0.2 mm smaller. A tower only drops into its own slot, and only one way around. The same notch is on the top plug, so the cap is keyed the same way. The recessed 1–4 on the bar and on the ring say which is which.

## Centre holes

| Part | Hole | What stops it |
|---|---|---|
| Ring | **Ø44.8 mm** | The Ø19 cable window. Its inner edge is 24.8 mm from the centre. The hole stops 2.4 mm short of that. The slots would have allowed about Ø63 mm. |
| Cap | **Ø63.4 mm** | The pockets. 2.4 mm of hoop inside them. The cap has no bolts and no cable window. |

The ring is still Ø128 mm, 4.5 mm thick. The four M3 holes stay on the 76.2 mm square. The cable window stays Ø19 at (+11.71, +32.24). That is the deck joint, and it did not move.

The cap's outside is Ø111.3 mm, 8 mm thick. It is inside the ring.

## Print

Creality CR-10 V2, 300 × 300 × 400, PLA. The STL and 3MF are in the print pose. Towers lie on the flat face, C in the layers. Ring and cap are flat. No supports.

| Part | On the bed (mm) |
|---|---|
| each tower | **18.2 × 126.1 × 10**. The 126 mm side is on the 300 mm axis. |
| `tower_coupon` | **48.6 × 38.5 × 10.2**. Print this first. |
| `tower_base` | **128 × 128 × 4.5**, hole Ø44.8 |
| `tower_cap` | **111.3 × 111.3 × 8**, hole Ø63.4 |

Stood up, the assembly is 128 × 128 × 126.1 mm.

`nurb check` has nothing to fix. The assembly and the cap are clean. The towers warn that the helix bore meets the bed at a few degrees, which polishes the first layer, and that the digit is a thin stroke.

## Reprint

Reprint the ring, the cap, the coupon, and all four towers. The earlier ring and cap do not have these slots.

## Assembly

1. Bolt the ring down. +Y, the cable window, points at the back. Match 1 to 1, then 2, 3, and 4. The tooth stops a tower going in the wrong slot or turned around.
2. Drop the cap on the same way.
3. Press the tube in from the outside, starting at the lowest opening, and wind counterclockwise. The cut end stops a little past the top opening on yellow.
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
