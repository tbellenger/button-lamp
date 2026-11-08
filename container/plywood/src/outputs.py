"""SVG templates, tiled Letter PDF, sheet layouts and cut list."""
import numpy as np
from fractions import Fraction
from shapely.geometry import Polygon, MultiPolygon, LineString, LinearRing, Point
from shapely import affinity
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas as rlcanvas

def inch(v):
    x = v / 25.4
    f = Fraction(round(x * 32), 32)
    whole = f.numerator // f.denominator; rem = f - whole
    s = (f"{whole}" if whole else "") + ((" " if whole else "") + f"{rem.numerator}/{rem.denominator}" if rem else "")
    return f'{s or "0"}" ({x:.3f}")'

def _rings(g):
    if isinstance(g, (Polygon,)):
        yield np.array(g.exterior.coords), 'cut'
        for r in g.interiors: yield np.array(r.coords), 'cut'
    elif isinstance(g, MultiPolygon):
        for q in g.geoms: yield from _rings(q)

DRILL_IN = {3.2: '1/8"', 3.4: '9/64"', 4.5: '11/64"', 6.0: '15/64"', 8.0: '5/16"', 10.0: '25/64"'}
def _din(d):
    k = min(DRILL_IN, key=lambda v: abs(v - d)); return DRILL_IN[k] if abs(k - d) < 0.3 else inch(d).split(' (')[0]

def cut_features(poly):
    """Classify every inside cutout for hand cutting.
    hole   -> plain drilled hole (centre crosshair, drill size)
    slot   -> narrow slot: drill both ends at slot width, saw/file between
    cutout -> saw-out window: hatched waste + 8 mm blade-entry holes in the corners"""
    polys = [poly] if isinstance(poly, Polygon) else list(poly.geoms)
    feats = []
    for P in polys:
        for r in P.interiors:
            h = Polygon(r); mrr = h.minimum_rotated_rectangle
            xs, ys = mrr.exterior.coords.xy
            e = sorted([np.hypot(xs[i+1]-xs[i], ys[i+1]-ys[i]) for i in range(2)])
            c = h.centroid
            if e[1] <= 12.5 and e[1] / max(e[0], 1e-6) < 1.15:
                d = round(2 * np.sqrt(h.area / np.pi), 1)
                feats.append(('hole', (c.x, c.y), d))
            elif e[0] < 9:
                pts = list(zip(xs, ys))[:4]
                # long axis end centres
                L = [((pts[i][0]+pts[(i+1)%4][0])/2, (pts[i][1]+pts[(i+1)%4][1])/2) for i in range(4)]
                lens = [np.hypot(pts[(i+1)%4][0]-pts[i][0], pts[(i+1)%4][1]-pts[i][1]) for i in range(4)]
                ends = [L[i] for i in range(4) if abs(lens[i] - e[0]) < 1e-6][:2]
                ux = np.subtract(ends[1], ends[0]); ux = ux / np.linalg.norm(ux) * e[0] / 2
                ends = [tuple(np.add(ends[0], ux)), tuple(np.subtract(ends[1], ux))]
                feats.append(('slot', ends, round(e[0], 1)))
            else:
                ins = h.buffer(-4.5, join_style=2)
                if e[0] < 30:
                    c0 = h.representative_point(); feats.append(('cutout', h, [(c0.x, c0.y)])); continue
                cand = list(ins.exterior.coords)[:-1] if not ins.is_empty and isinstance(ins, Polygon) else [(c.x, c.y)]
                minx, miny, maxx, maxy = h.bounds; picks = []
                for cx, cy in [(minx, miny), (maxx, miny), (maxx, maxy), (minx, maxy)]:
                    q = min(cand, key=lambda p: np.hypot(p[0]-cx, p[1]-cy))
                    if q not in picks: picks.append(q)
                feats.append(('cutout', h, picks))
    # one label per cluster of identical holes
    holes = [f for f in feats if f[0] == 'hole']; done = set(); out = [f for f in feats if f[0] != 'hole']
    for i, f in enumerate(holes):
        if i in done: continue
        grp = [i]; k = 0
        while k < len(grp):
            for j, g in enumerate(holes):
                if j not in grp and g[2] == f[2] and np.hypot(g[1][0]-holes[grp[k]][1][0], g[1][1]-holes[grp[k]][1][1]) < 20: grp.append(j)
            k += 1
        done |= set(grp)
        lab = max(grp, key=lambda j: holes[j][1][0] + holes[j][1][1])
        for j in grp: out.append(('hole', holes[j][1], holes[j][2], len(grp) if j == lab else 0))
    return out

