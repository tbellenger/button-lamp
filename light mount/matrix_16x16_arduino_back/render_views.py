"""Preview renders of the 16x16 matrix housing with the rear Arduino UNO R4 WiFi bay."""
import os
import numpy as np
import pyvista as pv

from system import layout

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
_L = layout()
COVER = HOUSING @ trans(0.0, 0.0, -(_L["shell_h"] + 3.0))
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


def envelope(mesh):
    xmin, xmax, ymin, ymax, zmin, zmax = mesh.bounds
    return "{:.1f} x {:.1f} x {:.1f}".format(xmax - xmin, ymax - ymin, zmax - zmin)


def render_print_layout():
    pl = pv.Plotter(off_screen=True, window_size=(1800, 1100))
    pl.set_background("#f7f9fc")
    for ox in (-180.0, 180.0):
        bed = pv.Box(bounds=(ox - 150, ox + 150, -150, 150, -2, 0))
        pl.add_mesh(bed, color="#dde3ea")
        pl.add_mesh(bed.extract_feature_edges(45), color="#7a8694", line_width=1.2)

    # Shell and cover moved down so the 86 mm footprint stays on the bed graphic.
    parts = [
        ("matrix_16x16_arduino_housing.stl", (-180, -60, 0), "#2b3a4a", "#111922"),
        ("matrix_16x16_desk_stand.stl", (-180, 90, 0), "#3d6b99", "#183049"),
        ("matrix_16x16_baffle_insert.stl", (180, -45, 0), "#e8edf2", "#556270"),
        ("matrix_16x16_arduino_shell.stl", (115, 40, 0), "#4d6a62", "#1c3330"),
        ("matrix_16x16_arduino_cover.stl", (250, 40, 0), "#d7ddd8", "#3d4a44"),
    ]
    for name, (x, y, z), col, edge in parts:
        m = load_stl(name).translate((x, y, z), inplace=False)
        add(pl, m, col, edge)
    sizes = {name: envelope(load_stl(name)) for name, _, _, _ in parts}
    shot(
        pl,
        np.array([0, 20, 10]),
        (0.2, -1.1, 0.9),
        1.15,
        os.path.join(REND, "parts_print_layout.png"),
        "CR-10 print layout, every part flat on a face, supports off\n"
        f"Housing {sizes['matrix_16x16_arduino_housing.stl']}    "
        f"Shell {sizes['matrix_16x16_arduino_shell.stl']}    "
        f"Cover {sizes['matrix_16x16_arduino_cover.stl']}\n"
        f"Baffle {sizes['matrix_16x16_baffle_insert.stl']}    "
        f"Desk stand {sizes['matrix_16x16_desk_stand.stl']}",
    )


def render_exploded():
    pl = pv.Plotter(off_screen=True, window_size=(1700, 1200))
    pl.set_background("#f7f9fc")
    add(pl, load_stl("matrix_16x16_desk_stand.stl").transform(trans(0, -30, 0), inplace=False), "#3d6b99", "#183049")
    add(pl, load_stl("matrix_16x16_arduino_housing.stl").transform(HOUSING, inplace=False), "#2b3a4a", "#111922")
    add(pl, load_stl("matrix_16x16_arduino_shell.stl").transform(trans(0, 80, 0) @ SHELL, inplace=False), "#4d6a62", "#1c3330")
    add(pl, load_stl("matrix_16x16_arduino_cover.stl").transform(trans(0, 155, 8) @ COVER, inplace=False), "#e4ebe6", "#3d4a44")
    add(pl, load_stl("matrix_16x16_baffle_insert.stl").transform(trans(0, -55, 28) @ BAFFLE, inplace=False), "#e8edf2", "#556270")
    shot(
        pl,
        np.array([0, 10, 100]),
        (-1.15, -0.9, 0.55),
        1.15,
        os.path.join(REND, "exploded_assembly.png"),
        "Exploded: desk stand, housing, baffle (slides in from the top),\n"
        "Arduino shell on the back, cover over the stacked terminal shield",
    )


