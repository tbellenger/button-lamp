import os as _os, sys as _sys; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from paths import ROOT, PLANS_STL
import trimesh, numpy as np, matplotlib
matplotlib.use('Agg'); import matplotlib.pyplot as plt
D=PLANS_STL
en=trimesh.load(D+"Enclosure.stl")
fig,axs=plt.subplots(1,3,figsize=(20,7))
for ax,(o,n,ix,t) in zip(axs,[([0,99,0],[0,1,0],(0,2),'XZ @ y=99 (centre)'),([0,30,0],[0,1,0],(0,2),'XZ @ y=30'),([0,0,8],[0,0,1],(0,1),'XY @ z=8')]):
    s=en.section(plane_origin=o,plane_normal=n)
    for d in s.discrete: ax.plot(d[:,ix[0]],d[:,ix[1]],'-',lw=0.8)
    ax.set_aspect('equal'); ax.grid(True,alpha=.3); ax.set_title(t)
fig.savefig('analysis/enclosure_sections.png',dpi=90)
# slope frame
ang=np.arctan2(116.84-27.95,245.7-121.9); print('slope deg',np.degrees(ang))
nrm=np.array([np.sin(ang),0,np.cos(ang)])
fig,ax=plt.subplots(figsize=(12,9))
for off in [-1.0,-3.0,-6.0]:
    p0=np.array([183.8,99,72.4])-nrm*(-off)  # point on surface-ish
    s=en.section(plane_origin=np.array([183.8,99,72.4])+nrm*off,plane_normal=nrm)
    if s is None: continue
    # project to slope coords: u along y, v along slope
    vdir=np.array([np.cos(ang),0,-np.sin(ang)])
    for d in s.discrete:
        u=d[:,1]; v=(d-np.array([121.9,0,116.84]))@vdir
        ax.plot(u,v,lw=0.8,label=f'off {off}')
        print('off',off,'loop u',round(u.min(),2),round(u.max(),2),'v',round(v.min(),2),round(v.max(),2))
ax.set_aspect('equal'); ax.grid(True,alpha=.3); ax.legend(); ax.invert_yaxis()
fig.savefig('analysis/enclosure_slope_sections.png',dpi=90)