def _feat_svg(feats, T):
    o = []
    for f in feats:
        if f[0] == 'hole':
            (x, y), d = T(*f[1]), f[2]; a = d / 2 + 2.5
            o.append(f'<path d="M {x-a:.2f} {y:.2f} H {x+a:.2f} M {x:.2f} {y-a:.2f} V {y+a:.2f}" stroke="#c01c28" stroke-width="0.25"/>')
            if f[3]: o.append(f'<text x="{x+a+0.5:.2f}" y="{y-1:.2f}" font-size="2.6" fill="#c01c28" font-family="sans-serif">drill {d:g} ({_din(d)}){" x%d" % f[3] if f[3] > 1 else ""}</text>')
        elif f[0] == 'slot':
            for p in f[1]:
                x, y = T(*p); o.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{f[2]/2:.2f}" fill="none" stroke="#c01c28" stroke-width="0.3"/><circle cx="{x:.2f}" cy="{y:.2f}" r="0.5" fill="#c01c28"/>')
            x, y = T(*f[1][1]); o.append(f'<text x="{x+f[2]+1.5:.2f}" y="{y+1:.2f}" font-size="2.6" fill="#c01c28" font-family="sans-serif">SLOT: drill {f[2]:g} ({_din(f[2])}) at both ends, saw/file between</text>')
        else:
            h, picks = f[1], f[2]
            d = 'M ' + ' L '.join(f'{T(x, y)[0]:.3f},{T(x, y)[1]:.3f}' for x, y in h.exterior.coords) + ' Z'
            o.append(f'<path d="{d}" fill="url(#waste)" stroke="none"/>')
            for p in picks:
                x, y = T(*p); o.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="4" fill="white" stroke="#c01c28" stroke-width="0.35"/><path d="M {x-2:.2f} {y:.2f} H {x+2:.2f} M {x:.2f} {y-2:.2f} V {y+2:.2f}" stroke="#c01c28" stroke-width="0.25"/>')
            c = h.representative_point(); x, y = T(c.x, c.y)
            if len(picks) == 1:
                bx, by = T(h.bounds[0], h.bounds[1]); o.append(f'<text x="{bx:.2f}" y="{by+4:.2f}" font-size="2.6" fill="#c01c28" font-family="sans-serif">WASTE: drill 8 mm (5/16") entry hole, saw out</text>'); continue
            o.append(f'<text x="{x:.2f}" y="{y:.2f}" font-size="3.4" fill="#c01c28" font-family="sans-serif" text-anchor="middle" font-weight="bold">WASTE</text>')
            o.append(f'<text x="{x:.2f}" y="{y+4.5:.2f}" font-size="2.6" fill="#c01c28" font-family="sans-serif" text-anchor="middle">drill 8 mm (5/16") entry holes in the red circles, thread blade, saw out</text>')
    return o

