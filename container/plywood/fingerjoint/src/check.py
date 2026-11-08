"""Geometric verification of the finger-joint casing. Writes check_report.json and prints a summary."""
import os, sys, json, math
import numpy as np, ezdxf, manifold3d as mf, trimesh
from shapely.geometry import Polygon, MultiPolygon, LineString, Point, box
from shapely.ops import unary_union
from shapely import affinity
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fj
from fj import T, CLR, CLR_D, MIN_FEAT, MIN_PART, PLACE, parts, ENV, L, W, H
rep = {'params': dict(T=T, CLR=CLR, CLR_D=CLR_D, DOG_R=fj.DOG_R, MIN_FEAT=MIN_FEAT, MIN_PART=MIN_PART, S0=fj.S0, S1=fj.S1)}
fails = []
def geoms(g):
    if g.is_empty: return []
    if g.geom_type == 'Polygon': return [g]
    return [x for x in g.geoms if x.geom_type == 'Polygon']

# ---------------------------------------------------------------- 1. DXF read-back: closed, simple, matches source
rep['dxf'] = {}
for nm in parts:
    fn = fj.fname(nm)
    d = ezdxf.readfile(fn); ents = list(d.modelspace())
    types = sorted(set(e.dxftype() for e in ents)); layers = sorted(set(e.dxf.layer for e in ents))
    closed = all(e.dxftype() == 'LWPOLYLINE' and e.closed for e in ents)
    rings = [Polygon([(x, y) for x, y, *_ in e.get_points()]) for e in ents]
    simple = all(r.is_valid and r.exterior.is_simple for r in rings)
    # nesting: biggest = outline, the rest must be strictly inside it and disjoint from each other
    rings.sort(key=lambda r: -r.area); outer, inner = rings[0], rings[1:]
    nest_ok = all(outer.contains(r) for r in inner) and all(not a.intersects(b) for i, a in enumerate(inner) for b in inner[i + 1:])
    rebuilt = Polygon(outer.exterior.coords, [r.exterior.coords for r in inner])
    same = abs(rebuilt.area - fj.FLAT[nm].area) < 1e-3 if nm in fj.FLAT else None
    bw, bh = outer.bounds[2] - outer.bounds[0], outer.bounds[3] - outer.bounds[1]
    ok = closed and simple and nest_ok and rebuilt.is_valid and types == ['LWPOLYLINE'] and layers == ['0'] and d.header['$INSUNITS'] == 4
    rep['dxf'][nm] = dict(file=os.path.relpath(fn, fj.ROOT), entities=len(ents), types=types, layers=layers, all_closed=closed, simple=simple, nested_ok=nest_ok,
                          valid=rebuilt.is_valid, bbox_mm=[round(bw, 2), round(bh, 2)], min_outer_dim=round(min(bw, bh), 2),
                          cutouts=len(inner), area_mm2=round(rebuilt.area, 1), ok=ok and min(bw, bh) >= MIN_PART)
    if not rep['dxf'][nm]['ok']: fails.append(f'dxf {nm}')

# ---------------------------------------------------------------- 2. min feature 2.03 mm (material webs + cut gaps)
r = MIN_FEAT / 2 - 1e-3
rep['min_feature'] = {}
for nm, poly in parts.items():
    opened = poly.buffer(-r, join_style=1).buffer(r, join_style=1)
    thin = [g for g in geoms(poly.difference(opened)) if g.area > 0.6]
    closed = poly.buffer(r, join_style=1).buffer(-r, join_style=1)
    narrow = [g for g in geoms(closed.difference(poly)) if g.area > 0.6]
    # smallest round hole
    hd = min([2 * math.sqrt(Polygon(i).area / math.pi) for i in poly.interiors] or [99])
    rep['min_feature'][nm] = dict(thin_webs=len(thin), narrow_cuts=len(narrow), worst_web_area=round(max([g.area for g in thin] or [0]), 2),
                                  smallest_hole_dia=round(hd, 2), ok=not thin and not narrow and hd >= MIN_FEAT)
    if not rep['min_feature'][nm]['ok']: fails.append(f'feature {nm}')

