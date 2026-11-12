"""Renders of the stackable cross mount.

    xvfb-run -a python3 render_views.py

No slicer is installed here. These are geometry previews, not sliced toolpaths.
"""

import math
import os

import numpy as np
import pyvista as pv

from system import (
    HUB_H,
    HUB_Z0,
    _pin,
    base_plate,
    cap,
    cross_high,
    cross_low,
    fit_coupon,
    levels_for_strip,
    lowest_angle,
    place_cross,
    spacer,
    specs,
)

HERE = os.path.dirname(os.path.abspath(__file__))
REND = os.path.join(HERE, "render")
os.makedirs(REND, exist_ok=True)

SPEC = specs(13.0, 100.0, 10.0, 2.4, 11.5, 14.0)
WIRE = (11.71, 32.24)
THETA = lowest_angle(*WIRE)
N_LEVELS, _STATIONS = levels_for_strip(1000.0, SPEC.radius, SPEC.pitch)


class Holes:
    bolt = 76.2
    bolt_d = 3.4
    wire_x = WIRE[0]
    wire_y = WIRE[1]
    cable_d = 15.0
    window_d = 19.0


def to_poly(shape, tol=0.15):
    verts, faces = shape.tessellate(tol)
    pts = np.array([[p.X, p.Y, p.Z] for p in verts], float)
    fac = np.asarray(faces, np.int64)
    cells = np.hstack([np.full((len(fac), 1), 3, np.int64), fac]).ravel()
    mesh = pv.PolyData(pts, cells)
    mesh.clean(inplace=True)
    return mesh


def tube_mesh():
    """Silicone from just before the first clip to the 15th station (end of 1 m)."""
    z0 = HUB_Z0 - 1.5 * SPEC.quarter
    climb = SPEC.pitch / (2.0 * math.pi)
    last = 14 * math.pi / 2.0
    theta = np.linspace(THETA - 0.35, THETA + last, 600)
    z = z0 + climb * (theta - THETA)
    pts = np.c_[SPEC.radius * np.cos(theta), SPEC.radius * np.sin(theta), z]
    return pv.Spline(pts, len(pts)).tube(radius=SPEC.tube_od / 2.0, n_sides=20)


def stack(explode=0.0):
    """Placed parts. explode adds a gap between levels, in mm."""
    low = cross_low(SPEC)
    high = cross_high(SPEC)
    sp = spacer(SPEC)
    top = cap(SPEC)
    base = to_poly(base_plate(SPEC, Holes))
    parts = [("base", base)]
    spin = math.degrees(THETA)
    for i in range(N_LEVELS):
        lift = i * explode
        z = HUB_Z0 + i * SPEC.pitch + lift
        parts.append(("low", to_poly(place_cross(low, THETA, z))))
        parts.append(("high", to_poly(place_cross(high, THETA + math.pi / 2.0, z))))
        seat = z + HUB_H / 2.0 + (explode * 0.45 if i < N_LEVELS - 1 or explode else 0.0)
        blank = sp if i < N_LEVELS - 1 else top
        moved = pv_rotate(to_poly(blank), spin)
        moved.translate((0, 0, seat), inplace=True)
        parts.append(("spacer" if i < N_LEVELS - 1 else "cap", moved))
    return parts


def pv_rotate(mesh, degrees):
    mesh.rotate_z(degrees, inplace=True)
    return mesh


COLORS = {
    "base": "#c4b8a5",
    "low": "#e07a3d",
    "high": "#3d7ea6",
    "spacer": "#e2c14a",
    "cap": "#6e6a64",
}


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


def assembled():
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1100))
    pl.set_background("white")
    for kind, mesh in stack(0.0):
        add_solid(pl, mesh, COLORS[kind])
    tube = tube_mesh()
    pl.add_mesh(tube, color="#f4f1ea", opacity=0.55, smooth_shading=True)
    shot(pl, (0, 0, 55), (1.3, -1.5, 0.9), 280, zoom=1.15)
    save(pl, "assembled_iso.png")


