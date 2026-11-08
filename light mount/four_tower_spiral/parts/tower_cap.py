"""Ring that drops onto the four tower tops and holds their radius."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import build_cap, specs


@part
def tower_cap(
    tube_od=13.0,
    coil_diameter=100.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    clip_width=14.0,
    strip_length=1000.0,
    extra_stations=0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm)
    clip_wall: plastic outside the clip bore (mm)
    snap_opening: width of the mouth the tube pushes through (mm)
    clip_width: tower thickness, and the width the cap pocket clears (mm)
    strip_length: LED strip length the towers are sized for (mm)
    extra_stations: clips added past the strip, one quarter turn each
    """
    spec = specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width, strip_length, extra_stations)
    return build_cap(spec)
