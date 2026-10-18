"""Printable spiral cage for a round silicone neon tube. All sizes in mm."""
import numpy as np, trimesh
from shapely.geometry import Polygon, Point, box, MultiPoint
from shapely.ops import unary_union
from helix import *

def default_params(tube_od=13.0):
    return dict(
        tube_od=tube_od,          # tube outside diameter (JARBIH 5mm-strip version = 13; 10mm-strip version = 16)
        R=34.3,                   # helix centre-line radius (original spiral: 34.29)
        pitch=34.0,               # rise per turn
        strip_len=900.0,          # LED strip length in the coil: BTF FCOB 160/m cut to 144 LEDs = 72 x 12.5 mm segments = 900 mm
        tube_extra=37.0,          # extra tube: 12 in the socket + 15 wire/solder joint + 10 top slack
        clip_w=10.0,              # clip length along the helix
        clip_wall=2.2,
        clip_open_deg=55.0,       # half-angle of the snap opening (faces outward)
        bore_clear=0.15,          # radial clearance on the tube (snug grip)
        col_t=5.0,                # column radial thickness
        n_cols=4,
        col_phase_deg=45.0,       # first column angle (lead-in socket is at 0 deg)
        flange=92.0, flange_t=4.0, flange_r=6.0,
        bolt_sq=76.2, bolt_d=3.4,  # M3 bolt square = original 3.0 in pattern
        wire_hole_d=8.0,
        socket_len=12.0,          # how far the tube end pushes into the socket
        hub_d=10.0,
    )

def build(p):
    d = p['tube_od']; R = p['R']; P = p['pitch']
    rb = d / 2 + p['bore_clear']; ro = rb + p['clip_wall']
    c = P / (2 * np.pi); ds = np.sqrt(R**2 + c**2)       # arc length per radian
    z0 = p['flange_t'] + ro + 2.0                        # helix centre height at theta = 0 (socket)
    tube_len = p['strip_len'] + p['tube_extra']
    th_end = tube_len / ds                                 # tube end sits at theta=0 inside the socket and runs to th_end
    th_sock0 = -(p['socket_len'] + 10) / ds               # socket solid block start
    hm = helix_mapper(R, P, z0)
    parts = []
    # ---- flange
    half = p['flange'] / 2
    fl = box(-half, -half, half, half).buffer(-p['flange_r']).buffer(p['flange_r'])
    parts.append(prism(fl, p['flange_t']))
    # ---- lead-in socket: block around tube from th_sock0 to +clip_w
    th_s1 = p['socket_len'] / ds
    blk = box(-ro, -(z0 - 0.5), ro, ro)                   # reaches down into flange
    parts.append(sweep(blk, hm, th_sock0, th_s1, 24))
    # ---- clips + columns
    ucol0 = -ro - p['col_t']; ucol1 = -ro + 0.6
    ring = Point(0, 0).buffer(ro, 32)
    a = np.radians(p['clip_open_deg'])
    wedge = Polygon([(0, 0), (3 * ro * np.cos(a), 3 * ro * np.sin(a)), (3 * ro, 3 * ro), (3 * ro, -3 * ro), (3 * ro * np.cos(a), -3 * ro * np.sin(a))])
    arc = [(ro * np.cos(t), ro * np.sin(t)) for t in np.radians(np.linspace(180, 360 - p['clip_open_deg'], 20))]
    vlow = -2.4 * ro - 5
    gus = MultiPoint(arc + [(ucol0, vlow), (ucol0, 0)]).convex_hull
    neck = box(ucol0, -ro, -rb + 0.8, ro)
    clip2d = unary_union([ring, gus, neck]).difference(wedge).difference(Point(0, 0).buffer(rb, 32))
    clip2d = clip2d.buffer(0)
    hw = p['clip_w'] / 2 / ds
    col_angles = [np.radians(p['col_phase_deg'] + k * 360 / p['n_cols']) for k in range(p['n_cols'])]
    clip_th = []
    for ca in col_angles:
        n = 0
        while True:
            th = ca + 2 * np.pi * n
            if th > th_end - hw - 14.0 / ds: break     # leave room for the end cap
            if th > th_s1 + hw: clip_th.append(th)
            n += 1
    for th in clip_th:
        parts.append(sweep(clip2d, hm, th - hw, th + hw, 6))
    # columns: to a common top height
    z_top = z0 + c * max(clip_th) + ro
    rc0 = R + ucol0; rc1 = R + ucol1; rcm = (rc0 + rc1) / 2
    for ca in col_angles:
        w = p['clip_w']
        poly = box(rc0, -w / 2, rc1, w / 2)
        m = prism(poly, z_top - p['flange_t'] + 0.5, p['flange_t'] - 0.5)
        m.apply_transform(trimesh.transformations.rotation_matrix(ca, [0, 0, 1]))
        parts.append(m)
    # hub post + 45 deg spokes up to the column tops (no bridges, no supports)
    s = 6.0
    z_hub = z_top - s / 2 - (rcm - s / 2)
    parts.append(cyl(p['hub_d'] / 2, z_hub + s / 2 - p['flange_t'] + 0.5, z0=p['flange_t'] - 0.5))
    for ca in col_angles:
        p0 = [0, 0, z_hub]; p1 = [rcm * np.cos(ca) * 0.97, rcm * np.sin(ca) * 0.97, z_top - s / 2]
        parts.append(bar(p0, p1, s))
    body = union(parts)
    # ---- cuts: tube bore through socket, wire channel, flange holes
    cuts = []
    cuts.append(sweep(Point(0, 0).buffer(rb + 0.1, 32), hm, 0.0, th_s1 + 0.2, 16))       # socket bore (mouth at theta=0... tube goes +theta)
    cuts.append(sweep(Point(0, 0).buffer(p['wire_hole_d'] / 2, 24), hm, th_sock0 * 0.55, 0.01, 8))
    thw = th_sock0 * 0.55
    wx, wy = R * np.cos(thw), R * np.sin(thw)
    cuts.append(cyl(p['wire_hole_d'] / 2, z0 + 1, wx, wy, -0.5))
    for sx in (-1, 1):
        for sy in (-1, 1):
            cuts.append(cyl(p['bolt_d'] / 2, p['flange_t'] + 1, sx * p['bolt_sq'] / 2, sy * p['bolt_sq'] / 2, -0.5))
    body = diff(body, cuts)
    body = trimesh.boolean.intersection([body, trimesh.creation.box([400, 400, 400], trimesh.transformations.translation_matrix([0, 0, 200]))], engine='manifold')
    info = dict(tube_od=d, R=R, pitch=P, z0=z0, th_end=th_end, turns=th_end / (2 * np.pi), tube_len_cut=tube_len,
                coil_od=2 * R + d, z_top=z_top, clip_count=len(clip_th), wire_hole_xy=(float(wx), float(wy)),
                helix_bend_radius=R * (1 + (c / R) ** 2), height_total=float(body.bounds[1][2]))
    return body, info

