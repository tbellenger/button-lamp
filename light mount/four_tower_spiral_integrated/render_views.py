"""Renders of the integrated four-tower spiral. Geometry only; no slicer is installed.

    xvfb-run -a python3 render_views.py
"""

import math
import os

import numpy as np
import pyvista as pv

from system import (
    FLANGE_T,
    R_PIN,
    THETA0,
    build_base,
    build_cap,
    build_tower,
    clip_y,
    counts,
    default_specs,
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
    cap = to_poly(build_cap(SPEC))
    cap.translate((0, 0, FLANGE_T + SPEC.cap_y), inplace=True)
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


def hook_closeup():
    """Print pose. The C is cut into the bar and opens away from the centre."""
    pl = pv.Plotter(off_screen=True, window_size=(1200, 900))
    pl.set_background("white")
    tower = to_poly(build_tower(SPEC, 1))
    add_solid(pl, tower, COLORS[1])
    y = clip_y(SPEC, 0)
    bed = pv.Plane(center=(SPEC.tube_x, y, 0), direction=(0, 0, 1), i_size=36, j_size=32)
    pl.add_mesh(bed, color="#e7e2d8", opacity=0.35)
    # From outside the coil, so the mouth is the thing in frame.
    shot(pl, (SPEC.tube_x + 2.0, y, 4.5), (1.15, -0.35, 0.42), 42, zoom=1.15)
    save(pl, "hook_closeup.png")


def top_view(kind):
    """Looking down. +Y, the back, is up the page."""
    pl = pv.Plotter(off_screen=True, window_size=(1100, 1100))
    pl.set_background("white")
    if kind == "base":
        mesh = to_poly(build_base(SPEC, Holes))
        color = COLORS["base"]
        name = "base_top.png"
        focus_z = 2.0
    else:
        mesh = to_poly(build_cap(SPEC))
        color = COLORS["cap"]
        name = "cap_top.png"
        focus_z = 4.0
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


if __name__ == "__main__":
    assembled()
    hook_closeup()
    print_layout()
    top_view("base")
    top_view("cap")
