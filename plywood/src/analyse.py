import os as _os, sys as _sys; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from paths import ROOT, PLANS_STL
import trimesh, numpy as np, json
D=PLANS_STL
def sec(m, origin, normal):
    s=m.section(plane_origin=origin, plane_normal=normal)
    if s is None: return None
    p,_=s.to_2D() if hasattr(s,'to_2D') else s.to_planar()
    return s
def loops(m,origin,normal):
    s=m.section(plane_origin=origin, plane_normal=normal)
    if s is None: return []
    out=[]
    for d in s.discrete:
        lo=d.min(0); hi=d.max(0)
        out.append((np.round(lo,2).tolist(),np.round(hi,2).tolist(),round(float(np.linalg.norm(hi-lo)),2)))
    return out
sp=trimesh.load(D+"Spiral (with base).stl")
print("SPIRAL slice y=0 (plane through axis):")
for l in sorted(loops(sp,[0,0,0],[0,1,0])): print(' ',l)
print("SPIRAL slices perpendicular to axis:")
for x in [79.5,81,83,85,88,92,100,150,200,240,246]:
    L=loops(sp,[x,0,0],[1,0,0]); print(' x=',x,len(L),[ (l[0][1:],l[1][1:]) for l in L][:6])
