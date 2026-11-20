"""Ring, four towers, and the cap, sized to the tube length unless extra_stations says otherwise."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import Pos, assembly, use

from system import FLANGE_T, place_tower, specs, tower_angle


@assembly
def tower_assembly(
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
    clip_width: length of each clip along the tube (mm)
    rib_radial: radial depth of the running rib (mm)
    rib_tangential: width of the running rib across the view through the coil (mm)
    clip_arm: length of the spoke along the tower at each clip (mm)
    joint_clear: extra opening on the slots and the cap pockets (mm). 0.2 is the snug dry-fit.
    tube_length: tube length along the centreline the towers are sized for (mm)
    extra_stations: clips added past the tube, one quarter turn each
    """
    spec = specs(
        float(tube_od), float(coil_diameter), float(pitch), float(clip_wall),
        float(snap_opening), float(clip_width), float(rib_radial), float(rib_tangential),
        float(clip_arm), float(joint_clear), float(tube_length), int(extra_stations),
    )
    common = dict(
        tube_od=float(tube_od),
        coil_diameter=float(coil_diameter),
        pitch=float(pitch),
        clip_wall=float(clip_wall),
        snap_opening=float(snap_opening),
        clip_width=float(clip_width),
        rib_radial=float(rib_radial),
        rib_tangential=float(rib_tangential),
        clip_arm=float(clip_arm),
        joint_clear=float(joint_clear),
        tube_length=float(tube_length),
        extra_stations=int(extra_stations),
    )
    parts = [use("tower_base", **common)]
    for index, name in enumerate(("0", "90", "180", "270")):
        parts.append(place_tower(use(f"tower_{name}", **common), tower_angle(index)))
    parts.append(Pos(0, 0, FLANGE_T + spec.cap_y) * use("tower_cap", **common))
    return tuple(parts)
