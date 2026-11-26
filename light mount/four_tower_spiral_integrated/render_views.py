"""Renders of the integrated four-tower spiral. Geometry only; no slicer is installed.

    xvfb-run -a python3 render_views.py
"""

from nurb import Pos

import math
import os

import numpy as np
import pyvista as pv

from system import (
    BASE_T,
    FLANGE_T,
    R_PIN,
    THETA0,
    TOWER_MARK,
    build_base,
    build_cap,
    build_tower,
    counts,
    default_specs,
    disc_world_z,
    helix_point,
    lead_in_points,
    place_tower,
    tower_angle,
)

HERE = os.path.dirname(os.path.abspath(__file__))
REND = os.path.join(HERE, "render")
os.makedirs(REND, exist_ok=True)

SPEC = default_specs()


class Holes:
    bolt = 76.2
    bolt_d = 3.4
    wire_x = 11.71
    wire_y = 32.24
    cable_d = 15.0
    window_d = 19.0


COLORS = {
    0: "#e07a3d",
    1: "#3d7ea6",
    2: "#d4a017",
    3: "#6a8f4e",
    "base": "#c4b8a5",
    "cap": "#6e6a64",
}


def to_poly(shape, tol=0.18):
    verts, faces = shape.tessellate(tol)
    pts = np.array([[p.X, p.Y, p.Z] for p in verts], float)
    fac = np.asarray(faces, np.int64)
    cells = np.hstack([np.full((len(fac), 1), 3, np.int64), fac]).ravel()
    mesh = pv.PolyData(pts, cells)
    mesh.clean(inplace=True)
    return mesh


def tube_points():
    """Straight tangent into the first hook, then the helix out to the cut end."""
    start, _bore = lead_in_points(SPEC)
    theta1 = THETA0 + SPEC.helix_length / SPEC.ds
    theta = np.linspace(THETA0, theta1, 500)
    helix = np.array([helix_point(SPEC, t) for t in theta], float)
    return np.vstack([np.array(start, float), helix])


def tube_mesh():
    pts = tube_points()
    return pv.Spline(pts, len(pts)).tube(radius=SPEC.tube_od / 2.0, n_sides=18)


def add_solid(pl, mesh, color):
    pl.add_mesh(mesh, color=color, specular=0.3, smooth_shading=False)
    pl.add_mesh(mesh.extract_feature_edges(35), color="#2a2622", line_width=1.0)


def shot(pl, center, direction, distance, up=(0, 0, 1), zoom=1.05):
    c = np.array(center, float)
    d = np.array(direction, float)
    d = d / np.linalg.norm(d)
    pl.camera.focal_point = c
    pl.camera.position = c + d * distance
    pl.camera.up = up
    pl.camera.zoom(zoom)


def save(pl, name):
    pl.render()
    path = os.path.join(REND, name)
    pl.screenshot(path)
    pl.close()
    print(path)


def placed():
    base = to_poly(build_base(SPEC, Holes))
    parts = [("base", base)]
    for index in range(4):
        ang = tower_angle(index)
        parts.append((index, to_poly(place_tower(build_tower(SPEC, index), ang))))
    cap = to_poly(Pos(0, 0, disc_world_z(SPEC)) * build_cap(SPEC))
    parts.append(("cap", cap))
    return parts


def assembled():
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1200))
    pl.set_background("white")
    for key, mesh in placed():
        add_solid(pl, mesh, COLORS["base"] if key == "base" else COLORS["cap"] if key == "cap" else COLORS[key])
    pl.add_mesh(tube_mesh(), color="#f4f1ea", opacity=0.45, smooth_shading=True)
    shot(pl, (0, 0, 62), (1.2, -1.4, 0.85), 280, zoom=1.05)
    save(pl, "assembled_iso.png")


def slot_closeup():
    """Assembled. The disc sits in the curved slot at one tower top."""
    pl = pv.Plotter(off_screen=True, window_size=(1200, 900))
    pl.set_background("white")
    ang = tower_angle(1)
    tower = to_poly(place_tower(build_tower(SPEC, 1), ang))
    disc = to_poly(Pos(0, 0, disc_world_z(SPEC)) * build_cap(SPEC))
    add_solid(pl, tower, COLORS[1])
    add_solid(pl, disc, COLORS["cap"])
    c = np.array([
        SPEC.radius * math.cos(ang),
        SPEC.radius * math.sin(ang),
        disc_world_z(SPEC) + SPEC.disc_t / 2.0,
    ])
    outward = np.array([math.cos(ang), math.sin(ang), 0.42])
    shot(pl, c, outward, 55, zoom=1.35)
    save(pl, "slot_closeup.png")


