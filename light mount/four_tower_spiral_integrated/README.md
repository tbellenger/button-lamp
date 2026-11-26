# Four-tower spiral, clips on a spine

Same coil as before. Each tower is a spine with a C-ring at every clip, opening outward. Between clips the spine widens out to the tube centreline. The foot is a tab in a rounded lobe on the base ring. A plain disc threads through a straight notch at the top. The recessed numbers say which tower is which.

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

The helix starts in the lowest opening (blue, 20.5 mm) and the cut end is the centre of the top opening (yellow, 105.0 mm). The whole set is turned **6.65°** so each tab lines up with its bolt. The towers stay a quarter turn apart, so these heights did not change.

## The bar

The spine is **8 mm** thick, so the tower is one slab on the bed. At the tab, at each bore, and past the end clips it is **4.0 mm** wide radially. Between clips the outer face follows the inner arc of the bore, 0.2 mm clear of it, and runs out to the tube centreline. There the spine is **11.0 mm** wide. That is the blue line on the sketch: the rings merge into the spine instead of hanging off a 1.3 mm bite.

Each clip is still a C-ring. Bore 13.4 mm, mouth 11.5 mm on the outer side, wall 1.6 mm, 8 mm along the tube. The centres stay on the 43.5 mm helix.

The foot is the spine itself, **4.0 × 8.0 mm**, **4.0 mm** long, standing on the deck. The old hooked slot at the top is gone. A straight notch, open upward, is cut through the full 8 mm:

| | |
|---|---|
| Height | **1.4 mm** (the 1.2 mm disc plus 0.2 mm), open at the top |
| Radial width | **3.36 mm**, from local x 19.44 to 22.80 |
| Through | the full **8 mm** thickness |
| Floor under the notch | **1.6 mm** |
| Side wall, outboard | **1.6 mm** |

A 2.6 mm slot (2.4 plus 0.2) does not pass the ring. Across 8 mm of thickness the 43.5 mm arc shifts, and the ring's footprint is 3.16 mm wide. The notch is that footprint plus 0.2 mm. The 4 mm spine itself sits inboard of the ring, around radius 32 to 37 mm, so a short head on the spine top reaches out to the ring and the notch is cut in the head. The underside of the head runs up from the top clip.

## Base ring

| | |
|---|---|
| Inside radius | **50.58 mm** (Ø101.2) |
| Outside radius | **57.18 mm** (Ø114.4) |
| Band width | **6.6 mm**. The Ø3.4 hole plus 1.6 mm of wall each side |
| Band thickness | **2.4 mm** |
| Lobes | four, rounded, **4.0 mm** tall, filleted into the band with a **5 mm** radius |
| Slot | **4.2 × 8.2 mm** through the lobe. 0.2 mm over the 4.0 × 8.0 mm tab |
| Lobe tip | about **28.3 mm** from the centre |

The bolts and the old tower angles both sit on 45° / 135° / 225° / 315°, so that offset is zero. The slot is the 8 mm spine, built out from the bed face, so its middle sat **6.65°** off the bolt (4 mm of tangential offset at radius 34.5 mm). Each tower is rotated by that 6.65° and the middle of the slot now lies on the bolt ray. The hole is on the band, further out on the same ray: the slot centre is at radius **34.5 mm**, the M3 hole at **53.9 mm**. The centre of the ring is open. The cable at (+11.71, +32.24) does not meet the lobes.

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

The hole is the open centre. The disc itself did not change.

The underside is **114.7 mm** above the deck. The top of the disc is **115.9 mm**. The tower tops are **116.1 mm**, 0.2 mm above the disc. The disc drops into the straight notch.

## Print

Creality CR-10 V2, 300 × 300 × 400, PLA. Towers lie on the flat face. The C is the layer outline. The ring and the disc print flat. No supports.

The widened spine is in the layer plane, full thickness, so nothing bridges when a tower lies on its side. The original 4 mm face still comes within about **0.08 mm** of the bore at the bed face and the top face, because the helix shifts the bore across the 8 mm. It does not enter the bore. Cutting that face back would leave a sliver, so it stays. Every new face is held 0.2 mm off the bore.

| Part | On the bed (mm) |
|---|---|
| each tower | spine **4 to 11** wide, **8** thick, clips out to about **17** radially, **116.1** long |
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
3. Drop the disc through the top notches. It is a straight slot, open at the top of each spine.
4. Press the tube in from the outside, starting at the lowest opening, and wind counterclockwise. The cut end stops in the top opening on yellow.
5. Glue once the coil sits where you want it.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The seven parts, in the print pose |
| `render/assembled_iso.png` | Assembly, ring, disc, and the tube |
| `render/tower_side.png` | One tower from the side, widened spine and clips |
| `render/slot_closeup.png` | Disc threaded through the straight top notch |
| `render/print_orientation.png` | Bed poses |
| `render/base_top.png` | Ring, rounded lobes, and the bolts, looking down |
| `render/base_slot_closeup.png` | Tab seated in a lobe |
| `render/disc_top.png` | Disc, looking down |
| `render/exploded.png` | Base, towers, and disc pulled apart. Numbers beside the towers |
