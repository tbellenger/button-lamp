"""Top cap. Clamps the last level onto the pin. No pin above it."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import cap, specs


@part
def spiral_cap(
    tube_od=13.0,
    coil_diameter=100.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    clip_width=14.0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm)
    clip_wall: unused on this part; kept so the stack shares one set of sliders
    snap_opening: unused on this part; kept so the stack shares one set of sliders
    clip_width: thickness of the crosses this cap clamps (mm)
    """
    return cap(specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width))
