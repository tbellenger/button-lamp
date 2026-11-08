"""Renders for the v2 finger-joint casing (control slope + DC jack).  Needs the STLs written by check.py.
xvfb-run -a python3 src/render.py"""
import os, sys, numpy as np, pyvista as pv
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from matplotlib.patches import PathPatch
from matplotlib.path import Path
from shapely import affinity
from shapely.geometry import box
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fj, controls as C, proxies as PX
R = os.path.join(fj.ROOT, 'render'); STL = os.path.join(R, 'stl')
COL = dict(side_L='#d9b98c', side_R='#d9b98c', back='#c9a26b', top='#e6c89a', slope='#d4ad78', front='#bf9460', bottom='#a88456')
ex = 70.0
OUT = dict(side_L=(0, -ex, 0), side_R=(0, ex, 0), back=(-ex, 0, 0), top=(0, 0, ex), front=(ex, 0, 0), bottom=(0, 0, -ex),
           slope=tuple(-ex * np.array([fj.N[0], 0, fj.N[1]])))
E3 = np.array([fj.E[0], 0, fj.E[1]]); N3 = np.array([fj.N[0], 0, fj.N[1]])
o2 = fj.A + (fj.PANEL_S0 + C.PANEL_H) * fj.E
PANEL_M = PX.frame((o2[0], fj.PANEL_Y0, o2[1]), (0, 1, 0), -E3, -N3)

def load(k): return pv.read(os.path.join(STL, f'{k}.stl'))
def scene(path, explode=False, cam=(1, -1.25, 0.9), title='', controls=True, ctr=(125, 105, 60), dist=900, zoom=1.25, view_angle=None):
    pl = pv.Plotter(off_screen=True, window_size=(1600, 1200)); pl.set_background('white')
    for k, c in COL.items():
        m = load(k)
        if explode: m = m.translate(OUT[k], inplace=False)
        pl.add_mesh(m, color=c, specular=0.2, smooth_shading=False)
        pl.add_mesh(m.extract_feature_edges(30), color='#4a3a28', line_width=1)
    for i in range(6):
        m = load(f'block{i}')
        if explode: m = m.translate((0, 0, -ex / 2), inplace=False)
        pl.add_mesh(m, color='#7a8899', opacity=0.9)
    if controls:
        M = PANEL_M.copy()
        if explode: M[:3, 3] += np.array(OUT['slope'])
        PX.add(pl, M)
    ctr = np.array(ctr, float); d = np.array(cam, float); d /= np.linalg.norm(d)
    pl.camera_position = [tuple(ctr + d * dist), tuple(ctr), (0, 0, 1)]
    if view_angle: pl.camera.view_angle = view_angle
    else: pl.reset_camera(); pl.camera.zoom(zoom)
    pl.add_text(title, font_size=12, color='black')
    pl.enable_anti_aliasing('ssaa'); pl.screenshot(path); pl.close()

def patch(poly, ax, fc, ec='#222', lw=0.6):
    for g in ([poly] if poly.geom_type == 'Polygon' else list(poly.geoms)):
        verts, codes = [], []
        for ring in [g.exterior] + list(g.interiors):
            cs = list(ring.coords); verts += cs; codes += [Path.MOVETO] + [Path.LINETO] * (len(cs) - 2) + [Path.CLOSEPOLY]
        ax.add_patch(PathPatch(Path(verts, codes), facecolor=fc, edgecolor=ec, lw=lw))

