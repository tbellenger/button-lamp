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

The helix starts in the lowest opening (blue, 20.5 mm) and the cut end is the centre of the top opening (yellow, 105.0 mm). Each tower stands on a bolt diagonal, 45° / 135° / 225° / 315°. The clip heights did not change.

## The bar

The spine is **5 mm** thick. At the tab, at each bore, and past the end clips it is **4.0 mm** wide radially. Between clips the outer face follows the inner arc of the bore, 0.2 mm clear of it, and runs out to the tube centreline.

Each clip is still a C-ring, **8 mm** along the tube. It shares the print-bed face with the spine and stands **3 mm** proud on the other side. Bore 13.4 mm, mouth 11.5 mm on the outer side, wall 1.6 mm. The bore centres stay on the 43.5 mm helix. Because the clip centre is 1.5 mm off the spine midplane, the bar is shifted in its plane so that offset is still on the helix. The heights did not change.

The foot is the spine itself, **4.0 × 5.0 mm** in section. The shoulder sits on the plate top, at **2.4 mm**. The tab below that is **2.4 mm** and ends flush with the underside of the plate. Its 5 mm side is square to the bolt ray. The head above the top clip is solid. An arc notch, open at the top, is cut through the **5 mm** spine. The ring is unchanged.

| | |
|---|---|
| Notch depth | **1.3 mm**. The 1.2 mm ring sits with its top flush and **0.1 mm** under it |
| Notch radii | **34.33 / 37.03 mm**. The ring plus **0.15 mm** each side |
| Radial width | **2.70 mm** |
| Inboard wall | **1.6 mm** at the bed face and the top face |
| Outboard wall | **1.6 mm** |
| Floor | the head is solid down to the top clip |

## Base ring

| | |
|---|---|
| Inside radius | **50.58 mm** (Ø101.2) |
| Outside radius | **57.18 mm** (Ø114.4) |
| Band width | **6.6 mm**. The Ø3.4 hole plus 1.6 mm of wall each side |
| Band thickness | **2.4 mm** |
| Lobes | four, rounded tip, same **2.4 mm** plate as the band. The sides flare out into the band on a **5 mm** radius, in plan only |
| Slot | **4.2 × 5.2 mm**, square to the bolt ray. 0.2 mm over the 4.0 × 5.0 mm tab |
| Slot centre | radius **34.5 mm**, on the bolt ray |
| M3 hole | radius **53.9 mm**, on the same ray |
| Lobe tip | about **28.3 mm** from the centre |

The centre is open. The cable at (+11.71, +32.24) does not meet the lobes.

## Top disc

A plain annulus. No tabs. It moved in onto the spine, so the diameters changed. The width and the thickness did not.

| | |
|---|---|
| Outside radius | **36.88 mm** (Ø73.77) |
| Hole | **34.48 mm** (Ø68.97) |
| Radial width | **2.4 mm** |
| Thickness | **1.2 mm** |
| Centreline | **35.68 mm** |

The underside is **114.8 mm** above the deck. The top of the disc is **116.0 mm**, flush with the towers.

## Print

Creality CR-10 V2, 300 × 300 × 400, PLA. Towers lie on the flat face. The C is the layer outline. The ring and the disc print flat. No supports.

The towers lie on the face the spine and the clips share. The extra 3 mm of each clip is in the build direction, grown up from that face, so it needs no chamfer and no support.

| Part | On the bed (mm) |
|---|---|
| each tower | spine **5** thick, clips **8** thick on the bed face, **17.0** from the spine back to the clip tips, **116.0** long |
| `tower_coupon` | short spine, one clip, and the tab. Print this first. |
| `tower_base` | **Ø114.4 × 2.4**, lobes included. One flat top |
| `tower_cap` | **Ø73.77 × 1.2**, hole Ø68.97 |

Stood up, the assembly is 114.4 × 114.4 × **116.0** mm. The tower tops and the disc top are the high point.

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
| `render/tab_flush.png` | Tab end flush with the underside of the plate |
| `render/disc_top.png` | Disc, looking down |
| `render/exploded.png` | Base, towers, and disc pulled apart. Numbers beside the towers |
