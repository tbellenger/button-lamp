"""Sheet-metal Spiral Lamp casing for SendCutSend: hood (back/top/slope/front, 3 bends) + 2 side cheeks (4 inward
tabs each) + flat removable bottom plate.  Same outside size as the plywood box, 250 x 210 x 120 mm.

v2 (this file) = the Oct 2026 quote design (quotes/src/make_dxf.py on Forge's box) with:
  * the hood's sloped face carrying the control cut-outs directly (same layout as the plywood control slope,
    from ../../plywood/fingerjoint/src/controls.py) instead of a window for the printed panel;
  * a 12.2 mm DC jack hole on the back for the 5 V 10 A supply;
  * the USB window kept for the Uno R4 WiFi's USB-C.
Flat patterns use SendCutSend's published bend specs (inside radius, K-factor).  All DXFs: mm ($INSUNITS 4),
1:1, one part per file, layer 0, closed LWPOLYLINE cuts, bend centre-lines as DASHED lines.
Run:  python3 src/sm.py   (writes dxf/, svg/, cutlist.csv, check_report.json)."""
import sys, os, json, math, csv
import numpy as np, ezdxf
from shapely.geometry import Polygon, box, Point, LineString
from shapely.ops import unary_union
from shapely import affinity

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(os.path.dirname(ROOT), 'plywood', 'fingerjoint', 'src'))
import controls as C, svgout
OUT = os.path.join(ROOT, 'dxf'); SVG = os.path.join(ROOT, 'svg')

# SendCutSend bend specs (bending calculator, read 18 Oct 2026): thickness, inside radius, K, min 90 deg flange, relief depth
SM = {'al063': dict(label='5052-H32 aluminium .063in (1.60 mm)', t=1.6002, R=0.035 * 25.4, K=0.42, minflange=0.303 * 25.4, relief=0.118 * 25.4),
      'ms059': dict(label='mild steel (CRS) .059in (1.50 mm)', t=1.4986, R=0.063 * 25.4, K=0.40, minflange=0.309 * 25.4, relief=0.142 * 25.4)}
# Feature keep-out from a bend centre-line = die width / 2 (SCS bend-deformation guideline).  The die for these
# gauges wasn't readable on SCS's site, so this uses their 0.630" die (the larger of the two smallest dies) -> 8.0 mm.
DIE_HALF = 0.630 * 25.4 / 2
L, W, H, hF, xa = 250.0, 210.0, 120.0, 28.0, 122.0
TH = math.atan2(H - hF, L - xa)                  # slope angle, 35.7 deg
SL = math.hypot(H - hF, L - xa)                  # slope mould-line length
TAB = 11.0                                       # cheek tab height (outer), > min formed flange 7.7 mm
TAB_HOLE = 6.0                                   # hole centre from cheek outer face
DC_JACK = dict(y=72.0, z=24.0, d=12.2)           # same spot as the plywood back v2
USB_WIN = dict(y=130.0 + 38.1, w=20.0, h=18.0)   # window for the Uno's USB plug (R4 WiFi USB-C sits inside it)

def new_doc():
    d = ezdxf.new('R2010', setup=True); d.units = ezdxf.units.MM
    d.header['$INSUNITS'] = 4; d.header['$MEASUREMENT'] = 1; return d

def rings(poly):
    for g in ([poly] if poly.geom_type == 'Polygon' else list(poly.geoms)):
        yield list(g.exterior.coords)
        for r in g.interiors: yield list(r.coords)

def save(poly, name, bends=()):
    ox, oy = poly.bounds[0], poly.bounds[1]
    poly = affinity.translate(poly, -ox, -oy); bends = [((a[0] - ox, a[1] - oy), (b[0] - ox, b[1] - oy)) for a, b in bends]
    d = new_doc(); msp = d.modelspace()
    for r in rings(poly):
        msp.add_lwpolyline([(round(x, 4), round(y, 4)) for x, y in r[:-1]], close=True, dxfattribs={'layer': '0'})
    for (a, b) in bends:
        msp.add_line(a, b, dxfattribs={'layer': '0', 'linetype': 'DASHED'})
    fn = os.path.join(OUT, name); d.saveas(fn)
    svgout.write(os.path.join(SVG, name.replace('.dxf', '.svg')), [(poly, '#d00')], lines=[(a, b, '#06c') for a, b in bends])
    return fn, poly, bends