def _feat_pdf(c, feats, ox, oy):
    c.setStrokeColorRGB(0.75, 0.11, 0.16); c.setFillColorRGB(0.75, 0.11, 0.16); c.setDash()
    P = lambda x, y: ((x + ox) * mm, (y + oy) * mm)
    for f in feats:
        if f[0] == 'hole':
            (x, y), d = P(*f[1]), f[2]; a = (d / 2 + 2.5) * mm; c.setLineWidth(0.3)
            c.line(x - a, y, x + a, y); c.line(x, y - a, x, y + a)
            if f[3]: c.setFont('Helvetica', 6); c.drawString(x + a + 1, y + 2, f'drill {d:g} ({_din(d)})' + (f' x{f[3]}' if f[3] > 1 else ''))
        elif f[0] == 'slot':
            c.setLineWidth(0.4)
            for p in f[1]:
                x, y = P(*p); c.circle(x, y, f[2] / 2 * mm, stroke=1, fill=0); c.circle(x, y, 0.5 * mm, stroke=0, fill=1)
            x, y = P(*f[1][1]); c.setFont('Helvetica', 6); c.drawString(x + (f[2] + 1.5) * mm, y - 2, f'SLOT: drill {f[2]:g} ({_din(f[2])}) both ends, saw/file between')
        else:
            h, picks = f[1], f[2]
            c.saveState(); pth = c.beginPath(); x, y = P(*h.exterior.coords[0]); pth.moveTo(x, y)
            for q in h.exterior.coords[1:]: pth.lineTo(*P(*q))
            pth.close(); c.clipPath(pth, stroke=0); c.setLineWidth(0.2); c.setStrokeColorRGB(0.8, 0.8, 0.8)
            minx, miny, maxx, maxy = h.bounds
            for k in np.arange(minx - (maxy - miny), maxx, 4):
                c.line(*P(k, miny), *P(k + (maxy - miny), maxy))
            c.restoreState(); c.setStrokeColorRGB(0.75, 0.11, 0.16); c.setFillColorRGB(0.75, 0.11, 0.16); c.setLineWidth(0.45)
            for p in picks:
                x, y = P(*p); c.circle(x, y, 4 * mm, stroke=1, fill=0); c.setLineWidth(0.3); c.line(x - 2*mm, y, x + 2*mm, y); c.line(x, y - 2*mm, x, y + 2*mm); c.setLineWidth(0.45)
            r = h.representative_point(); x, y = P(r.x, r.y)
            if len(picks) == 1:
                c.setFont('Helvetica', 6); c.drawString(*P(h.bounds[0], h.bounds[1] - 4), 'WASTE: drill 8 mm (5/16") entry hole, saw out'); continue
            c.setFont('Helvetica-Bold', 9); c.drawCentredString(x, y, 'WASTE')
            c.setFont('Helvetica', 6); c.drawCentredString(x, y - 9, 'drill 8 mm (5/16") entry holes in red circles, thread blade, saw out')
    c.setFillColorRGB(0, 0, 0); c.setStrokeColorRGB(0, 0, 0)

