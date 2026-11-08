from nurb import *


@assembly
def matrix_16x16_assembly():
    base = use("matrix_16x16_base_mount")
    housing_pose = Pos(0.0, -10.0, 96.0) * Rot(90.0, 0.0, 0.0)
    housing = housing_pose * use("matrix_16x16_housing")
    baffle = housing_pose * Pos(0.0, 0.0, 7.7) * use("matrix_16x16_baffle_insert")
    return base, housing, baffle
