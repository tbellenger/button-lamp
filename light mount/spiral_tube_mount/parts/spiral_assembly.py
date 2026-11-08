"""Base, the cross levels a 1 m strip needs, the spacers between them, and the cap."""

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import Pos, Rot, assembly, measured, use

from system import HUB_H, HUB_Z0, deck, levels_for_strip, lowest_angle, specs


@assembly
def spiral_assembly(
    tube_od=13.0,
    coil_diameter=100.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    clip_width=14.0,
    extra_levels=0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm)
    clip_wall: plastic outside the bore (mm)
    snap_opening: width of the mouth the tube pushes through (mm)
    clip_width: length of each clip along the tube (mm)
    extra_levels: whole levels added above the count for a 1 m strip
    """
    spec = specs(
        float(tube_od), float(coil_diameter), float(gap), float(clip_wall),
        float(snap_opening), float(clip_width),
    )
    holes = deck()
    count, _stations = levels_for_strip(measured("strip_length"), spec.radius, spec.pitch)
    count += int(extra_levels)
    theta = lowest_angle(holes.wire_x, holes.wire_y)
    common = dict(
        tube_od=float(tube_od),
        coil_diameter=float(coil_diameter),
        gap=float(gap),
        clip_wall=float(clip_wall),
        snap_opening=float(snap_opening),
        clip_width=float(clip_width),
    )
    parts = [use("spiral_base", **common)]
    spin = Rot(0, 0, math.degrees(theta))
    for i in range(count):
        z = HUB_Z0 + i * spec.pitch
        parts.append(place(use("spiral_cross_low", **common), theta, z))
        parts.append(place(use("spiral_cross_high", **common), theta + math.pi / 2.0, z))
        seat = z + HUB_H / 2.0
        if i < count - 1:
            parts.append(Pos(0, 0, seat) * spin * use("spiral_spacer", **common))
        else:
            parts.append(Pos(0, 0, seat) * spin * use("spiral_cap", **common))
    return tuple(parts)


def place(solid, theta, z_hub):
    from system import place_cross
    return place_cross(solid, theta, z_hub)
