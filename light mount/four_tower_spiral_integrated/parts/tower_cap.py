"""Plain disc that drops into the four tower tops."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import build_cap, specs


@part
def tower_cap(
    tube_od=13.0,
    coil_diameter=100.0,
    pitch=26.0,
    clip_wall=1.6,
    snap_opening=11.5,
    clip_width=8.0,
    joint_clear=0.2,
    tube_length=965.2,
    extra_stations=0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    pitch: rise per turn (mm). The clear gap is pitch minus the tube OD.
    clip_wall: plastic outside the hook bore (mm)
    snap_opening: width of the mouth the tube pushes through (mm)
    clip_width: length of each hook along the tube (mm)
    joint_clear: extra opening on the slot, both ways (mm). 0.2 is the snug dry-fit.
    tube_length: cut length of the tube, straight entry included (mm)
    extra_stations: hooks added past the tube, one quarter turn each
    """
    spec = specs(tube_od, coil_diameter, pitch, clip_wall, snap_opening, clip_width, joint_clear, tube_length, extra_stations)
    return build_cap(spec)
