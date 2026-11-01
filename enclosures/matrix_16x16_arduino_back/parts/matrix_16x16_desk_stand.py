from nurb import *

from system import teardrop


@part
def matrix_16x16_desk_stand(
    foot_thickness=6.0,
    cradle_height=10.5,
    pillar_width=12.0,
    pillar_thickness=12.0,
    draft=False,
):
    """foot_thickness: thickness of the desk foot (mm)
    cradle_height: height of the shelf the housing bottom edge sits on (mm)
    pillar_width: left-right width of each upright (mm)
    pillar_thickness: front-to-back thickness of each upright (mm)
    """
    m3_hole = measured("m3_clearance")
    pitch = measured("pixel_pitch")
    pcb_size = measured("matrix_pcb_size")

    if foot_thickness < 4.0:
        reject("foot_thickness under 4.0mm flexes under the uprights", param="foot_thickness")
    if cradle_height < foot_thickness + 3.0:
        reject("cradle_height must clear the foot", param="cradle_height")
    if pillar_width < 10.0:
        reject("pillar_width under 10.0mm leaves too little wall around the M3 bores", param="pillar_width")
    if pillar_thickness < 8.0:
        reject("pillar_thickness under 8.0mm is too thin for the upright", param="pillar_thickness")

    # Same hole math as matrix_16x16_base_mount, so this stand and that base
    # mount both bolt to the housing's four original M3 holes.
    housing_half = (pcb_size + 3.0 + 8.0) / 2.0
    z_hole_low = cradle_height + (housing_half - 6.0 * pitch)
    z_hole_high = cradle_height + (housing_half - 3.0 * pitch)
    pillar_top_z = z_hole_high + 10.0

    y_mount = -10.0
    y_cradle_front = y_mount - 22.0
    y_pillar_back = y_mount + pillar_thickness
    gusset_reach = 18.0
    y_gusset_back = y_pillar_back + gusset_reach

    foot_x = 150.0
    foot_y0 = -68.0
    foot_y1 = 52.0
    foot_y = foot_y1 - foot_y0
    foot_yc = (foot_y0 + foot_y1) / 2.0

    body = Pos(0.0, foot_yc, 0.0) * extrude(RectangleRounded(foot_x, foot_y, 8.0), amount=foot_thickness)

    gusset_toe_z = foot_thickness + 4.0
    gusset_top_z = gusset_toe_z + gusset_reach

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

    cross_w = 6.0 * pitch - pillar_width + 2.0
    body += Pos(0.0, y_mount + pillar_thickness / 2.0, cradle_height / 2.0) * Box(
        cross_w, pillar_thickness, cradle_height
    )
    # Shelf wide enough to carry the 171 mm housing bottom, not just the pillars.
    body += Pos(0.0, (y_cradle_front + y_mount) / 2.0, cradle_height / 2.0) * Box(
        140.0, y_mount - y_cradle_front, cradle_height
    )

    # Lighten the foot so the first layer stays under the warp threshold.
    for wx in (-52.0, 52.0):
        body -= Pos(wx, 8.0, -0.5) * extrude(RectangleRounded(28.0, 46.0, 6.0), amount=foot_thickness + 1.0)

    r_m3 = m3_hole / 2.0
    hole_len = pillar_thickness + 4.0
    y_hole_start = y_mount - 2.0
    for px in (-3.0 * pitch, 3.0 * pitch):
        for hz in (z_hole_low, z_hole_high):
            bore = Plane.XZ * Pos(px, hz) * teardrop(r_m3)
            body -= Pos(0.0, y_hole_start, 0.0) * extrude(bore, amount=-hole_len)

    if draft:
        return body

    keep = body.edges().filter_by(
        lambda e: e.bounding_box().min.Z > 0.1
        and (
            e.bounding_box().min.Z > pillar_top_z - 0.15
            or (
                e.bounding_box().max.Z <= foot_thickness + 0.15
                and abs(e.bounding_box().min.X) > foot_x / 2.0 - 8.5
            )
        )
    )
    keep = keep - concave_edges(body)
    return polish(body, keep, 1.0)
