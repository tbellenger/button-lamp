from nurb import *

from system import layout


@part
def matrix_16x16_arduino_cover(
    plate_thickness=3.0,
    lip_height=2.6,
    lip_thickness=2.0,
    fit_clearance=0.3,
    draft=False,
):
    """plate_thickness: cover plate thickness (mm)
    lip_height: how far the locating lip drops into the shell (mm)
    lip_thickness: lip wall thickness (mm)
    fit_clearance: extra opening around the lip, total per side, against the shell cavity (mm)
    """
    L = layout()
    m3_hole = measured("m3_clearance")

    if plate_thickness < 2.0:
        reject("plate_thickness under 2.0mm is below the project minimum", param="plate_thickness")
    if lip_thickness < 2.0:
        reject("lip_thickness under 2.0mm is below the project minimum", param="lip_thickness")
    if fit_clearance < 0.3:
        reject("fit_clearance under 0.3mm will bind on a second printer", param="fit_clearance")
    if lip_height > 3.0:
        reject("lip_height over 3.0mm hits the cover-screw bosses", param="lip_height")

    ox0, ox1, oy0, oy1 = L["ox0"], L["ox1"], L["oy0"], L["oy1"]
    ix0, ix1, iy0, iy1 = L["ix0"], L["ix1"], L["iy0"], L["iy1"]
    ocx, ocy = (ox0 + ox1) / 2.0, (oy0 + oy1) / 2.0
    ow, oh = ox1 - ox0, oy1 - oy0

    # Lip is 0.3 mm per side smaller than the cavity, and its corner radius
    # shrinks by the same amount so the gap stays even around the corners.
    lip_w = (ix1 - ix0) - 2.0 * fit_clearance
    lip_h = (iy1 - iy0) - 2.0 * fit_clearance
    lip_r = max(1.0, L["inner_r"] - fit_clearance)
    lcx, lcy = (ix0 + ix1) / 2.0, (iy0 + iy1) / 2.0

    body = Pos(ocx, ocy, 0.0) * extrude(RectangleRounded(ow, oh, L["corner_r"]), amount=plate_thickness)
    body += Pos(lcx, lcy, plate_thickness) * extrude(
        RectangleRounded(lip_w, lip_h, lip_r), amount=lip_height
    )
    # Hollow the lip so it is a 2 mm wall, not a plug.
    inner_w = lip_w - 2.0 * lip_thickness
    inner_h = lip_h - 2.0 * lip_thickness
    body -= Pos(lcx, lcy, plate_thickness) * extrude(
        RectangleRounded(inner_w, inner_h, max(0.8, lip_r - lip_thickness)),
        amount=lip_height + 1.0,
    )

    # Notch the lip where the shell slots are open, so the lip does not close
    # the top of the USB, barrel or 5 V entry. The plate still caps them.
    notch_top = plate_thickness + lip_height + 0.5

    def lip_notch(x, y, w, h):
        return Pos(x, y, (plate_thickness + notch_top) / 2.0) * Box(w, h, notch_top - plate_thickness + 0.2)

    # Cut from outside the plate, through the lip, and a little past it. Stopping
    # the cut inside the lip leaves a hairline face.
    def edge_notch(y, opening, on_left):
        reach = lip_thickness + 2.0
        if on_left:
            x0, x1 = ox0 - 1.0, ix0 + reach
        else:
            x0, x1 = ix1 - reach, ox1 + 1.0
        return lip_notch((x0 + x1) / 2.0, y, x1 - x0, opening + 2.0 * fit_clearance)

    body -= edge_notch(L["usb_y"], 15.0, True)
    body -= edge_notch(L["jack_y"], 14.0, True)
    body -= edge_notch(L["power_notch_y"], 8.0, False)

    for sx, sy in L["cover_screws"]:
        body -= Pos(sx, sy, -0.5) * extrude(Circle(m3_hole / 2.0), amount=plate_thickness + 1.5)

    # Window over the R4's 12x8 matrix. 28 x 20 mm clears the 24.8 x 16.4 mm
    # LED extent measured on the official STEP, with about 1.5 mm of margin.
    body -= Pos(L["matrix_x"], L["matrix_y"], -0.5) * extrude(
        RectangleRounded(28.0, 20.0, 3.0), amount=plate_thickness + 1.0
    )

    # A few vents in the plate, clear of the window and the screw holes.
    for vx, vy in ((-28.0, 40.0), (28.0, 18.0), (28.0, 58.0)):
        body -= Pos(vx, vy, -0.5) * extrude(RectangleRounded(3.0, 14.0, 1.2), amount=plate_thickness + 1.0)

    if draft:
        return body

    keep = body.edges().filter_by(
        lambda e: e.bounding_box().min.Z > plate_thickness + lip_height - 0.15
        or (
            e.bounding_box().min.Z > 0.1
            and e.bounding_box().max.Z < plate_thickness + 0.15
            and (
                e.bounding_box().min.X < ox0 + 0.3
                or e.bounding_box().max.X > ox1 - 0.3
                or e.bounding_box().min.Y < oy0 + 0.3
                or e.bounding_box().max.Y > oy1 - 0.3
            )
        )
    )
    keep = keep - concave_edges(body)
    return polish(body, keep, 0.6)
