# Four-tower spiral

A separate way to hold the 13 mm silicone tube. It does not replace the cross mount in [`../spiral_tube_mount/`](../spiral_tube_mount/).

The base is only a ring: the existing M3 bolts, a clear path for the light cable, and four slots. Four towers stand at 0°, 90°, 180° and 270°. Clips on the towers follow one helix. A cap drops onto the four tops and holds them on their circle.

The four feet are the same shape. The towers differ only in clip height and in a recessed number. Tom dry-fits them, then glues. The numbers are the placement guide.

## How thin the rib is

The first version of this mount was a slab: **26 mm radial by 14 mm tangential**, full height. That blocked the view through the coil. The running member is a rib **6.0 mm radial by 3.2 mm tangential**. Each clip and the spoke into that clip are local. The cap is a hoop about 11 mm wide instead of a 22 mm band.

nurb has no stress solver, and there is no measured uncoil force for this tube. The 6.0 × 3.2 mm section is a printable default, not a calculated minimum.

What sets it:

- **3.2 mm tangential** is the width you see edge-on through the coil. It is eight perimeters on a 0.4 mm nozzle, solid, on the 3 mm wall floor. Thinner than 2.4 mm is a ribbon that splits when the tube is pushed into the clip.
- **6.0 mm radial** is the depth that takes the uncoil bend. That bend is in the layer plane, because the tower prints on its side. Six millimetres is enough to read as a beam over the span from the ring to the cap, and still much smaller than the 10 mm gap between turns. It is not derived from a tube force.
- The **foot is the same rib**, carried 4.5 mm down through the ring. There is no pad under it.
- Each **clip stays 14 mm** along the tube. The spoke that joins it to the rib is 6 mm along the tower, the same as the rib's radial width, with a 45° gusset at the tower.

If a trial print flexes, raise `rib_radial` first (6, then 8). That adds depth where the coil pushes out, and it barely changes the view. `clip_arm` will not go under that radial width, so raise it with the rib. Raise `rib_tangential` only if the rib twists. The slot and the cap pocket follow the rib, plus `joint_clear`.

## The spoke

In the tower's print frame, +X is radial (out toward the tube), +Y is up the tower, and +Z is print-up. Stood on the ring, +Z is the tangential direction.

| | Was | Is |
|---|---|---|
| Spoke height, along the tower (+Y) | 4.0 mm | 6.0 mm, the rib's full radial width |
| Where it meets the rib (+X) | a 1.5 mm bite into the outer face, from X = 1.5 to 3.0 | the whole rib, X = −3.0 to +3.0 |
| Corner at the tower | square | 45° gusset, 3.0 mm leg. Material added in the two elbows |
| Far end of the spoke | X = 14.4, 0.4 mm short of the bore | unchanged |
| Tube centre | X = 21.5 | unchanged |
| Clip bore and mouth | 13 mm tube, 0.2 mm bore clearance, 11.5 mm mouth | unchanged |

The gusset is in the layer plane, so it is a vertical wall while the tower prints on its side. It does not need support. The rib everywhere else stays 6.0 × 3.2 mm. This is not a measured strength change.

## Why the towers print on their side

Clips printed upright, with the layer lines running along the tube, broke across those layers. A clip printed on its side, so the C is the layer plane, holds. Each tower is that second pose for every clip on it. The snap arms are loaded in the layers, not across them. Infill does not fix the upright version.

## Coil

| | |
|---|---|
| Tube | 13 mm OD |
| Centre-line radius | 43.5 mm. Outside of the coil is 100 mm. |
| Pitch | 23 mm. Quarter turn is 5.75 mm of rise. |
| Towers | 0° (+X, right), 90° (+Y, back), 180° (−X), 270° (−Y, front) |
| Where the helix starts | **Number 1**, the 90° tower. Lowest clip. |
| Lead-in | The cable hole is at (+11.71, +32.24), about 70° from +X. The first clip is at 90°, so the tube climbs about 20° from the hole to that clip, counterclockwise. |

A 1 m strip is 15 clip stations (k = 0…14). Station 15 is past the strip.

The number is helix order, counterclockwise and up. The same digit is recessed into the tower and into the ring beside that tower's slot.

| Number | Tower | Place | Clips on a 1 m strip | Stations |
|---|---|---|---|---|
| 1 | `tower_90` | 90°, back, +Y | 4, including the lowest | 0, 4, 8, 12 |
| 2 | `tower_180` | 180°, −X | 4 | 1, 5, 9, 13 |
| 3 | `tower_270` | 270°, front, −Y | 4, including the highest | 2, 6, 10, 14 |
| 4 | `tower_0` | 0°, right, +X | 3 | 3, 7, 11 |

`strip_length` and `extra_stations` change the count and the tower height together. The four towers, the cap, and the ring have to use the same values.

## The joint

Every tower uses the same slot. The rib is the tab. Nothing else is inside the slot.

| | Radial | Tangential | Length |
|---|---|---|---|
| Tab (the rib) | 6.0 mm | 3.2 mm | 4.5 mm through the ring, flush with the underside |
| Ring slot | 6.2 mm | 3.4 mm | through the 4.5 mm ring |
| Cap tenon | 6.0 mm | 3.2 mm | 8 mm, the rib continued |
| Cap pocket | 6.2 mm | 3.4 mm | through the 8 mm hoop |

