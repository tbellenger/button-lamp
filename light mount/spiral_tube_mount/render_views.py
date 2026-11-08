"""Preview renders of the spiral tube mount: base, clip, stack, and print layout.

Run from this directory:

    xvfb-run -a python3 render_views.py
"""

import math
import os

import numpy as np
import pyvista as pv

from system import FLANGE_T, clip_centered, clipped_turn_numbers, frame_at, placement_angles, specs
from parts.spiral_tube_base import spiral_tube_base

HERE = os.path.dirname(os.path.abspath(__file__))
REND = os.path.join(HERE, "render")
os.makedirs(REND, exist_ok=True)

SPEC = specs(13.0, 80.0, 10.0, 2.4, 11.5, 5.0, 14.0)


def to_poly(shape, tol=0.12):
    verts, faces = shape.tessellate(tol)
    pts = np.array([[p.X, p.Y, p.Z] for p in verts], float)
    fac = np.asarray(faces, np.int64)
    cells = np.hstack([np.full((len(fac), 1), 3, np.int64), fac]).ravel()
    mesh = pv.PolyData(pts, cells)
    mesh.clean(inplace=True)
    return mesh


def helix_tube():
    """The silicone tube from the deck wire hole through the 1000 mm strip."""
    first = placement_angles(SPEC)[0][0]
    wire = math.atan2(SPEC.deck.wire_y, SPEC.deck.wire_x)
    end = first + SPEC.turns * 2.0 * math.pi
    theta = np.linspace(wire, end, 700)
    z = SPEC.z0 + (SPEC.pitch / (2.0 * math.pi)) * (theta - first)
    pts = np.c_[SPEC.radius * np.cos(theta), SPEC.radius * np.sin(theta), z]
    return pv.Spline(pts, len(pts)).tube(radius=SPEC.tube_od / 2.0, n_sides=24)


def placed_clips():
    blank = clip_centered(SPEC)
    meshes = []
    for theta, z_base in placement_angles(SPEC):
        for turn in clipped_turn_numbers(SPEC):
            z = z_base + (turn - 1) * SPEC.pitch
            meshes.append(to_poly(frame_at(SPEC, theta, z) * blank))
    return meshes


def add_solid(pl, mesh, color, edge):
    pl.add_mesh(mesh, color=color, specular=0.35, smooth_shading=False)
    pl.add_mesh(mesh.extract_feature_edges(35), color=edge, line_width=1.0)


def shot(pl, center, direction, distance, up=(0, 0, 1), zoom=1.15):
    d = np.array(direction, float)
    d /= np.linalg.norm(d)
    c = np.array(center, float)
    pl.camera_position = [tuple(c + d * distance), tuple(c), up]
    pl.reset_camera()
    pl.camera.zoom(zoom)
    pl.enable_anti_aliasing("ssaa")


def render_base():
    pl = pv.Plotter(off_screen=True, window_size=(1600, 1200))
    pl.set_background("#f7f9fc")
    add_solid(pl, to_poly(spiral_tube_base(draft=True)), "#3d6b99", "#183049")
    shot(pl, (0, 0, 16), (1.15, -1.35, 0.85), 280, zoom=1.2)
    pl.add_text(
        "Base  90.6 x 90.6 x 40.6 mm\n"
        "Four M3 holes on the 76.2 mm deck square. Clips step up 5.75 mm.\n"
        "Print flat on the ring, openings up. No supports.",
        font_size=11,
        color="#1a222c",
    )
    pl.screenshot(os.path.join(REND, "base_iso.png"))
    pl.close()


def render_clip_views():
    """End, side, and top of one clip, matching the three views in Tom's sketch."""
    mesh = to_poly(clip_centered(SPEC), 0.05)
    # Centred on the origin: +Y is the saddle axis, +Z runs  along the tube.
    views = [
        ((0, 0, 1), (0, 1, 0), "Along the tube: sleeve, stem, saddles"),
        ((1, 0, 0), (0, 0, 1), "Side: 14 mm band, saddle curve top and bottom"),
        ((0.85, -0.55, 0.55), (0, 0, 1), "Iso, opening facing out"),
    ]
    pl = pv.Plotter(shape=(1, 3), off_screen=True, window_size=(1800, 720))
    pl.set_background("#f7f9fc")
    for i, (direction, up, caption) in enumerate(views):
        pl.subplot(0, i)
        add_solid(pl, mesh, "#e0a045", "#6a4a16")
        shot(pl, (0, 0, 0), direction, 80, up=up, zoom=1.35)
        pl.add_text(caption, font_size=12, color="#1a222c")
    pl.screenshot(os.path.join(REND, "clip_views.png"))
    pl.close()