def piece_svg(name, pc, path, title_extra=''):
    poly = pc['poly']; minx, miny, maxx, maxy = poly.bounds
    pad = 22
    W = maxx - minx + 2 * pad + 40; H = maxy - miny + 2 * pad + 22
    def T(x, y): return (x - minx + pad, (maxy - y) + pad + 18)   # flip y so +v is up
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H:.2f}mm" viewBox="0 0 {W:.2f} {H:.2f}">',
           '<defs><pattern id="waste" width="4" height="4" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><path d="M 0 0 V 4" stroke="#bbbbbb" stroke-width="0.5"/></pattern></defs>',
           '<rect width="100%" height="100%" fill="white"/>']
    feats = cut_features(poly)
    out += _feat_svg(feats, T)
    for ring, kind in _rings(poly):
        d = 'M ' + ' L '.join(f'{T(x, y)[0]:.3f},{T(x, y)[1]:.3f}' for x, y in ring) + ' Z'
        out.append(f'<path d="{d}" fill="none" stroke="black" stroke-width="0.6"/>')
    for g, lab in pc.get('marks', []):
        if isinstance(g, Point):
            x, y = T(*g.coords[0]); out.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="0.8" fill="#1a5fb4"/>')
            out.append(f'<text x="{x + 2:.2f}" y="{y - 2:.2f}" font-size="3" fill="#1a5fb4" font-family="sans-serif">{lab}</text>')
        else:
            coords = np.array(g.coords)
            d = 'M ' + ' L '.join(f'{T(x, y)[0]:.3f},{T(x, y)[1]:.3f}' for x, y in coords)
            out.append(f'<path d="{d}" fill="none" stroke="#1a5fb4" stroke-width="0.25" stroke-dasharray="2,1.5"/>')
            x, y = T(*coords[0]); out.append(f'<text x="{x + 1:.2f}" y="{y - 1.5:.2f}" font-size="2.8" fill="#1a5fb4" font-family="sans-serif">{lab}</text>')
    w, h = maxx - minx, maxy - miny
    out.append(f'<text x="{pad}" y="7" font-size="4.2" font-family="sans-serif" font-weight="bold">{name.upper()} x{pc["qty"]}  {w:.1f} x {h:.1f} mm  {title_extra}</text>')
    out.append(f'<text x="{pad}" y="12" font-size="3" font-family="sans-serif">{pc["desc"]}  </text>')
    out.append(f'<text x="{pad}" y="16.5" font-size="2.8" font-family="sans-serif">Black = cut line (saw on the waste side, sand to the line). Red = drill first. Hatched = waste. Blue dashed = layout marks only.</text>')
    # scale bar 50 mm
    sx, sy = pad, H - 5
    out.append(f'<path d="M {sx} {sy} h 50 M {sx} {sy - 2} v 4 M {sx + 50} {sy - 2} v 4 M {sx+25} {sy-1.2} v 2.4" stroke="black" stroke-width="0.4" fill="none"/>')
    out.append(f'<text x="{sx + 52}" y="{sy + 1}" font-size="3" font-family="sans-serif">50 mm check bar (print at 100%)</text>')
    out.append('</svg>')
    open(path, 'w').write('\n'.join(out))

def _draw_geom(c, poly, marks, ox, oy, flipH):
    """draw in mm coords (already includes offset); y up native in PDF."""
    _feat_pdf(c, cut_features(poly), ox, oy)
    c.setLineWidth(0.7); c.setStrokeColorRGB(0, 0, 0)
    for ring, _ in _rings(poly):
        pth = c.beginPath(); x, y = ring[0]; pth.moveTo((x + ox) * mm, (y + oy) * mm)
        for x, y in ring[1:]: pth.lineTo((x + ox) * mm, (y + oy) * mm)
        pth.close(); c.drawPath(pth, stroke=1, fill=0)
    c.setStrokeColorRGB(0.1, 0.37, 0.7); c.setFillColorRGB(0.1, 0.37, 0.7); c.setDash(4, 3); c.setLineWidth(0.3)
    for g, lab in marks:
        if isinstance(g, Point):
            x, y = g.coords[0]; c.circle((x + ox) * mm, (y + oy) * mm, 0.8 * mm, fill=1, stroke=0)
            c.setFont('Helvetica', 7); c.drawString((x + ox + 2) * mm, (y + oy + 2) * mm, lab)
        else:
            co = np.array(g.coords); pth = c.beginPath(); pth.moveTo((co[0][0] + ox) * mm, (co[0][1] + oy) * mm)
            for x, y in co[1:]: pth.lineTo((x + ox) * mm, (y + oy) * mm)
            c.drawPath(pth, stroke=1, fill=0)
            c.setFont('Helvetica', 6.5); c.drawString((co[0][0] + ox + 1) * mm, (co[0][1] + oy + 1.5) * mm, lab)
    c.setDash(); c.setFillColorRGB(0, 0, 0); c.setStrokeColorRGB(0, 0, 0)

