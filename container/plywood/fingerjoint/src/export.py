"""SVG copies of every v2 DXF, the 24 x 24 in sheet layout, and cutlist.csv.  Run after fj.py (FJ_VARIANT=v2)."""
import os, sys, csv, itertools
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fj, svgout
from shapely import affinity
SVG = os.path.join(fj.ROOT, 'svg'); os.makedirs(SVG, exist_ok=True)
parts = dict(fj.parts); parts['faceplate'] = fj.faceplate()
files = {nm: fj.fname(nm) for nm in fj.parts}; files['faceplate'] = fj.FACEPLATE_FILE
for nm, poly in parts.items():
    p0 = affinity.translate(poly, -poly.bounds[0], -poly.bounds[1])
    svgout.write(os.path.join(SVG, os.path.basename(files[nm]).replace('.dxf', '.svg')), [(p0, '#d00')])
# sheet layout: one full v2 set (2 sides) + the optional faceplate on 24 x 24 in
SHEET = 24 * 25.4
items = [('side A', parts['side']), ('side B', parts['side'])] + [(nm, parts[nm]) for nm in ['back', 'top', 'slope', 'front', 'bottom', 'faceplate']]
placed, used_h = svgout.pack(items, SHEET)
assert used_h <= SHEET, used_h
for (a, pa), (b, pb) in itertools.combinations(placed, 2): assert pa.distance(pb) >= 5.99, (a, b)
lab = dict(slope='control slope', back='back v2', faceplate='faceplate (optional)')
svgout.write(os.path.join(fj.ROOT, 'sheet_layout_v2_24x24in.svg'), [(p, '#d00' if l != 'faceplate' else '#c80') for l, p in placed],
             texts=[(p.bounds[0] + 3, p.bounds[3] - 6, lab.get(l, l), 5) for l, p in placed], sheet=(SHEET, SHEET))
def stats(poly):
    gs = [poly] if poly.geom_type == 'Polygon' else list(poly.geoms)
    return sum(g.exterior.length + sum(r.length for r in g.interiors) for g in gs), sum(1 + len(g.interiors) for g in gs)
NOTES = dict(
    side='wedge profile; 8 open notches (2 back, 2 top, 3 slope, 1 front) with dogbones; make 2 (flip one). Unchanged from the ordered set',
    back='v2: as ordered + 12.2 mm DC jack hole (y 72, z 24). 2 tabs each end, 3 tabs into the top; USB-C slot 10 x 4.5 + USB window 20 x 18 (drawn from INSIDE)',
    top='unchanged: 4 x 3.4 cage holes on 76.2 sq + 10 mm wire hole (drawn from ABOVE)',
    slope='v2 CONTROL SLOPE: replaces the printed Switch Panel. 3 tabs each end; 11 control cut-outs in the printed layout (drawn from OUTSIDE)',
    front='unchanged: 1 tab (12 mm) each end; 28 mm tall',
    bottom='unchanged removable tray: 6 x 3.4 screw holes, 4 x 3.2 Uno holes, 12 x 8 vents',
    faceplate='OPTIONAL retrofit only: 1/8" birch copy of the printed panel for the slope already ordered (S7872337); 4 x M3 on 171.45 x 120.65')
with open(os.path.join(fj.ROOT, 'cutlist.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['part', 'qty', 'w_mm', 'h_mm', 'cut_len_mm', 'pierces', 'file', 'notes'])
    for nm, poly in parts.items():
        b = poly.bounds; cl, pc = stats(poly)
        w.writerow([nm if nm != 'slope' else 'control_slope', fj.QTY.get(nm, 1), round(b[2] - b[0], 2), round(b[3] - b[1], 2), round(cl), pc,
                    os.path.relpath(files[nm], fj.ROOT), NOTES[nm]])
print('sheet used', round(used_h, 1), 'of', SHEET)