print('stage 3', flush=True)
# ---------------------------------------------------------------- 3. 3D solids
def to_cs(poly):
    cont = []
    for g in geoms(poly):
        cont.append(np.array(g.exterior.coords[:-1])); cont += [np.array(i.coords[:-1]) for i in g.interiors]
    return mf.CrossSection(cont, mf.FillRule.EvenOdd)
SOL, MAT = {}, {}
for inst, (nm, Mx) in PLACE.items():
    SOL[inst] = to_cs(parts[nm]).extrude(T).transform(Mx); MAT[inst] = Mx
for i, (bx, by) in enumerate(fj.BLOCKS):
    SOL[f'block{i}'] = mf.Manifold.cube([20, 20, 18], center=False).translate([bx - 10, by - 10, T])
rep['solids'] = {k: dict(volume_cm3=round(v.volume() / 1000, 2), status=str(v.status())) for k, v in SOL.items()}

print('pairs', flush=True)
# pairwise interference + gaps
names = list(SOL); inter = []; gaps = {}
for i, a in enumerate(names):
    for b in names[i + 1:]:
        v = (SOL[a] ^ SOL[b]).volume()
        g = SOL[a].min_gap(SOL[b], 2.0)
        if v > 1e-3: inter.append((a, b, round(v, 4)))
        if g < 2.0: gaps[f'{a}|{b}'] = round(g, 3)
rep['interference_pairs'] = inter
rep['near_pairs_gap_mm'] = gaps
contact_ok = {k for k in gaps if 'block' in k}
bad_gap = {k: g for k, g in gaps.items() if k not in contact_ok and g < CLR_D - 1e-3}
rep['clearance_violations'] = bad_gap
if inter: fails.append('interference')
if bad_gap: fails.append('clearance')
U = mf.Manifold.batch_boolean([SOL[k] for k in names if not k.startswith('block')], mf.OpType.Add)
bb = U.bounding_box(); rep['assembly_bbox'] = [round(x, 3) for x in bb]
size = [bb[3] - bb[0], bb[4] - bb[1], bb[5] - bb[2]]
rep['assembly_size'] = [round(s, 3) for s in size]
if max(abs(size[0] - L), abs(size[1] - W), abs(size[2] - H)) > 1e-3 or max(abs(x) for x in bb[:3]) > 1e-3: fails.append('bbox')

print('stage 4', flush=True)
# ---------------------------------------------------------------- 4. joint matching in each partner's plane
def footprint(src, dst):
    """cross-section of solid src inside dst's sheet slab, expressed in dst's local 2D frame."""
    Mx = MAT[dst]; R4 = np.eye(4); R4[:3, :] = Mx; Inv = np.linalg.inv(R4)[:3, :]
    slab = mf.Manifold.cube([2000, 2000, T], center=False).translate([-1000, -1000, 0])
    loc = SOL[src].transform(Inv) ^ slab
    if loc.is_empty(): return Polygon()
    polys = loc.project().to_polygons()
    shp = [Polygon(p_) for p_ in polys]
    # even-odd assemble
    out = Polygon()
    for s_ in sorted(shp, key=lambda q: -abs(q.area)): out = out.symmetric_difference(s_.buffer(0))
    return out.buffer(0)
def axes(g):
    mr = g.minimum_rotated_rectangle; cs = np.array(mr.exterior.coords[:4])
    e1, e2 = cs[1] - cs[0], cs[2] - cs[1]
    if np.linalg.norm(e1) < np.linalg.norm(e2): e1, e2 = e2, e1
    return e1 / np.linalg.norm(e1), e2 / np.linalg.norm(e2), np.linalg.norm(e1), np.linalg.norm(e2)