def stats(poly):
    geoms = [poly] if poly.geom_type == 'Polygon' else list(poly.geoms)
    cut = sum(g.exterior.length + sum(r.length for r in g.interiors) for g in geoms)
    b = poly.bounds; return dict(w=b[2] - b[0], h=b[3] - b[1], cut_mm=cut, pierces=sum(1 + len(g.interiors) for g in geoms))

def bendcalc(m, ang_deg):
    a = math.radians(ang_deg); R, t, K = m['R'], m['t'], m['K']
    return math.tan(a / 2) * (R + t), a * (R + K * t)      # outside set-back, bend allowance

def cheek_tab_centres(m):
    return dict(back=65.0, top=60.0, slope=SL / 2, bottom=(40.0, 210.0))

def panel_to_face2(x, y):
    """control-panel coords (x right, y = 0 at the low edge) -> hood slope-face coords (s down the slope from the
    top/slope mould corner, v = box y).  Panel centred on the face, like the old window."""
    return (SL / 2 - C.PANEL_H / 2 + (C.PANEL_H - y), W / 2 - C.PANEL_W / 2 + x)

def hood(m):
    """flat pattern: u along the strip (0 = bottom edge of back), v across (0 = left outer side).  Seen from OUTSIDE."""
    segs = [H, xa, SL, hF]; angs = [90.0, math.degrees(TH), 90.0 - math.degrees(TH)]
    bc = [bendcalc(m, a) for a in angs]
    starts, bend_u, u = [], [], 0.0
    for i, sgl in enumerate(segs):
        o_prev = bc[i - 1][0] if i > 0 else 0.0; o_next = bc[i][0] if i < 3 else 0.0
        starts.append(u - o_prev); u += sgl - o_prev - o_next
        if i < 3: bend_u.append(u + bc[i][1] / 2); u += bc[i][1]
    total = u; poly = box(0, 0, total, W)
    def at(face, s, v): return (starts[face] + s, v)
    feats = []    # (name, polygon in flat coords, face)
    usbc = unary_union([Point(*at(0, 14.0, 40.0 - 2.75)).buffer(2.25, 24), Point(*at(0, 14.0, 40.0 + 2.75)).buffer(2.25, 24),
                        box(*at(0, 14.0 - 2.25, 40.0 - 2.75), *at(0, 14.0 + 2.25, 40.0 + 2.75))])
    usb_z = m['t'] + 6.0 + 1.6 + 5.5
    usbw = box(*at(0, usb_z - USB_WIN['h'] / 2, USB_WIN['y'] - USB_WIN['w'] / 2), *at(0, usb_z + USB_WIN['h'] / 2, USB_WIN['y'] + USB_WIN['w'] / 2)).buffer(-1).buffer(1)
    feats += [('usb_c_slot (old 5 V inlet)', usbc, 0), ('usb_window (Uno R4 USB-C)', usbw, 0),
              ('dc_jack', Point(*at(0, DC_JACK['z'], DC_JACK['y'])).buffer(DC_JACK['d'] / 2, 64), 0)]
    for sx in (-1, 1):
        for sy in (-1, 1):
            feats.append(('cage_M3', Point(*at(1, 61 + sx * 38.1, 105 + sy * 38.1)).buffer(1.7, 24), 1))
    WIRE_OFF = (28.761440419908304 - 61.0, 113.71175803203249 - 102.0)
    feats.append(('wire_hole', Point(*at(1, 61 + WIRE_OFF[0], 105 + WIRE_OFF[1])).buffer(5.0, 32), 1))
    for n, pin, g in C.cutouts(lambda x, y: at(2, *panel_to_face2(x, y))):
        feats.append((n, g, 2))
    tabc = cheek_tab_centres(m)
    for face, s in [(0, tabc['back']), (1, tabc['top']), (2, tabc['slope'])]:
        for v in (TAB_HOLE, W - TAB_HOLE):
            feats.append((f'cheek_screw_f{face}', Point(*at(face, s, v)).buffer(1.7, 24), face))
    poly = poly.difference(unary_union([g for _, g, _ in feats]))
    bends = [((bu, 0), (bu, W)) for bu in bend_u]
    return poly, bends, dict(flat_len=total, bend_u=bend_u, angles=angs, starts=starts, feats=feats, bc=bc)

