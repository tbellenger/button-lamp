"""Finger-joint (tab & notch) version of the Spiral Lamp plywood casing for SendCutSend laser cutting.
Units mm. Box coords: X back->front (0..250), Y left->right (0..210), Z up (0..120).
Joint scheme: back / top / slope / front carry tabs on both Y-ends that drop into open notches in the side
edges (tab ends finish flush with the side faces). Back->top is finger-jointed (3 back tabs into top notches).
Top/slope and slope/front (non-90 deg) are NOT jointed: each panel is held only by its side tabs and stops
CLR_D short of its neighbour (small glue/filler gap). Bottom = removable tray, unchanged, screws into the
6 printed corner blocks (also unchanged)."""
import sys, os, math, json, csv
import numpy as np, ezdxf
from shapely.geometry import Polygon, box, Point, LineString, MultiPolygon
from shapely.ops import unary_union
from shapely import affinity
from shapely.geometry.polygon import orient

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
PLY = os.path.dirname(ROOT); sys.path.insert(0, os.path.join(PLY, 'src')); sys.path.insert(0, HERE)   # repo layout: plywood/src + plywood/fingerjoint/src
import box as B
import controls as C

# VARIANT 'v2' (default): the slope IS the control panel (all control cut-outs, no printed panel) and the back
#                         gets a DC jack hole for the 5 V 10 A supply.  -> dxf/
# VARIANT 'ordered':      exactly the set ordered from SendCutSend (S7872337): slope with window for the
#                         printed panel, no DC jack.                       -> dxf/as_ordered_S7872337/
VARIANT = os.environ.get('FJ_VARIANT', 'v2')

# ------------------------------------------------------------------ parameters
T = 3.175          # sheet thickness: SendCutSend Baltic birch .125" (3.18 mm). Change here only, then re-run.
CLR = 0.30         # slip fit: slot length = tab length + CLR (CLR/2 each end)
CLR_D = 0.15       # depth play: notch depth = mating thickness + CLR_D (slot width = T + 2*CLR_D = T + 0.30)
DOG_R = 1.10       # dogbone radius (dia 2.2 mm > 2.03 mm min feature)
MIN_FEAT = 0.08 * 25.4   # 2.032 mm
MIN_PART = 25.4
L, W, H, hF, xa = 250.0, 210.0, 120.0, 28.0, 122.0
TH = math.atan2(H - hF, L - xa); S = math.hypot(H - hF, L - xa)
E = np.array([math.cos(TH), -math.sin(TH)]); N = np.array([-math.sin(TH), -math.cos(TH)])   # (x,z); N points into the box
A = np.array([xa, H])
DP = T + CLR_D     # depth of every notch / gap
TAB = 24.0         # standard side-tab length
TABS = dict(back=[30.0, 86.0], top=[32.0, 92.0], front=[14.0], backtop=[40.0, 105.0, 170.0])
TAB_LEN = dict(back=TAB, top=TAB, slope=TAB, front=12.0, backtop=28.0)

# ------------------------------------------------------------------ old geometry (holes kept exactly as now)
p = B.params(T)
WIRE_OFF = (28.761440419908304 - 61.0, 113.71175803203249 - 102.0)     # same as quotes/src/make_dxf.py
old, geo = B.pieces(p, {'wire_hole_xy': (-WIRE_OFF[0], -WIRE_OFF[1])}, 180.0)
S0_OLD = geo['s0']

def holes_of(poly):
    gs = [poly] if poly.geom_type == 'Polygon' else list(poly.geoms)
    return [Polygon(r) for g in gs for r in g.interiors]

def tr(polys, dx, dy): return [affinity.translate(q, dx, dy) for q in polys]
HOLES = dict(back=tr(holes_of(old['back']['poly']), T, 0),        # old local (y-t, z) -> (y, z)
             top=tr(holes_of(old['top']['poly']), 0, T),          # old local (x, y-t) -> (x, y)
             slope=tr(holes_of(old['slope']['poly']), S0_OLD, T)) # old local (s-s0, y-t) -> (s, y)

HOLES_ORDERED_SLOPE = list(HOLES['slope'])
# ---- v2: control panel footprint on the slope = exactly where the printed panel used to sit (same centre)
PANEL_S0 = S0_OLD + geo['Ls'] / 2 - C.PANEL_H / 2     # s of the panel's HIGH (back) edge; s grows toward the front
PANEL_Y0 = W / 2 - C.PANEL_W / 2                       # y of the panel's left edge (seen from the front)
def panel_to_slope(x, y):                              # panel (x right, y=0 at the low/front edge) -> slope local (s, y)
    return (PANEL_S0 + C.PANEL_H - y, PANEL_Y0 + x)
# DC jack for the 5 V 10 A supply (back, box coords y, z).  12.2 mm suits a GX12 2-pin aviation socket or a
# 12 mm-thread metal 5.5 x 2.5 barrel jack; set 8.0 for a DC-022B (but those are only rated 0.5-3 A).
DC_JACK = dict(y=72.0, z=24.0, d=12.2)
if VARIANT == 'v2':
    HOLES['slope'] = [g for _, _, g in C.cutouts(panel_to_slope)]
    HOLES['back'] = HOLES['back'] + [Point(DC_JACK['y'], DC_JACK['z']).buffer(DC_JACK['d'] / 2, 64)]

