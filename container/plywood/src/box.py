"""Plywood enclosure geometry. Units mm. X: back->front, Y: left->right, Z: up."""
import numpy as np, trimesh
from shapely.geometry import Polygon, box, Point
from shapely.ops import unary_union
from shapely import affinity

PANEL = dict(w=184.15, h=133.35, t=3.17, hole_inset=6.35, hole_d=3.4)   # measured from Switch Panel.stl
STRIP = 6.35   # 1/4" square glue strip

def params(t=3.0):
    return dict(t=t, L=250.0, W=210.0, H=120.0, hF=28.0, xa=122.0, lip=4.0, tab=13.0, clear=0.5,
                block=20.0, block_h=18.0, strip=STRIP)

def derived(p):
    L, H, hF, xa = p['L'], p['H'], p['hF'], p['xa']
    th = np.arctan2(H - hF, L - xa); S = np.hypot(H - hF, L - xa)
    e = np.array([np.cos(th), -np.sin(th)]); n = np.array([-np.sin(th), -np.cos(th)])   # (x,z)
    return th, S, e, n

def profile(p):
    return Polygon([(0, 0), (p['L'], 0), (p['L'], p['hF']), (p['xa'], p['H']), (0, p['H'])])

def xz_regions(p):
    """2D (x,z) footprints of every board, used to settle the joint lengths."""
    t = p['t']; L, H, xa = p['L'], p['H'], p['xa']
    th, S, e, n = derived(p); A = np.array([xa, H])
    prof = profile(p)
    top = box(0, H - t, xa, H).intersection(prof)
    back = box(0, 0, t, H - t)
    def slope_poly(s0, s1):
        P = [A + s0 * e, A + s1 * e, A + s1 * e + t * n, A + s0 * e + t * n]
        return Polygon(P)
    lo, hi = 0.0, 20.0
    for _ in range(50):
        mid = (lo + hi) / 2
        if slope_poly(mid, S).intersection(top).area > 1e-6: lo = mid
        else: hi = mid
    s0 = np.ceil(hi * 2) / 2
    sl = slope_poly(s0, S)
    lo, hi = 0.0, p['hF']
    for _ in range(50):
        mid = (lo + hi) / 2
        if box(L - t, 0, L, mid).intersection(sl).area > 1e-6: hi = mid
        else: lo = mid
    hfb = np.floor(lo * 2) / 2
    front = box(L - t, 0, L, hfb)
    return dict(top=top, back=back, slope=sl, front=front, s0=s0, S=S, th=th, hfb=hfb)

