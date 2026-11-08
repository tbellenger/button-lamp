"""Renders of the folded sheet-metal casing (bend radii drawn sharp) + a flat-pattern sheet.  xvfb-run -a python3 src/render.py"""
import os, sys, numpy as np, pyvista as pv, trimesh
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from matplotlib.patches import PathPatch
from matplotlib.path import Path
from shapely import affinity
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sm, proxies as PX
R = os.path.join(sm.ROOT, 'render'); os.makedirs(R, exist_ok=True)

def solid(poly, t, Mx):
    ms = []
    for g in ([poly] if poly.geom_type == 'Polygon' else list(poly.geoms)):
        if g.area < 1: continue
        tm = trimesh.creation.extrude_polygon(g, t); tm.apply_transform(Mx); ms.append(tm)
    return pv.wrap(trimesh.util.concatenate(ms))

def scene(tag, path, colour, cam, title, edge='#555555'):
    m, P = sm.build(tag); parts, PM = sm.assemble(tag, m, P)
    pl = pv.Plotter(off_screen=True, window_size=(1600, 1200)); pl.set_background('white')
    for k, (poly, Mx) in parts.items():
        mesh = solid(poly, m['t'], Mx)
        pl.add_mesh(mesh, color=colour if k != 'bottom' else '#8a8f94', specular=0.6, specular_power=20, smooth_shading=False)
        pl.add_mesh(mesh.extract_feature_edges(30), color=edge, line_width=1)
    PX.add(pl, PM)
    ctr = np.array([125, 105, 60]); d = np.array(cam, float); d /= np.linalg.norm(d)
    pl.camera_position = [tuple(ctr + d * 900), tuple(ctr), (0, 0, 1)]; pl.reset_camera(); pl.camera.zoom(1.25)
    pl.add_text(title, font_size=12, color='black'); pl.enable_anti_aliasing('ssaa'); pl.screenshot(path); pl.close()

def patch(poly, ax, fc):
    for g in ([poly] if poly.geom_type == 'Polygon' else list(poly.geoms)):
        verts, codes = [], []
        for ring in [g.exterior] + list(g.interiors):
            cs = list(ring.coords); verts += cs; codes += [Path.MOVETO] + [Path.LINETO] * (len(cs) - 2) + [Path.CLOSEPOLY]
        ax.add_patch(PathPatch(Path(verts, codes), facecolor=fc, edgecolor='#222', lw=0.6))

def flats(tag, path):
    m, P = sm.build(tag); fig, ax = plt.subplots(figsize=(16, 9), dpi=130)
    pos = dict(hood=(0, 0), cheek_right=(440, 0), cheek_left=(440 + 520, 150), bottom=(440, 160))
    pos['cheek_left'] = (440 + 255.3, 0) ; pos['bottom'] = (440, 150)
    for n, (poly, bends, _) in P.items():
        ox, oy = pos[n]; dx, dy = ox - poly.bounds[0], oy - poly.bounds[1]
        patch(affinity.translate(poly, dx, dy), ax, '#cfd3d6')
        for a, b in bends: ax.plot([a[0] + dx, b[0] + dx], [a[1] + dy, b[1] + dy], ls='--', color='#06c', lw=0.9)
        bb = poly.bounds; ax.text(ox + (bb[2] - bb[0]) / 2, oy + (bb[3] - bb[1]) + 4, f'{n}  {bb[2]-bb[0]:.1f} x {bb[3]-bb[1]:.1f}  ({len(bends)} bends)', ha='center', fontsize=9)
    hi = P['hood'][2]
    for i, (bu, a) in enumerate(zip(hi['bend_u'], hi['angles'])): ax.text(bu, -8, f'{a:.1f}°', ha='center', fontsize=8, color='#06c')
    ax.set_aspect('equal'); ax.autoscale_view(); ax.margins(0.03); ax.grid(alpha=0.2)
    ax.set_title(f'Sheet-metal casing v2, {m["label"]}: flat patterns (blue dashed = bend centre-lines), hood seen from outside', fontsize=11)
    plt.tight_layout(); plt.savefig(path); plt.close()

if __name__ == '__main__':
    scene('al063', os.path.join(R, 'al063_front_iso.png'), '#c3c8cc', (1.1, 1.0, 0.8), 'Sheet metal v2, 5052 aluminium .063" bare: controls mounted straight in the hood')
    scene('ms059', os.path.join(R, 'ms059_black_front_iso.png'), '#2e2f31', (1.1, 1.0, 0.8), 'Sheet metal v2, mild steel .059" + matte black powder coat', edge='#888888')
    scene('al063', os.path.join(R, 'al063_back_iso.png'), '#c3c8cc', (-1.1, -1.0, 0.7), 'Back: USB window (Uno R4 USB-C), old USB-C slot, 12.2 mm DC jack')
    flats('al063', os.path.join(R, 'flat_patterns_al063.png'))
    print('ok')