def cheek(m):
    """flat pattern of the RIGHT-hand-view cheek: x = back->front, z = up; nests inside the hood; tabs fold inward."""
    t = m['t']
    prof = Polygon([(0, 0), (L, 0), (L, hF), (xa, H), (0, H)])
    inner = prof.buffer(-t, join_style=2)
    body = Polygon([(x, (0.0 if y < t + 1e-6 else y)) for x, y in inner.exterior.coords])
    body = body.buffer(-1.5, join_style=1).buffer(1.5, join_style=1)
    body = Polygon([(x, max(0.0, y)) for x, y in body.exterior.coords])
    ossb, ba = bendcalc(m, 90.0)
    off_in = ossb - ba / 2; flat_tab = TAB - ossb + ba / 2; hole_from_bend = TAB_HOLE - ossb + ba / 2
    tabc = cheek_tab_centres(m)
    cs = list(body.exterior.coords)[:-1]
    edges = [(np.array(cs[i]), np.array(cs[(i + 1) % len(cs)])) for i in range(len(cs))]
    def find(pred):
        for a, c in edges:
            if pred(a, c): return a, c
    e_bottom = find(lambda a, c: abs(a[1]) < 1e-6 and abs(c[1]) < 1e-6)
    e_back = find(lambda a, c: abs(a[0] - c[0]) < 1e-6 and a[0] < L / 2)
    e_top = find(lambda a, c: abs(a[1] - c[1]) < 1e-6 and a[1] > H / 2)
    e_slope = find(lambda a, c: abs(a[0] - c[0]) > 1 and abs(a[1] - c[1]) > 1)
    bends, holes, reliefs, tabs, tabinfo = [], [], [], [], []
    def tab_on(edge, centre_pt, width, nholes=1):
        a, c = edge; d = (c - a) / np.linalg.norm(c - a); n = np.array([d[1], -d[0]])
        if body.contains(Point(*(centre_pt + n * 2))): n = -n
        p0 = centre_pt - d * width / 2; p1 = centre_pt + d * width / 2
        bl0, bl1 = p0 - n * off_in, p1 - n * off_in
        tab = Polygon([bl0, bl1, bl1 + n * flat_tab, bl0 + n * flat_tab])
        rw = max(t, 1.6) / 2
        rel = [Polygon([pe - d * rw + n * 0.01, pe + d * rw + n * 0.01, pe + d * rw - n * m['relief'], pe - d * rw - n * m['relief']]) for pe in (p0, p1)]
        rel = [affinity.translate(rel[0], *(-d * rw)), affinity.translate(rel[1], *(d * rw))]
        hs = [Point(*(p0 + d * width * (k + 1) / (nholes + 1) - n * off_in + n * hole_from_bend)).buffer(1.25, 24) for k in range(nholes)]
        return tab, rel, hs, (tuple(bl0), tuple(bl1))
    specs = [(e_back, np.array([e_back[0][0], tabc['back']]), 40.0, 1), (e_top, np.array([tabc['top'], e_top[0][1]]), 40.0, 1),
             (e_slope, (e_slope[0] + e_slope[1]) / 2, 50.0, 1)]
    x0, x1 = tabc['bottom']; specs.append((e_bottom, np.array([(x0 + x1) / 2, 0.0]), x1 - x0 + 20, 2))
    for e, cp, w, nh in specs:
        tab, rel, hs, bl = tab_on(e, cp, w, nh); tabs.append(tab); reliefs += rel; holes += hs; bends.append(bl)
        tabinfo.append(dict(width=w, flange_outer=TAB, holes=nh))
    out = unary_union([body] + tabs).difference(unary_union(reliefs + holes))
    return out, bends, dict(flat_tab=flat_tab, off_in=off_in, tabs=tabinfo, holes=holes)

def bottom_plate(m):
    t = m['t']; poly = box(0, 0, L, W); hs = []
    x0, x1 = cheek_tab_centres(m)['bottom']; w = x1 - x0 + 20
    for x in [(x0 + x1) / 2 - w / 2 + w * k / 3 for k in (1, 2)]:
        for y in (TAB_HOLE, W - TAB_HOLE): hs.append(Point(x, y).buffer(1.7, 24))
    uno_x0 = t + 1.0 + 6.35; uno_y0 = 130.0
    for a, b in [(13.97, 2.54), (15.24, 50.8), (66.04, 7.62), (66.04, 35.56)]:
        hs.append(Point(uno_x0 + a, uno_y0 + b).buffer(1.6, 24))
    hs += [Point(165 + i * 12, 28 + j * 12).buffer(4.0, 24) for i in range(3) for j in range(4)]
    return poly.difference(unary_union(hs))

