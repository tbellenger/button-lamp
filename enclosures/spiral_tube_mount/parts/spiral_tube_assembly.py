"""Base plus the saddle clips that belong on a 1000 mm strip, lined up with the base clips."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import assembly, use

from system import clipped_turn_numbers, frame_at, placement_angles, print_location, specs


@assembly
def spiral_tube_assembly(
    tube_od=13.0,
    coil_diameter=80.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    pad_thickness=5.0,
    clip_width=14.0,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm)
    clip_wall: plastic thickness of the sleeves (mm)
    snap_opening: width of the mouth the tube pushes through (mm)
    pad_thickness: radial thickness of the stem between the saddles (mm)
    clip_width: length of each clip along the tube (mm)
    """
    spec = specs(
        float(tube_od), float(coil_diameter), float(gap), float(clip_wall),
        float(snap_opening), float(pad_thickness), float(clip_width),
    )
    base = use(
        "spiral_tube_base",
        tube_od=float(tube_od),
        coil_diameter=float(coil_diameter),
        gap=float(gap),
        clip_wall=float(clip_wall),
        snap_opening=float(snap_opening),
        clip_width=float(clip_width),
    )
    parts = [base]
    for theta, z_base in placement_angles(spec):
        for turn in clipped_turn_numbers(spec):
            z = z_base + (turn - 1) * spec.pitch
            printed = use(
                "spiral_tube_clip",
                tube_od=float(tube_od),
                coil_diameter=float(coil_diameter),
                gap=float(gap),
                clip_wall=float(clip_wall),
                snap_opening=float(snap_opening),
                pad_thickness=float(pad_thickness),
                clip_width=float(clip_width),
            )
            # The STL stands on a helical end. Put that pose back on the coil.
            local = print_location(spec).inverse() * printed
            parts.append(frame_at(spec, theta, z) * local)
    return tuple(parts)