def tube_mesh(p, info, th0=None, th1=None):
    """visual tube along helix (for renders)"""
    hm = helix_mapper(p['R'], p['pitch'], info['z0'], stretch=False)
    ds = np.sqrt(p['R']**2 + (p['pitch'] / 2 / np.pi) ** 2)
    th0 = 0.0 if th0 is None else th0
    th1 = info['th_end'] if th1 is None else th1
    return sweep(Point(0, 0).buffer(p['tube_od'] / 2, 16), hm, th0, th1, 400)

def end_cap(p):
    """push-on end cap (bullet nose, 45 deg so it prints with no supports). Print open end down."""
    d = p['tube_od']; ri = d / 2 + 0.25; wall = 1.6; ro = ri + wall; depth = 10.0
    # revolve profile
    from shapely.geometry import Polygon
    prof = Polygon([(0, 0), (ro, 0), (ro, depth), (ro - (ro - 1.0), depth + (ro - 1.0)), (0, depth + (ro - 1.0))])
    inner = Polygon([(0, -1), (ri, -1), (ri, depth - 0.0), (ri - (ri - 1.0), depth + (ri - 1.0) - 0.0), (0, depth + ri - 1.0)])
    prof = prof.difference(inner)
    pts = np.array(prof.exterior.coords)[:-1]
    m = trimesh.creation.revolve(pts[::-1] if Polygon(pts).exterior.is_ccw else pts, sections=64)
    if m.volume < 0: m.invert()
    return m

def backer(p, t_ply):
    """printed backing plate that sits under the plywood top: nut traps for the 4 M3 bolts + wire hole."""
    half = p['flange'] / 2
    fl = box(-half, -half, half, half).buffer(-p['flange_r']).buffer(p['flange_r'])
    T = 5.0
    body = prism(fl.difference(Point(0, 0).buffer(22)), T)
    cuts = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            x, y = sx * p['bolt_sq'] / 2, sy * p['bolt_sq'] / 2
            cuts.append(cyl(p['bolt_d'] / 2, T + 1, x, y, -0.5))
            cuts.append(hex_prism(5.7, 2.8, x, y, -0.01))        # M3 nut trap (5.5 AF + clearance), on the underside
    return body, cuts
