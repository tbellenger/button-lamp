# Four-tower spiral, clips on a spine

Same coil as before. Each tower is a 4 mm spine with a C-ring at every clip, opening outward. The foot is a tab in a lobe on the base ring. A plain disc sits in a curved slot at the top. The recessed numbers say which tower is which.

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

The spine is **4.0 mm** radially and **8 mm** thick, the same thickness as each clip, so the tower is one slab. 4 mm is as wide as the spine can be without entering the 13.4 mm bore: the bore's inner edge is 0.3 mm outside the spine, and the spine bites **1.3 mm** into the clip wall. That bite is about **8.9 mm** along the tower.

Each clip is a C-ring, not a hole in a bar. Bore 13.4 mm, mouth 11.5 mm on the outer side, wall 1.6 mm, 8 mm along the tube. The centres stay on the 43.5 mm helix.

The foot is the spine itself, **4.0 × 8.0 mm**, **4.0 mm** long, standing on the deck. The top pad carries the same disc slot as before: arcs 42.2 and 44.8 mm, 1.4 mm deep.

## Base ring

| | |
|---|---|
| Inside radius | **50.58 mm** (Ø101.2) |
| Outside radius | **57.18 mm** (Ø114.4) |
| Band width | **6.6 mm**. The Ø3.4 hole plus 1.6 mm of wall each side |
| Band thickness | **2.4 mm** |
| Lobes | four, one under each tower, **4.0 mm** tall |
| Slot | **4.2 × 8.2 mm** through the lobe. 0.2 mm over the 4.0 × 8.0 mm tab |

The four M3 holes stay on the 76.2 mm square, on the band just outside each lobe. The centre is open. The cable at (+11.71, +32.24) is 34.3 mm from the middle. The deck hole reaches 41.8 mm. The ring starts at 50.6 mm, so the cable clears the band by about 9 mm.

## Top disc

A plain annulus. No tabs, no pockets.

| | |
|---|---|
| Outside radius | **44.7 mm** (Ø89.4) |
| Hole | **42.3 mm** (Ø84.6) |
| Radial width | **2.4 mm**. Six perimeters on a 0.4 mm nozzle, centred on the 43.5 mm coil |
| Thickness | **1.2 mm**, six layers at 0.2 mm |

The hole is the open centre. About **6.8 mm** of bar stays outside the slot and **6.3 mm** stays inside it, so the curved walls remain.

The underside is **114.7 mm** above the deck. The top of the disc is **115.9 mm**. The tower tops are **116.1 mm**, 0.2 mm above the disc. The slot arcs are **42.2 / 44.8 mm**, 1.4 mm deep.

## Print

Creality CR-10 V2, 300 × 300 × 400, PLA. Towers lie on the flat face. The C is the layer outline. The ring and the disc print flat. No supports.

The clip root is 1.3 mm of the clip wall. That is the joint the tube loads when it snaps in. The spine is 4 mm across the layers and 8 mm up from the bed.

| Part | On the bed (mm) |
|---|---|
| each tower | spine **4 × 8**, clips out to about **17** radially, **116.1** long |
| `tower_coupon` | short spine, one clip, and the tab. Print this first. |
| `tower_base` | **Ø114.4 × 4.0** with the lobes. The band itself is 2.4 mm |
| `tower_cap` | **Ø89.4 × 1.2**, hole Ø84.6 |

Stood up, the assembly is 114.4 × 114.4 × **116.1** mm. The tower tops are the high point.

`nurb check` has nothing to fix.

## Reprint

Reprint the four towers, the base, and the coupon. The top disc did not change.

## Assembly

1. Bolt the ring down. +Y, toward the cable, points at the back. The cable comes up through the open middle.
2. Drop each tower tab into its lobe. Match 1 to 1, then 2, 3, and 4.
3. Drop the disc into the top slots.
4. Press the tube in from the outside, starting at the lowest opening, and wind counterclockwise. The cut end stops in the top opening on yellow.
5. Glue once the coil sits where you want it.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The seven parts, in the print pose |
| `render/assembled_iso.png` | Assembly, ring, disc, and the tube |
| `render/tower_side.png` | One tower from the side, clips on the spine |
| `render/print_orientation.png` | Bed poses |
| `render/base_top.png` | Ring and lobes, looking down |
| `render/base_slot_closeup.png` | Tab seated in a lobe |
| `render/disc_top.png` | Disc, looking down |
| `render/exploded.png` | Base, towers, and disc pulled apart. Numbers beside the towers |
