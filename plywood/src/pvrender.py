import numpy as np, pyvista as pv
pv.OFF_SCREEN = True

def tm2pv(m):
    f = np.c_[np.full(len(m.faces), 3), m.faces].ravel()
    return pv.PolyData(np.asarray(m.vertices), f)

def render(items, path, view='iso', size=(1400, 1050), ortho=True, title=None, bg='white', zoom=1.0, labels=None, edges=False):
    """items: list of (trimesh, color[, opacity])"""
    pl = pv.Plotter(off_screen=True, window_size=size)
    pl.set_background(bg)
    for it in items:
        m, col = it[0], it[1]; op = it[2] if len(it) > 2 else 1.0
        pl.add_mesh(tm2pv(m), color=col, opacity=op, smooth_shading=False, specular=0.15, show_edges=False)
        if edges:
            pl.add_mesh(tm2pv(m).extract_feature_edges(35), color='#333333', line_width=1)
    if isinstance(view, str):
        {'iso': pl.view_isometric, 'top': pl.view_xy, 'front': pl.view_xz, 'side': pl.view_yz}[view]()
        if view == 'front': pl.camera.azimuth = 0
    else:
        pos, focal, up = view
        pl.camera_position = [pos, focal, up]
        pl.reset_camera(); 
    if ortho: pl.enable_parallel_projection()
    pl.camera.zoom(zoom)
    if title: pl.add_text(title, font_size=11, color='black')
    if labels:
        for (pt, txt) in labels:
            pl.add_point_labels([pt], [txt], font_size=14, point_size=1, shape_opacity=0.6, always_visible=True)
    pl.enable_anti_aliasing('ssaa')
    pl.screenshot(path)
    pl.close()

def cam(dirv, up=(0, 0, 1)):
    d = np.asarray(dirv, float)
    return (tuple(d * 1000), (0, 0, 0), up)
