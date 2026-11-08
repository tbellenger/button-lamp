"""Renders of the four-tower spiral. Geometry only; no slicer is installed.

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
    TOWER_NAMES,
    build_base,
    build_cap,
    build_tower,
    clip_y,
    place_tower,
    specs,
    tower_angle,
)

HERE = os.path.dirname(os.path.abspath(__file__))
REND = os.path.join(HERE, "render")
os.makedirs(REND, exist_ok=True)

SPEC = specs(13.0, 100.0, 10.0, 2.4, 11.5, 14.0, 6.0, 3.2, 6.0, 3.0, 0.2, 1000.0, 0)


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


def tube_mesh():
    climb = SPEC.pitch / (2.0 * math.pi)
    last = SPEC.k_max * math.pi / 2.0
    theta = np.linspace(THETA0 - 0.35, THETA0 + last, 500)
    z = FLANGE_T + clip_y(SPEC, 0) + climb * (theta - THETA0)
    pts = np.c_[SPEC.radius * np.cos(theta), SPEC.radius * np.sin(theta), z]
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


def placed(explode=0.0):
    base = to_poly(build_base(SPEC, Holes))
    parts = [("base", base)]
    for index in range(4):
        ang = tower_angle(index)
        mesh = to_poly(place_tower(build_tower(SPEC, index), ang))
        if explode:
            mesh.translate((explode * math.cos(ang), explode * math.sin(ang), explode * 0.15), inplace=True)
        parts.append((index, mesh))
    cap = to_poly(build_cap(SPEC))
    cap.translate((0, 0, FLANGE_T + SPEC.cap_y + explode), inplace=True)
    parts.append(("cap", cap))
    return parts


def assembled():
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1200))
    pl.set_background("white")
    for key, mesh in placed(0.0):
        add_solid(pl, mesh, COLORS["base"] if key == "base" else COLORS["cap"] if key == "cap" else COLORS[key])
    pl.add_mesh(tube_mesh(), color="#f4f1ea", opacity=0.45, smooth_shading=True)
    shot(pl, (0, 0, 60), (1.2, -1.4, 0.85), 280, zoom=1.1)
    save(pl, "assembled_iso.png")


def exploded():
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1400))
    pl.set_background("white")
    for key, mesh in placed(28.0):
        add_solid(pl, mesh, COLORS["base"] if key == "base" else COLORS["cap"] if key == "cap" else COLORS[key])
    shot(pl, (0, 0, 80), (1.1, -1.3, 0.9), 360, zoom=1.0)
    save(pl, "exploded.png")


def through_view(name, direction):
    """Look through the coil so the far clips stay visible past the near rib."""
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1100))
    pl.set_background("white")
    for key, mesh in placed(0.0):
        add_solid(pl, mesh, COLORS["base"] if key == "base" else COLORS["cap"] if key == "cap" else COLORS[key])
    pl.add_mesh(tube_mesh(), color="#f4f1ea", opacity=0.55, smooth_shading=True)
    shot(pl, (0, 0, 62), direction, 260, zoom=1.15)
    save(pl, name)


def clip_closeup():
    """The start tower lying as it prints, camera on the lowest clip."""
    pl = pv.Plotter(off_screen=True, window_size=(1200, 900))
    pl.set_background("white")
    tower = to_poly(build_tower(SPEC, 1))
    tower.translate((0, 0, SPEC.clip_width / 2.0), inplace=True)
    add_solid(pl, tower, COLORS[1])
    y = clip_y(SPEC, 0)
    shot(pl, (SPEC.tube_x, y, SPEC.clip_width / 2.0), (0.2, -1.0, 0.45), 55, zoom=1.25)
    save(pl, "clip_closeup.png")


def joint_closeup():
    """Tab lifted out of its slot, with the recessed 1 on the rib and on the ring."""
    pl = pv.Plotter(off_screen=True, window_size=(1200, 1000))
    pl.set_background("white")
    base = to_poly(build_base(SPEC, Holes)).clip_box((-24, 22, 10, 46, -1, 8), invert=False)
    tower = to_poly(place_tower(build_tower(SPEC, 1), tower_angle(1)))
    tower.translate((0, 0, 18.0), inplace=True)
    # Keep the foot and the recessed digit. The first clip starts above this box.
    tower = tower.clip_box((-4, 14, 14, 30, 10, 40), invert=False)
    add_solid(pl, base, COLORS["base"])
    add_solid(pl, tower, COLORS[1])
    # From the numbered face, so the tab sits over the slot and both 1s face the camera.
    shot(pl, (2.0, 27.0, 16.0), (1.15, 0.45, 0.55), 78, zoom=1.15)
    save(pl, "four_tower_joint.png")


def print_layout():
    pl = pv.Plotter(off_screen=True, window_size=(1600, 1000))
    pl.set_background("white")
    base = to_poly(build_base(SPEC, Holes))
    base.translate((-160, 40, 0), inplace=True)
    cap = to_poly(build_cap(SPEC))
    cap.translate((145, 20, 0), inplace=True)
    add_solid(pl, base, COLORS["base"])
    add_solid(pl, cap, COLORS["cap"])
    for index, x in enumerate((-75, -25, 25, 75)):
        tower = to_poly(build_tower(SPEC, index))
        tower.translate((x, 0, SPEC.clip_width / 2.0), inplace=True)
        add_solid(pl, tower, COLORS[index])
    pl.add_text("towers on their side, clips in the layers    ring and cap flange down", position="upper_edge", font_size=12, color="black")
    shot(pl, (0, 40, 0), (0.05, -0.4, 1.5), 420, zoom=0.8)
    save(pl, "print_orientation.png")


if __name__ == "__main__":
    assembled()
    exploded()
    through_view("four_tower_front.png", (0.0, -1.0, 0.15))
    through_view("four_tower_side.png", (1.0, 0.05, 0.12))
    clip_closeup()
    joint_closeup()
    print_layout()
