from nurb import *


@part
def matrix_16x16_baffle_insert(
    grid_count=16,
    baffle_depth=8.5,
    grid_wall=1.2,
    frame_rim=1.8,
    draft=False,
):
    """grid_count: number of LED pixels per side (16 for the 16x16 WS2812B matrix)
    baffle_depth: height of the pixel isolation walls between the LEDs and the diffuser (mm)
    grid_wall: thickness of the internal pixel divider walls (mm, 3 perimeters at a 0.4 mm nozzle)
    frame_rim: outer perimeter border thickness around the pixel grid (mm)
    """
    pitch = measured("pixel_pitch")
    led_size = measured("led_package_size")

    if grid_count < 2:
        reject("grid_count must be at least 2", param="grid_count")
    if grid_wall < 0.8:
        reject("grid_wall under 0.8mm cannot print as two clean perimeters", param="grid_wall")
    cell_open = pitch - grid_wall
    if cell_open < led_size + 1.0:
        reject(
            f"cell opening {cell_open:.1f}mm is too narrow for the {led_size:.1f}mm 5050 LED package; reduce grid_wall",
            param="grid_wall",
        )
    if baffle_depth < 4.0:
        reject("baffle_depth under 4.0mm leaves LED hot-spots on the diffuser", param="baffle_depth")
    if frame_rim < 1.2:
        reject("frame_rim under 1.2mm is too thin to seat in the housing pocket", param="frame_rim")

    # Identical envelope to enclosures/matrix_16x16_mount so either print drops
    # into this housing: 162.4 x 162.4 x 8.5 mm, 0.3 mm per side in the 163 mm track.
    grid_span = grid_count * pitch
    outer_size = grid_span - grid_wall + 2.0 * frame_rim
    corner_r = frame_rim + 0.8

    slab = extrude(RectangleRounded(outer_size, outer_size, corner_r), amount=baffle_depth)
    if not draft:
        top_edges = slab.edges().filter_by(lambda e: e.bounding_box().min.Z > baffle_depth - 0.1)
        slab = polish(slab, top_edges, 0.8)

    cells = extrude(
        Sketch() + GridLocations(pitch, pitch, grid_count, grid_count) * Rectangle(cell_open, cell_open),
        amount=baffle_depth + 2.0,
    )
    return slab - Pos(0, 0, -1.0) * cells
