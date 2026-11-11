from nurb import *

from system import layout


@assembly
def matrix_16x16_arduino_assembly():
    """Housing stood on the desk stand, Arduino shell and cover on the back, baffle in the track."""
    L = layout()
    stand = use("matrix_16x16_desk_stand")
    housing_pose = Pos(0.0, -10.0, 96.0) * Rot(90.0, 0.0, 0.0)

    # Shell prints cavity-up. Turn it over about the horizontal axis through the
    # bay centre: (x, y, z) -> (x, 2*bay_cy - y, -z). That is a real rotation.
    # The mating face lands on the backplate and the bay opens rearward.
    def onto_back(shape):
        return housing_pose * Pos(0.0, 2.0 * L["bay_cy"], 0.0) * Rot(180.0, 0.0, 0.0) * shape

    housing = housing_pose * use("matrix_16x16_arduino_housing")
    shell = onto_back(use("matrix_16x16_arduino_shell"))
    # Cover prints plate-down, lip-up. The lip points back into the bay, so print
    # +Z maps toward the housing and the plate sits just outside the shell rim.
    cover = housing_pose * Pos(0.0, 0.0, -(L["shell_h"] + 3.0)) * use("matrix_16x16_arduino_cover")
    baffle = housing_pose * Pos(0.0, 0.0, 7.7) * use("matrix_16x16_baffle_insert")
    return stand, housing, shell, cover, baffle
