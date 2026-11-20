"""Tower at 90 degrees (+Y, back). Four clips. Recessed 1. This one holds the lowest clip."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import build_tower, specs


@part
def tower_90(
    tube_od=13.0,
    coil_diameter=100.0,
    pitch=26.0,
    clip_wall=2.4,
    snap_opening=11.5,
    clip_width=10.0,
    rib_radial=6.0,
    rib_tangential=3.2,
    clip_arm=4.29,
    joint_clear=0.2,
    tube_length=965.2,
    extra_stations=0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    pitch: rise per turn (mm). The clear gap is pitch minus the tube OD.
    clip_wall: plastic outside the clip bore (mm)
    snap_opening: width of the mouth the tube pushes through (mm)
    clip_width: length of each clip along the tube (mm). The rib is thinner than this.
    rib_radial: radial depth of the running rib (mm). Raise this first if a tower flexes.
    rib_tangential: width of the running rib across the view through the coil (mm)
    clip_arm: length of the spoke along the tower at each clip (mm)
    joint_clear: extra opening on the slot, both ways (mm). 0.2 is the snug dry-fit.
    tube_length: tube length along the centreline this tower is sized for (mm)
    extra_stations: clips added past the tube, one quarter turn each
    """
    spec = specs(tube_od, coil_diameter, pitch, clip_wall, snap_opening, clip_width, rib_radial, rib_tangential, clip_arm, joint_clear, tube_length, extra_stations)
    return build_tower(spec, 1)
