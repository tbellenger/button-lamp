from nurb import *

from system import layout


@assembly
def matrix_16x16_arduino_assembly():
    """Housing stood on the desk stand, Arduino shell and cover on the back, baffle in the track."""
    L = layout()
    stand = use("matrix_16x16_desk_stand")
    housing_pose = Pos(0.0, -10.0, 96.0) * Rot(90.0, 0.0, 0.0)

    # Shell prints cavity-up. RotX(180) sends +Z to -Z and flips Y; mirroring
    # across XZ first puts Y back, so a shell point (x, y, z) lands at housing
    # (x, y, -z): the mating face stays on the backplate and the bay opens rearward.
    def onto_back(shape):
        flipped = shape.mirror(Plane.XZ)
        return housing_pose * Rot(180.0, 0.0, 0.0) * flipped

    housing = housing_pose * use("matrix_16x16_arduino_housing")
    shell = onto_back(use("matrix_16x16_arduino_shell"))
    # Cover prints plate-down, lip-up. The lip points back into the bay, so print
    # +Z maps toward the housing and the plate sits just outside the shell rim.
    cover = housing_pose * Pos(0.0, 0.0, -(L["shell_h"] + 3.0)) * use("matrix_16x16_arduino_cover")
    baffle = housing_pose * Pos(0.0, 0.0, 7.7) * use("matrix_16x16_baffle_insert")
    return stand, housing, shell, cover, baffle
