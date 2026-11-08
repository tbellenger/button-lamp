"""Centre spacer. Socket down on the printer, pin up. One pitch between seating faces."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import part

from system import spacer, specs


@part
def spiral_spacer(
    tube_od=13.0,
    coil_diameter=100.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    clip_width=14.0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm). The spacer's seating faces are one pitch apart, and the web between the crosses is pitch minus the hub height.
    clip_wall: unused on this part; kept so the stack shares one set of sliders
    snap_opening: unused on this part; kept so the stack shares one set of sliders
    clip_width: thickness of the crosses this spacer clamps (mm)
    """
    return spacer(specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width))
