# Four-tower spiral, hooks in the towers

The same ring and cap as [`../four_tower_spiral/`](../four_tower_spiral/), with the hooks printed as part of each tower. There is no separate clip and no clip-to-arm joint. That folder is unchanged. This one is the set to reprint the towers from.

The towers lie on their side on the bed. The flat face is one plane. The C of each hook is the layer outline, so the snap bends in the layers, not across them. A 3 mm fillet on each side of the neck is the gusset where the hook leaves the rib.

## What was wrong with the count

The earlier generator treated all 965.2 mm as helix. One turn at the 43.5 mm centreline is

`sqrt((2 π R)² + pitch²) = sqrt((2 π × 43.5)² + 26²) = 274.55 mm`.

965.2 / 274.55 = **3.516 turns**. It placed a hook every quarter turn (68.64 mm along the tube, 6.5 mm of rise) and forced the cut end onto the last hook by parking the 4.27 mm remainder as helix *before* the first hook. That is 15 hooks: 4 + 4 + 4 + 3. Coil rise 91.4 mm. Tower on the bed 132.6 mm.

The tube does not do that. It enters from the side on a straight tangent. From the first hook, back along that tangent, to the outer edge of the ring (radius 64 mm) is

`sqrt(64² − 43.5²) = 46.94 mm`.

That length is part of the 38 inches and it is not helix. The coil is the rest.

| | Earlier set | This set |
|---|---|---|
| Straight entry | none (a 4.27 mm leftover was still called helix) | **46.94 mm**, tangent, ring edge to the first hook |
| Helix length | 965.2 mm | **918.26 mm** |
| One turn | 274.55 mm | 274.55 mm |
| Turns | 3.516 | **3.345** |
| Hooks | 15 (4 + 4 + 4 + 3) | **14 (4 + 4 + 3 + 3)** |
| Rise, first hook to cut end | 91.4 mm | **87.0 mm** |
| Rise, first hook to last hook | 91.0 mm | **84.5 mm** |
| Tower on the bed | 132.6 mm | **126.1 mm** |
| Assembly height | 132.6 mm | **126.1 mm** |

The hook that dropped is the old top one on green, tower 3, which the earlier notes called the cut end. It sat 42.7 mm past the end of the tube. The tube now ends on yellow, tower 2: 26.0 mm past that hook's centre, 21 mm past the 10 mm grip, and short of the next tower.

There is no straight tail past the cut end. The earlier model did not have one either.

A second miss, separate from the count: the earlier helix was drawn on each tower's radial plane, and the bore is centred half a clip off that plane because the clip sits on the bed. Every bore missed the tube centre by **5.0 mm**. These bores are on the helix. The arm is pulled in to 21.21 mm so the 5 mm offset still lands on the 43.5 mm centreline. The phase shift is −6.60°.

## Where the hooks sit

Same rotation as the earlier set. Yellow 2 and green 3 are the front corners. +Y is the back. −Y is the buttons. World Z is the hook centre above the deck. The ring top is at 4.5 mm. The first centre is 16 mm above that.

| Number | Colour | File | Place | Hooks | Stations | World Z (mm) |
|---|---|---|---|---|---|---|
| 1 | blue | `tower_90` | back-left, 135° | 4, including the lowest | 0, 4, 8, 12 | 20.5, 46.5, 72.5, 98.5 |
| 2 | yellow | `tower_180` | front-left, 225° | 4, including the highest | 1, 5, 9, 13 | 27.0, 53.0, 79.0, 105.0 |
| 3 | green | `tower_270` | front-right, 315° | 3 | 2, 6, 10 | 33.5, 59.5, 85.5 |
| 4 | orange | `tower_0` | back-right, 45° | 3 | 3, 7, 11 | 40.0, 66.0, 92.0 |

Pitch is 26 mm, so hooks on one tower are 26 mm apart and the neighbour is 6.5 mm higher. That did not change.

## The hook

Same grip as the earlier clip. Taken from that code, not retuned.

| | |
|---|---|
| Tube | 13 mm OD |
| Bore | 13.4 mm (0.2 mm clearance on the diameter) |
| Wall | 2.4 mm |
| Mouth | 11.5 mm |
| Along the tube | 10 mm |
| Neck | 6 mm across, was a 4.29 mm spoke |
| Root | 3 mm fillet each side of the neck, in the layer plane |

The fillet is the gusset. It adds material where the neck leaves the rib. It does not stick past the rib's top, and it does not hang below the bed.

## Base and cap

They did not have to change. The foot is still the 6.0 × 3.2 mm rib, 4.5 mm through the ring. The slot and the cap pocket are still 6.2 × 3.4 mm, `joint_clear` 0.2 mm on both axes. The hoop is still 54.8 mm across and 8 mm tall.

The files in this folder are that same ring and that same cap, so the set is complete. A ring and a cap already printed from `four_tower_spiral` fit these towers. The cap sits 6.5 mm lower because one hook of rise is gone. The pocket is the same pocket.

## Print

Creality CR-10, PLA, 0.4 mm nozzle. Tom slices in Cura. No slicer is installed here. `nurb slice` says so, and the 3MF files are geometry only, already lying in the print pose. Flat face on the bed. Nothing below z = 0.

| Part | Qty | Size (mm) | On the bed |
|---|---|---|---|
| `tower_base` | 1 | 128 × 128 × 4.5 | Ring down. Reuse the earlier one if you have it. |
| `tower_90` | 1 | 31.6 × 126.1 × 10 | On its side. Number 1. Four hooks. |
| `tower_180` | 1 | 31.6 × 126.1 × 10 | On its side. Number 2. Four hooks, the top of the coil. |
| `tower_270` | 1 | 31.6 × 126.1 × 10 | On its side. Number 3. Three hooks. |
| `tower_0` | 1 | 31.6 × 126.1 × 10 | On its side. Number 4. Three hooks. |
| `tower_cap` | 1 | 54.8 × 54.8 × 8 | Ring down. Reuse the earlier one if you have it. |
| `tower_coupon` | 1 | 58.6 × 35.6 × 10 | On its side. Print this first. |

Stood up, the assembly is 128 × 128 × 126.1 mm. The long dimension of a tower on the bed is 126.1 mm. Both fit the CR-10 (300 × 300 × 400).

`nurb check` reports nothing to fix. It does warn. The curved bore meets the bed at about 5–8°, which polishes the first layer, same as the earlier clip. The recessed digits leave a thin stroke. The fillet is faceted, and the checker calls those facets a cosmetic strip. None of that is a support.

## Reprint

Reprint the coupon, then the four towers. Leave the earlier ring and cap on the lamp if they are already printed. Do not reprint the towers from `four_tower_spiral`: they are one hook too tall, and their bores are 5 mm off this tube.

## Assembly

1. Bolt the ring down. +Y, the cable window, points at the back. Number 1 is back-left. Then 2, 3, and 4 going counterclockwise as you look down.
2. Drop each tower into the slot with the same number. The lowest hook on number 1 faces out, toward the back-left.
3. Dry-fit the cap.
4. Bring the tube in from the side, along the tangent, into the lowest hook, and wind counterclockwise and up. The cut end stops a little past the top hook on yellow.
5. Glue the feet and the cap once the coil sits where you want it.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The seven parts, in the print pose |
| `render/assembled_iso.png` | Assembly and the tube, straight entry included |
| `render/hook_closeup.png` | One hook on the bed, fillet at the rib |
| `render/print_orientation.png` | Bed poses |
| `system.py` | Shared geometry |
