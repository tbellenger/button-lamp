"""Generate multi-angle high-res PNG renders of the 16x16 LED matrix frame parts, exploded view,
and full assembly mounted on the Button Lamp top deck."""
import os
import numpy as np
import pyvista as pv
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, "build")
REND = os.path.join(HERE, "render")
os.makedirs(REND, exist_ok=True)

STL_LAMP = "/workspace/buttonbox/fingerjoint/render/stl"
import sys
sys.path.insert(0, "/workspace/buttonbox/repo_stage/plywood/fingerjoint/src")
import fj, controls as C, proxies as PX


def load_stl(path):
    return pv.read(path)


def rot_x(deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    M = np.eye(4)
    M[1, 1], M[1, 2] = c, -s
    M[2, 1], M[2, 2] = s, c
    return M


def rot_z(deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    M = np.eye(4)
    M[0, 0], M[0, 1] = c, -s
    M[1, 0], M[1, 1] = s, c
    return M


def trans(dx, dy, dz):
    M = np.eye(4)
    M[:3, 3] = [dx, dy, dz]
    return M


def make_pcb_mesh():
    """160 x 160 x 1.2 mm black FPCB + 256 white 5050 SMD LEDs on 10 mm pitch."""
    pcb = pv.Box(bounds=(-80.0, 80.0, -80.0, 80.0, 0.0, 1.0))
    leds = []
    for i in range(16):
        for j in range(16):
            cx = (i - 7.5) * 10.0
            cy = (j - 7.5) * 10.0
            leds.append(pv.Box(bounds=(cx - 2.5, cx + 2.5, cy - 2.5, cy + 2.5, 1.0, 2.2)))
    return pcb, leds[0].merge(leds[1:])


def make_diffuser_mesh(thickness=2.0):
    """162 x 162 mm polycarbonate diffuser sheet."""
    return pv.Box(bounds=(-81.0, 81.0, -81.0, 81.0, 0.0, thickness))


def render_print_layout():
    """All 3 parts flat in their support-free print orientation on a 300x300 mm CR-10 bed."""
    pl = pv.Plotter(off_screen=True, window_size=(1800, 1250))
    pl.set_background("#f7f9fc")

    # Draw two 300x300 mm CR-10 build plates side by side
    for plate_idx, (ox, label) in enumerate([(-170.0, "Plate 1: Housing (171 x 171 x 22.2 mm) + Base Mount (92 x 92 x 76 mm)"),
                                             (170.0, "Plate 2: 16x16 Pixel Baffle Insert (162.4 x 162.4 x 8.5 mm)")]):
        bed = pv.Box(bounds=(ox - 150.0, ox + 150.0, -150.0, 150.0, -2.0, 0.0))
        pl.add_mesh(bed, color="#dde3ea", specular=0.1)
        pl.add_mesh(bed.extract_feature_edges(45), color="#7a8694", line_width=1.5)

    housing = load_stl(os.path.join(BUILD, "matrix_16x16_housing.stl")).translate((-170.0, -45.0, 0.0), inplace=False)
    base = load_stl(os.path.join(BUILD, "matrix_16x16_base_mount.stl")).translate((-170.0, 95.0, 0.0), inplace=False)
    baffle = load_stl(os.path.join(BUILD, "matrix_16x16_baffle_insert.stl")).translate((170.0, 0.0, 0.0), inplace=False)

    for m, col, ecol in [
        (housing, "#2b3a4a", "#111922"),
        (base, "#3d6b99", "#183049"),
        (baffle, "#e8edf2", "#556270"),
    ]:
        pl.add_mesh(m, color=col, specular=0.35, smooth_shading=False)
        pl.add_mesh(m.extract_feature_edges(30), color=ecol, line_width=1.0)

    ctr = np.array([0.0, 0.0, 15.0])
    cam_dir = np.array([0.15, -1.15, 1.05])
    cam_dir /= np.linalg.norm(cam_dir)
    pl.camera_position = [tuple(ctr + cam_dir * 760.0), tuple(ctr), (0, 0, 1)]
    pl.reset_camera()
    pl.camera.zoom(1.22)
    pl.add_text(
        "Creality CR-10 (300 x 300 mm bed) Support-Free Print Layout:\n"
        "1) matrix_16x16_housing.stl (171.0 x 171.0 x 22.2 mm)   "
        "2) matrix_16x16_base_mount.stl (92.0 x 92.0 x 76.0 mm)   "
        "3) matrix_16x16_baffle_insert.stl (162.4 x 162.4 x 8.5 mm)",
        font_size=11,
        color="#1a222c",
    )
    pl.enable_anti_aliasing("ssaa")
    pl.screenshot(os.path.join(REND, "parts_print_layout.png"))
    pl.close()


def render_exploded():
    """Exploded assembly showing base mount, housing, 16x16 WS2812B PCB, 16x16 baffle, and 162x162 diffuser."""
    pl = pv.Plotter(off_screen=True, window_size=(1700, 1300))
    pl.set_background("#f7f9fc")

    # Pose helper: housing local -> upright assembly frame (-Y = front, +Z = up)
    pose_upright = trans(0.0, -10.0, 96.0) @ rot_x(90.0)

    base = load_stl(os.path.join(BUILD, "matrix_16x16_base_mount.stl")).transform(trans(0.0, 65.0, -25.0), inplace=False)
    housing = load_stl(os.path.join(BUILD, "matrix_16x16_housing.stl")).transform(pose_upright, inplace=False)

    pcb_board, pcb_leds = make_pcb_mesh()
    pcb_pose = trans(0.0, -58.0, 0.0) @ pose_upright @ trans(0.0, 0.0, 4.2)
    pcb_board = pcb_board.transform(pcb_pose, inplace=False)
    pcb_leds = pcb_leds.transform(pcb_pose, inplace=False)

    baffle = load_stl(os.path.join(BUILD, "matrix_16x16_baffle_insert.stl")).transform(
        trans(0.0, -115.0, 35.0) @ pose_upright @ trans(0.0, 0.0, 7.7), inplace=False
    )
    diffuser = make_diffuser_mesh(2.0).transform(
        trans(0.0, -170.0, 70.0) @ pose_upright @ trans(0.0, 0.0, 16.2), inplace=False
    )

    pl.add_mesh(base, color="#3d6b99", specular=0.35, smooth_shading=False)
    pl.add_mesh(base.extract_feature_edges(30), color="#183049", line_width=1.0)

    pl.add_mesh(housing, color="#2b3a4a", specular=0.35, smooth_shading=False)
    pl.add_mesh(housing.extract_feature_edges(30), color="#111922", line_width=1.0)

    pl.add_mesh(pcb_board, color="#1a1a1a", specular=0.2)
    pl.add_mesh(pcb_leds, color="#ff9800", specular=0.5)

    pl.add_mesh(baffle, color="#e8edf2", specular=0.25, smooth_shading=False)
    pl.add_mesh(baffle.extract_feature_edges(30), color="#556270", line_width=0.8)

    pl.add_mesh(diffuser, color="#ffffff", opacity=0.65, specular=0.6)
    pl.add_mesh(diffuser.extract_feature_edges(30), color="#3b6fb6", line_width=1.5)

    ctr = np.array([0.0, -55.0, 95.0])
    cam_dir = np.array([-1.25, -1.15, 0.72])
    cam_dir /= np.linalg.norm(cam_dir)
    pl.camera_position = [tuple(ctr + cam_dir * 620.0), tuple(ctr), (0, 0, 1)]
    pl.reset_camera()
    pl.camera.zoom(1.22)
    pl.add_text(
        "Exploded Assembly (16x16 WS2812B Matrix Attachment):\n"
        "Base Mount (4x M3 on 76.2mm sq) -> Housing -> 160x160mm Flex PCB (drop-in) -> "
        "16x16 Baffle Insert (top slide-in) -> 162x162mm Polycarbonate Diffuser (top slide-in)",
        font_size=10,
        color="#1a222c",
    )
    pl.enable_anti_aliasing("ssaa")
    pl.screenshot(os.path.join(REND, "exploded_assembly.png"))
    pl.close()


def render_on_button_lamp(cam_dir, out_name, title, show_diffuser=True):
    """Render the 16x16 matrix assembly bolted onto the Button Lamp top deck (at x=61, y=105, z=120)."""
    pl = pv.Plotter(off_screen=True, window_size=(1700, 1300))
    pl.set_background("#f7f9fc")

    # 1. Button Lamp enclosure panels from /workspace/buttonbox/fingerjoint/render/stl
    col_lamp = dict(
        side_L="#d9b98c",
        side_R="#d9b98c",
        back="#c9a26b",
        top="#e6c89a",
        slope="#d4ad78",
        front="#bf9460",
        bottom="#a88456",
    )
    for k, c in col_lamp.items():
        p = os.path.join(STL_LAMP, f"{k}.stl")
        if os.path.exists(p):
            m = load_stl(p)
            pl.add_mesh(m, color=c, specular=0.2, smooth_shading=False)
            pl.add_mesh(m.extract_feature_edges(30), color="#4a3a28", line_width=1.0)

    # Control stand-ins on the sloped face
    E3 = np.array([fj.E[0], 0, fj.E[1]])
    N3 = np.array([fj.N[0], 0, fj.N[1]])
    o2 = fj.A + (fj.PANEL_S0 + C.PANEL_H) * fj.E
    PANEL_M = PX.frame((o2[0], fj.PANEL_Y0, o2[1]), (0, 1, 0), -E3, -N3)
    PX.add(pl, PANEL_M)

    # 2. Place our 16x16 matrix attachment on the top deck at (xc=61.0, yc=105.0, z=120.0):
    #    In box coords: +X is Front (toward slope), -X is Back, +Y is Right (seen from front), +Z is Up.
    #    In our base_mount coords: -Y is Front, +Y is Back, +X is Right (seen from front), +Z is Up.
    #    Wait: rot_z(-90) maps base_mount's -Y (0, -1, 0) -> (+1, 0, 0) = +X_box (Front),
    #    and base_mount's +X (1, 0, 0) -> (0, -1, 0). Wait: in box coords, seen from the front (+X looking toward -X),
    #    Left is Y=0 and Right is Y=210, so +Y_box is Right!
    #    Wait: if +X_base -> +Y_box (0, 1, 0) and -Y_base -> +X_box (1, 0, 0), that's a 2D reflection unless we check handedness:
    #    In right-handed 3D: if Facing Front is +X_box and Up is +Z_box, then Right (seen from front, looking in -X_box) is +Y_box!
    #    Because X_front = +X_box, looking AT the front means looking in -X_box: to your right is +Y_box, and (-Y_base) x (+X_base) = (1,0,0) x (0,1,0) = +Z_box!
    #    Wait: (0, -1, 0) rotated by +90 deg around Z:
    #    rot_z(90) maps (1, 0, 0) -> (0, 1, 0) (+Y_box) and (0, 1, 0) -> (-1, 0, 0) (-X_box = Back), so (0, -1, 0) -> (+1, 0, 0) (+X_box = Front)!
    #    Let's verify: under rot_z(90), (x, y, z) -> (-y, x, z)!
    #    Check wire hole: in base_mount, wire bay is at (X = +11.71, Y = +32.24).
    #    Under rot_z(90) + trans(61.0, 105.0, 120.0):
    #    (x, y) -> (61.0 - 32.24, 105.0 + 11.71) = (28.76, 116.71) == EXACT top-deck wire hole in fj125_top_qty1_mm.dxf!
    M_top = trans(61.0, 105.0, 120.0) @ rot_z(90.0)
    pose_upright = M_top @ trans(0.0, -10.0, 96.0) @ rot_x(90.0)

    base = load_stl(os.path.join(BUILD, "matrix_16x16_base_mount.stl")).transform(M_top, inplace=False)
    housing = load_stl(os.path.join(BUILD, "matrix_16x16_housing.stl")).transform(pose_upright, inplace=False)
    baffle = load_stl(os.path.join(BUILD, "matrix_16x16_baffle_insert.stl")).transform(
        pose_upright @ trans(0.0, 0.0, 7.7), inplace=False
    )
    pcb_board, pcb_leds = make_pcb_mesh()
    pcb_board = pcb_board.transform(pose_upright @ trans(0.0, 0.0, 4.2), inplace=False)
    pcb_leds = pcb_leds.transform(pose_upright @ trans(0.0, 0.0, 4.2), inplace=False)

    pl.add_mesh(base, color="#3d6b99", specular=0.35, smooth_shading=False)
    pl.add_mesh(base.extract_feature_edges(30), color="#183049", line_width=1.0)

    pl.add_mesh(housing, color="#2b3a4a", specular=0.35, smooth_shading=False)
    pl.add_mesh(housing.extract_feature_edges(30), color="#111922", line_width=1.0)

    pl.add_mesh(pcb_board, color="#1a1a1a", specular=0.2)
    pl.add_mesh(pcb_leds, color="#ff6d00", specular=0.5)

    pl.add_mesh(baffle, color="#e8edf2", specular=0.25, smooth_shading=False)
    pl.add_mesh(baffle.extract_feature_edges(30), color="#556270", line_width=0.7)

    if show_diffuser:
        diffuser = make_diffuser_mesh(1.5).transform(pose_upright @ trans(0.0, 0.0, 16.2), inplace=False)
        pl.add_mesh(diffuser, color="#ffffff", opacity=0.38, specular=0.6)

    ctr = np.array([115.0, 105.0, 145.0])
    d = np.array(cam_dir, float)
    d /= np.linalg.norm(d)
    pl.camera_position = [tuple(ctr + d * 880.0), tuple(ctr), (0, 0, 1)]
    pl.reset_camera()
    pl.camera.zoom(1.25)
    pl.add_text(title, font_size=11, color="#1a222c")
    pl.enable_anti_aliasing("ssaa")
    pl.screenshot(os.path.join(REND, out_name))
    pl.close()


if __name__ == "__main__":
    render_print_layout()
    render_exploded()
    render_on_button_lamp(
        (1.2, 1.0, 0.65),
        "assembled_front_iso.png",
        "16x16 WS2812B Matrix Attachment Mounted on Button Lamp Top Deck (Front Iso)\n"
        "Swaps directly onto the 76.2 x 76.2 mm (3.0 in) 4-bolt pattern; 162x162mm polycarbonate diffuser slides in from top",
    )
    render_on_button_lamp(
        (-1.2, -1.0, 0.65),
        "assembled_back_iso.png",
        "16x16 WS2812B Matrix Attachment on Button Lamp (Rear Iso)\n"
        "Twin gusseted pillars (4x M3), windowed anti-warp backplate, and wire channel aligned with top-deck Ø10mm wire hole",
        show_diffuser=False,
    )
    print("Renders written to", REND)