def chord(geom, c, d, ext=400):
    ln = LineString([tuple(c - d * ext), tuple(c + d * ext)]); x = ln.intersection(geom)
    # segment containing c
    segs = [x] if x.geom_type == 'LineString' else list(getattr(x, 'geoms', []))
    for s_ in segs:
        if s_.distance(Point(*c)) < 1e-6: return s_.length
    return 0.0
JOINTS = [('back', 'side_L'), ('back', 'side_R'), ('top', 'side_L'), ('top', 'side_R'), ('slope', 'side_L'), ('slope', 'side_R'),
          ('front', 'side_L'), ('front', 'side_R'), ('back', 'top')]
rep['joints'] = []
for a, b in JOINTS:
    for src, dst in [(a, b), (b, a)]:
        fp = footprint(src, dst); dstpoly = parts[PLACE[dst][0]]; env = ENV[PLACE[dst][0]]
        void = env.difference(Polygon(dstpoly.exterior))
        for g in geoms(fp):
            if g.area < 1.0: continue
            overlap = g.intersection(dstpoly).area
            dist = g.distance(dstpoly)
            inside_env = g.difference(env).area < 1e-3
            row = dict(src=src, dst=dst, area=round(g.area, 1), overlap_mm2=round(overlap, 4), gap_to_material=round(dist, 3), within_outline=inside_env)
            if src in ('back', 'top', 'slope', 'front') and dst.startswith('side') or (src == 'back' and dst == 'top'):
                # this is a TAB going into a SLOT: measure length and depth clearance through its centre
                d1, d2, l1, l2 = axes(g); cc = np.array(g.representative_point().coords[0])
                cc = np.array(g.centroid.coords[0])
                slot = [v for v in geoms(void) if v.intersects(g)]
                sv = slot[0] if slot else Polygon()
                row.update(tab_len=round(l1, 3), slot_len=round(chord(sv, cc, d1), 3), tab_depth=round(l2, 3), slot_depth=round(chord(sv, cc, d2), 3))
                row['len_clear'] = round(row['slot_len'] - row['tab_len'], 3); row['depth_clear'] = round(row['slot_depth'] - row['tab_depth'], 3)
                row['kind'] = 'tab->slot'
                row['ok'] = overlap < 1e-3 and inside_env and abs(row['len_clear'] - CLR) < 0.01 and abs(row['depth_clear'] - CLR_D) < 0.01 and abs(dist - CLR_D) < 0.01
            else:
                row['kind'] = 'finger->gap'
                row['ok'] = overlap < 1e-3 and dist >= CLR_D - 1e-3
            rep['joints'].append(row)
            if not row['ok']: fails.append(f'joint {src}->{dst}')
    # every slot in dst filled? (void components of the outline intersecting the slab line of the partner)
# orphan slots: each side/top notch must contain a tab
orph = []
for dst, srcs in [('side_L', ['back', 'top', 'slope', 'front']), ('top', ['back', 'side_L', 'side_R']), ('back', ['top', 'side_L', 'side_R']), ('slope', ['side_L', 'side_R']), ('front', ['side_L', 'side_R'])]:
    dstpoly = parts[PLACE[dst][0]]; env = ENV[PLACE[dst][0]]
    fps = unary_union([footprint(s_, dst) for s_ in srcs])
    for v in geoms(env.difference(Polygon(dstpoly.exterior))):
        if v.area > 20 and v.intersection(fps).area < 1.0: orph.append((dst, round(v.area, 1), [round(x, 1) for x in v.bounds]))
rep['orphan_slots'] = orph
if orph: fails.append('orphan slots')
ntab = sum(1 for j in rep['joints'] if j['kind'] == 'tab->slot')
rep['tab_count'] = ntab

