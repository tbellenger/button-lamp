from nurb import *


@part
def matrix_16x16_housing(
    pcb_clearance=1.0,
    diffuser_slot_gap=2.5,
    baffle_depth=8.5,
    bezel_wall=4.0,
    back_thickness=4.2,
    draft=False,
):
    """pcb_clearance: total extra pocket width over the 160.0mm flexible LED PCB (mm)
    diffuser_slot_gap: front slot depth for 1 or 2 stacked 1.0mm polycarbonate diffuser sheets (mm)
    baffle_depth: pocket height reserved for the 16x16 pixel baffle insert (mm)
    bezel_wall: outer perimeter wall thickness around the diffuser track (mm)
    back_thickness: rear mounting plate thickness carrying recessed M3 screw heads (mm)
    """
    pcb_size = measured("matrix_pcb_size")
    pcb_t = measured("matrix_pcb_thickness")
    diff_size = measured("diffuser_size")
    diff_t = measured("diffuser_sheet_thickness")
    pitch = measured("pixel_pitch")
    m3_hole = measured("m3_clearance")
    m3_head_d = measured("m3_head_dia")

    if pcb_clearance < 0.4:
        reject("pcb_clearance under 0.4mm will buckle the flexible 160mm PCB", param="pcb_clearance")
    if diffuser_slot_gap < diff_t + 0.3:
        reject(
            f"diffuser_slot_gap {diffuser_slot_gap:.1f}mm is narrower than one {diff_t:.1f}mm sheet plus clearance",
            param="diffuser_slot_gap",
        )
    if baffle_depth < 4.0:
        reject("baffle_depth under 4.0mm is too shallow for pixel isolation", param="baffle_depth")
    if bezel_wall < 3.0:
        reject("bezel_wall under 3.0mm is too thin for the perimeter frame", param="bezel_wall")
    if back_thickness < 3.6:
        reject("back_thickness under 3.6mm leaves too little floor under recessed M3 heads", param="back_thickness")

    pcb_pocket = pcb_size + pcb_clearance          # 161.0 mm (+/-80.5)
    track_width = diff_size + 1.0                  # 163.0 mm (+/-81.5)
    step_out = (track_width - pcb_pocket) / 2.0    # 1.0 mm 45-deg outward flare
    aperture = pcb_size - 1.2                      # 158.8 mm (+/-79.4) clear view
    corbel_in = (track_width - aperture) / 2.0     # 2.1 mm 45-deg inward bezel overhang
    rim_land = 1.4

    outer_size = track_width + 2.0 * bezel_wall    # 171.0 mm (+/-85.5)
    z0 = back_thickness                            # 4.2
    z1 = z0 + pcb_t                                # 6.7 (top of PCB pocket)
    z2 = z1 + step_out                             # 7.7 (floor of baffle/diffuser slide track)
    z3 = z2 + baffle_depth + diffuser_slot_gap     # 18.7 (top of diffuser track)
    z4 = z3 + corbel_in                            # 20.8 (top of 45-deg bezel corbel)
    z5 = z4 + rim_land                             # 22.2 (front face of bezel)

    # Concentric corner radii so every 45-deg lofted corner is a true 45.0-deg cone
    r_ap = 2.0
    r_pcb = r_ap + (pcb_pocket - aperture) / 2.0   # 2.0 + 1.1 = 3.1 mm
    r_trk = r_ap + corbel_in                       # 2.0 + 2.1 = 4.1 mm

    # 1. Outer block with 8.0mm rounded vertical corners
    body = extrude(RectangleRounded(outer_size, outer_size, 8.0), amount=z5)

    # 2. Lower PCB pocket (Z = z0..z2, closed on all 4 sides to capture the 160x160 PCB)
    s_z0 = Pos(0, 0, z0) * RectangleRounded(pcb_pocket, pcb_pocket, r_pcb)
    s_z1 = Pos(0, 0, z1) * RectangleRounded(pcb_pocket, pcb_pocket, r_pcb)
    s_z2 = Pos(0, 0, z2 + 0.02) * RectangleRounded(track_width, track_width, r_trk)
    body -= loft([s_z0, s_z1], ruled=True)
    body -= loft([s_z1, s_z2], ruled=True)

    # 3. Upper slide-in track & 45-deg front bezel corbel (Z = z2..z5, open through +Y top edge
    #    so both the 16x16 baffle insert and 162x162 diffuser sheet slide straight in from the top)
    ext_y = outer_size / 2.0 + 10.0
    trk_len = track_width / 2.0 + ext_y
    trk_yc = (ext_y - track_width / 2.0) / 2.0
    ap_len = aperture / 2.0 + ext_y
    ap_yc = (ext_y - aperture / 2.0) / 2.0

    u_z2 = Pos(0, trk_yc, z2) * RectangleRounded(track_width, trk_len, r_trk)
    u_z3 = Pos(0, trk_yc, z3) * RectangleRounded(track_width, trk_len, r_trk)
    u_z4 = Pos(0, ap_yc, z4) * RectangleRounded(aperture, ap_len, r_ap)
    u_z5 = Pos(0, ap_yc, z5 + 0.5) * RectangleRounded(aperture, ap_len, r_ap)

    body -= loft([u_z2, u_z3], ruled=True)
    body -= loft([u_z3, u_z4], ruled=True)
    body -= loft([u_z4, u_z5], ruled=True)

    # 4. Backplate ventilation & rear wire-exit windows (keeps first layer < 18,000 mm2 for zero warp risk
    #    while preserving vertical mounting ribs at X = +/-30.0 mm and central horizontal rib at Y = 0)
    for wy in (-37.0, 37.0):
        body -= Pos(0.0, wy, -0.5) * extrude(RectangleRounded(42.0, 52.0, 4.0), amount=z0 + 1.0)
        for wx in (-53.0, 53.0):
            body -= Pos(wx, wy, -0.5) * extrude(RectangleRounded(28.0, 52.0, 4.0), amount=z0 + 1.0)

    # 5. Four M3 mounting holes on the vertical ribs (X = +/-30.0 mm, Y = -60.0 and -30.0 mm,
    #    aligned with pixel grid intersections so no LED chip sits over a screw head).
    head_recess_z = z0 - 2.2
    for mx in (-3.0 * pitch, 3.0 * pitch):
        for my in (-6.0 * pitch, -3.0 * pitch):
            body -= Pos(mx, my, -0.5) * extrude(Circle(m3_hole / 2.0), amount=z0 + 1.0)
            body -= Pos(mx, my, head_recess_z) * extrude(Circle(m3_head_d / 2.0), amount=2.5)

    if draft:
        return body

    outer_half = outer_size / 2.0
    keep = body.edges().filter_by(
        lambda e: e.bounding_box().min.Z > 0.1
        and (
            e.bounding_box().min.Z > z5 - 0.1
            or (
                abs(e.bounding_box().min.X) > outer_half - 8.5
                and abs(e.bounding_box().min.Y) > outer_half - 8.5
            )
        )
    )
    keep = keep - concave_edges(body)
    return polish(body, keep, 1.0)
