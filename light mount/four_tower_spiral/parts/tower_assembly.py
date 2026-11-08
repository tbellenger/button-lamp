"""Ring, four towers, and the cap, for a 1 m strip unless extra_stations says otherwise."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import Pos, assembly, use

from system import FLANGE_T, place_tower, specs, tower_angle


@assembly
def tower_assembly(
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
    clip_width: length of each clip along the tube (mm)
    strip_length: LED strip length the towers are sized for (mm)
    extra_stations: clips added past the strip, one quarter turn each
    """
    spec = specs(
        float(tube_od), float(coil_diameter), float(gap), float(clip_wall),
        float(snap_opening), float(clip_width), float(strip_length), int(extra_stations),
    )
    common = dict(
        tube_od=float(tube_od),
        coil_diameter=float(coil_diameter),
        gap=float(gap),
        clip_wall=float(clip_wall),
        snap_opening=float(snap_opening),
        clip_width=float(clip_width),
        strip_length=float(strip_length),
        extra_stations=int(extra_stations),
    )
    parts = [use("tower_base", **common)]
    for index, name in enumerate(("0", "90", "180", "270")):
        parts.append(place_tower(use(f"tower_{name}", **common), tower_angle(index)))
    parts.append(Pos(0, 0, FLANGE_T + spec.cap_y) * use("tower_cap", **common))
    return tuple(parts)