# ---------------------------------------------------------------- 5. outer-skin coverage (finds unexpected openings)
print('stage 5', flush=True)
def skin(name, Mx, region, d_in=0.4):
    R4 = np.eye(4); R4[:3, :] = Mx; Inv = np.linalg.inv(R4)[:3, :]
    slab = mf.Manifold.cube([2000, 2000, 0.2], center=False).translate([-1000, -1000, d_in - 0.1])
    loc = U.transform(Inv) ^ slab
    cov = Polygon()
    for p_ in sorted([Polygon(q) for q in loc.project().to_polygons()], key=lambda q: -q.area): cov = cov.symmetric_difference(p_.buffer(0))
    cov = cov.buffer(0); openv = region.difference(cov).buffer(-0.2, join_style=2).buffer(0.2, join_style=2)   # ignore <=0.4 mm glue lines
    cl = []
    for g in geoms(openv):
        b_ = g.bounds; w_, h_ = b_[2] - b_[0], b_[3] - b_[1]
        if max(w_, h_) >= 3.0 and g.area > 1.0: cl.append(dict(u0=round(b_[0], 1), v0=round(b_[1], 1), w=round(w_, 1), h=round(h_, 1), area=round(g.area, 1)))
    faces[name] = dict(coverage_pct=round(100 * cov.intersection(region).area / region.area, 2), openings_ge3mm=sorted(cl, key=lambda c: -c['area']))
faces = {}
ex, ez = fj.E; nx_, nz_ = fj.N
skin('back (u=y, v=z)', fj.M((0, 1, 0), (0, 0, 1), (1, 0, 0), (0, 0, 0)), box(0, 0, W, H))
skin('top (u=x, v=y)', fj.M((1, 0, 0), (0, -1, 0), (0, 0, -1), (0, W, H)), box(0, 0, fj.xa, W))
skin('front (u=y, v=z)', fj.M((0, -1, 0), (0, 0, 1), (-1, 0, 0), (L, W, 0)), box(0, 0, W, fj.hF))
skin('bottom (u=x, v=y)', fj.M((1, 0, 0), (0, 1, 0), (0, 0, 1), (0, 0, 0)), box(0, 0, L, W))
skin('slope (u=s, v=y)', fj.M((ex, 0, ez), (0, -1, 0), (nx_, 0, nz_), (fj.A[0], W, fj.A[1])), box(0, 0, fj.S, W))
skin('side_L (u=x, v=z)', fj.M((-1, 0, 0), (0, 0, 1), (0, 1, 0), (L, 0, 0)), affinity.scale(fj.prof, -1, 1, origin=(L/2, 0)))
rep['skin'] = faces