def pieces(p, cage_info=None, cage_rot_deg=180.0):
    """Returns dict name -> dict(poly=shapely polygon in local 2D coords incl. holes, qty, desc, marks=list of (geom, label))"""
    t = p['t']; L, W, H, xa = p['L'], p['W'], p['H'], p['xa']
    R = xz_regions(p); th, S = R['th'], R['S']; s0 = R['s0']
    Wi = W - 2 * t
    out = {}
    # sides: full profile
    out['side'] = dict(poly=profile(p), qty=2, desc='Side (make 2). Same outline both sides. Marks = inside face of the RIGHT side; flip the template for the left side.', marks=[])
    # back
    back = box(0, 0, Wi, H - t)
    usbc_y, usbc_z = 40.0 - t, 14.0
    usbc = box(usbc_y - 5.0, usbc_z - 2.25, usbc_y + 5.0, usbc_z + 2.25).buffer(0.0)
    usbc = Point(usbc_y - 2.75, usbc_z).buffer(2.25).union(Point(usbc_y + 2.75, usbc_z).buffer(2.25)).union(box(usbc_y - 2.75, usbc_z - 2.25, usbc_y + 2.75, usbc_z + 2.25))
    uno_y0 = 130.0   # Uno board edge (box y)
    usbb_c = uno_y0 + 38.1 - t        # Uno R3 USB-B jack centre is ~38 mm from the board's lower long edge
    usbb_z = t + 6.0 + 1.6 + 5.5       # 6 mm standoff + 1.6 mm PCB + half the ~11 mm jack
    usbb = box(usbb_c - 10, usbb_z - 9, usbb_c + 10, usbb_z + 9)   # 20 x 18: room for a typical moulded USB-B plug
    back = back.difference(usbc).difference(usbb)
    out['back'] = dict(poly=back, qty=1, desc='Back. Drawn as seen from INSIDE the box. USB-C power slot + window for the Arduino USB-B plug.',
                       marks=[(Point(usbc_y - 5, usbc_z + 7), 'USB-C power inlet 10 x 4.5'), (Point(usbb_c - 10, usbb_z + 13), 'Arduino USB-B window 20 x 18 (hold the Uno up to check)')])
    # top
    top = box(0, 0, xa, Wi)
    xc, yc = xa / 2, Wi / 2
    holes = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            holes.append(Point(xc + sx * 38.1, yc + sy * 38.1).buffer(1.7, 16))
    wx, wy = None, None
    if cage_info is not None:
        a = np.radians(cage_rot_deg); hx, hy = cage_info['wire_hole_xy']
        wx = xc + hx * np.cos(a) - hy * np.sin(a); wy = yc + hx * np.sin(a) + hy * np.cos(a)
        holes.append(Point(wx, wy).buffer(5.0, 24))
    top = top.difference(unary_union(holes))
    out['top'] = dict(poly=top, qty=1, desc='Top, seen from above. Spiral holder bolts here (4 x M3) + 10 mm wire hole (toward the back).',
                      marks=[(Point(xc, yc), 'spiral centre'), (box(xc - 46, yc - 46, xc + 46, yc + 46).exterior, 'holder flange outline')],
                      spiral_c=(xc, yc), wire=(wx, wy))
    # slope
    Ls = S - s0
    sl = box(0, 0, Ls, Wi)
    pc_u = Ls / 2; pc_v = Wi / 2       # panel centre on the slope board
    pw, ph = PANEL['w'], PANEL['h']
    # panel occupies u in [pc_u - ph/2, pc_u + ph/2] (along slope), v in [pc_v - pw/2, pc_v + pw/2]
    u0, u1 = pc_u - ph / 2, pc_u + ph / 2; v0, v1 = pc_v - pw / 2, pc_v + pw / 2
    win = box(u0 + p['lip'], v0 + p['lip'], u1 - p['lip'], v1 - p['lip'])
    tb = p['tab']
    for cu, cv in [(u0, v0), (u0, v1), (u1, v0), (u1, v1)]:
        su = 1 if cu == u0 else -1; sv = 1 if cv == v0 else -1
        win = win.difference(box(min(cu, cu + su * tb), min(cv, cv + sv * tb), max(cu, cu + su * tb), max(cv, cv + sv * tb)))
    win = win.buffer(-1.5).buffer(1.5)  # 1.5 mm inside radius = drill-able corners
    hs = []
    hi = PANEL['hole_inset']
    for cu, cv in [(u0, v0), (u0, v1), (u1, v0), (u1, v1)]:
        su = 1 if cu == u0 else -1; sv = 1 if cv == v0 else -1
        hs.append(Point(cu + su * hi, cv + sv * hi).buffer(1.7, 16))
    sl = sl.difference(win).difference(unary_union(hs))
    out['slope'] = dict(poly=sl, qty=1, desc='Sloped control face. Printed switch panel lays ON TOP and covers the window; 4 x M3 at the corners.',
                        marks=[(box(u0, v0, u1, v1).exterior, 'printed panel outline 184.15 x 133.35'),
                               (Point(u1 - 8, pc_v), 'FRONT (low) edge ->')],
                        panel_box=(u0, v0, u1, v1))
    # front
    out['front'] = dict(poly=box(0, 0, Wi, R['hfb']), qty=1, desc='Front strip.', marks=[])
    # bottom (removable)
    c = p['clear']
    BL, BW = L - 2 * t - 2 * c, Wi - 2 * c
    bot = box(0, 0, BL, BW)
    ox, oy = t + c, t + c    # bottom local -> box
    blocks = block_positions(p)
    bh = [Point(bx - ox, by - oy).buffer(1.7, 16) for bx, by in blocks]
    uno_x0 = t + 1.0 + 6.35   # USB-B jack face ends 1 mm behind the back board
    uno_holes = [(13.97, 2.54), (15.24, 50.8), (66.04, 7.62), (66.04, 35.56)]
    uh = [Point(uno_x0 + a - ox, uno_y0 + b - oy).buffer(1.6, 16) for a, b in uno_holes]
    vents = [Point(165 + i * 12 - ox, 28 + j * 12 - oy).buffer(4.0, 16) for i in range(3) for j in range(4)]   # 12 x 8 mm drilled vent holes
    bot = bot.difference(unary_union(bh + uh + vents))
    zones = [(box(uno_x0 - ox, uno_y0 - oy, uno_x0 + 68.58 - ox, uno_y0 + 53.34 - oy).exterior, 'Arduino Uno + screw shield (68.6 x 53.3)'),
             (box(30 - ox, 30 - oy, 85 - ox, 90 - oy).exterior, 'WAGO 5V / GND rails + 1000uF cap'),
             (box(95 - ox, 75 - oy, 131 - ox, 95 - oy).exterior, 'MT3608 12V boost (approx 36 x 17)')]
    feet = [(Point(30 - ox + t, 40 - oy + t), 'foot'), (Point(30 - ox + t, W - 40 - oy - t), 'foot'), (Point(L - 40 - ox - t, 40 - oy + t), 'foot'), (Point(L - 40 - ox - t, W - 40 - oy - t), 'foot')]
    out['bottom'] = dict(poly=bot, qty=1, desc='Removable bottom, seen from above (inside face). 6 screws up into the printed blocks. Feet go on the underside.',
                         marks=zones + [(Point(*f[0].coords[0]).buffer(6).exterior, 'rubber foot (underside)') for f in feet] + [(Point(5, BW / 2), 'BACK edge')],
                         origin=(ox, oy))
    geo = dict(R=R, Ls=Ls, s0=s0, S=S, th=th, Wi=Wi, uno=(uno_x0, uno_y0), usbc=(usbc_y + t, usbc_z), hfb=R['hfb'])
    return out, geo