def top_view(kind):
    """Looking down. +Y, the back, is up the page."""
    pl = pv.Plotter(off_screen=True, window_size=(1100, 1100))
    pl.set_background("white")
    if kind == "base":
        mesh = to_poly(build_base(SPEC, Holes))
        color = COLORS["base"]
        name = "base_top.png"
        focus_z = 3.0
    else:
        mesh = to_poly(build_cap(SPEC))
        color = COLORS["cap"]
        name = "disc_top.png"
        focus_z = 3.0
    add_solid(pl, mesh, color)
    shot(pl, (0, 0, focus_z), (0, 0, 1), 220, up=(0, 1, 0), zoom=1.05)
    save(pl, name)


def print_layout():
    pl = pv.Plotter(off_screen=True, window_size=(1600, 1000))
    pl.set_background("white")
    base = to_poly(build_base(SPEC, Holes))
    base.translate((-170, 30, 0), inplace=True)
    cap = to_poly(build_cap(SPEC))
    cap.translate((155, 20, 0), inplace=True)
    add_solid(pl, base, COLORS["base"])
    add_solid(pl, cap, COLORS["cap"])
    # Number order, left to right: 1 blue, 2 yellow, 3 green, 4 orange.
    for index, x in zip((1, 2, 3, 0), (-50, -18, 14, 46)):
        tower = to_poly(build_tower(SPEC, index))
        tower.translate((x, 0, 0), inplace=True)
        add_solid(pl, tower, COLORS[index])
    counted = counts(SPEC)
    pl.add_text(
        "on their side, flat face down, C cut into the bar    "
        f"openings {counted[0]}+{counted[1]}+{counted[2]}+{counted[3]}",
        position="upper_edge",
        font_size=12,
        color="black",
    )
    shot(pl, (0, 40, 0), (0.05, -0.35, 1.6), 420, zoom=0.85)
    save(pl, "print_orientation.png")


def _frame(ang):
    radial = np.array([math.cos(ang), math.sin(ang), 0.0])
    tangent = np.array([math.sin(ang), -math.cos(ang), 0.0])
    return radial, tangent


def _world(x, y, z, ang):
    """Print-frame point of a tower at ang, in world millimetres."""
    radial, tangent = _frame(ang)
    return (R_PIN + x) * radial + z * tangent + np.array([0.0, 0.0, FLANGE_T + y])


def _label(pl, points, texts, size=22):
    pl.add_point_labels(
        np.asarray(points, float),
        list(texts),
        font_size=size,
        text_color="black",
        font_family="arial",
        shape_color="white",
        shape_opacity=0.9,
        always_visible=True,
        show_points=False,
    )


def _dim(pl, a, b, text):
    pl.add_mesh(pv.Line(np.asarray(a, float), np.asarray(b, float)), color="#1a1a1a", line_width=3)
    _label(pl, [(np.asarray(a) + np.asarray(b)) / 2.0], [text], size=16)


def exploded():
    """Base, towers, and disc pulled apart on the lamp axis. Numbers sit beside the towers."""
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1700))
    pl.set_background("white")
    tower_lift = 55.0
    disc_gap = 36.0
    add_solid(pl, to_poly(build_base(SPEC, Holes)), COLORS["base"])
    marks = []
    texts = []
    for index in range(4):
        ang = tower_angle(index)
        mesh = to_poly(place_tower(build_tower(SPEC, index), ang))
        mesh.translate((0, 0, tower_lift), inplace=True)
        add_solid(pl, mesh, COLORS[index])
        radial, _tangent = _frame(ang)
        marks.append(radial * 78.0 + np.array([0.0, 0.0, tower_lift + 55.0]))
        texts.append(TOWER_MARK[index])
    disc = to_poly(Pos(0, 0, disc_world_z(SPEC)) * build_cap(SPEC))
    disc_lift = tower_lift + (FLANGE_T + SPEC.cap_y - disc_world_z(SPEC)) + disc_gap
    disc.translate((0, 0, disc_lift), inplace=True)
    add_solid(pl, disc, COLORS["cap"])
    _label(pl, marks, texts, size=32)
    shot(pl, (0, 0, 100), (0.85, -0.45, 1.15), 560, zoom=0.82)
    save(pl, "exploded.png")


