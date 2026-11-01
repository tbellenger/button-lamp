"""Shared layout for the 16x16 matrix housing with a rear Arduino UNO R4 WiFi bay.

Board coordinates come from Arduino's official ABX00087 manufacturing data:

- Outline: Gerber PCB.GM4, origin at the corner opposite the connector tab,
  X along the 68.58 mm length toward the tab, Y along the 53.34 mm width.
  USB-C and the barrel jack are on the X = 0 edge. Y = 0 is the analog/power
  edge. Y = 53.34 is the digital-header edge.
- Mounting holes: NC drill PCB-RoundHoles.TXT tool T8 (3.2 mm NPTH), four
  hits, read as Excellon 2:5 inch with trailing zeros omitted:
  (0.550, 0.100), (2.600, 0.300), (2.600, 1.400), (0.600, 2.000) in
  -> (13.97, 2.54), (66.04, 7.62), (66.04, 35.56), (15.24, 50.80) mm.
- USB-C, barrel jack and the 12x8 matrix were measured on the official
  UNO_R4_WIFI.step (downloaded from docs.arduino.cc).

Housing coordinates match matrix_16x16_housing: origin at the bezel center,
+Z toward the LEDs, +Y up when the matrix is standing on the lamp.
"""

from nurb import measured

# Official ABX00087 T8 mounting-hole centers, mm from the board origin above.
UNO_HOLES = (
    (13.97, 2.54),
    (66.04, 7.62),
    (66.04, 35.56),
    (15.24, 50.80),
)

# Board origin (connector-edge / analog-edge corner) in housing XY.
# Chosen so the bay sits above the original base-mount pillars (those contact
# the backplate only for local Y <= -20) and the 4 shell screws land on the
# solid X = +/-30 ribs between the original backplate windows.
BOARD_X0 = -40.0
BOARD_Y0 = 10.0


def layout():
    """Every mating number the housing, shell, cover and stand share."""
    board_l = measured("uno_board_length")
    board_w = measured("uno_board_width")
    board_t = measured("uno_board_thickness")
    wall = 2.4
    floor_t = 2.4
    standoff_h = 6.0
    interior_h = 23.0
    corner_r = 8.0
    boss_wall = 2.2
    insert_hole = measured("insert_hole_dia")
    boss_od = insert_hole + 2.0 * boss_wall  # 8.5 mm

    # Interior cavity. Connector side clears the 1.96 mm jack overhang.
    # +X past the board is the wiring bay (capacitor, 5 V entry, cable slot).
    # -Y / +Y margins clear the cover-screw bosses and the header pins.
    ix0 = BOARD_X0 - 3.2
    ix1 = BOARD_X0 + board_l + 26.0
    iy0 = BOARD_Y0 - 10.0
    iy1 = BOARD_Y0 + board_w + 12.0
    ox0, oy0 = ix0 - wall, iy0 - wall
    ox1, oy1 = ix1 + wall, iy1 + wall

    holes = tuple((BOARD_X0 + hx, BOARD_Y0 + hy) for hx, hy in UNO_HOLES)

    # Shell-to-housing screws. On the 10 mm pixel grid so the heads sit between
    # LEDs, and on the X = +/-30 ribs that the original backplate leaves solid.
    mount_screws = ((-30.0, 30.0), (-30.0, 50.0), (30.0, 30.0), (30.0, 60.0))

    # Cover screws, inset far enough that an 8.5 mm boss clears the 2 mm lip.
    cover_screws = ((-10.0, 8.0), (20.0, 8.0), (-8.0, 67.0), (46.0, 67.0))

    # Floor slot into the matrix pocket. Sits inside the original upper-right
    # backplate window (X 39..67, Y 11..63) so the housing does not need a new cut.
    cable_slot = (45.0, 42.0, 10.0, 14.0)  # cx, cy, w, h  -> X 40..50, Y 35..49

    # Capacitor channel in the wiring bay, open toward +Y (the cable slot).
    # Side-wall length 24.4 mm leaves ~22 mm inside for a 10x20 mm can.
    # Centered so the right wall stays 2 mm off the bay wall and the left wall
    # stays clear of the shell-screw nut at X = 30.
    cap = (44.0, 22.5, 12.4, 24.4)  # cx, cy, inner w, side-wall length
    power_notch_y = 56.0

    usb_y = BOARD_Y0 + measured("uno_usb_y")
    jack_y = BOARD_Y0 + measured("uno_jack_y")
    # Opening centers above the shell floor (board bottom rests on the standoffs).
    usb_z = standoff_h + board_t + measured("uno_usb_z")
    jack_z = standoff_h + board_t + measured("uno_jack_z")

    matrix_x = BOARD_X0 + measured("uno_matrix_cx")
    matrix_y = BOARD_Y0 + measured("uno_matrix_cy")

    info = dict(
        board_l=board_l,
        board_w=board_w,
        board_t=board_t,
        board_x0=BOARD_X0,
        board_y0=BOARD_Y0,
        wall=wall,
        floor_t=floor_t,
        standoff_h=standoff_h,
        interior_h=interior_h,
        shell_h=floor_t + interior_h,
        corner_r=corner_r,
        inner_r=corner_r - wall,
        boss_od=boss_od,
        insert_hole=insert_hole,
        insert_depth=measured("insert_length") + 1.0,
        ix0=ix0,
        ix1=ix1,
        iy0=iy0,
        iy1=iy1,
        ox0=ox0,
        ox1=ox1,
        oy0=oy0,
        oy1=oy1,
        holes=holes,
        mount_screws=mount_screws,
        cover_screws=cover_screws,
        cable_slot=cable_slot,
        cap=cap,
        power_notch_y=power_notch_y,
        usb_y=usb_y,
        jack_y=jack_y,
        usb_z=usb_z,
        jack_z=jack_z,
        matrix_x=matrix_x,
        matrix_y=matrix_y,
        # Lands filled back into the housing so the shell wall has plastic to sit on
        # where the original side windows pass behind it. (x0, x1, y0, y1)
        lands=(
            (ox0 - 0.4, ix0 + 1.2, oy0, oy1),
            (ix1 - 1.2, ox1 + 0.4, oy0, oy1),
        ),
    )
    _validate(info)
    return info


