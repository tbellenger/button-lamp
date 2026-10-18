"""Regenerate everything: python3 build_all.py  (renders need: xvfb-run -a python3 build_all.py)
Change tube size / ply thickness in the CONFIG block below."""
import os as _os, sys as _sys; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from paths import ROOT, PLANS_STL
import sys, os, json, numpy as np, trimesh
sys.path.insert(0, os.path.dirname(__file__))
import box as B, spiral as SP, assemble as AS, outputs as O
from shapely.geometry import LineString, Point, box as sbox

from paths import ROOT
TUBES = {'13mm': 13.0}                   # JARBIH 'for 5MM' = 13 mm OD, B0CKZ2WWJF (fits the 5 mm FCOB strip). Add '16mm': 16.0 for the 10MM tube.
PLY = {'3mm': 3.0, '6mm': 6.0}
BED = (300.0, 300.0, 400.0)              # CR-10
SHEET = (299.0, 299.0)                   # '12 x 12 in' packs are often 11.8 in actual -> plan on 299 mm
report = {}

def side_marks(p, geo):
    t, s, H, xa = p['t'], p['strip'], p['H'], p['xa']
    th, S, e, n = B.derived(p); A = np.array([xa, H])
    zb = t + p['block_h']
    mk = []
    mk.append((LineString([(t, H - t), (xa, H - t)]), 'inside face: top board underside'))
    mk.append((sbox(t, zb, t + s, H - t - s).exterior, 'glue strip'))
    mk.append((sbox(t + s, H - t - s, xa - 10, H - t).exterior, 'glue strip'))
    sa, sb = geo['s0'] + 10, S - 14
    P = [A + sa * e + t * n, A + sb * e + t * n, A + sb * e + (t + s) * n, A + sa * e + (t + s) * n]
    mk.append((LineString(P + [P[0]]), 'glue strip'))
    for bx in (t, p['L'] / 2 - p['block'] / 2, p['L'] - t - p['block']):
        mk.append((sbox(bx, t, bx + p['block'], t + p['block_h']).exterior, 'printed block'))
    mk.append((LineString([(t, 0), (t, H - t)]), 'back board'))
    mk.append((LineString([(p['L'] - t, 0), (p['L'] - t, geo['hfb'])]), 'front board'))
    mk.append((Point(60, 50), 'INSIDE FACE (mirror marks for the other side)'))
    return mk

def mm_in(v): return f'{v:.1f} mm = {O.inch(v)}'