def block_positions(p):
    """centre (x,y) of the printed bottom blocks (box coords)."""
    t, L, W, b = p['t'], p['L'], p['W'], p['block']
    a = t + b / 2
    return [(a, a), (a, W - a), (L - a, a), (L - a, W - a), (L / 2, a), (L / 2, W - a)]

def glue_strips(p):
    """list of (name, qty, length_mm, 3D box (x0,y0,z0,x1,y1,z1) or slope spec)"""
    t, L, W, H, xa, s = p['t'], p['L'], p['W'], p['H'], p['xa'], p['strip']
    R = xz_regions(p); th, S, e, n = derived(p); s0 = R['s0']
    Wi = W - 2 * t
    zb = t + p['block_h']
    out = []
    out.append(('back/top cross strip', 1, Wi, (t, t, H - t - s, t + s, W - t, H - t)))
    vlen = (H - t - s) - zb
    out.append(('back corner upright (each side)', 2, vlen, [(t, t, zb, t + s, t + s, H - t - s), (t, W - t - s, zb, t + s, W - t, H - t - s)]))
    tlen = (xa - 10) - (t + s)
    out.append(('side/top strip', 2, tlen, [(t + s, t, H - t - s, xa - 10, t + s, H - t), (t + s, W - t - s, H - t - s, xa - 10, W - t, H - t)]))
    sa, sb = s0 + 10, S - 14
    out.append(('side/slope strip (under slope board)', 2, sb - sa, ('slope', sa, sb)))
    return out
