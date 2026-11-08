"""Shared layout for the 16x16 matrix housing with a rear Arduino UNO R4 WiFi bay.

The ElectroCookie terminal shield (EC_PS_UNO_TB_V1) stacks on the Uno. Its
drawing gives a 68.5 x 53.5 mm Uno-footprint board and 24 mm from the shield
underside to the top of the spring terminals. The headers, not the kit's
nylon standoffs, set the gap. See measurements.toml for which of those
numbers are still waiting on calipers.

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
# The shield uses the same corner, so its stacking headers land on these holes.
BOARD_X0 = -40.0
BOARD_Y0 = 10.0

# Cavity past the stacked keep-out on the header edges (±Y). An 8.5 mm cover
# boss has to sit between the 2 mm lip and the boards, which the old 10 mm
# margin could not do once the shield fills the whole Uno outline.
HEADER_MARGIN = 14.0

# M3x18 under the head. Grip is shield PCB + header gap + Uno PCB
# (1.6 + 11 + 1.6 = 14.2). The insert hole is 5 mm deep, so 18 mm leaves
# 3.8 mm of thread and does not bottom out. A longer screw hits the blind hole.
STACK_SCREW = 18.0


def layout():
    """Every mating number the housing, shell, cover and stand share."""
    board_l = measured("uno_board_length")
    board_w = measured("uno_board_width")
    board_t = measured("uno_board_thickness")
    shield_l = measured("tb_board_length")
    shield_w = measured("tb_board_width")
    shield_h = measured("tb_stack_height")
    shield_t = measured("tb_board_thickness")
    stack_gap = measured("tb_stack_gap")
    wire_clear = measured("tb_wire_clearance")
    wall = 2.4
    floor_t = 2.4
    standoff_h = 6.0
    # Floor of the cavity up to the cover. The 24 mm shield height starts at
    # the shield underside, so it already includes the shield PCB.
    terminal_top = standoff_h + board_t + stack_gap + shield_h
    interior_h = terminal_top + wire_clear
    corner_r = 8.0
    boss_wall = 2.2
    insert_hole = measured("insert_hole_dia")
    boss_od = insert_hole + 2.0 * boss_wall  # 8.5 mm

    # Union of the two boards, origin-aligned. The drawing's 68.5 x 53.5 and
    # the Uno's 68.58 x 53.34 differ by the rounding of 2.7 in and 2.1 in.
    keep_l = max(board_l, shield_l)
    keep_w = max(board_w, shield_w)

    # Interior cavity. Connector side (-X) stays 3.2 mm past the board so the
    # USB-C and barrel openings do not move. +X is the wiring bay. ±Y grew
    # from the old 10/12 mm margins so a cover boss fits off the shield.
    ix0 = BOARD_X0 - 3.2
    ix1 = BOARD_X0 + board_l + 26.0
    iy0 = BOARD_Y0 - HEADER_MARGIN
    iy1 = BOARD_Y0 + keep_w + HEADER_MARGIN
    ox0, oy0 = ix0 - wall, iy0 - wall
    ox1, oy1 = ix1 + wall, iy1 + wall

    holes = tuple((BOARD_X0 + hx, BOARD_Y0 + hy) for hx, hy in UNO_HOLES)

    # Shell-to-housing screws. On the 10 mm pixel grid so the heads sit between
    # LEDs, and on the X = +/-30 ribs that the original backplate leaves solid.
    mount_screws = ((-30.0, 30.0), (-30.0, 50.0), (30.0, 30.0), (30.0, 60.0))

    # Cover screws sit off the stacked boards, in the ±Y margins, and inboard
    # of the 2 mm lip. The old (±Y = 8 and 67) bosses bit into the Uno outline.
    cover_screws = ((-34.0, 4.4), (10.0, 4.4), (-14.0, 69.2), (40.0, 69.2))

    # Floor slot into the matrix pocket. Sits inside the original upper-right
    # backplate window (X 39..67, Y 11..63) so the housing does not need a new cut.
    cable_slot = (45.0, 42.0, 10.0, 14.0)  # cx, cy, w, h  -> X 40..50, Y 35..49
    # Zip-tie bridge across the +Y end of that slot. The remaining opening is
    # the -Y 6 mm of the slot (Y 35..41), still over the same window.
    matrix_bridge = (45.0, 46.0, 12.0, 10.0)  # cx, cy, x length, y length -> X 39..51, Y 41..51

    # Arcade-button exit on the -Y wall (down, toward the lamp when the matrix
    # is standing). Open at the rim so the leads drop in. The eye is just inboard.
    button_notch_x = 40.0
    button_notch_w = 12.0
    button_eye = (40.0, 3.0, 10.0, 10.0)  # cx, cy, x length, y length -> X 35..45, Y -2..8

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
        shield_l=shield_l,
        shield_w=shield_w,
        shield_h=shield_h,
        shield_t=shield_t,
        stack_gap=stack_gap,
        wire_clear=wire_clear,
        terminal_top=terminal_top,
        stack_screw=STACK_SCREW,
        keep_l=keep_l,
        keep_w=keep_w,
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
        matrix_bridge=matrix_bridge,
        button_notch_x=button_notch_x,
        button_notch_w=button_notch_w,
        button_eye=button_eye,
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


def crowned_profile(half_width, below, above):
    """Opening profile: a rectangle plus a 45 degree crown, origin at the middle.

    Local +Y is up. `below` and `above` are the straight flanks. The crown
    rises `half_width`, so the roof is 45 degrees and prints without a bridge.
    """
    from nurb import Polygon

    hw = half_width
    return Polygon(
        (-hw, -below),
        (hw, -below),
        (hw, above),
        (0.0, above + hw),
        (-hw, above),
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

    # Cable slot must stay inside the original upper-right window (X 39..67, Y 11..63)
    # and clear of the right-hand land, which starts 1.2 mm inside the cavity.
    if slot[0] < 39.5 or slot[1] > min(66.0, L["ix1"] - 1.4) or slot[2] < 12.0 or slot[3] > 62.0:
        raise RuntimeError(f"cable slot {slot} is not inside the existing backplate window")

    if L["wire_clear"] < 15.0:
        raise RuntimeError("tb_wire_clearance under 15 mm leaves no room for a wire and a screwdriver")
    if abs(L["interior_h"] - (L["terminal_top"] + L["wire_clear"])) > 1e-6:
        raise RuntimeError("interior height does not match the stack plus the wire clearance")

    # Stacked keep-out. Cover bosses and the button eye have to stay outside it.
    kx0 = L["board_x0"]
    kx1 = L["board_x0"] + L["keep_l"]
    ky0 = L["board_y0"]
    ky1 = L["board_y0"] + L["keep_w"]
    lip_inset = 0.3 + 2.0 + boss_r + 0.8

    def outside_keep(cx, cy, r, gap):
        qx = min(max(cx, kx0), kx1)
        qy = min(max(cy, ky0), ky1)
        return ((cx - qx) ** 2 + (cy - qy) ** 2) ** 0.5 >= r + gap

    for s in L["cover_screws"]:
        if not (L["ix0"] + lip_inset < s[0] < L["ix1"] - lip_inset and L["iy0"] + lip_inset < s[1] < L["iy1"] - lip_inset):
            raise RuntimeError(f"cover screw {s} hits the lip")
        if not outside_keep(s[0], s[1], boss_r, 1.0):
            raise RuntimeError(f"cover screw {s} hits the stacked boards")

    grip = L["shield_t"] + L["stack_gap"] + L["board_t"]
    if not (grip + 3.0 <= L["stack_screw"] <= grip + L["insert_depth"] - 0.3):
        raise RuntimeError(
            f"M3x{L['stack_screw']:.0f} does not suit a {grip:.1f} mm grip and a {L['insert_depth']:.1f} mm insert hole"
        )

    bx, by, bw, bh = L["button_eye"]
    if not outside_keep(bx, by, max(bw, bh) / 2.0, 0.6):
        raise RuntimeError("button-wire eye hits the stacked boards")
    if by - bh / 2.0 < L["iy0"] + 1.6:
        raise RuntimeError("button-wire eye melts into the shell wall")

    # The bridge must leave a real opening in the floor slot, toward -Y.
    mx, my, mw, mh = L["matrix_bridge"]
    if my - mh / 2.0 < slot[2] + 5.0 or my + mh / 2.0 > slot[3] + 2.2:
        raise RuntimeError("matrix zip bridge covers the floor slot or misses its end")
    if mx - mw / 2.0 > slot[0] or mx + mw / 2.0 < slot[1]:
        raise RuntimeError("matrix zip bridge does not reach the sides of the floor slot")
