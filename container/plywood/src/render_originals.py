import os as _os, sys as _sys; _sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from paths import ROOT, PLANS_STL
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import trimesh, numpy as np
from pvrender import render, cam
from PIL import Image
D = PLANS_STL
OUT = ROOT + '/render/'
for name, col in [('Enclosure', '#c8a26b'), ('Switch Panel', '#5a8fd0'), ('Spiral (with base)', '#dddddd'), ('Spiral End Cap', '#e0b0b0')]:
    m = trimesh.load(D + name + '.stl'); m.apply_translation(-m.bounds.mean(0))
    tiles = []
    for v, t in [((1, -1, 0.8), 'iso'), ((0, 0, 1), 'top'), ((0, -1, 0), 'front (-Y)'), ((1, 0, 0), 'side (+X)')]:
        up = (0, 1, 0) if v == (0, 0, 1) else (0, 0, 1)
        fn = f'/tmp/o_{t[:3]}.png'
        render([(m, col)], fn, view=cam(v, up), size=(700, 520), title=t)
        tiles.append(Image.open(fn))
    W = Image.new('RGB', (1400, 1040), 'white')
    for i, im in enumerate(tiles): W.paste(im, ((i % 2) * 700, (i // 2) * 520))
    fn = OUT + 'original_' + name.replace(' ', '_').replace('(', '').replace(')', '') + '.png'
    W.save(fn); print(fn, np.round(m.extents, 2))