`joint_clear` is 0.2 mm, added on both the radial and the tangential opening. That is the snug row of the fit table. It is a design allowance, not a measured fit. The slider refuses anything under 0.2 mm, because that would be a press, and it allows up to 1.0 mm if a trial is tight. The bore clearance on the tube is a separate 0.2 mm (`BORE_CLEAR`) and is unchanged.

The 4.5 mm of rib inside the ring is the glue length. If the glue wets all four faces, that is about 83 mm² of side contact. That is a practical prototype joint, not a tested strength.

The slots are identical, so a tower will also drop into the wrong slot. The recessed numbers are what keep the helix in order. Match 1 to 1, then 2, 3, and 4, dry-fit the cap, then glue.

The digit is DejaVu Sans, 5 mm font, recessed 0.6 mm. Each glyph is about 2.2–2.7 mm wide and 3.7 mm tall, so it sits inside the 6 mm rib and in the top of the 4.5 mm ring. On the tower it is on the top face of the print, 8 mm above the ring, so the pocket is open upward. On the ring it is just outboard of the slot, also open upward. Both read upright: the tower digit reads up the tower once it is stood, and the ring digits all read the same way when you look down on the deck.

## Print

Creality CR-10, PLA, 0.4 mm nozzle. Tom slices in Cura. No slicer is installed here. `nurb slice` says so, and the 3MF files are geometry only. These checks are the solids, not a sliced toolpath.

| Part | Qty | Size (mm) | On the bed |
|---|---|---|---|
| `tower_base` | 1 | 128 × 128 × 4.5 | Ring down. Slots are vertical holes. |
| `tower_90` | 1 | 32 × 122 × 14 overall | On its side. Number 1. Start of the coil. |
| `tower_180` | 1 | 32 × 122 × 14 overall | On its side. Number 2. |
| `tower_270` | 1 | 32 × 122 × 14 overall | On its side. Number 3. |
| `tower_0` | 1 | 32 × 122 × 14 overall | On its side. Number 4. Three clips. |
| `tower_cap` | 1 | 55 × 55 × 8 | Ring down. Hoop about 11 mm wide. |
| `tower_coupon` | 1 | 59 × 36 × 14 | On its side. Print this first. |

The rib is 6.0 × 3.2 and sits on the bed. The top of each tower is the same rib continued, full thickness, so the tenon sits on the bed too. The slot in the ring and the pocket in the cap are vertical through-holes. The digit pockets open on the top face. `nurb check` reports nothing to fix. It does warn: the curved bore meets the bed at 5–8°, which polishes the first layer, and the engraved digits leave small faces and a 0.36 mm section at the glyph stroke. That stroke is the number, not the rib.

## Fits — not measured

| Fit | Allowance |
|---|---|
| Rib into the ring slot | 0.2 mm extra on the 6.0 mm radial and on the 3.2 mm tangential. Opening 6.2 × 3.4 mm. |
| Rib into the cap pocket | The same 0.2 mm, both ways. |
| Coupon gauge | The same opening: a 6.2 mm gap between cheeks that are 3.4 mm tall, open on top. |

Nothing here has been printed. Print the coupon, snap the link, and set the tab into the gauge. If it binds, raise `joint_clear` and reprint the coupon before the four towers.

## Assembly

1. Bolt the ring down. Number 1 is at the back, next to the cable window. Then 2, 3, and 4 going counterclockwise as you look down.
2. Drop each tower into the slot with the same number. The lowest clip on number 1 faces out, toward the back of the lamp.
3. Dry-fit the cap. It is inboard of the tube.
4. Either snap the tube in from the outside, starting at the lowest clip and winding counterclockwise, or snap the tube first and drop the cap on from above. The cap does not have to pass the tube.
5. Glue the feet and the cap once the coil sits where you want it.

The clips stay reachable either way. The cap is what keeps the tops from spreading while the glue sets. There is no pin, tooth, or lug to stop a tower going in the wrong slot or turned end for end. The numbers do that job, and the glue holds it after the trial fit.

## Already-printed towers

The ring and the cap are the same parts as the slotted mount. Reprint the four towers and the coupon. The spoke is the only change, so an older tower will still drop into the slot, but its clip arm is the narrow one. The cross mount is a different part.

## Cable

The ring leaves a Ø19 window at (+11.71, +32.24). The deck and the backer are Ø15. The nearest slot is number 1, and it clears that window. USB, the DC jack, and the button holes are not part of this mount.

## Files

| File | |
|---|---|
| `stl/`, `3mf/` | The seven parts |
| `render/four_tower_front.png` | Through the front, far coil visible |
| `render/four_tower_side.png` | Through the side |
| `render/exploded.png` | Ring, towers, cap |
| `render/four_tower_joint.png` | Number 1's tab over its slot |
| `render/four_tower_arm_change.png` | Spoke before and after, same view |
| `render/four_tower_arm_before.png` | The 4 mm spoke |
| `render/four_tower_arm_after.png` | The 6 mm spoke and the gusset |
| `render/clip_closeup.png` | Lowest clip, in the print pose |
| `render/print_orientation.png` | Bed poses, with the numbers |
