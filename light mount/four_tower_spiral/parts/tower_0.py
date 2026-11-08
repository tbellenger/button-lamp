"""Tower at 0 degrees (+X, right). Three clips. Recessed 4. Not the start of the coil."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import build_tower, specs


@part
def tower_0(
    tube_od=13.0,
    coil_diameter=100.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    clip_width=14.0,
    rib_radial=6.0,
    rib_tangential=3.2,
    clip_arm=6.0,
    arm_bevel=3.0,
    joint_clear=0.2,
    strip_length=1000.0,
    extra_stations=0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm)
    clip_wall: plastic outside the clip bore (mm)
    snap_opening: width of the mouth the tube pushes through (mm)
    clip_width: length of each clip along the tube (mm). The rib is thinner than this.
    rib_radial: radial depth of the running rib (mm). Raise this first if a tower flexes.
    rib_tangential: width of the running rib across the view through the coil (mm)
    clip_arm: height of the spoke along the tower (mm). At least the tower's radial width.
    arm_bevel: 45 degree gusset at the tower, leg length both ways (mm)
    joint_clear: extra opening on the slot, both ways (mm). 0.2 is the snug dry-fit.
    strip_length: LED strip length this tower is sized for (mm)
    extra_stations: clips added past the strip, one quarter turn each
    """
    spec = specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width, rib_radial, rib_tangential, clip_arm, arm_bevel, joint_clear, strip_length, extra_stations)
    return build_tower(spec, 0)