def panel_drawing(path):
    """the control slope as seen from the front/outside, low edge at the bottom, with labels and key dimensions."""
    sl = fj.parts['slope']
    # slope local (s, y) -> view (X = y, Y = -s): front (high s) at the bottom
    view = affinity.affine_transform(sl, [0, 1, -1, 0, 0, 0])
    fig, ax = plt.subplots(figsize=(13, 10), dpi=130)
    patch(view, ax, '#e9d3a9')
    fpv = affinity.affine_transform(box(fj.PANEL_S0, fj.PANEL_Y0, fj.PANEL_S0 + C.PANEL_H, fj.PANEL_Y0 + C.PANEL_W), [0, 1, -1, 0, 0, 0])
    ax.plot(*fpv.exterior.xy, ls='--', color='#3b6fb6', lw=0.8)
    ax.text(fpv.bounds[0] + 2, fpv.bounds[3] + 2, 'old printed-panel footprint 184.15 x 133.35 (layout reference only)', color='#3b6fb6', fontsize=8)
    for n, pin, k, c, s in C.CONTROLS:
        sv, yv = fj.panel_to_slope(*c); X, Y = yv, -sv
        size = f'Ø{s}' if k == 'circle' else (f'{s[0]} x {s[1]}' if k == 'rect' else f'slot {s[0]} x {s[1]}')
        if n.startswith('fader_screw'): continue
        dy = -4 if k == 'slot' else -((s / 2) if k == 'circle' else s[1] / 2) - 4
        ax.text(X, Y + dy, f'{pin}\n{size}', ha='center', va='top', fontsize=7.5)
    ax.text(105, -fj.S1 + 2.5, 'FRONT (low) edge: fader side faces you', ha='center', va='bottom', fontsize=8, weight='bold')
    ax.text(105, -fj.S0 - 6, 'top crease (back)', ha='center', fontsize=8)
    ax.text(fj.PANEL_Y0 + 32.26, -(fj.PANEL_S0 + C.PANEL_H - 13.46) - 4, 'M3', fontsize=7, ha='center', va='top')
    ax.text(fj.PANEL_Y0 + 151.89, -(fj.PANEL_S0 + C.PANEL_H - 13.46) - 4, 'M3', fontsize=7, ha='center', va='top')
    ax.set_aspect('equal'); ax.autoscale_view(); ax.margins(0.04)
    b = view.bounds
    ax.set_title(f'Control slope, 1/8" Baltic birch, seen from outside: {b[2]-b[0]:.1f} x {b[3]-b[1]:.1f} mm '
                 f'(3 finger tabs each side into the side panels)', fontsize=11)
    ax.set_xlabel('mm across the box (left -> right, seen from the front)'); ax.grid(alpha=0.2)
    plt.tight_layout(); plt.savefig(path); plt.close()

def flat_parts(path):
    order = [('side', 0, 0), ('side', 265, 0), ('back', 530, 0), ('bottom', 0, 140), ('top', 260, 140), ('slope', 400, 140), ('front', 570, 140)]
    fig, ax = plt.subplots(figsize=(16, 9.5), dpi=130)
    for nm, ox, oy in order:
        pg = affinity.translate(fj.parts[nm], -fj.parts[nm].bounds[0] + ox, -fj.parts[nm].bounds[1] + oy)
        patch(pg, ax, '#e9d3a9' if nm != 'bottom' else '#d8c29a')
        b = pg.bounds; lab = {'slope': 'control slope', 'back': 'back v2'}.get(nm, nm)
        ax.text((b[0] + b[2]) / 2, b[3] + 4, f'{lab}  {b[2]-b[0]:.1f} x {b[3]-b[1]:.1f} mm  (qty {fj.QTY[nm]})', ha='center', fontsize=9)
    fpp = affinity.translate(fj.faceplate(), 0, 370)
    patch(fpp, ax, '#f1e2c4'); ax.text(92, 370 + 138, 'optional retrofit faceplate 184.15 x 133.35 (for the slope already ordered)', ha='center', fontsize=9)
    ax.set_aspect('equal'); ax.autoscale_view(); ax.margins(0.02)
    ax.set_title(f'Finger-joint casing v2, 1/8" Baltic birch (T = {fj.T} mm): flat parts at 1:1 shape', fontsize=12)
    ax.set_xlabel('mm'); ax.grid(alpha=0.2)
    plt.tight_layout(); plt.savefig(path); plt.close()

if __name__ == '__main__':
    scene(os.path.join(R, 'assembled_front_iso.png'), title='v2 assembled: plywood control slope with all controls (250 x 210 x 120 mm)', cam=(1.1, 1.0, 0.8))
    scene(os.path.join(R, 'assembled_back_iso.png'), title='v2 back: USB-C window for the Uno R4, old USB-C slot, new 12.2 mm DC jack hole', cam=(-1.1, -1.0, 0.7))
    scene(os.path.join(R, 'exploded.png'), explode=True, title='v2 exploded: control slope tabs drop into the side notches like the rest', cam=(1.1, 1.0, 0.9))
    ctr = tuple(np.array([fj.A[0], 0, fj.A[1]]) + E3 * (fj.PANEL_S0 + C.PANEL_H / 2) + np.array([0, 105, 0]))
    scene(os.path.join(R, 'panel_closeup.png'), title='Close-up: the control slope (no printed panel), controls shown as rough stand-ins',
          cam=(np.sin(fj.TH) + 0.25, 0.35, np.cos(fj.TH) + 0.1), ctr=ctr, dist=420, view_angle=34)
    panel_drawing(os.path.join(R, 'control_slope_drawing.png'))
    flat_parts(os.path.join(R, 'flat_parts.png'))
    print('ok')