# ---------------------------------------------------------------- 6. v2: control cut-outs + faceplate + DC jack
if fj.VARIANT == 'v2':
    import controls as C
    sl = parts['slope']; outline = Polygon(sl.exterior)
    rows = []
    for n, pin, g in C.cutouts(fj.panel_to_slope):
        inside = outline.buffer(-0.01).contains(g)
        edge = outline.exterior.distance(g)
        rows.append(dict(name=n, pin=pin, inside=inside, edge_mm=round(edge, 2), ok=inside and edge >= 2 * T))
        if not rows[-1]['ok']: fails.append(f'control {n}')
    webs = min(a.distance(b) for i, (_, _, a) in enumerate(C.cutouts(fj.panel_to_slope)) for _, _, b in C.cutouts(fj.panel_to_slope)[i + 1:])
    rep['controls'] = dict(cutouts=rows, min_web_between_cutouts_mm=round(webs, 2), layout_problems=C.check_layout())
    if webs < MIN_FEAT or C.check_layout(): fails.append('control webs')
    # faceplate: closed rings, inside, min feature; and its 4 holes line up with the ORDERED slope's 4 holes
    fp = fj.faceplate(); d = ezdxf.readfile(fj.FACEPLATE_FILE); ents = list(d.modelspace())
    rr = sorted([Polygon([(x, y) for x, y, *_ in e.get_points()]) for e in ents], key=lambda r: -r.area)
    fp_ok = all(e.dxftype() == 'LWPOLYLINE' and e.closed for e in ents) and all(rr[0].contains(r) for r in rr[1:])
    opened = fp.buffer(-(MIN_FEAT / 2 - 1e-3), join_style=1).buffer(MIN_FEAT / 2 - 1e-3, join_style=1)
    thin = [g for g in geoms(fp.difference(opened)) if g.area > 0.6]
    # the ordered slope's corner holes, in panel coords
    holes_old = [Polygon(h.exterior) for h in fj.HOLES_ORDERED_SLOPE if abs(h.area - math.pi * 1.7 ** 2) < 0.5]
    inv = lambda s, y: (y - fj.PANEL_Y0, fj.PANEL_S0 + C.PANEL_H - s)
    match = sorted(round(min(Point(*inv(h.centroid.x, h.centroid.y)).distance(Point(*c)) for c in C.CORNER_HOLES), 3) for h in holes_old)
    rep['faceplate'] = dict(file=os.path.relpath(fj.FACEPLATE_FILE, fj.ROOT), rings=len(ents), closed_nested=fp_ok, thin_webs=len(thin), bbox=[round(v, 2) for v in rr[0].bounds],
                            corner_holes_vs_ordered_slope_mm=match, ok=fp_ok and not thin and len(match) == 4 and max(match) < 0.01)
    if not rep['faceplate']['ok']: fails.append('faceplate')
    bk = parts['back']; jack = Point(fj.DC_JACK['y'], fj.DC_JACK['z']).buffer(fj.DC_JACK['d'] / 2)
    others = [Polygon(r) for r in bk.interiors if not Polygon(r).buffer(0.01).contains(jack.centroid)]
    rep['dc_jack'] = dict(centre_yz=[fj.DC_JACK['y'], fj.DC_JACK['z']], d=fj.DC_JACK['d'], edge_mm=round(Polygon(bk.exterior).exterior.distance(jack), 2),
                          nearest_cutout_mm=round(min(jack.distance(o) for o in others), 2),
                          clear_of_blocks=all(jack.distance(box(by - 10, T, by + 10, T + 18)) > 3 for bx, by in fj.BLOCKS if bx < 30))
    if rep['dc_jack']['edge_mm'] < 2 * T or not rep['dc_jack']['clear_of_blocks']: fails.append('dc jack')
rep['FAILS'] = fails
json.dump(rep, open(os.path.join(fj.ROOT, 'check_report.json' if fj.VARIANT == 'v2' else 'check_report_as_ordered.json'), 'w'), indent=1, default=float)
# ---- export meshes for rendering
STLD = os.path.join(fj.ROOT, 'render', 'stl' if fj.VARIANT == 'v2' else 'stl_as_ordered'); os.makedirs(STLD, exist_ok=True)
for k, v in SOL.items():
    mm = v.to_mesh(); trimesh.Trimesh(np.asarray(mm.vert_properties)[:, :3], np.asarray(mm.tri_verts)).export(os.path.join(STLD, f'{k}.stl'))
print('FAILS:', fails)
if 'controls' in rep: print(json.dumps(dict(controls=rep['controls'], faceplate=rep['faceplate'], dc_jack=rep['dc_jack']), indent=0, default=float))
print('bbox', rep['assembly_size'], 'tabs', ntab, 'interference', inter, 'clearance viol', bad_gap, 'orphans', orph)
for nm, v in rep['dxf'].items(): print(nm, v['bbox_mm'], 'cutouts', v['cutouts'], 'ok', v['ok'], rep['min_feature'][nm])
for j in rep['joints']:
    if j['kind'] == 'tab->slot': print(j['src'], '->', j['dst'], j['tab_len'], j['slot_len'], j['len_clear'], j['depth_clear'], j['gap_to_material'], j['ok'])
print('finger rows', sum(1 for j in rep['joints'] if j['kind'] != 'tab->slot'), 'all ok', all(j['ok'] for j in rep['joints']))
print(json.dumps(gaps, indent=0))
for k, v in faces.items(): print(k, v['coverage_pct'], [(c['u0'], c['v0'], c['w'], c['h']) for c in v['openings_ge3mm']][:12])
