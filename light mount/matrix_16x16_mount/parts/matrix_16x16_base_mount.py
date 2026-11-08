from nurb import *


@part
def matrix_16x16_base_mount(
    flange_thickness=5.0,
    cradle_height=10.5,
    pillar_width=12.0,
    pillar_thickness=12.0,
    gusset_reach=22.0,
    draft=False,
):
    """flange_thickness: thickness of the 92x92mm 4-bolt top-deck base plate (mm)
    cradle_height: height of the front support shelf above the lamp top deck (mm)
    pillar_width: left-right width of each twin upright mounting column (mm)
    pillar_thickness: front-back thickness of each upright mounting column (mm)
    gusset_reach: rear 45-degree gusset projection behind each column (mm)
    """
    flange_w = measured("top_deck_flange_size")
    bolt_sq = measured("top_deck_bolt_spacing")
    m3_hole = measured("m3_clearance")
    m3_head_d = measured("m3_head_dia")
    pitch = measured("pixel_pitch")
    pcb_size = measured("matrix_pcb_size")

    if flange_thickness < 4.0:
        reject("flange_thickness under 4.0mm flexes over the top-deck wire opening", param="flange_thickness")
    if cradle_height < flange_thickness + 4.0:
        reject("cradle_height must clear the M3 base bolt heads by at least 4mm", param="cradle_height")
    if pillar_width < 10.0:
        reject("pillar_width under 10.0mm leaves too little wall around the M3 bores", param="pillar_width")
    if pillar_thickness < 8.0:
        reject("pillar_thickness under 8.0mm is too thin for rigid upright support", param="pillar_thickness")
    if gusset_reach < 15.0:
        reject("gusset_reach under 15.0mm gives insufficient anti-tip bracing", param="gusset_reach")

    # Housing outer half-height is (162.0 + 1.0 + 2 * 4.0) / 2 = 85.5 mm.
    # Housing M3 holes sit at Y_local = -60.0 and -30.0 mm, so above the bottom edge (-85.5 mm)
    # they sit at +25.5 mm and +55.5 mm above cradle_height.
    housing_half = (pcb_size + 3.0 + 8.0) / 2.0
    z_hole_low = cradle_height + (housing_half - 6.0 * pitch)     # 10.5 + 25.5 = 36.0 mm
    z_hole_high = cradle_height + (housing_half - 3.0 * pitch)    # 10.5 + 55.5 = 66.0 mm
    pillar_top_z = z_hole_high + 10.0                             # 76.0 mm

    y_mount = -10.0                                               # front face where housing backplate bolts
    y_cradle_front = y_mount - 22.0                               # -32.0 mm (supports 22.2mm housing depth)
    y_pillar_back = y_mount + pillar_thickness                    # +2.0 mm
    y_gusset_back = y_pillar_back + gusset_reach                  # +24.0 mm

    # 1. Base flange: 92.0 x 92.0 mm with R8.0 corners
    body = extrude(RectangleRounded(flange_w, flange_w, 8.0), amount=flange_thickness)

    # 2. Twin upright pillars + front cradle shelf + rear 45-degree truncated gusset at X = +/-30.0 mm
    #    Drawn as a single clean YZ profile per side and extruded in X so there are no thin-web boolean seams.
    gusset_toe_z = flange_thickness + 5.0                         # 10.0 mm vertical toe (5mm tall)
    gusset_top_z = gusset_toe_z + gusset_reach                    # 32.0 mm (45-deg slope, clear of z_hole_low=36.0)

    yz_pts = [
        (y_cradle_front, 0.0),
        (y_gusset_back, 0.0),
        (y_gusset_back, gusset_toe_z),
        (y_pillar_back, gusset_top_z),
        (y_pillar_back, pillar_top_z),
        (y_mount, pillar_top_z),
        (y_mount, cradle_height),
        (y_cradle_front, cradle_height),
    ]
    side_profile = Plane.YZ * Polygon(*yz_pts, align=None)
    pillar_solid = extrude(side_profile, amount=pillar_width / 2.0, both=True)

    for px in (-3.0 * pitch, 3.0 * pitch):
        body += Pos(px, 0.0, 0.0) * pillar_solid

    # 3. Crossbar bridge connecting the two pillars between Z = 0 and cradle_height at Y = y_mount..y_pillar_back
    cross_w = 6.0 * pitch - pillar_width + 2.0
    cross_d = pillar_thickness
    body += Pos(0.0, y_mount + cross_d / 2.0, cradle_height / 2.0) * Box(cross_w, cross_d, cradle_height)

    # 4. Central + rear wire pass-through bay in the flange:
    #    Covers BOTH the (0, 0) center hole AND the SendCutSend laser-cut Ø10mm wire hole at (+11.71, +32.24).
    wire_bay_w = 36.0
    wire_bay_l = 37.0
    wire_bay_yc = 21.5                                            # spans Y = +3.0 .. +40.0 mm
    body -= Pos(0.0, wire_bay_yc, -0.5) * extrude(
        RectangleRounded(wire_bay_w, wire_bay_l, 6.0), amount=flange_thickness + 1.0
    )
    # Also open a front-center wire drop window (Y = -28.0 .. -11.0 mm) for center-routed leads
    body -= Pos(0.0, -19.5, -0.5) * extrude(
        RectangleRounded(36.0, 17.0, 5.0), amount=flange_thickness + 1.0
    )

    # 5. Four top-deck M3 mounting holes on the 76.2 x 76.2 mm (3.000 in) square pattern
    half_bolt = bolt_sq / 2.0
    for bx in (-half_bolt, half_bolt):
        for by in (-half_bolt, half_bolt):
            body -= Pos(bx, by, -0.5) * extrude(Circle(m3_hole / 2.0), amount=flange_thickness + 1.0)

    # 6. Four horizontal M3 mounting holes through the twin pillars (X = +/-30.0, Z = 36.0 and 66.0)
    #    Uses a 45-degree teardrop crown so horizontal bores print round with zero bridge sag.
    r_m3 = m3_hole / 2.0
    td_top = r_m3 * 1.4142
    td_x = r_m3 * 0.7071
    hole_len = pillar_thickness + 4.0
    y_hole_start = y_mount - 2.0

    for px in (-3.0 * pitch, 3.0 * pitch):
        for hz in (z_hole_low, z_hole_high):
            cyl_cut = Plane.XZ * Pos(px, hz) * Circle(r_m3)
            roof_cut = Plane.XZ * Polygon(
                (px - td_x, hz + td_x),
                (px + td_x, hz + td_x),
                (px, hz + td_top),
                align=None,
            )
            bore_2d = cyl_cut + roof_cut
            body -= Pos(0.0, y_hole_start, 0.0) * extrude(bore_2d, amount=-hole_len)

    if draft:
        return body

    # Polish outer flange vertical corners and top of pillars (exclude bed Z=0, concave edges, and gusset webs)
    flange_half = flange_w / 2.0
    keep = body.edges().filter_by(
        lambda e: e.bounding_box().min.Z > 0.1
        and (
            e.bounding_box().min.Z > pillar_top_z - 0.1
            or (
                e.bounding_box().max.Z <= flange_thickness + 0.1
                and abs(e.bounding_box().min.X) > flange_half - 8.5
                and abs(e.bounding_box().min.Y) > flange_half - 8.5
            )
        )
    )
    keep = keep - concave_edges(body)
    return polish(body, keep, 1.0)