def teardrop(radius):
    """XZ profile of a horizontal bore: round hole plus a 45 degree crown.

    The point of the crown is `radius` above the bore center so every layer
    leans on the one below instead of bridging the crown. Same construction
    as matrix_16x16_base_mount.
    """
    from nurb import Circle, Polygon

    r = radius
    crown = r * 1.4142
    shoulder = r * 0.7071
    return Circle(r) + Polygon(
        (-shoulder, shoulder),
        (shoulder, shoulder),
        (0.0, crown),
        align=None,
    )


def _validate(L):
    """Refuse a layout whose bosses, slots and the lamp pillars occupy the same space."""
    def dist(a, b):
        return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5

    boss_r = L["boss_od"] / 2.0
    nut_r = 3.4
    pad = boss_r + nut_r + 0.6

    for h in L["holes"]:
        if not (L["ix0"] + boss_r < h[0] < L["ix1"] - boss_r and L["iy0"] + boss_r < h[1] < L["iy1"] - boss_r):
            raise RuntimeError(f"standoff {h} does not fit in the cavity")
        for s in L["mount_screws"]:
            if dist(h, s) < pad:
                raise RuntimeError(f"standoff {h} collides with shell screw {s}")
        for s in L["cover_screws"]:
            if dist(h, s) < L["boss_od"] + 0.8:
                raise RuntimeError(f"standoff {h} collides with cover boss {s}")

    cx, cy, cw, ch = L["cable_slot"]
    slot = (cx - cw / 2.0, cx + cw / 2.0, cy - ch / 2.0, cy + ch / 2.0)

    def hits_slot(p, r):
        return (slot[0] - r < p[0] < slot[1] + r) and (slot[2] - r < p[1] < slot[3] + r)

    for s in L["mount_screws"] + L["cover_screws"] + L["holes"]:
        if hits_slot(s, boss_r):
            raise RuntimeError(f"feature {s} falls in the cable slot")

    # Original pillars contact the backplate for housing Y <= -20.
    if L["oy0"] < -16.0:
        raise RuntimeError("shell bottom collides with the base-mount pillars")
    if L["ox0"] < -84.0 or L["ox1"] > 84.0 or L["oy1"] > 84.0:
        raise RuntimeError("shell falls off the 171 mm housing")

    # Cable slot must stay inside the original upper-right window (X 39..67, Y 11..63).
    if slot[0] < 39.5 or slot[1] > 66.0 or slot[2] < 12.0 or slot[3] > 62.0:
        raise RuntimeError(f"cable slot {slot} is not inside the existing backplate window")