def main(do_render=True):
    os.makedirs(f'{ROOT}/stl', exist_ok=True); os.makedirs(f'{ROOT}/templates', exist_ok=True); os.makedirs(f'{ROOT}/render', exist_ok=True)
    checks = []
    # ---------------- printed parts
    for tk, od in TUBES.items():
        sp = SP.default_params(od)
        cage, info = SP.build(sp)
        cap = SP.end_cap(sp)
        report[f'spiral_{tk}'] = {k: (float(v) if isinstance(v, (float, np.floating)) else v) for k, v in info.items()}
        for nm, m in [(f'spiral_cage_tube{tk}.stl', cage), (f'end_cap_tube{tk}.stl', cap)]:
            m.export(f'{ROOT}/stl/{nm}'); checks.append((nm, m))
    p3 = B.params(3.0); sp13 = SP.default_params(13.0); _, info13 = SP.build(sp13)
    bk = AS.backer_mesh(sp13, info13); bk.export(f'{ROOT}/stl/top_backer_plate.stl'); checks.append(('top_backer_plate.stl', bk))
    for tk, t in PLY.items():
        blk = AS.bottom_block(B.params(t)); nm = 'bottom_block_x6.stl'
        blk.export(f'{ROOT}/stl/{nm}'); checks.append((nm, blk))
    # strip-feed test ring: short clip to test the snap fit before the big print
    for tk, od in TUBES.items():
        sp = SP.default_params(od); test = clip_test(sp); nm = f'clip_fit_test_tube{tk}.stl'
        test.export(f'{ROOT}/stl/{nm}'); checks.append((nm, test))
    stl_report = []
    seen = set()
    for nm, m in checks:
        if nm in seen: continue
        seen.add(nm)
        ext = m.extents
        fits = bool(ext[0] <= BED[0] and ext[1] <= BED[1] and ext[2] <= BED[2])
        stl_report.append(dict(file=nm, watertight=bool(m.is_watertight), volume_cm3=round(m.volume / 1000, 1),
                               approx_PLA_g_solid=round(m.volume / 1000 * 1.24, 0), extents_mm=[round(float(x), 1) for x in ext], fits_CR10=fits,
                               bodies=len(m.split(only_watertight=False))))
    report['stl'] = stl_report
    # ---------------- plywood
    for tk, t in PLY.items():
        p = B.params(t)
        pcs, geo = B.pieces(p, info13, 180.0)
        pcs['side']['marks'] = side_marks(p, geo)
        names = ['side', 'back', 'top', 'slope', 'front', 'bottom']
        d = f'{ROOT}/templates/ply{tk}'; os.makedirs(d, exist_ok=True)
        for n in names: O.piece_svg(n, pcs[n], f'{d}/{n}.svg', f'({tk} ply)')
        O.tiled_pdf([(n, pcs[n]) for n in names], f'{d}/templates_letter_tiled_ply{tk}.pdf', f'Spiral Lamp plywood base - full-size templates ({tk} ply)')
        sheets = O.sheet_layout([(n, pcs[n]) for n in names], SHEET)
        O.sheets_svg(sheets, SHEET, f'{d}/sheet_layout_12x12in.svg', f'{tk} ply')
        cut = []
        for n in names:
            b = pcs[n]['poly'].bounds; w, h = b[2] - b[0], b[3] - b[1]
            cut.append(dict(piece=n, qty=pcs[n]['qty'], a_mm=round(w, 1), b_mm=round(h, 1), a_in=O.inch(w), b_in=O.inch(h), note=pcs[n]['desc']))
        strips = [dict(piece=nm, qty=q, length_mm=round(ln, 1), length_in=O.inch(ln)) for nm, q, ln, _ in B.glue_strips(p)]
        report[f'ply_{tk}'] = dict(cut=cut, strips=strips, sheets_12x12=len(sheets), geo=dict(s0=geo['s0'], slope_len=geo['Ls'], slope_deg=float(np.degrees(geo['th'])), front_h=geo['hfb'], inner_w=geo['Wi'], uno=geo['uno'], usbc=geo['usbc']),
                                    wire_hole_top=pcs['top']['wire'], spiral_c_top=pcs['top']['spiral_c'], panel_box_on_slope=pcs['slope']['panel_box'])
        # interior volume
        prof = B.profile(p); inner = prof.buffer(-t, join_style=2)
        report[f'ply_{tk}']['interior_L'] = round(inner.area * (p['W'] - 2 * t) / 1e6, 2)
    json.dump(report, open(f'{ROOT}/analysis/build_report.json', 'w'), indent=1, default=lambda o: float(o) if isinstance(o, (np.floating,)) else str(o))
    if do_render: renders()
    print(json.dumps(report['stl'], indent=1))

def clip_test(sp):
    """one column with its first two clips + a scrap of flange: quick print to check the snap fit."""
    cage, info = SP.build(sp)
    a = np.radians(sp['col_phase_deg'])
    zt = sp['flange_t'] + 2.2 * sp['pitch']
    bx = trimesh.creation.box([40, 26, zt]); bx.apply_translation([33, 0, zt / 2 - 0.01])
    bx.apply_transform(trimesh.transformations.rotation_matrix(a, [0, 0, 1]))
    return trimesh.boolean.intersection([cage, bx], engine='manifold')