def foot_closeup(with_ring, name):
    """One tower foot, from outside and a little below. The notch opens outward."""
    pl = pv.Plotter(off_screen=True, window_size=(1200, 900))
    pl.set_background("white")
    ang = tower_angle(1)
    tower = to_poly(place_tower(build_tower(SPEC, 1), ang))
    add_solid(pl, tower, COLORS[1])
    if with_ring:
        add_solid(pl, to_poly(build_base(SPEC, Holes)), COLORS["base"])
    focus = _world(SPEC.x_out - 1.2, -FLANGE_T + 1.0, SPEC.clip_width / 2.0, ang)
    radial, tangent = _frame(ang)
    if with_ring:
        # Beside the foot, a little outside and below, so the ring is seen in the notch.
        shot(pl, focus, tangent * 1.15 + radial * 0.4 + np.array([0.0, 0.0, -0.55]), 36, zoom=1.9)
    else:
        shot(pl, focus, np.array([radial[0], radial[1], -1.05]), 42, zoom=1.7)
    save(pl, name)


def _half(mesh, ang):
    """Cut away one side of the bar and the far half of the ring."""
    radial, tangent = _frame(ang)
    mid = radial * R_PIN + tangent * (SPEC.clip_width / 2.0)
    mesh = mesh.clip(normal=tangent, origin=mid)
    mesh = mesh.clip(normal=-radial, origin=np.array([0.0, 0.0, 2.0]))
    return mesh


def foot_section():
    """Radial cut through tower 1's foot and the ring, with the notch sizes."""
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1000))
    pl.set_background("white")
    ang = tower_angle(1)
    radial, tangent = _frame(ang)
    add_solid(pl, _half(to_poly(place_tower(build_tower(SPEC, 1), ang)), ang), COLORS[1])
    add_solid(pl, _half(to_poly(build_base(SPEC, Holes)), ang), COLORS["base"])
    z = SPEC.clip_width / 2.0
    y0 = -FLANGE_T
    y1 = -FLANGE_T + SPEC.base_slot_depth
    x_face = SPEC.x_out
    x_notch = math.sqrt(SPEC.base_slot_r_inner ** 2 - z * z) - R_PIN
    x_ring = math.sqrt(SPEC.base_r_in ** 2 - z * z) - R_PIN
    face_r = math.hypot(R_PIN + x_face, z)
    engage = face_r - SPEC.base_r_in
    depth = face_r - SPEC.base_slot_r_inner
    _dim(
        pl,
        _world(x_notch - 2.2, y0, z, ang),
        _world(x_notch - 2.2, y1, z, ang),
        f"{SPEC.base_slot_depth:.1f} mm high",
    )
    _dim(
        pl,
        _world(x_notch, y1 + 2.2, z, ang),
        _world(x_face, y1 + 2.2, z, ang),
        f"notch {depth:.1f} mm deep",
    )
    _dim(
        pl,
        _world(x_ring, y0 - 2.4, z, ang),
        _world(x_face, y0 - 2.4, z, ang),
        f"engages {engage:.1f} mm",
    )
    x_ring_mid = (x_face + (math.sqrt(SPEC.base_r_out ** 2 - z * z) - R_PIN)) / 2.0
    _dim(
        pl,
        _world(x_ring_mid, y0, z, ang),
        _world(x_ring_mid, y0 + BASE_T, z, ang),
        f"ring {BASE_T:.1f} mm",
    )
    _label(pl, [_world(x_face + 3.5, (y0 + y1) / 2.0, z, ang)], ["opens outward"], size=16)
    focus = _world((x_notch + x_face) / 2.0, y0 + 1.3, z, ang)
    pl.enable_parallel_projection()
    shot(pl, focus, -tangent, 40, up=(0, 0, 1), zoom=1.0)
    pl.camera.parallel_scale = 14.0
    save(pl, "foot_section.png")


if __name__ == "__main__":
    assembled()
    slot_closeup()
    print_layout()
    top_view("base")
    top_view("cap")
    exploded()
    foot_closeup(False, "foot_notch.png")
    foot_closeup(True, "foot_seated.png")
    foot_section()
