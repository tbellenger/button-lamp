"""Ring that bolts to the deck. Four keyed pins, one per tower."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import build_base, deck, specs


@part
def tower_base(
    tube_od=13.0,
    coil_diameter=100.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    clip_width=14.0,
    rib_radial=6.0,
    rib_tangential=3.2,
    clip_arm=4.0,
    strip_length=1000.0,
    extra_stations=0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm)
    clip_wall: plastic outside the clip bore (mm)
    snap_opening: width of the mouth the tube pushes through (mm)
    clip_width: length of each clip along the tube (mm). The rib is thinner.
    rib_radial: radial depth of the running rib (mm). Raise this first if a tower flexes.
    rib_tangential: width of the running rib across the view through the coil (mm)
    clip_arm: vertical size of the spoke at each clip (mm)
    strip_length: LED strip length the towers are sized for (mm)
    extra_stations: clips added past the strip, one quarter turn each
    """
    spec = specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width, rib_radial, rib_tangential, clip_arm, strip_length, extra_stations)
    return build_base(spec, deck())
