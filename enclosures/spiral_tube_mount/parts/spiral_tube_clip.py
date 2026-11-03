"""Standalone saddle clip. Print the end on the bed; every wall is vertical."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import Pos, concave_edges, polish, part

from system import clip_centered, specs


@part
def spiral_tube_clip(
    tube_od=13.0,
    coil_diameter=80.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    pad_thickness=5.0,
    clip_width=14.0,
    draft=False,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm). Pitch is tube_od plus this.
    clip_wall: plastic thickness of the sleeve around the tube (mm)
    snap_opening: width of the mouth the tube pushes through (mm). Narrower than the tube.
    pad_thickness: radial thickness of the vertical stem between the saddles (mm)
    clip_width: length of the clip along the tube (mm)
    """
    spec = specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, pad_thickness, clip_width)
    # Sleeve centre sits at z = clip_width/2 so the band end is the first layer.
    body = Pos(0, 0, clip_width / 2.0) * clip_centered(spec)
    if draft:
        return body
    keep = body.edges().filter_by(lambda e: e.bounding_box().min.Z > 0.2)
    keep = keep - concave_edges(body)
    return polish(body, keep, 0.6)
