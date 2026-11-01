"""Preview renders of the 16x16 matrix housing with the rear Arduino UNO R4 WiFi bay."""
import os
import numpy as np
import pyvista as pv

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, "build")
REND = os.path.join(HERE, "render")
os.makedirs(REND, exist_ok=True)


def load_stl(name):
    return pv.read(os.path.join(BUILD, name))


def rot_x(deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    M = np.eye(4)
    M[1, 1], M[1, 2] = c, -s
    M[2, 1], M[2, 2] = s, c
    return M


def trans(dx, dy, dz):
    M = np.eye(4)
    M[:3, 3] = [dx, dy, dz]
    return M


def mirror_y():
    M = np.eye(4)
    M[1, 1] = -1.0
    return M


# Print-frame point (x, y, z) -> world. Housing +Z (toward the LEDs) is world -Y.
HOUSING = trans(0.0, -10.0, 96.0) @ rot_x(90.0)
# Shell mating face on the housing back; cavity opens toward world +Y.
SHELL = HOUSING @ rot_x(180.0) @ mirror_y()
# Cover plate just outside the shell rim, lip pointing back into the bay.
COVER = HOUSING @ trans(0.0, 0.0, -28.4)
BAFFLE = HOUSING @ trans(0.0, 0.0, 7.7)


def add(pl, mesh, color, edge="#1a222c", opacity=1.0, edges=True):
    pl.add_mesh(mesh, color=color, opacity=opacity, specular=0.35, smooth_shading=False)
    if edges and opacity > 0.9:
        pl.add_mesh(mesh.extract_feature_edges(30), color=edge, line_width=1.0)


def shot(pl, center, direction, zoom, path, title):
    d = np.array(direction, float)
    d /= np.linalg.norm(d)
    pl.camera_position = [tuple(center + d * 720.0), tuple(center), (0, 0, 1)]
    pl.reset_camera()
    pl.camera.zoom(zoom)
    pl.add_text(title, font_size=11, color="#1a222c")
    pl.enable_anti_aliasing("ssaa")
    pl.screenshot(path)
    pl.close()


def render_print_layout():
    pl = pv.Plotter(off_screen=True, window_size=(1800, 1100))
    pl.set_background("#f7f9fc")
    for ox in (-180.0, 180.0):
        bed = pv.Box(bounds=(ox - 150, ox + 150, -150, 150, -2, 0))
        pl.add_mesh(bed, color="#dde3ea")
        pl.add_mesh(bed.extract_feature_edges(45), color="#7a8694", line_width=1.2)

    parts = [
        ("matrix_16x16_arduino_housing.stl", (-180, -60, 0), "#2b3a4a", "#111922"),
        ("matrix_16x16_desk_stand.stl", (-180, 90, 0), "#3d6b99", "#183049"),
        ("matrix_16x16_baffle_insert.stl", (180, -45, 0), "#e8edf2", "#556270"),
        ("matrix_16x16_arduino_shell.stl", (115, 100, 0), "#4d6a62", "#1c3330"),
        ("matrix_16x16_arduino_cover.stl", (245, 100, 0), "#d7ddd8", "#3d4a44"),
    ]
    for name, (x, y, z), col, edge in parts:
        m = load_stl(name).translate((x, y, z), inplace=False)
        add(pl, m, col, edge)
    shot(
        pl,
        np.array([0, 20, 10]),
        (0.2, -1.1, 0.9),
        1.15,
        os.path.join(REND, "parts_print_layout.png"),
        "CR-10 print layout, every part flat on a face, supports off\n"
        "Housing 171 x 171 x 22.2    Shell 102.6 x 80.1 x 25.4    Cover 102.6 x 80.1 x 5.6\n"
        "Baffle 162.4 x 162.4 x 8.5    Desk stand 150 x 120 x 76",
    )


def render_exploded():
    pl = pv.Plotter(off_screen=True, window_size=(1700, 1200))
    pl.set_background("#f7f9fc")
    add(pl, load_stl("matrix_16x16_desk_stand.stl").transform(trans(0, -30, 0), inplace=False), "#3d6b99", "#183049")
    add(pl, load_stl("matrix_16x16_arduino_housing.stl").transform(HOUSING, inplace=False), "#2b3a4a", "#111922")
    add(pl, load_stl("matrix_16x16_arduino_shell.stl").transform(trans(0, 48, 0) @ SHELL, inplace=False), "#4d6a62", "#1c3330")
    add(pl, load_stl("matrix_16x16_arduino_cover.stl").transform(trans(0, 88, 8) @ COVER, inplace=False), "#e4ebe6", "#3d4a44")
    add(pl, load_stl("matrix_16x16_baffle_insert.stl").transform(trans(0, -55, 28) @ BAFFLE, inplace=False), "#e8edf2", "#556270")
    shot(
        pl,
        np.array([0, 10, 100]),
        (-1.15, -0.9, 0.55),
        1.15,
        os.path.join(REND, "exploded_assembly.png"),
        "Exploded: desk stand, housing, baffle (slides in from the top),\n"
        "Arduino shell on the back, cover with the R4 matrix window",
    )


def render_assembled(direction, name, title):
    pl = pv.Plotter(off_screen=True, window_size=(1700, 1200))
    pl.set_background("#f7f9fc")
    add(pl, load_stl("matrix_16x16_desk_stand.stl"), "#3d6b99", "#183049")
    add(pl, load_stl("matrix_16x16_arduino_housing.stl").transform(HOUSING, inplace=False), "#2b3a4a", "#111922")
    add(pl, load_stl("matrix_16x16_arduino_shell.stl").transform(SHELL, inplace=False), "#4d6a62", "#1c3330")
    add(pl, load_stl("matrix_16x16_arduino_cover.stl").transform(COVER, inplace=False), "#e4ebe6", "#3d4a44")
    add(pl, load_stl("matrix_16x16_baffle_insert.stl").transform(BAFFLE, inplace=False), "#f4f7fa", "#556270")
    # UNO R4 outline sitting on the standoffs, component side toward the cover.
    board = pv.Box(bounds=(-40.0, 28.58, 10.0, 63.34, -10.0, -8.4))
    add(pl, board.transform(HOUSING, inplace=False), "#14532d", "#0b3a1e", edges=False)
    leds = pv.Box(bounds=(5.96 - 12.4, 5.96 + 12.4, 29.18 - 8.2, 29.18 + 8.2, -10.15, -9.7))
    add(pl, leds.transform(HOUSING, inplace=False), "#ef4444", "#7f1d1d", edges=False)
    shot(pl, np.array([0, 0, 100]), direction, 1.2, os.path.join(REND, name), title)


if __name__ == "__main__":
    render_print_layout()
    render_exploded()
    render_assembled(
        (-1.15, -1.05, 0.55),
        "assembled_front_iso.png",
        "Front: same 16x16 bezel, baffle and diffuser slot as the original mount\n"
        "Desk stand uses the original four M3 holes (X = +/-30, Z = 36 and 66)",
    )
    render_assembled(
        (1.2, 1.15, 0.62),
        "assembled_back_iso.png",
        "Back: UNO R4 WiFi bay, USB-C and barrel slots on the left of the matrix,\n"
        "5 V entry on the right, cover window over the onboard 12x8 matrix",
    )
    print("wrote", REND)