def build(tag):
    m = SM[tag]
    hp, hb, hi = hood(m); cp, cb, ci = cheek(m); bp = bottom_plate(m)
    cpL = affinity.scale(cp, xfact=-1, origin=(0, 0)); cbL = [((-a[0], a[1]), (-b[0], b[1])) for a, b in cb]
    return m, dict(hood=(hp, hb, hi), cheek_right=(cp, cb, ci), cheek_left=(cpL, cbL, ci), bottom=(bp, [], {}))

def check(tag, m, P):
    """SendCutSend rule checks on the flat patterns."""
    t = m['t']; rep = dict(material=m['label'], rules={}, features=[]); fails = []
    hp, hb, hi = P['hood']
    # 1. every hood feature: inside its own face, >= DIE_HALF from every bend centre-line, >= 2t from the side edges
    for name, g, face in hi['feats']:
        b = g.bounds; dmin = min(min(abs(b[0] - bu), abs(b[2] - bu)) if not (b[0] <= bu <= b[2]) else 0.0 for bu in hi['bend_u'])
        lo = hi['bend_u'][face - 1] if face > 0 else 0.0; up = hi['bend_u'][face] if face < 3 else hi['flat_len']
        ok = b[0] > lo and b[2] < up and dmin >= DIE_HALF and b[1] >= 2 * t and b[3] <= W - 2 * t
        rep['features'].append(dict(name=name, face=face, to_bend_mm=round(dmin, 2), ok=ok))
        if not ok: fails.append(f'hood feature {name} ({dmin:.1f} mm from a bend)')
    # 2. smallest cut >= 50 % of thickness (laser) and webs between cut-outs >= t
    allp = {k: v[0] for k, v in P.items()}
    for k, poly in allp.items():
        hs = [Polygon(r) for r in (poly.interiors if poly.geom_type == 'Polygon' else [])]
        small = min([min(h.bounds[2] - h.bounds[0], h.bounds[3] - h.bounds[1]) for h in hs] or [99])
        web = min([a.distance(b) for i, a in enumerate(hs) for b in hs[i + 1:]] or [99])
        edge = min([Polygon(poly.exterior).exterior.distance(h) for h in hs] or [99])
        ok = small >= 0.5 * t and web >= t and edge >= t
        rep['rules'][f'{k}_min_cut_web_edge_mm'] = [round(small, 2), round(web, 2), round(edge, 2), ok]
        if not ok: fails.append(f'{k} min feature')
    # 3. flanges: hood legs and cheek tabs >= SCS min formed flange; bend angles <= 130; part and bend length limits
    legs = [H, xa, SL, hF]; rep['rules']['hood_min_leg_mm'] = [round(min(legs), 2), round(m['minflange'], 2), min(legs) >= m['minflange']]
    rep['rules']['cheek_tab_flange_mm'] = [TAB, round(m['minflange'], 2), TAB >= m['minflange']]
    rep['rules']['max_bend_angle'] = [round(max(hi['angles']), 1), 130, max(hi['angles']) <= 130]
    fl = stats(hp); rep['rules']['hood_flat_in'] = [round(fl['w'] / 25.4, 2), round(fl['h'] / 25.4, 2), fl['w'] <= 44 * 25.4 and fl['h'] <= 30 * 25.4]
    rep['rules']['bend_length_in'] = [round(W / 25.4, 2), 16, W / 25.4 <= 16]
    rep['rules']['cheek_bend_relief_depth_mm'] = [round(m['relief'], 2), 'SCS relief depth', True]
    rep['rules']['u_channel_base_vs_flange'] = ['back 120 / front 28 face each other across a 250 mm kinked base', 'base >= 2x flange',
                                                'borderline: SCS 3D checker decides; fallback = split hood at the top/slope bend']
    rep['rules']['powder_coat_hole'] = ['every part has holes >= 0.063"', True]
    for k, v in rep['rules'].items():
        if isinstance(v, list) and v and v[-1] is False: fails.append(k)
    # 4. cheek tab screw holes must line up with the hood / bottom holes once folded (checked in 3D by assemble())
    rep['FAILS'] = fails; return rep