def render_assembled(direction, name, title):
    pl = pv.Plotter(off_screen=True, window_size=(1700, 1200))
    pl.set_background("#f7f9fc")
    add(pl, load_stl("matrix_16x16_desk_stand.stl"), "#3d6b99", "#183049")
    add(pl, load_stl("matrix_16x16_arduino_housing.stl").transform(HOUSING, inplace=False), "#2b3a4a", "#111922")
    add(pl, load_stl("matrix_16x16_arduino_shell.stl").transform(SHELL, inplace=False), "#4d6a62", "#1c3330")
    add(pl, load_stl("matrix_16x16_arduino_cover.stl").transform(COVER, inplace=False), "#e4ebe6", "#3d4a44")
    add(pl, load_stl("matrix_16x16_baffle_insert.stl").transform(BAFFLE, inplace=False), "#f4f7fa", "#556270")
    shot(pl, np.array([0, 0, 100]), direction, 1.2, os.path.join(REND, name), title)


def board_box(x0, x1, y0, y1, z0, z1):
    xs, ys, zs = sorted((x0, x1)), sorted((y0, y1)), sorted((z0, z1))
    return pv.Box(bounds=(xs[0], xs[1], ys[0], ys[1], zs[0], zs[1]))


def add_stack(pl):
    """Uno, header gap and the terminal shield, in housing coordinates."""
    L = _L
    x0, y0 = L["board_x0"], L["board_y0"]
    floor = L["floor_t"]
    # Housing +Z points at the LEDs. The bay opens toward -Z.
    uno_bot = -(floor + L["standoff_h"])
    uno_top = uno_bot - L["board_t"]
    shield_bot = uno_top - L["stack_gap"]
    shield_pcb_top = shield_bot - L["shield_t"]
    terminal_top = shield_bot - L["shield_h"]

    uno = board_box(x0, x0 + L["board_l"], y0, y0 + L["board_w"], uno_top, uno_bot)
    add(pl, uno.transform(HOUSING, inplace=False), "#14532d", "#0b3a1e", edges=False)
    # Stacking headers along the long edges, filling the 11 mm gap.
    for hy in (y0 + 1.6, y0 + L["shield_w"] - 3.6):
        hdr = board_box(x0 + 6.0, x0 + L["shield_l"] - 8.0, hy, hy + 2.2, shield_bot, uno_top)
        add(pl, hdr.transform(HOUSING, inplace=False), "#1c1917", "#0c0a09", edges=False)
    pcb = board_box(x0, x0 + L["shield_l"], y0, y0 + L["shield_w"], shield_pcb_top, shield_bot)
    add(pl, pcb.transform(HOUSING, inplace=False), "#292524", "#0c0a09", edges=False)
    # Four terminal rows across the short axis. The drawing shows them between
    # the headers; the exact pitch is not dimensioned, so this is the picture.
    span0 = x0 + 16.0
    span1 = x0 + L["shield_l"] - 10.0
    step = (span1 - span0) / 4.0
    y_lo, y_hi = y0 + 8.0, y0 + L["shield_w"] - 8.0
    for i in range(4):
        cx = span0 + (i + 0.5) * step
        bar = board_box(cx - 3.6, cx + 3.6, y_lo, y_hi, terminal_top, shield_pcb_top)
        add(pl, bar.transform(HOUSING, inplace=False), "#9ca3af", "#374151", edges=False)


def render_bay():
    pl = pv.Plotter(off_screen=True, window_size=(1700, 1200))
    pl.set_background("#f7f9fc")
    add(pl, load_stl("matrix_16x16_desk_stand.stl"), "#3d6b99", "#183049")
    add(pl, load_stl("matrix_16x16_arduino_housing.stl").transform(HOUSING, inplace=False), "#2b3a4a", "#111922")
    add(pl, load_stl("matrix_16x16_arduino_shell.stl").transform(SHELL, inplace=False), "#4d6a62", "#1c3330", opacity=0.18)
    add_stack(pl)
    shot(
        pl,
        np.array([10, 30, 100]),
        (0.55, 1.35, 0.85),
        1.7,
        os.path.join(REND, "bay_boards.png"),
        "Open bay: Uno R4 on the standoffs, ElectroCookie shield stacked on the headers.\n"
        "15 mm of clearance above the terminals. Cover off so the boards stay visible.",
    )


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
        "Back: stacked Uno R4 and ElectroCookie shield under the cover,\n"
        "USB-C and barrel crowns on the left, 5 V entry on the right",
    )
    render_bay()
    print("wrote", REND)
