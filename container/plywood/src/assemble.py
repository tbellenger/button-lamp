"""3D assembly of plywood box + printed parts, for renders and checks."""
import os as _os, sys as _sys; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from paths import ROOT, PLANS_STL
import numpy as np, trimesh
from shapely.geometry import box as sbox
import box as B, spiral as SP
from helix import prism, cyl, union, diff, hex_prism

PANEL_STL = PLANS_STL + "Switch Panel.stl"

def place(poly, h, origin, bu, bv, bw):
    m = prism(poly, h)
    M = np.eye(4); M[:3, 0] = bu; M[:3, 1] = bv; M[:3, 2] = bw; M[:3, 3] = origin
    m.apply_transform(M)
    if m.volume < 0: m.invert()
    return m

def build(p, sp_params, cage_rot=180.0):
    t, L, W, H, xa = p['t'], p['L'], p['W'], p['H'], p['xa']
    cage, cinfo = SP.build(sp_params)
    pcs, geo = B.pieces(p, cinfo, cage_rot)
    th, S, e2, n2 = B.derived(p)
    e = np.array([e2[0], 0, e2[1]]); n = np.array([n2[0], 0, n2[1]])
    X, Y, Z = np.eye(3)
    A = np.array([xa, 0, H])
    parts = {}
    side = pcs['side']['poly']
    parts['side_L'] = place(side, t, [0, t, 0], X, Z, -Y)      # occupies y in [0,t]
    parts['side_R'] = place(side, t, [0, W, 0], X, Z, -Y)      # y in [W-t, W]
    parts['back'] = place(pcs['back']['poly'], t, [0, t, 0], Y, Z, X)
    parts['top'] = place(pcs['top']['poly'], t, [0, t, H - t], X, Y, Z)
    s0 = geo['s0']
    parts['slope'] = place(pcs['slope']['poly'], t, A + s0 * e + t * Y, e, Y, n)
    parts['front'] = place(pcs['front']['poly'], t, [L - t, t, 0], Y, Z, X)
    ox, oy = pcs['bottom']['origin']
    parts['bottom'] = place(pcs['bottom']['poly'], t, [ox, oy, 0], X, Y, Z)
    # glue strips
    strips = []
    for name, q, ln, spec in B.glue_strips(p):
        specs = spec if isinstance(spec, list) else [spec]
        if isinstance(spec, tuple) and spec[0] == 'slope':
            _, sa, sb = spec; s = p['strip']
            for y0 in (t, W - t - s):
                strips.append(place(sbox(sa, 0, sb, s), s, A + sa * 0 + t * n + y0 * Y, e, Y, n))
            continue
        for (x0, y0, z0, x1, y1, z1) in specs:
            b = trimesh.creation.box([x1 - x0, y1 - y0, z1 - z0]); b.apply_translation([(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2]); strips.append(b)
    parts['strips'] = trimesh.util.concatenate(strips)
    # printed bottom blocks
    blk = bottom_block(p)
    bl = []
    for (bx, by) in B.block_positions(p):
        m = blk.copy(); m.apply_translation([bx, by, t]); bl.append(m)
    parts['blocks'] = trimesh.util.concatenate(bl)
    # switch panel (printed, as-is)
    pan = trimesh.load(PANEL_STL)
    u0, v0, u1, v1 = pcs['slope']['panel_box']
    M = np.eye(4); M[:3, 0] = Y; M[:3, 1] = -e; M[:3, 2] = -n
    M[:3, 3] = A + (s0 + u1) * e + (t + v0) * Y
    pan.apply_transform(M); parts['panel'] = pan
    # spiral cage, tube, cap, backer
    xc, yc = pcs['top']['spiral_c']
    T = trimesh.transformations.rotation_matrix(np.radians(cage_rot), [0, 0, 1]); T[:3, 3] = [xc, t + yc, H]
    c = cage.copy(); c.apply_transform(T); parts['cage'] = c
    tube = SP.tube_mesh(sp_params, cinfo); tube.apply_transform(T); parts['tube'] = tube
    cap = SP.end_cap(sp_params)
    # orient cap at the tube end
    R_, P_ = sp_params['R'], sp_params['pitch']; cc = P_ / 2 / np.pi; the = cinfo['th_end']
    pt = np.array([R_ * np.cos(the), R_ * np.sin(the), cinfo['z0'] + cc * the])
    tg = np.array([-R_ * np.sin(the), R_ * np.cos(the), cc]); tg /= np.linalg.norm(tg)
    Mc = trimesh.geometry.align_vectors([0, 0, 1], tg); Mc[:3, 3] = pt - tg * 9.0
    cap.apply_transform(Mc); cap.apply_transform(T); parts['cap'] = cap
    bk = backer_mesh(sp_params, cinfo)
    Tb = T.copy(); Tb[:3, 3] = [xc, t + yc, H - t - 5.0]; bk2 = bk.copy(); bk2.apply_transform(Tb); parts['backer'] = bk2
    # placeholder electronics
    ux, uy = geo['uno']
    el = []
    def bx(x0, y0, z0, dx, dy, dz):
        b = trimesh.creation.box([dx, dy, dz]); b.apply_translation([x0 + dx / 2, y0 + dy / 2, z0 + dz / 2]); return b
    el.append(bx(ux, uy, t + 6, 68.58, 53.34, 1.6))
    el.append(bx(ux - 6.35, uy + 32.1, t + 7.6, 16.0, 12.0, 11.0))     # USB-B
    el.append(bx(ux + 2, uy + 2, t + 7.6 + 12, 64, 49, 1.6))            # screw shield
    el.append(bx(ux + 4, uy + 44, t + 21, 60, 8, 10))                   # screw terminals
    elec = trimesh.util.concatenate(el)
    wago = trimesh.util.concatenate([bx(35, 35 + i * 22, t, 30, 18, 8.5) for i in range(2)])
    mt = bx(95, 75, t, 36, 17, 6)
    parts['uno'] = elec; parts['wago'] = wago; parts['mt3608'] = mt
    return parts, pcs, geo, cage, cinfo, bk

def bottom_block(p):
    b, h = p['block'], p['block_h']
    m = trimesh.creation.box([b, b, h]); m.apply_translation([0, 0, h / 2])
    m = diff(m, [cyl(1.25, 15.0, 0, 0, -0.5)])   # 2.5 mm pilot for M3 self-tap or #4 wood screw
    return m

def backer_mesh(sp, cinfo):
    body, cuts = SP.backer(sp, 3.0)
    hx, hy = cinfo['wire_hole_xy']
    cuts.append(cyl(5.0, 7, hx, hy, -1))
    return diff(body, cuts)

COLORS = dict(side_L='#d9b382', side_R='#d9b382', back='#cfa774', top='#e2be8e', slope='#d4ad7b', front='#cfa774',
              bottom='#c49a68', strips='#a8794a', blocks='#7a7a7a', panel='#3d6fb6', cage='#f5f0e6', tube='#ffffff',
              cap='#f5f0e6', backer='#7a7a7a', uno='#1f6f8b', wago='#e8e8e8', mt3608='#2a7a2a')