def exploded():
    pl = pv.Plotter(off_screen=True, window_size=(1400, 1600))
    pl.set_background("white")
    for kind, mesh in stack(36.0):
        add_solid(pl, mesh, COLORS[kind])
    shot(pl, (0, 0, 170), (1.2, -1.4, 0.85), 520, zoom=0.95)
    save(pl, "exploded.png")


def joint_closeup():
    """One level, pulled apart just enough to see the lap and the pin."""
    pl = pv.Plotter(off_screen=True, window_size=(1200, 1000))
    pl.set_background("white")
    low = to_poly(place_cross(cross_low(SPEC), THETA, HUB_Z0))
    high = to_poly(place_cross(cross_high(SPEC), THETA + math.pi / 2.0, HUB_Z0 + 18))
    sp = to_poly(spacer(SPEC))
    sp.rotate_z(math.degrees(THETA), inplace=True)
    sp.translate((0, 0, HUB_Z0 + HUB_H / 2.0 + 36), inplace=True)
    base = to_poly(base_plate(SPEC, Holes))
    add_solid(pl, base, COLORS["base"])
    add_solid(pl, low, COLORS["low"])
    add_solid(pl, high, COLORS["high"])
    add_solid(pl, sp, COLORS["spacer"])
    shot(pl, (0, 0, 30), (1.1, -0.8, 0.7), 90, zoom=1.3)
    save(pl, "lap_and_pin.png")


def print_layout():
    """How each part sits on the bed. Crosses lie flat. Spacer and base stand up."""
    pl = pv.Plotter(off_screen=True, window_size=(1600, 1000))
    pl.set_background("white")
    low = to_poly(cross_low(SPEC))
    low.translate((0, 0, 7), inplace=True)  # bed face was at z=-7
    high = to_poly(cross_high(SPEC))
    high.translate((115, 10, 7), inplace=True)
    base = to_poly(base_plate(SPEC, Holes))
    base.translate((-120, -10, 0), inplace=True)
    sp = to_poly(spacer(SPEC))
    sp.translate((20, 55, 0), inplace=True)
    top = to_poly(cap(SPEC))
    top.translate((60, 55, 0), inplace=True)
    coupon = to_poly(fit_coupon(SPEC))
    coupon.translate((-50, 50, 0), inplace=True)
    add_solid(pl, low, COLORS["low"])
    add_solid(pl, high, COLORS["high"])
    add_solid(pl, base, COLORS["base"])
    add_solid(pl, sp, COLORS["spacer"])
    add_solid(pl, top, COLORS["cap"])
    add_solid(pl, coupon, "#b9b4ae")
    pl.add_text("crosses flat, C in the layers    base flange down    spacer socket down", position="upper_edge", font_size=12, color="black")
    shot(pl, (10, 15, 0), (0.15, -0.35, 1.6), 420, zoom=0.95)
    save(pl, "print_orientation.png")


def diamond_closeup():
    """The hub hole as it prints, point up, with the matching pin beside it."""
    pl = pv.Plotter(off_screen=True, window_size=(1200, 900))
    pl.set_background("white")
    low = to_poly(cross_low(SPEC))
    low.translate((0, 0, SPEC.clip_width / 2.0), inplace=True)
    pin = to_poly(_pin(18.0, groove_z0=0.0))
    # In front of the hub, clear of the arm, so both diamonds sit in one view.
    pin.translate((12, 22, 0), inplace=True)
    add_solid(pl, low, COLORS["low"])
    add_solid(pl, pin, COLORS["spacer"])
    shot(pl, (6, 8, 10), (0.05, 1.0, 0.35), 52, up=(0, 0, 1), zoom=1.15)
    save(pl, "diamond_bore.png")


if __name__ == "__main__":
    assembled()
    exploded()
    joint_closeup()
    print_layout()
    diamond_closeup()