def tiled_pdf(pieces, path, title):
    PW, PH = letter[0] / mm, letter[1] / mm
    M = 12.0; OV = 10.0
    aw, ah = PW - 2 * M, PH - 2 * M - 14     # printable area for drawing (leave room for header/scale bar)
    c = rlcanvas.Canvas(path, pagesize=letter)
    # cover
    c.setFont('Helvetica-Bold', 16); c.drawString(M * mm, (PH - 25) * mm, title)
    c.setFont('Helvetica', 10)
    lines = ['Print at 100% / "Actual size" (turn OFF "fit to page"). Check the 100 mm and 4 in bars on every page.',
             'Each piece is tiled across Letter pages with a 10 mm overlap. Line up the grey crosshairs, tape, then',
             'spray-glue or tape the template onto the plywood and cut on the black lines. Blue dashed lines are layout marks.',
             'HAND CUTTING (scroll saw / jigsaw): saw on the waste side of each black line and sand to it. Red crosshairs =',
             'drill holes (size printed). Hatched WASTE areas = inside cutouts: drill the 8 mm (5/16") red-circle entry holes,',
             'thread the scroll-saw blade (or drop the jigsaw blade in) and saw out. Slots: drill both ends, saw/file between.',
             'Page labels: PIECE  row r / col c (row 1 = bottom of the piece).', '']
    for i, (n, pc) in enumerate(pieces):
        b = pc['poly'].bounds; lines.append(f'  {n}: {b[2]-b[0]:.1f} x {b[3]-b[1]:.1f} mm, make {pc["qty"]}')
    for i, l in enumerate(lines): c.drawString(M * mm, (PH - 35 - i * 5.5) * mm, l)
    _scalebar(c, M, M)
    c.showPage()
    for n, pc in pieces:
        poly = pc['poly']; minx, miny, maxx, maxy = poly.bounds
        minx -= 5; miny -= 5; maxx += 5; maxy += 5
        sx, sy = aw - OV, ah - OV
        nc = int(np.ceil((maxx - minx - OV) / sx)) if maxx - minx > aw else 1
        nr = int(np.ceil((maxy - miny - OV) / sy)) if maxy - miny > ah else 1
        for r in range(nr):
            for col in range(nc):
                x0 = minx + col * sx; y0 = miny + r * sy
                c.saveState()
                pth = c.beginPath(); pth.rect(M * mm, (M + 14) * mm, aw * mm, ah * mm); c.clipPath(pth, stroke=0)
                ox, oy = M - x0, M + 14 - y0
                _draw_geom(c, poly, pc.get('marks', []), ox, oy, False)
                # crosshairs every 50 mm (alignment)
                c.setStrokeColorRGB(0.6, 0.6, 0.6); c.setLineWidth(0.25)
                for gx in np.arange(np.floor(minx / 50) * 50, maxx + 50, 50):
                    for gy in np.arange(np.floor(miny / 50) * 50, maxy + 50, 50):
                        X, Y = gx + ox, gy + oy
                        c.line((X - 3) * mm, Y * mm, (X + 3) * mm, Y * mm); c.line(X * mm, (Y - 3) * mm, X * mm, (Y + 3) * mm)
                c.restoreState()
                c.setLineWidth(0.2); c.setStrokeColorRGB(0.5, 0.5, 0.5); c.setDash(1, 2)
                c.rect(M * mm, (M + 14) * mm, aw * mm, ah * mm); c.setDash()
                c.setStrokeColorRGB(0, 0, 0)
                c.setFont('Helvetica-Bold', 10); c.drawString(M * mm, (PH - 7) * mm, f'{n.upper()}  (make {pc["qty"]})   row {r + 1}/{nr}  col {col + 1}/{nc}')
                c.setFont('Helvetica', 7); c.drawString(M * mm, (PH - 10.5) * mm, 'Overlap 10 mm with neighbouring tiles. Align grey crosshairs (50 mm grid).')
                _scalebar(c, M, M)
                c.showPage()
    c.save()

def _scalebar(c, x, y):
    c.setLineWidth(0.6)
    c.line(x * mm, (y + 4) * mm, (x + 100) * mm, (y + 4) * mm)
    for k in range(0, 101, 10): c.line((x + k) * mm, (y + 4) * mm, (x + k) * mm, (y + (6.5 if k % 50 == 0 else 5.2)) * mm)
    c.setFont('Helvetica', 7); c.drawString((x + 102) * mm, (y + 3.2) * mm, '100 mm')
    c.line((x) * mm, (y + 0.5) * mm, (x + 101.6) * mm, (y + 0.5) * mm)
    for k in range(5): c.line((x + k * 25.4) * mm, (y + 0.5) * mm, (x + k * 25.4) * mm, (y + 2.4) * mm)
    c.drawString((x + 102) * mm, (y - 0.3) * mm, '4 in')
    c.drawString((x + 125) * mm, (y + 1.6) * mm, 'Print at 100%. Both bars must measure true.')

