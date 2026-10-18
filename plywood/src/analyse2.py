import os as _os, sys as _sys; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from paths import ROOT, PLANS_STL
import trimesh, numpy as np
D=PLANS_STL
def loops(m,origin,normal):
    s=m.section(plane_origin=origin, plane_normal=normal)
    if s is None: return []
    return sorted([(np.round(d.min(0),2).tolist(),np.round(d.max(0),2).tolist()) for d in s.discrete])
sw=trimesh.load(D+"Switch Panel.stl")
print("SWITCH PANEL z=1.5 loops (holes):")
for l in loops(sw,[0,0,1.5],[0,0,1]):
    lo,hi=l; print('  ctr',np.round((np.array(lo[:2])+hi[:2])/2,2),'size',np.round(np.array(hi[:2])-lo[:2],2))
print("SWITCH PANEL z=-3 (bosses):")
for l in loops(sw,[0,0,-3],[0,0,1]):
    lo,hi=l; print('  ctr',np.round((np.array(lo[:2])+hi[:2])/2,2),'size',np.round(np.array(hi[:2])-lo[:2],2))
en=trimesh.load(D+"Enclosure.stl")
print("ENCLOSURE profile slice y=99 (XZ):")
for l in loops(en,[0,99,0],[0,1,0]): print(' ',l)
print("ENCLOSURE slice y=5:")
for l in loops(en,[0,5,0],[0,1,0]): print(' ',l)
for z in [1,3,6,20,50,80,100,110,115]:
    print("ENC z=",z)
    for l in loops(en,[0,0,z],[0,0,1]): print('   ',l)
for x in [2,60,120,200,240]:
    print("ENC x=",x)
    for l in loops(en,[x,0,0],[1,0,0]): print('   ',l)
