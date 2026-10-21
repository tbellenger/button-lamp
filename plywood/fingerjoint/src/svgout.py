"""Tiny SVG writer (mm units, 1:1) for shapely polygons + optional dashed lines, and a shelf packer for sheet layouts."""
from shapely import affinity

def _path(poly):
    gs = [poly] if poly.geom_type == 'Polygon' else list(poly.geoms); d = []
    for g in gs:
        for ring in [g.exterior] + list(g.interiors):
            cs = list(ring.coords)[:-1]
            d.append('M ' + ' L '.join(f'{x:.4f},{y:.4f}' for x, y in cs) + ' Z')
    return ' '.join(d)

def write(fn, items, lines=(), texts=(), margin=2.0, sheet=None):
    """items: [(poly, stroke)], lines: [((x0,y0),(x1,y1), stroke)], texts: [(x, y, str, size)].  Y is flipped so
    the SVG reads the same way up as the DXF."""
    from shapely.ops import unary_union
    allg = unary_union([p for p, _ in items])
    x0, y0, x1, y1 = (0, 0, sheet[0], sheet[1]) if sheet else allg.bounds
    W, H = x1 - x0 + 2 * margin, y1 - y0 + 2 * margin
    f = lambda g: affinity.affine_transform(g, [1, 0, 0, -1, margin - x0, y1 + margin])
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.3f}mm" height="{H:.3f}mm" viewBox="0 0 {W:.3f} {H:.3f}">']
    if sheet: out.append(f'<rect x="{margin}" y="{margin}" width="{sheet[0]}" height="{sheet[1]}" fill="none" stroke="#999" stroke-width="0.3" stroke-dasharray="4 2"/>')
    for p, col in items:
        out.append(f'<path d="{_path(f(p))}" fill="none" fill-rule="evenodd" stroke="{col}" stroke-width="0.2"/>')
    for (a, b, col) in lines:
        ax, ay = a[0] + margin - x0, y1 + margin - a[1]; bx, by = b[0] + margin - x0, y1 + margin - b[1]
        out.append(f'<line x1="{ax:.4f}" y1="{ay:.4f}" x2="{bx:.4f}" y2="{by:.4f}" stroke="{col}" stroke-width="0.3" stroke-dasharray="3 2"/>')
    for (x, y, s, sz) in texts:
        out.append(f'<text x="{x + margin - x0:.2f}" y="{y1 + margin - y:.2f}" font-family="sans-serif" font-size="{sz}" fill="#3b6fb6">{s}</text>')
    out.append('</svg>'); open(fn, 'w').write('\n'.join(out))

def pack(parts, sheet_w, gap=6.0, edge=6.0):
    """parts: [(label, poly)] -> [(label, placed_poly)] using simple shelves, longest side horizontal. Raises if it won't fit."""
    def norm(p):
        b = p.bounds
        if (b[3] - b[1]) > (b[2] - b[0]): p = affinity.rotate(p, 90, origin=(0, 0))
        b = p.bounds; return affinity.translate(p, -b[0], -b[1])
    ps = sorted([(l, norm(p)) for l, p in parts], key=lambda t: -(t[1].bounds[3]))
    out, x, y, row_h = [], edge, edge, 0.0
    for l, p in ps:
        w, h = p.bounds[2], p.bounds[3]
        if x + w > sheet_w - edge:
            x, y, row_h = edge, y + row_h + gap, 0.0
        if x + w > sheet_w - edge: raise ValueError(f'{l} wider than sheet')
        out.append((l, affinity.translate(p, x, y))); x += w + gap; row_h = max(row_h, h)
    return out, y + row_h + edge
