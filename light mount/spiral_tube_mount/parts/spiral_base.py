"""Plate that bolts to the top deck. First pin, four M3 holes, light-cable window."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import base_plate, deck, specs


@part
def spiral_base(
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
    clip_width: thickness of the crosses this pin passes through (mm)
    """
    spec = specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width)
    return base_plate(spec, deck())