def render_assembly():
    pl = pv.Plotter(off_screen=True, window_size=(1700, 1300))
    pl.set_background("#f7f9fc")
    deck = pv.Box(bounds=(-60, 60, -60, 60, -3.2, 0))
    pl.add_mesh(deck, color="#e6c89a", specular=0.15, opacity=0.95)
    add_solid(pl, to_poly(spiral_tube_base(draft=True)), "#3d6b99", "#183049")
    for mesh in placed_clips():
        add_solid(pl, mesh, "#e0a045", "#6a4a16")
    pl.add_mesh(helix_tube(), color="#f4f1ea", opacity=0.55, specular=0.4)
    shot(pl, (0, 0, 60), (1.2, -1.25, 0.7), 340, zoom=1.18)
    pl.add_text(
        "1000 mm strip, 4.72 turns, pitch 23 mm\n"
        "8 identical clips on turns 2 and 4, in line with the base clips\n"
        "The turn between two clips sits in both saddles. The clips do not touch.",
        font_size=11,
        color="#1a222c",
    )
    pl.screenshot(os.path.join(REND, "assembled_iso.png"))
    pl.close()


def render_stack():
    """Look along the tube at the lowest base angle so the saddles read as arcs."""
    pl = pv.Plotter(off_screen=True, window_size=(1100, 1400))
    pl.set_background("#f7f9fc")
    add_solid(pl, to_poly(spiral_tube_base(draft=True), 0.08), "#3d6b99", "#183049")
    for mesh in placed_clips():
        add_solid(pl, mesh, "#e0a045", "#6a4a16")
    pl.add_mesh(helix_tube(), color="#ffd9a0", opacity=0.45, specular=0.3)
    theta = placement_angles(SPEC)[0][0]
    target = np.array([
        SPEC.radius * math.cos(theta),
        SPEC.radius * math.sin(theta),
        SPEC.z0 + 2.0 * SPEC.pitch,
    ])
    # Camera sits on the helix tangent so the turns stack as circles.
    tangent = np.array([-math.sin(theta), math.cos(theta), 0.0])
    shot(pl, target, tangent, 160, zoom=1.35)
    pl.add_text(
        "Along the tube at one base-clip angle\n"
        "Turns 2 and 4 carry clips. Turn 3 sits in both saddles.\n"
        "10.6 mm of air between the two clips.",
        font_size=11,
        color="#1a222c",
    )
    pl.screenshot(os.path.join(REND, "stack_section.png"))
    pl.close()


def render_print_layout():
    pl = pv.Plotter(off_screen=True, window_size=(1600, 1200))
    pl.set_background("#f7f9fc")
    bed = pv.Box(bounds=(-150, 150, -150, 150, -2, 0))
    pl.add_mesh(bed, color="#dde3ea", specular=0.1)
    pl.add_mesh(bed.extract_feature_edges(45), color="#7a8694", line_width=1.5)
    base = to_poly(spiral_tube_base(draft=True)).translate((0, 20, 0), inplace=False)
    add_solid(pl, base, "#3d6b99", "#183049")
    clip = to_poly(clip_centered(SPEC), 0.08).translate((0, 0, SPEC.clip_width / 2.0), inplace=False)
    # Eight copies, print orientation, around the base. Footprint of each is
    # about 16 x 35 mm, 14 mm tall.
    spots = [(-70, -70), (-25, -70), (25, -70), (70, -70),
             (-70, -115), (-25, -115), (25, -115), (70, -115)]
    for x, y in spots:
        add_solid(pl, clip.translate((x, y, 0), inplace=False), "#e0a045", "#6a4a16")
    shot(pl, (0, -30, 10), (0.2, -1.1, 1.05), 420, zoom=1.15)
    pl.add_text(
        "CR-10 300 x 300 mm, PLA, supports off\n"
        "Base flat on the ring. Eight clips standing on a band end (14 mm tall).",
        font_size=12,
        color="#1a222c",
    )
    pl.screenshot(os.path.join(REND, "parts_print_layout.png"))
    pl.close()


if __name__ == "__main__":
    render_base()
    render_clip_views()
    render_assembly()
    render_stack()
    render_print_layout()
    print("wrote", REND)