def renders():
    from pvrender import render, cam
    from PIL import Image, ImageDraw, ImageFont
    p = B.params(3.0); sp = SP.default_params(13.0)
    parts, pcs, geo, cage, cinfo, bk = AS.build(p, sp)
    C = AS.COLORS
    items = lambda keys: [(parts[k], C[k]) + ((0.92,) if k == 'tube' else ()) for k in keys]
    allk = list(parts.keys())
    R = f'{ROOT}/render'
    render(items(allk), f'{R}/assembled_front_iso.png', view=cam((1.25, -1, 0.75)), size=(1400, 1300), ortho=False, zoom=1.15)
    render(items(allk), f'{R}/assembled_back_iso.png', view=cam((-1.3, 0.9, 0.55)), size=(1400, 1300), ortho=False, zoom=0.95)
    render(items([k for k in allk if k not in ('side_R', 'top', 'cage', 'tube', 'cap')]), f'{R}/inside_cutaway.png', view=cam((0.5, 1.1, 1.0)), size=(1400, 1200), ortho=False, zoom=1.1)
    # side elevation (ortho)
    render(items(allk), f'{R}/assembled_side_elevation.png', view=cam((0, -1, 0)), size=(1200, 1100), ortho=True)
    # exploded
    off = dict(side_L=(0, -90, 0), side_R=(0, 90, 0), back=(-90, 0, 0), front=(80, 0, -10), top=(0, 0, 70), slope=(70, 0, 70),
               panel=(130, 0, 130), bottom=(0, 0, -110), uno=(0, 0, -70), wago=(0, 0, -70), mt3608=(0, 0, -70), blocks=(0, 0, -35),
               strips=(0, 0, 0), backer=(0, 0, 30), cage=(0, 0, 130), tube=(0, 0, 130), cap=(0, 0, 170))
    ex = []
    for k in allk:
        m = parts[k].copy(); m.apply_translation(off.get(k, (0, 0, 0))); ex.append((m, C[k]) + ((0.92,) if k == 'tube' else ()))
    render(ex, f'{R}/exploded.png', view=cam((1.2, -1.1, 0.7)), size=(1600, 1500), ortho=False, zoom=1.0)
    # spiral holder close-ups (both tube sizes)
    tiles = []
    for tk, od in TUBES.items():
        s = SP.default_params(od); cg, inf = SP.build(s); tb = SP.tube_mesh(s, inf)
        fn1 = f'/tmp/cg_{tk}a.png'; fn2 = f'/tmp/cg_{tk}b.png'
        render([(cg, '#f2a541')], fn1, view=cam((1, -1, 0.35)), size=(700, 900), title=f'cage for {od:.0f} mm tube (as printed)')
        render([(cg, '#f2a541'), (tb, '#ffffff', 0.95)], fn2, view=cam((1, -1, 0.35)), size=(700, 900), title=f'with tube clipped in')
        tiles += [fn1, fn2]
    W = Image.new('RGB', (700 * len(tiles), 900), 'white')
    for i, f in enumerate(tiles): W.paste(Image.open(f), (i * 700, 0))
    W.save(f'{R}/spiral_holder_13mm.png')
    # comparison vs original
    D = PLANS_STL
    en = trimesh.load(D + 'Enclosure.stl'); spo = trimesh.load(D + 'Spiral (with base).stl')
    # original spiral is exported lying along +X; stand it on the enclosure top pocket (centre x=68.3, y=99.06, z=116.84)
    spo.apply_translation([-78.74, 0, 0]); spo.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [0, 1, 0]))
    spo.apply_translation([68.3, 99.06, 116.84 - 5.08])
    pan = trimesh.load(D + 'Switch Panel.stl')
    render([(en, '#c8a26b'), (spo, '#dddddd')], '/tmp/orig_asm.png', view=cam((0, -1, 0)), size=(900, 900), ortho=True, title='original (printed)')
    render(items(allk), '/tmp/new_asm.png', view=cam((0, -1, 0)), size=(900, 900), ortho=True, title='plywood + 13 mm tube')
    W = Image.new('RGB', (1800, 900), 'white'); W.paste(Image.open('/tmp/orig_asm.png'), (0, 0)); W.paste(Image.open('/tmp/new_asm.png'), (900, 0))
    W.save(f'{R}/original_vs_plywood_side.png')
    import cairosvg; cairosvg.svg2png(url=f'{ROOT}/templates/ply3mm/bottom.svg', write_to=f'{R}/bottom_tray_layout.png', output_width=1400)

if __name__ == '__main__':
    main(do_render='--norender' not in sys.argv)
