"""Control-panel cut-outs for the Spiral Lamp, shared by the plywood control slope, the plywood retrofit
faceplate and the sheet-metal hood.

Panel frame (mm), as seen from OUTSIDE: x = 0..184.15 left -> right, y = 0..133.35 with y = 0 at the LOW
(front) edge, i.e. the fader edge faces you.  This is the footprint of the original printed `Switch Panel.stl`
(original-plans/3D Printable Parts (STL Files)/), and every centre below was measured from a section through
that STL (hole_report() reprints them).  Sizes are the printed sizes plus a small clearance, because laser-cut
wood / metal holes come out true to size and have none of PLA's give:

    printed  -> here    what
    27.69    -> 28.0    arcade buttons (28 mm thread)
    22.10    -> 22.3    key switch + mushroom (22 mm, IEC 22.3 mm hole)
    12.19    -> 12.4    toggle + covered toggle (12 mm / 15/32" bushing)
    9.13     -> 9.4     speed pot (M9 bushing)
    20.95 x 37.85 (kept exact) big rocker, snap-in: snap-ins want a tight hole
    106.43 x 1.52 -> 106.43 x 2.2   fader slot (widened: SendCutSend's smallest cut in .125" birch is 0.08")
    3.20     -> 3.4     fader M3 screws, 119.63 apart
"""
from shapely.geometry import Point, box, LineString
from shapely.ops import unary_union

PANEL_W, PANEL_H = 184.15, 133.35
CORNER_HOLES = [(6.35, 6.35), (177.80, 6.35), (6.35, 127.00), (177.80, 127.00)]   # printed panel bolt pattern 171.45 x 120.65

# Rocker cut-out. Default = the printed panel's rectangle (the original "jumbo" rocker).  A small KCD1-size
# rocker wants 19.0 x 13.0 (or 21.2 x 15.0): measure the snap body (not the bezel) and change this before ordering.
ROCKER_CUT = (20.95, 37.85)      # (across x, along y)
FADER_SLOT_W = 2.2

# name, pin, kind, centre (x, y), size
CONTROLS = [
    ('arcade_left',   'D8 red arcade (or D4)',      'circle', (25.40, 53.97),  28.0),
    ('arcade_top',    'D4 white arcade (or D8)',    'circle', (38.10, 104.77), 28.0),
    ('key',           'D5 key switch',              'circle', (92.07, 107.95), 22.3),
    ('mushroom',      'D11 chaos mushroom',         'circle', (146.05, 104.78), 22.3),
    ('toggle_left',   'D6 toggle',                  'circle', (60.32, 53.98),  12.4),
    ('rocker',        'D7 main rocker',             'rect',   (92.08, 53.98),  ROCKER_CUT),
    ('toggle_right',  'D10 covered (wave) toggle',  'circle', (123.82, 53.98), 12.4),
    ('knob',          'A1 speed knob',              'circle', (158.75, 53.98), 9.4),
    ('fader_slot',    'A0 brightness fader',        'slot',   (92.07, 13.46),  (106.43, FADER_SLOT_W)),
    ('fader_screw_l', 'A0 fader M3',                'circle', (32.26, 13.46),  3.4),
    ('fader_screw_r', 'A0 fader M3',                'circle', (151.89, 13.46), 3.4),
]

def shape(kind, c, size, res=64):
    x, y = c
    if kind == 'circle':
        return Point(x, y).buffer(size / 2, res)
    if kind == 'rect':
        w, h = size; return box(x - w / 2, y - h / 2, x + w / 2, y + h / 2)
    if kind == 'slot':
        ln, w = size; return LineString([(x - ln / 2 + w / 2, y), (x + ln / 2 - w / 2, y)]).buffer(w / 2, 32)
    raise ValueError(kind)

def cutouts(to_xy=None, corner_holes=False, corner_d=3.4):
    """list of (name, pin, polygon) in panel coords, or mapped through to_xy(x, y) -> (u, v) (must be a rigid map)."""
    from shapely import affinity
    out = [(n, p, shape(k, c, s)) for n, p, k, c, s in CONTROLS]
    if corner_holes:
        out += [(f'corner{i}', 'M3 panel screw', Point(*xy).buffer(corner_d / 2, 32)) for i, xy in enumerate(CORNER_HOLES)]
    if to_xy is not None:
        from shapely.ops import transform
        out = [(n, p, transform(lambda xs, ys, z=None: _map(to_xy, xs, ys), g)) for n, p, g in out]
    return out

def _map(f, xs, ys):
    us, vs = [], []
    for a, b in zip(xs, ys):
        u, v = f(a, b); us.append(u); vs.append(v)
    return us, vs

def union(to_xy=None, corner_holes=False):
    return unary_union([g for _, _, g in cutouts(to_xy, corner_holes)])

def check_layout(min_web=3.0, min_edge=6.0):
    """no two cut-outs closer than min_web, all at least min_edge inside the panel footprint."""
    cs = cutouts(corner_holes=False); probs = []
    fp = box(0, 0, PANEL_W, PANEL_H)
    for i, (a, _, ga) in enumerate(cs):
        if fp.exterior.distance(ga) < min_edge or not fp.contains(ga): probs.append(('edge', a, round(fp.exterior.distance(ga), 2)))
        for b, _, gb in cs[i + 1:]:
            if ga.distance(gb) < min_web: probs.append(('web', a, b, round(ga.distance(gb), 2)))
    return probs

def hole_report(stl_path):
    """re-measure the printed panel (section 0.3 mm above its underside)."""
    import trimesh, numpy as np
    from shapely.geometry import Polygon
    m = trimesh.load(stl_path); s = m.section(plane_origin=[0, 0, 0.3], plane_normal=[0, 0, 1])
    P2, _ = s.to_2D(to_2D=np.eye(4)); rows = []
    for g in P2.polygons_full:
        for r in g.interiors:
            q = Polygon(r); b = q.bounds; rows.append((round(q.centroid.x, 2), round(q.centroid.y, 2), round(b[2] - b[0], 2), round(b[3] - b[1], 2)))
    return rows

if __name__ == '__main__':
    import os, sys
    print('layout problems:', check_layout())
    stl = os.path.join(os.path.dirname(__file__), '..', '..', '..', 'original-plans', '3D Printable Parts (STL Files)', 'Switch Panel.stl')
    if len(sys.argv) > 1: stl = sys.argv[1]
    if os.path.exists(stl):
        for r in hole_report(stl): print(r)
