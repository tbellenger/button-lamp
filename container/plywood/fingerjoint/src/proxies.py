"""Rough 3D stand-ins for the panel controls (renders only), placed through controls.CONTROLS.
frame(origin, ax_x, ax_y, ax_z) maps panel-local (x right, y up-slope, z out of the face) to box coords."""
import numpy as np, pyvista as pv
import controls as C

def frame(origin, ax_x, ax_y, ax_z):
    M = np.eye(4); M[:3, 0], M[:3, 1], M[:3, 2], M[:3, 3] = ax_x, ax_y, ax_z, origin; return M

def _cyl(c, d, h, z0=0.0, res=48):
    return pv.Cylinder(center=(c[0], c[1], z0 + h / 2), direction=(0, 0, 1), radius=d / 2, height=h, resolution=res)

def _box(c, w, l, h, z0=0.0):
    return pv.Box(bounds=(c[0] - w / 2, c[0] + w / 2, c[1] - l / 2, c[1] + l / 2, z0, z0 + h))

COL = dict(red='#c62828', white='#f2f2f2', black='#222222', steel='#b0b4b8', amber='#ffb300', blue='#1e63c6')

def meshes():
    """[(mesh in panel-local coords, colour)]"""
    P = {n: (c, s) for n, _, _, c, s in C.CONTROLS}; out = []
    c = P['arcade_left'][0]; out += [(_cyl(c, 34, 3), COL['red']), (_cyl(c, 24, 9, 3), COL['red'])]
    c = P['arcade_top'][0]; out += [(_cyl(c, 34, 3), COL['white']), (_cyl(c, 24, 9, 3), COL['white'])]
    c = P['key'][0]; out += [(_cyl(c, 27, 4), COL['steel']), (_box(c, 2.5, 9, 0.6, 4), COL['black'])]
    c = P['mushroom'][0]; out += [(_cyl(c, 26, 6), COL['black']), (pv.Sphere(radius=20, center=(c[0], c[1], 6)).clip('z', origin=(0, 0, 6), invert=False), COL['red'])]
    c = P['toggle_left'][0]; out += [(_cyl(c, 15, 2.5), COL['steel']), (pv.Cylinder(center=(c[0], c[1] + 3, 12), direction=(0, 0.35, 1), radius=2, height=20), COL['steel'])]
    c = P['toggle_right'][0]; out += [(_cyl(c, 15, 2.5), COL['steel']), (_box((c[0], c[1] - 4), 20, 34, 3), COL['red']), (_box((c[0], c[1] - 4), 18, 30, 16, 3), COL['red'])]
    c = P['rocker'][0]; out += [(_box(c, 25, 42, 3), COL['black']), (_box((c[0], c[1] + 4), 19, 18, 7, 3), COL['black']), (_box((c[0], c[1] - 6), 19, 16, 4, 3), COL['black'])]
    c = P['knob'][0]; out += [(_cyl(c, 22, 15), COL['black']), (_box((c[0], c[1] + 6), 1.5, 8, 0.4, 15), COL['white'])]
    c = P['fader_slot'][0]; out += [(_box((c[0] - 15, c[1]), 12, 20, 11), COL['black']), (_box((c[0] - 15, c[1]), 12, 1.5, 0.4, 11), COL['white'])]
    return out

def add(pl, M_panel_to_box):
    for m, col in meshes():
        pl.add_mesh(m.transform(M_panel_to_box, inplace=False), color=col, specular=0.4, smooth_shading=True)