# ------------------------------------------------------------------ helpers
def slope_rect(s_a, s_b, k_a, k_b):
    return Polygon([tuple(A + s * E + k * N) for s, k in [(s_a, k_a), (s_b, k_a), (s_b, k_b), (s_a, k_b)]])

def intervals_outside(tabs, half, lo, hi, clr=0.0):
    """complement of [c-half-clr, c+half+clr] inside [lo, hi] (extended by 1 mm past the ends)."""
    cuts, a = [], lo - 1.0
    for c in sorted(tabs):
        cuts.append((a, c - half - clr)); a = c + half + clr
    cuts.append((a, hi + 1.0)); return cuts

def add_dogbones(poly, r=DOG_R):
    poly = orient(poly, 1); cs = np.array(poly.exterior.coords[:-1]); n = len(cs); circ = []
    for i in range(n):
        v, pr, nx = cs[i], cs[i - 1], cs[(i + 1) % n]
        a = (pr - v) / np.linalg.norm(pr - v); b = (nx - v) / np.linalg.norm(nx - v)
        cross = (v - pr)[0] * (nx - v)[1] - (v - pr)[1] * (nx - v)[0]
        if cross < -1e-9 and abs(np.dot(a, b)) < 0.05:          # reflex, ~90 deg = joint inside corner
            w = (a + b) / np.linalg.norm(a + b)
            circ.append(Point(*(v + w * r)).buffer(r, 32))   # centre on the void side, circle passes through the corner
    return poly.difference(unary_union(circ)) if circ else poly, len(circ)

# ------------------------------------------------------------------ slope extent (clear the top and the front by CLR_D)
TOP_XZ = box(0, H - T, xa, H); FRONT_XZ = box(L - T, 0, L, hF)
def bisect(f, lo, hi, it=60):
    for _ in range(it):
        m = (lo + hi) / 2
        if f(m): hi = m
        else: lo = m
    return hi
S0 = bisect(lambda s: slope_rect(s, S, 0, T).distance(TOP_XZ) >= CLR_D, 0.0, 15.0)
S1 = S - bisect(lambda d: slope_rect(S0, S - d, 0, T).distance(FRONT_XZ) >= CLR_D, 0.0, 15.0)
S0 = math.ceil(S0 * 100) / 100; S1 = math.floor(S1 * 100) / 100
SM = (S0 + S1) / 2; TABS['slope'] = [SM - 48.0, SM, SM + 48.0]

# ------------------------------------------------------------------ parts (local 2D coords)
def panel_with_side_tabs(env, along_lo, along_hi, tabs, half, along_is_x, extra_gaps=()):
    """env: full rectangle incl. tab zone. Side gaps of depth DP at local 'y-ish' coordinate 0 and W."""
    gaps = []
    for a0, a1 in intervals_outside(tabs, half, along_lo, along_hi):
        if along_is_x:
            gaps += [box(a0, -1, a1, DP), box(a0, W - DP, a1, W + 1)]
        else:
            gaps += [box(-1, a0, DP, a1), box(W - DP, a0, W + 1, a1)]
    return env.difference(unary_union(gaps + list(extra_gaps)))

parts, meta = {}, {}
# back: local (u=y, v=z)
bt_gaps = [box(a0, H - DP, a1, H + 1) for a0, a1 in intervals_outside(TABS['backtop'], TAB_LEN['backtop'] / 2, 0, W)]
back = panel_with_side_tabs(box(0, 0, W, H), 0, H, TABS['back'], TAB / 2, False, bt_gaps)
# top: local (u=x, v=y); notches for the back's tabs at x in [0, DP]
top_notch = [box(-1, c - TAB_LEN['backtop'] / 2 - CLR / 2, DP, c + TAB_LEN['backtop'] / 2 + CLR / 2) for c in TABS['backtop']]
top = panel_with_side_tabs(box(0, 0, xa, W), 0, xa, TABS['top'], TAB / 2, True, top_notch)
# slope: local (u=s, v=y)
slope = panel_with_side_tabs(box(S0, 0, S1, W), S0, S1, TABS['slope'], TAB / 2, True)
# front: local (u=y, v=z)
front = panel_with_side_tabs(box(0, 0, W, hF), 0, hF, TABS['front'], TAB_LEN['front'] / 2, False)
# side: local (x, z)
prof = B.profile(p)
sn = []
for c in TABS['back']: sn.append(box(-1, c - TAB / 2 - CLR / 2, DP, c + TAB / 2 + CLR / 2))
for c in TABS['top']: sn.append(box(c - TAB / 2 - CLR / 2, H - DP, c + TAB / 2 + CLR / 2, H + 1))
for c in TABS['slope']: sn.append(slope_rect(c - TAB / 2 - CLR / 2, c + TAB / 2 + CLR / 2, -1.0, DP))
for c in TABS['front']: sn.append(box(L - DP, c - TAB_LEN['front'] / 2 - CLR / 2, L + 1, c + TAB_LEN['front'] / 2 + CLR / 2))
side = prof.difference(unary_union(sn))