def sheet_layout(pieces, sheet=(304.8, 304.8), kerf=4.0):
    """greedy shelf packing of bounding boxes (rotation allowed). returns list of sheets of (name, idx, poly_placed)."""
    items = []
    for n, pc in pieces:
        for i in range(pc['qty']): items.append((n, i, pc['poly']))
    items.sort(key=lambda it: -(it[2].bounds[3] - it[2].bounds[1]) * (it[2].bounds[2] - it[2].bounds[0]))
    sheets = []
    for n, i, poly in items:
        placed = False
        for opt in (poly, affinity.rotate(poly, 90, origin=(0, 0))):
            b = opt.bounds; w, h = b[2] - b[0] + kerf, b[3] - b[1] + kerf
            if w > sheet[0] or h > sheet[1]: continue
            for sh in sheets:
                for shelf in sh['shelves']:
                    if h <= shelf['h'] and shelf['x'] + w <= sheet[0]:
                        sh['items'].append((n, i, affinity.translate(opt, shelf['x'] - b[0] + kerf / 2, shelf['y'] - b[1] + kerf / 2))); shelf['x'] += w; placed = True; break
                if placed: break
                y = sum(s['h'] for s in sh['shelves'])
                if y + h <= sheet[1]:
                    sh['shelves'].append(dict(y=y, h=h, x=w)); sh['items'].append((n, i, affinity.translate(opt, -b[0] + kerf / 2, y - b[1] + kerf / 2))); placed = True; break
            if placed: break
        if not placed:
            opt = poly; b = opt.bounds; w, h = b[2] - b[0] + kerf, b[3] - b[1] + kerf
            if w > sheet[0] or h > sheet[1]:
                opt = affinity.rotate(poly, 90, origin=(0, 0)); b = opt.bounds; w, h = b[2] - b[0] + kerf, b[3] - b[1] + kerf
            sheets.append(dict(shelves=[dict(y=0, h=h, x=w)], items=[(n, i, affinity.translate(opt, -b[0] + kerf / 2, -b[1] + kerf / 2))]))
    return sheets

def sheets_svg(sheets, sheet, path, label):
    gap = 15; W = sheet[0]; H = len(sheets) * (sheet[1] + gap) + 10
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W + 10:.1f}mm" height="{H:.1f}mm" viewBox="-5 -5 {W + 10:.1f} {H:.1f}">', '<rect x="-5" y="-5" width="100%" height="100%" fill="white"/>']
    for k, sh in enumerate(sheets):
        oy = k * (sheet[1] + gap)
        out.append(f'<rect x="0" y="{oy}" width="{sheet[0]}" height="{sheet[1]}" fill="#f6ead8" stroke="#999" stroke-width="0.4"/>')
        out.append(f'<text x="2" y="{oy - 1.5}" font-size="5" font-family="sans-serif">{label} sheet {k + 1} ({sheet[0]:.0f} x {sheet[1]:.0f} mm, fits a 12 x 12 in sheet even if it is really 11.8 in)</text>')
        for n, i, poly in sh['items']:
            for ring, _ in _rings(poly):
                d = 'M ' + ' L '.join(f'{x:.2f},{oy + sheet[1] - y:.2f}' for x, y in ring) + ' Z'
                out.append(f'<path d="{d}" fill="none" stroke="black" stroke-width="0.35"/>')
            cx, cy = poly.representative_point().coords[0]
            out.append(f'<text x="{cx:.1f}" y="{oy + sheet[1] - cy:.1f}" font-size="6" font-family="sans-serif" fill="#a33">{n}</text>')
    out.append('</svg>'); open(path, 'w').write('\n'.join(out))
