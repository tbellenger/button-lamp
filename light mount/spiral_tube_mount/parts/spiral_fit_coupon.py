"""One solid to print before a full level: lap tongues, a pin, and a socket, with break-off tabs."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import fit_coupon, specs


@part
def spiral_fit_coupon(
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
    clip_wall: unused on this part; kept so the coupon matches the stack
    snap_opening: unused on this part; kept so the coupon matches the stack
    clip_width: thickness of the lap tongues (mm)
    """
    return fit_coupon(specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width))