ndog = {}
for nm, poly in [('side', side), ('back', back), ('top', top), ('slope', slope), ('front', front)]:
    poly, k = add_dogbones(poly); ndog[nm] = k
    if nm in HOLES: poly = poly.difference(unary_union(HOLES[nm]))
    parts[nm] = poly
parts['bottom'] = old['bottom']['poly']                       # unchanged removable tray
QTY = dict(side=2, back=1, top=1, slope=1, front=1, bottom=1)
ENV = dict(side=prof, back=box(0, 0, W, H), top=box(0, 0, xa, W), slope=box(S0, 0, S1, W), front=box(0, 0, W, hF),
           bottom=box(*parts['bottom'].bounds))

# ------------------------------------------------------------------ 3D placement: P = O + u*U + v*V + k*Nn, k in [0,T]; det(U,V,Nn)=+1
c = p['clear']
def M(U, V, Nn, O): return np.array([[U[i], V[i], Nn[i], O[i]] for i in range(3)], float)
ex, ez = E; nx_, nz_ = N
PLACE = {
    'side_L': ('side', M((1, 0, 0), (0, 0, 1), (0, -1, 0), (0, T, 0))),
    'side_R': ('side', M((1, 0, 0), (0, 0, 1), (0, -1, 0), (0, W, 0))),
    'back': ('back', M((0, 1, 0), (0, 0, 1), (1, 0, 0), (0, 0, 0))),
    'top': ('top', M((1, 0, 0), (0, 1, 0), (0, 0, 1), (0, 0, H - T))),
    'slope': ('slope', M((ex, 0, ez), (0, 1, 0), (-nx_, 0, -nz_), (A[0] + T * nx_, 0, A[1] + T * nz_))),
    'front': ('front', M((0, 1, 0), (0, 0, 1), (1, 0, 0), (L - T, 0, 0))),
    'bottom': ('bottom', M((1, 0, 0), (0, 1, 0), (0, 0, 1), (T + c, T + c, 0))),
}
BLOCKS = [(bx, by) for bx, by in B.block_positions(p)]

# ------------------------------------------------------------------ DXF out
def new_doc():
    d = ezdxf.new('R2010'); d.units = ezdxf.units.MM; d.header['$INSUNITS'] = 4; d.header['$MEASUREMENT'] = 1; return d
def rings(poly):
    gs = [poly] if poly.geom_type == 'Polygon' else list(poly.geoms)
    for g in gs:
        yield list(g.exterior.coords)
        for r in g.interiors: yield list(r.coords)
FLAT = {}
OUTDIR = os.path.join(ROOT, 'dxf') if VARIANT == 'v2' else os.path.join(ROOT, 'dxf', 'as_ordered_S7872337')
FILE_TAG = dict(slope='control_slope', back='back_v2') if VARIANT == 'v2' else {}
def fname(nm, q=None):
    q = QTY.get(nm, 1) if q is None else q
    return os.path.join(OUTDIR, f'fj125_{FILE_TAG.get(nm, nm)}_qty{q}_mm.dxf')
def write_dxf(nm, poly, fn=None):
    poly = affinity.translate(poly, -poly.bounds[0], -poly.bounds[1]); FLAT[nm] = poly
    d = new_doc(); msp = d.modelspace()
    for r in rings(poly):
        pts = [(round(x, 4), round(y, 4)) for x, y in r[:-1]]
        msp.add_lwpolyline(pts, close=True, dxfattribs={'layer': '0'})
    fn = fn or fname(nm); d.saveas(fn); return fn

# ---- retrofit faceplate (v2 only, not part of the v2 assembly): a 1/8" birch copy of the printed Switch Panel
# that screws onto the ALREADY-ORDERED slope (window + 4 x M3), with the same cut-outs as the control slope.
def faceplate():
    fp = box(0, 0, C.PANEL_W, C.PANEL_H).buffer(-3.0, join_style=2).buffer(3.0, 32)   # 3 mm corner radius like the print
    return fp.difference(C.union(corner_holes=True))
FACEPLATE_FILE = os.path.join(ROOT, 'dxf', 'retrofit', 'fj125_faceplate_qty1_mm.dxf')

if __name__ == '__main__':
    os.makedirs(OUTDIR, exist_ok=True)
    for f in os.listdir(OUTDIR):
        if f.endswith('.dxf'): os.remove(os.path.join(OUTDIR, f))
    files = {nm: write_dxf(nm, parts[nm]) for nm in parts}
    if VARIANT == 'v2':
        os.makedirs(os.path.dirname(FACEPLATE_FILE), exist_ok=True)
        files['faceplate'] = write_dxf('faceplate', faceplate(), FACEPLATE_FILE)
    print(json.dumps(dict(VARIANT=VARIANT, T=T, S0=S0, S1=S1, S0_old=S0_OLD, panel_s0=PANEL_S0, panel_y0=PANEL_Y0,
                          slope_tabs=TABS['slope'], dogbones=ndog, files=files), indent=1))
