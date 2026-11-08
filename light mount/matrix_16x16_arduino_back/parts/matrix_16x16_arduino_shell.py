from nurb import *

from system import crowned_profile, layout, teardrop


@part
def matrix_16x16_arduino_shell(
    wall=2.4,
    floor_thickness=2.4,
    standoff_height=6.0,
    interior_height=57.6,
    draft=False,
):
    """wall: bay wall thickness (mm)
    floor_thickness: floor that bolts against the matrix housing backplate (mm)
    standoff_height: height of the Arduino standoffs above the floor (mm)
    interior_height: open depth from the floor up to the cover rim (mm).
        This is the stacked shield (standoffs + Uno + header gap + 24 mm
        terminals) plus tb_wire_clearance. It has to match layout().
    """
    L = layout()
    m3_hole = measured("m3_clearance")
    nut_c = measured("m3_nut_across_corners")
    nut_t = measured("m3_nut_thickness")
    insert_hole = measured("insert_hole_dia")

    if wall < 2.0:
        reject("wall under 2.0mm is below the project minimum", param="wall")
    if floor_thickness < 2.0:
        reject("floor_thickness under 2.0mm will not hold the shell screws", param="floor_thickness")
    if standoff_height < measured("insert_length") + 1.5:
        reject("standoff_height leaves no plastic under the heat-set insert", param="standoff_height")
    need = (
        standoff_height
        + measured("uno_board_thickness")
        + measured("tb_stack_gap")
        + measured("tb_stack_height")
        + measured("tb_wire_clearance")
    )
    if interior_height + 1e-6 < need:
        reject(
            "interior_height is shorter than the stacked shield plus the wire clearance",
            param="interior_height",
        )

    # The printed part uses the keyword values. layout() is the mating contract
    # with the housing; these defaults are that contract, and moving one without
    # the other prints a bay that will not bolt on.
    if abs(wall - L["wall"]) > 1e-6 or abs(floor_thickness - L["floor_t"]) > 1e-6:
        reject("wall and floor_thickness are shared with the housing lands; leave them at the defaults")
    if abs(standoff_height - L["standoff_h"]) > 1e-6 or abs(interior_height - L["interior_h"]) > 1e-6:
        reject("standoff_height and interior_height are shared with the cover and the port positions")

    ox0, ox1, oy0, oy1 = L["ox0"], L["ox1"], L["oy0"], L["oy1"]
    ix0, ix1, iy0, iy1 = L["ix0"], L["ix1"], L["iy0"], L["iy1"]
    ocx, ocy = (ox0 + ox1) / 2.0, (oy0 + oy1) / 2.0
    icx, icy = (ix0 + ix1) / 2.0, (iy0 + iy1) / 2.0
    ow, oh = ox1 - ox0, oy1 - oy0
    iw, ih = ix1 - ix0, iy1 - iy0
    shell_h = floor_thickness + interior_height

    body = Pos(ocx, ocy, 0.0) * extrude(RectangleRounded(ow, oh, L["corner_r"]), amount=shell_h)
    body -= Pos(icx, icy, floor_thickness) * extrude(
        RectangleRounded(iw, ih, L["inner_r"]), amount=interior_height + 1.0
    )

    # Arduino standoffs. Blind Ø4.1 holes take a ruthex RX-M3Sx4.0 insert from above.
    boss_r = L["boss_od"] / 2.0
    hole_depth = L["insert_depth"]
    for hx, hy in L["holes"]:
        body += Pos(hx, hy, floor_thickness) * extrude(Circle(boss_r), amount=standoff_height)
        body -= Pos(hx, hy, floor_thickness + standoff_height - hole_depth) * extrude(
            Circle(insert_hole / 2.0), amount=hole_depth + 0.5
        )

    # Cover-screw bosses, stopped 3.2 mm short of the rim so the cover lip clears them.
    lip_clear = 3.2
    for sx, sy in L["cover_screws"]:
        body += Pos(sx, sy, floor_thickness) * extrude(Circle(boss_r), amount=interior_height - lip_clear)
        top = floor_thickness + interior_height - lip_clear
        body -= Pos(sx, sy, top - hole_depth) * extrude(Circle(insert_hole / 2.0), amount=hole_depth + 0.5)

    # Shell-to-housing screws. Nut drops in from above; the screw comes through the
    # housing backplate. Web under the nut is 1.6 mm.
    web = 1.6
    for sx, sy in L["mount_screws"]:
        body += Pos(sx, sy, 0.0) * extrude(Circle(nut_c / 2.0 + 2.2), amount=web + nut_t)
        body -= Pos(sx, sy, web) * extrude(RegularPolygon(nut_c / 2.0, 6, rotation=30.0), amount=nut_t + 0.4)
        body -= Pos(sx, sy, -0.5) * extrude(Circle(m3_hole / 2.0), amount=web + 1.0)

    # Cable slot through the floor, aligned with the housing's upper-right window.
    cx, cy, cw, ch = L["cable_slot"]
    body -= Pos(cx, cy, -0.5) * extrude(RectangleRounded(cw, ch, 2.0), amount=floor_thickness + 1.0)

    # USB-C and barrel openings. A slot open all the way up a 60 mm wall would
    # take the connector side out, and a flat bridge over a 14 mm hole sags.
    # The crown is 45 degrees, same idea as the desk-stand bores, so the roof
    # prints with the cavity facing up and no supports. z_center is already
    # the connector height above the cavity floor.
    def crowned_port(y_center, z_above_floor, width, below, above):
        z_center = floor_thickness + z_above_floor
        face = Plane.YZ * Pos(y_center, z_center) * crowned_profile(width / 2.0, below, above)
        span = (ix0 + 1.0) - (ox0 - 1.0)
        return Pos(ox0 - 1.0, 0.0, 0.0) * extrude(face, amount=span)

    body -= crowned_port(L["usb_y"], L["usb_z"], 15.0, 5.0, 6.0)
    body -= crowned_port(L["jack_y"], L["jack_z"], 14.0, 7.0, 6.0)

    # External 5 V lead: notch in the wiring-bay wall, open at the rim, plus a
    # teardrop zip-tie eye beside it.
    notch_y = L["power_notch_y"]
    notch_z0 = floor_thickness + 5.0
    notch_x0, notch_x1 = ix1 - 1.0, ox1 + 1.0
    body -= Pos((notch_x0 + notch_x1) / 2.0, notch_y, (notch_z0 + shell_h + 1.0) / 2.0) * Box(
        notch_x1 - notch_x0, 8.0, shell_h + 1.0 - notch_z0
    )
    # Zip-tie eye on solid floor just inboard of the notch. 10 mm wide so the
    # Ø4.2 teardrop keeps 2 mm of plastic on either side. The cover boss on
    # this side is at Y = 69.2, above the eye.
    eye_x = 46.0
    eye_y0, eye_y1 = 53.0, 61.0
    eye_z = floor_thickness + 8.0
    body += Pos(eye_x, (eye_y0 + eye_y1) / 2.0, floor_thickness) * Box(
        10.0, eye_y1 - eye_y0, 13.0, align=(Align.CENTER, Align.CENTER, Align.MIN)
    )
    bore = Plane.XZ * Pos(eye_x, eye_z) * teardrop(2.1)
    body -= Pos(0.0, eye_y0, 0.0) * extrude(bore, amount=eye_y1 - eye_y0)

    # Matrix leads: a bar across the +Y end of the floor slot, with a Ø4.2
    # teardrop the zip tie passes through. The -Y end of the slot stays open
    # into the matrix pocket. The bar fills the slot down to the bed, so the
    # hole is not a bridge.
    bx, by, bw, bh = L["matrix_bridge"]
    bridge_h = 14.0
    body += Pos(bx, by, 0.0) * Box(bw, bh, floor_thickness + bridge_h, align=(Align.CENTER, Align.CENTER, Align.MIN))
    bridge_bore = Plane.YZ * Pos(by, floor_thickness + 7.0) * teardrop(2.1)
    body -= Pos(bx - bw / 2.0, 0.0, 0.0) * extrude(bridge_bore, amount=bw)

    # Arcade buttons (red D8, white D4, and their grounds). Notch in the -Y
    # wall, open at the rim, and a zip-tie eye just inside. -Y is down toward
    # the lamp when the matrix is standing.
    bnx = L["button_notch_x"]
    bnw = L["button_notch_w"]
    bnz0 = floor_thickness + 4.0
    bny0, bny1 = oy0 - 1.0, iy0 + 1.0
    body -= Pos(bnx, (bny0 + bny1) / 2.0, (bnz0 + shell_h + 1.0) / 2.0) * Box(
        bnw, bny1 - bny0, shell_h + 1.0 - bnz0
    )
    ex, ey, ew, eh = L["button_eye"]
    body += Pos(ex, ey, floor_thickness) * Box(
        ew, eh, 13.0, align=(Align.CENTER, Align.CENTER, Align.MIN)
    )
    # Wires run in Y out the notch, so the zip-tie bore runs in X.
    button_bore = Plane.YZ * Pos(ey, floor_thickness + 8.0) * teardrop(2.1)
    body -= Pos(ex - ew / 2.0, 0.0, 0.0) * extrude(button_bore, amount=ew)

    # 1000 uF can lying on its side. Channel is open toward the cable slot.
    cap_x, cap_y, cap_w, cap_l = L["cap"]
    cap_wall = 2.4
    cap_h = 8.0
    body += Pos(cap_x, cap_y - cap_l / 2.0, floor_thickness) * Box(
        cap_w + 2.0 * cap_wall, cap_wall, cap_h, align=(Align.CENTER, Align.MIN, Align.MIN)
    )
    body += Pos(cap_x - cap_w / 2.0, cap_y, floor_thickness) * Box(
        cap_wall, cap_l, cap_h, align=(Align.MAX, Align.CENTER, Align.MIN)
    )
    body += Pos(cap_x + cap_w / 2.0, cap_y, floor_thickness) * Box(
        cap_wall, cap_l, cap_h, align=(Align.MIN, Align.CENTER, Align.MIN)
    )

    # Vent slots, 3 mm wide, short enough to bridge and clear of the ports.
    # The low row is the original one. The high row is there because the bay
    # is now deep enough to cover the shield, and the low slots alone would
    # leave the terminals in a closed pocket.
    def wall_vents(z0, height):
        zc = z0 + height / 2.0
        for vx in (-20.0, 0.0, 20.0):
            body_cuts.append(Pos(vx, (oy0 - 1.0 + iy0 + 1.0) / 2.0, zc) * Box(3.0, (iy0 + 1.0) - (oy0 - 1.0), height))
            body_cuts.append(Pos(vx, (iy1 - 1.0 + oy1 + 1.0) / 2.0, zc) * Box(3.0, (oy1 + 1.0) - (iy1 - 1.0), height))
        for vy in (36.0, 66.0):
            body_cuts.append(Pos((ix1 - 1.0 + ox1 + 1.0) / 2.0, vy, zc) * Box((ox1 + 1.0) - (ix1 - 1.0), 3.0, height))

    body_cuts = []
    wall_vents(floor_thickness + 6.0, 10.0)
    wall_vents(floor_thickness + 36.0, 12.0)
    for cut in body_cuts:
        body -= cut

    if draft:
        return body

    keep = body.edges().filter_by(
        lambda e: e.bounding_box().min.Z > 0.1
        and (
            e.bounding_box().min.Z > shell_h - 0.15
            or (
                e.bounding_box().max.Z - e.bounding_box().min.Z > shell_h - 1.0
                and (
                    e.bounding_box().min.X < ox0 + 0.2
                    or e.bounding_box().max.X > ox1 - 0.2
                    or e.bounding_box().min.Y < oy0 + 0.2
                    or e.bounding_box().max.Y > oy1 - 0.2
                )
            )
        )
    )
    keep = keep - concave_edges(body)
    return polish(body, keep, 0.6)
