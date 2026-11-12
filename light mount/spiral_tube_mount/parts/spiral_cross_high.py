"""Upper half of one level. Turned 90 degrees from the low piece. Its own tooth clocks it."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import cross_high, specs


@part
def spiral_cross_high(
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
    clip_wall: plastic outside the bore (mm)
    snap_opening: width of the mouth the tube pushes through (mm)
    clip_width: length of each clip along the tube, and the thickness of this piece (mm)
    """
    return cross_high(specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width))
