# Original one-piece 13 mm cage

This is the **older printed cage**: one part, four columns, clips up the helix, flange bolted to the plywood top. It is not the stackable cross mount in [`../spiral_tube_mount/`](../spiral_tube_mount/).

The shape is unchanged. These files were moved here so the cage is easy to find. The generator is still [`container/plywood/src/spiral.py`](../../container/plywood/src/spiral.py), which also makes `top_backer_plate.stl` and the bottom blocks. Those stay with the box, under [`container/plywood/stl/`](../../container/plywood/stl/). `src/build_all.py` writes the cage, the end cap, the clip fit test, and `spiral_holder_13mm.png` into this folder.

| File | What |
|---|---|
| `spiral_cage_tube13mm.stl` | The cage. Flange on the bed. No supports. |
| `end_cap_tube13mm.stl` | Push-on cap. Open end down. |
| `clip_fit_test_tube13mm.stl` | One column, two clips. Print this before the full cage. |
| `spiral_holder_13mm.png` | Render of the cage and the tube. |

Print notes are in [`container/plywood/PLAN.md`](../../container/plywood/PLAN.md) (the cage rows of the print table).
