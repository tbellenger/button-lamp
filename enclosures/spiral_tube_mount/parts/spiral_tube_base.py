"""Base ring. Four ramped C-clips, M3 holes on the lamp's existing square, a window for the leads."""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from nurb import concave_edges, polish, part

from system import FLANGE_T, above_bed, base_clip_centered, frame_at, placement_angles, ring_and_spokes, specs


@part
def spiral_tube_base(
    tube_od=13.0,
    coil_diameter=80.0,
    gap=10.0,
    clip_wall=2.4,
    snap_opening=11.5,
    clip_width=14.0,
    draft=False,
):
    """tube_od: outside diameter of the silicone tube (mm)
    coil_diameter: outside diameter of the whole coil, tube included (mm)
    gap: clear space between turns (mm). The four clips step up by a quarter of tube_od plus this.
    clip_wall: plastic thickness of each base clip (mm)
    snap_opening: width of the upward mouth each base clip snaps the tube through (mm)
    clip_width: length of each base clip along the tube (mm)
    """
    # pad_thickness is a clip-only slider. The base only needs a legal value so the shared checks run.
    spec = specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, 5.0, clip_width)
    body = ring_and_spokes(spec)
    blank = base_clip_centered(spec)
    keeper = above_bed()
    for theta, z in placement_angles(spec):
        stood = frame_at(spec, theta, z) * blank
        body += stood & keeper
    if draft:
        return body
    # Chamfer the ring and the spokes only. A chamfer on the snap lips would
    # dull the mouth, and the tilted towers throw slivers if every edge is cut.
    keep = body.edges().filter_by(
        lambda e: e.bounding_box().min.Z > FLANGE_T - 0.3
        and e.bounding_box().max.Z < FLANGE_T + 0.3
        and e.length > 6.0
    )
    keep = keep - concave_edges(body)
    return polish(body, keep, 0.8)
