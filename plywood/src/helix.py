"""Helical extrusion of 2D (u,v) shapes in the meridian plane, plus helpers."""
import numpy as np, trimesh
from shapely.geometry import Polygon, MultiPolygon, Point, box
from shapely.ops import unary_union

def _cap(poly):
    v, f = trimesh.creation.triangulate_polygon(poly, engine='earcut')
    return np.asarray(v, float), np.asarray(f, int)

def sweep(poly, mapper, t0, t1, n):
    """Extrude 2D polygon (with holes ok) along parameter t in [t0,t1] with n steps.
    mapper(u, v, t) -> (x, y, z). Returns watertight trimesh."""
    if isinstance(poly, MultiPolygon):
        return trimesh.util.concatenate([sweep(p, mapper, t0, t1, n) for p in poly.geoms])
    v2, f2 = _cap(poly)
    m = len(v2)
    # boundary edges: edges used once, oriented as in faces (CCW faces)
    e = np.vstack([f2[:, [0, 1]], f2[:, [1, 2]], f2[:, [2, 0]]])
    key = np.sort(e, axis=1)
    _, inv, cnt = np.unique(key, axis=0, return_inverse=True, return_counts=True)
    be = e[cnt[inv] == 1]
    ts = np.linspace(t0, t1, n + 1)
    verts = []
    for t in ts:
        x, y, z = mapper(v2[:, 0], v2[:, 1], t)
        verts.append(np.c_[x, y, z])
    V = np.vstack(verts)
    F = [f2[:, ::-1], f2 + n * m]
    for i in range(n):
        a = be[:, 0] + i * m; b = be[:, 1] + i * m
        a1 = a + m; b1 = b + m
        F.append(np.c_[a, b, b1]); F.append(np.c_[a, b1, a1])
    mesh = trimesh.Trimesh(V, np.vstack(F), process=True)
    mesh.remove_unreferenced_vertices()
    if mesh.volume < 0: mesh.invert()
    return mesh

def helix_mapper(R, pitch, z0, stretch=True):
    c = pitch / (2 * np.pi)
    k = np.sqrt(R**2 + c**2) / R if stretch else 1.0   # meridian section of a round tube is taller by 1/cos(helix angle)
    def f(u, v, th):
        r = R + u
        return r * np.cos(th), r * np.sin(th), z0 + c * th + v * k
    return f

def linear_mapper(axis_origin, du, dv, dt):
    o = np.asarray(axis_origin, float); du = np.asarray(du, float); dv = np.asarray(dv, float); dt = np.asarray(dt, float)
    def f(u, v, t):
        P = o[None, :] + np.outer(u, du) + np.outer(v, dv) + t * dt[None, :]
        return P[:, 0], P[:, 1], P[:, 2]
    return f

def circle(r, c=(0, 0), n=64):
    return Point(c).buffer(r, quad_segs=max(4, n // 4))

def union(meshes):
    return trimesh.boolean.union(meshes, engine='manifold')

def diff(a, bs):
    return trimesh.boolean.difference([a] + list(bs), engine='manifold')

def prism(poly, h, z0=0.0):
    m = trimesh.creation.extrude_polygon(poly, h)
    m.apply_translation([0, 0, z0])
    return m

def cyl(r, h, x=0, y=0, z0=0, n=48):
    m = trimesh.creation.cylinder(radius=r, height=h, sections=n)
    m.apply_translation([x, y, z0 + h / 2])
    return m

def hex_prism(af, h, x=0, y=0, z0=0):
    R = af / np.sqrt(3)
    pts = [(x + R * np.cos(a), y + R * np.sin(a)) for a in np.radians(np.arange(0, 360, 60))]
    return prism(Polygon(pts), h, z0)

def bar(p0, p1, s):
    """square-ish bar between two points (convex hull of two cubes)"""
    c0 = trimesh.creation.box([s, s, s]); c0.apply_translation(p0)
    c1 = trimesh.creation.box([s, s, s]); c1.apply_translation(p1)
    return trimesh.util.concatenate([c0, c1]).convex_hull