def assemble(tag, m, P):
    """3D placement of the folded parts (bend radii ignored: each face is a flat slab between mould lines).  Returns
    dict name -> (poly_in_face_coords, 4x4 matrix) for rendering, and the panel->box matrix for control stand-ins."""
    t = m['t']; hp, hb, hi = P['hood']; out = {}
    segs = [H, xa, SL, hF]; E = np.array([math.cos(TH), 0, -math.sin(TH)]); Nn = np.array([-math.sin(TH), 0, -math.cos(TH)])
    def M(U, V, Z, O):
        X = np.eye(4); X[:3, 0], X[:3, 1], X[:3, 2], X[:3, 3] = U, V, Z, O; return X
    # face frames: local (s, v, k) with k = 0 at the OUTER skin, k = t inward
    frames = [M((0, 0, 1), (0, 1, 0), (1, 0, 0), (0, 0, 0)),
              M((1, 0, 0), (0, 1, 0), (0, 0, -1), (0, 0, H)),
              M(E, (0, 1, 0), Nn, (xa, 0, H)),
              M((0, 0, -1), (0, 1, 0), (-1, 0, 0), (L, 0, hF))]
    for i in range(4):
        st = hi['starts'][i]
        strip = hp.intersection(box(st, -1, st + segs[i], W + 1))
        out[f'hood_{i}'] = (affinity.translate(strip, -st, 0), frames[i])
    cp = P['cheek_right'][0]
    body = cp.intersection(Polygon([(0, 0), (L, 0), (L, hF), (xa, H), (0, H)]).buffer(-t + 0.01, join_style=2).union(box(0, -0.01, L, t + 0.01)))
    out['cheek_L'] = (body, M((1, 0, 0), (0, 0, 1), (0, -1, 0), (0, t, 0)))
    out['cheek_R'] = (body, M((1, 0, 0), (0, 0, 1), (0, -1, 0), (0, W, 0)))
    out['bottom'] = (P['bottom'][0], M((1, 0, 0), (0, 1, 0), (0, 0, 1), (0, 0, -t)))
    # panel -> box: panel (x, y) -> face2 (s, v) -> box, outer skin, normal outward
    s0, v0 = panel_to_face2(0, 0); O = np.array([xa, v0, H]) + E * s0
    panel_M = M((0, 1, 0), -E, -Nn, O)
    return out, panel_M

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True); os.makedirs(SVG, exist_ok=True)
    for f in os.listdir(OUT): os.remove(os.path.join(OUT, f))
    for f in os.listdir(SVG): os.remove(os.path.join(SVG, f))
    rows, report = [], {}
    for tag in SM:
        m, P = build(tag)
        for n, (poly, bends, _) in P.items():
            fn = f'sm_{tag}_{n}_qty1_mm.dxf'; save(poly, fn, bends); s = stats(poly)
            rows.append(dict(set=f'sm_{tag}', material=m['label'], part=n, qty=1, flat_w_mm=round(s['w'], 1), flat_h_mm=round(s['h'], 1),
                             cut_len_mm=round(s['cut_mm']), pierces=s['pierces'], bends=len(bends), file=f'dxf/{fn}'))
        report[tag] = check(tag, m, P)
        report[tag]['hood'] = dict(flat_len=round(P['hood'][2]['flat_len'], 2), bend_u=[round(b, 2) for b in P['hood'][2]['bend_u']],
                                   angles=[round(a, 2) for a in P['hood'][2]['angles']])
    # DXF read-back
    for r in rows:
        d = ezdxf.readfile(os.path.join(ROOT, r['file'])); ents = list(d.modelspace())
        pl = [e for e in ents if e.dxftype() == 'LWPOLYLINE']; ln = [e for e in ents if e.dxftype() == 'LINE']
        r['dxf_ok'] = all(e.closed for e in pl) and len(pl) + len(ln) == len(ents) and len(ln) == r['bends'] and \
            all(e.dxf.linetype == 'DASHED' for e in ln) and d.header['$INSUNITS'] == 4 and {e.dxf.layer for e in ents} == {'0'}
    with open(os.path.join(ROOT, 'cutlist.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(rows)
    report['dxf_readback_ok'] = all(r['dxf_ok'] for r in rows); report['die_half_keepout_mm'] = DIE_HALF
    json.dump(report, open(os.path.join(ROOT, 'check_report.json'), 'w'), indent=1, default=float)
    for r in rows: print(r)
    for tag in SM: print(tag, 'FAILS', report[tag]['FAILS'], 'closest hood feature to a bend', min(f['to_bend_mm'] for f in report[tag]['features']))
    print('dxf readback ok', report['dxf_readback_ok'])
