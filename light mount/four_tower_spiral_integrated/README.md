# Four-tower spiral, openings in the bar

Same coil as before. The bar stands on the tube and the C is cut into its outer face. There is no arm and no plug. Each end of the bar has a curved slot. A thin ring sits in the bottom slots and a thin disc sits in the top slots. The recessed numbers on the towers say which bar is which.

The ring and the cap in [`../four_tower_spiral/`](../four_tower_spiral/) do not fit these bars.

## The coil

| | |
|---|---|
| Tube | 13 mm OD, 965.2 mm cut length (38 in) |
| Straight entry | **72.90 mm** |
| Helix | **892.30 mm, 3.25 turns**. Exactly 13 quarter-turns. One turn is 274.55 mm |
| Pitch | 26 mm. The bore climbs at **5.4°** |
| Openings | **14** (4 + 4 + 3 + 3) |

| Number | Colour | File | Openings, world Z (mm) |
|---|---|---|---|
| 1 blue, back-left | `tower_90` | 20.5, 46.5, 72.5, 98.5 |
| 2 yellow, front-left | `tower_180` | 27.0, 53.0, 79.0, **105.0** |
| 3 green, front-right | `tower_270` | 33.5, 59.5, 85.5 |
| 4 orange, back-right | `tower_0` | 40.0, 66.0, 92.0 |

The helix starts in the lowest opening (blue, 20.5 mm) and the cut end is the centre of the top opening (yellow, 105.0 mm).

## The bar

The bar is **16.6 mm** radially and **8 mm** tangentially. Unchanged.

| | |
|---|---|
| Bore | 13.4 mm (13 mm tube, 0.2 mm clearance) |
| Wall | **1.6 mm** |
| Mouth | 11.5 mm, on the outer face |
| Along the tube | **8 mm** |

## Base ring

The ring is only as wide as the four M3 holes. The holes stay on the deck's **76.2 mm** square, centres (±38.1, ±38.1), drilled **Ø3.4** so they match the deck. The bolt circle is 53.88 mm from the lamp axis.

| | |
|---|---|
| Inside radius | **49.18 mm** (Ø98.4) |
| Outside radius | **58.58 mm** (Ø117.2) |
| Width | **9.4 mm**. 3 mm of PLA past each edge of the Ø3.4 hole, enough for an M3 socket head to bear |
| Thickness | **2.4 mm** |

The cable leaves the deck at (+11.71, +32.24), 34.3 mm from the centre. The deck hole is Ø15, so it reaches 41.8 mm. The ring's inner edge is at 49.2 mm. The gap from the deck-hole edge to the ring is **7.4 mm**. The old Ø19 window would have reached 43.8 mm, still **5.4 mm** inside the ring. The middle of the ring is open. Nothing covers the cable.

The bolt circle sits outside the bar. The bar's outer face is at 51.6 mm on the bed edge, and the hole's inner edge is at 52.2 mm, so the screws clear the towers by about 0.6 mm. The ring's inner band is what enters the bar: **2.4 mm** at the bed face, **3.1 mm** on the far face. The slot therefore opens through the outer face of the bar. The inner arc is the fit.

## Top disc

| | |
|---|---|
| Outside radius | **48 mm** (Ø96) |
| Hole | **40 mm** (Ø80). The annulus is 8 mm wide |
| Thickness | **2.4 mm** |

48 mm puts the rim through the bar with **3.5 mm** of bar outside the slot and **4.0 mm** inside it. The hole leaves the middle open.

## Slots and guides

Both plates are 2.4 mm. Each slot is **2.6 mm** deep, 0.2 mm over the plate. The plate sits on the floor of the slot.

| Slot | Arc radii | Radial width | Depth |
|---|---|---|---|
| Top | **39.9 / 48.1 mm** | **8.2 mm** (0.2 mm over the 8 mm annulus, 0.1 mm a side) | **2.6 mm** |
| Bottom | inner arc **48.98 mm** (0.2 mm under the ring's 49.18 mm). The outer side is open | the overlap, 2.4–3.1 mm | **2.6 mm** |

The arcs are concentric with the lamp axis.

Each plate has two tabs at every tower, standing perpendicular to the plate. They flank the 8 mm bar.

| | |
|---|---|
| Tab | **2.0 mm** thick, **4.0 mm** tall |
| Gap | **8.2 mm** (0.1 mm each side of the bar) |
| Top tabs | 4 mm along the radius, from r 42 to 46. They hang down beside the bar. Print the disc tabs-up and turn it over |
| Base tabs | 2.4 mm along the radius, on the inner edge of the ring, standing up. That keeps them **0.6 mm** clear of the M3 hole |

## Print

Creality CR-10 V2, 300 × 300 × 400, PLA. Towers lie on the flat face, C in the layers. Ring and disc are flat, tabs up. No supports.

On a tower the slot is a straight notch in each layer. The arc shows up as that notch shifting across the 8 mm of layers, about **0.7 mm**, roughly 5° from vertical. That is polish, not a support.

| Part | On the bed (mm) |
|---|---|
| each tower | **16.6 × 117.3 × 8** |
| `tower_coupon` | **16.6 × 38.5 × 8**. One opening and the bottom slot. Print this first. |
| `tower_base` | **Ø117.2 × 6.4** with the tabs. The ring itself is 2.4 mm |
| `tower_cap` | **Ø96 × 6.4** with the tabs. The disc itself is 2.4 mm. Turn it over |

Stood up, the assembly is 117.2 × 117.2 × **117.3** mm. The disc's top is at 117.1 mm. The tower tops are the high point.

`nurb check` has nothing to fix. The assembly, the ring, and the disc are clean. The towers still warn that the helix bore meets the bed at a few degrees, and that the digit is a thin stroke.

## Reprint

Reprint the ring, the disc, the coupon, and all four towers.

## Assembly

1. Bolt the ring down. The cable window side, +Y, points at the back. The cable comes up through the open middle.
2. Drop the four towers onto the ring. The numbers face you. The tabs take the sides.
3. Turn the disc over so the tabs point down, and drop it into the top slots.
4. Press the tube in from the outside, starting at the lowest opening, and wind counterclockwise. The cut end stops in the top opening on yellow.
5. Glue once the coil sits where you want it.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The seven parts, in the print pose |
| `render/assembled_iso.png` | Assembly, ring, disc, and the tube |
| `render/slot_closeup.png` | Top slot, tower on its side |
| `render/print_orientation.png` | Bed poses |
| `render/base_top.png` | Ring, looking down |
| `render/disc_top.png` | Disc, looking down, tabs up |
