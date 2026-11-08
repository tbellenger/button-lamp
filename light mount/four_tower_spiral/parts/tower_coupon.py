"""Foot, diamond gauge, and one clip. Print this before the four towers."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import build_coupon, specs


@part
def tower_coupon(
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
    clip_width: length of the clip along the tube (mm)
    strip_length: unused on the coupon; kept so the stack shares one set of sliders
    extra_stations: unused on the coupon; kept so the stack shares one set of sliders
    """
    spec = specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width, strip_length, extra_stations)
    return build_coupon(spec)
