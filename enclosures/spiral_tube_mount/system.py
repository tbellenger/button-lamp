"""Shared geometry for the spiral tube mount.

Two printed parts share one helix:

- ``spiral_tube_base`` bolts to the lamp's top deck and carries four C-clips.
  The clips are 90 degrees apart and one quarter-pitch apart in height, so the
  first turn of the tube climbs at the real helix slope.
- ``spiral_tube_clip`` is one identical sleeve. Its saddles cradle the turn
  above and the turn below. Clips go on alternating turns (2, 4, 6, ...),
  in line with the base clips, so every gap has exactly one saddle and the
  clips never meet.

All lengths are millimetres. The coil diameter is the outside diameter of the
finished coil (tube included), 12.5 percent under the original printed spiral.
"""

import math
from types import SimpleNamespace

from nurb import Circle, Cylinder, Polygon, Pos, Rectangle, Rot, Box, Plane, Vector, extrude, measured, reject

# Bore is a hair over the tube so the silicone and the LED strip are not crushed.
# The snap opening, not the bore, is what holds the tube.
BORE_CLEAR = 0.2
FLANGE_T = 4.0
SPOKE_W = 10.0
# Flat the base clip sits on, each side of centre. 8 mm total. The sides rise at 45 degrees.
PED_HALF = 4.0
# Saddle is this much narrower than the snap opening, total, so it nests in the base clip.
SADDLE_SIDE_CLEAR = 1.5
# First base clip sits this many degrees of helix past the deck wire hole.
LEAD_IN_DEG = 40.0
# How far the foot blank extends below the clip before the bed plane cuts it.
FOOT_DROP = 80.0


def specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, pad_thickness, clip_width):
    """Derived sizes, or a refusal when a combination cannot be printed or assembled."""
    if tube_od < 8.0 or tube_od > 20.0:
        reject("tube_od must stay between 8 and 20 mm for this sleeve", param="tube_od")
    if gap < 6.0:
        reject("gap under 6 mm leaves no room for a saddle between turns", param="gap")
    if clip_wall < 1.6:
        reject("clip_wall under 1.6 mm is under four perimeters on a 0.4 mm nozzle", param="clip_wall")
    if clip_width < 10.0 or clip_width > 20.0:
        reject("clip_width must stay between 10 and 20 mm along the tube", param="clip_width")
    if snap_opening >= tube_od - 0.3:
        reject(
            f"snap_opening {snap_opening:.1f} mm is not narrower than the {tube_od:.1f} mm tube; "
            "drop it at least 0.4 mm under the tube",
            param="snap_opening",
        )

    pitch = tube_od + gap
    radius = (coil_diameter - tube_od) / 2.0
    if radius < 24.0:
        reject(
            "coil_diameter is too tight for a 13 mm tube on this deck; raise it",
            param="coil_diameter",
        )

    rb = tube_od / 2.0 + BORE_CLEAR
    ro = rb + clip_wall
    # Inner surface of an upward-opening cradle. Past 45 degrees above the equator
    # the lip overhangs when the base is printed openings-up.
    if snap_opening / 2.0 >= rb:
        reject("snap_opening is wider than the bore, so the clip cannot close on the tube", param="snap_opening")
    lip_above_equator = math.degrees(math.acos((snap_opening / 2.0) / rb))
    if lip_above_equator > 42.0:
        need = 2.0 * rb * math.cos(math.radians(42.0))
        reject(
            f"snap_opening {snap_opening:.1f} mm wraps so far over the tube that the base clip "
            f"overhangs; raise it to at least {need:.1f} mm",
            param="snap_opening",
        )

    half_chord = snap_opening / 2.0 - SADDLE_SIDE_CLEAR / 2.0
    if half_chord < 3.0:
        reject(
            "snap_opening is too narrow to leave a saddle that nests in the base clip",
            param="snap_opening",
        )
    if half_chord >= tube_od / 2.0:
        reject(
            "snap_opening leaves a saddle wider than the tube; lower it",
            param="snap_opening",
        )
    if pad_thickness < 2.4:
        reject("pad_thickness under 2.4 mm will not print as a solid spacer stem", param="pad_thickness")
    if pad_thickness > 2.0 * half_chord - 1.0:
        reject(
            f"pad_thickness {pad_thickness:.1f} mm is wider than the saddle "
            f"({2 * half_chord:.1f} mm), so the stem hits the base-clip arms; lower it",
            param="pad_thickness",
        )

    alpha = math.atan(pitch / (2.0 * math.pi * radius))
    # Saddle centres sit this far apart in the plane normal to the tube.
    # Installed on the helix, that is exactly one pitch in world Z.
    saddle_dist = pitch * math.cos(alpha)
    neck = saddle_dist - tube_od / 2.0 - rb - clip_wall
    if neck < 2.0:
        reject(
            f"gap {gap:.1f} mm leaves only {neck:.1f} mm of plastic between the sleeve and the saddle; raise gap",
            param="gap",
        )
    # The real neighbour tube is this far along the clip from the saddle's midplane.
    along = pitch * math.sin(alpha)
    if clip_width / 2.0 < along + 2.0:
        reject(
            f"clip_width {clip_width:.1f} mm is shorter than the helix needs "
            f"(the next turn meets the saddle {along:.1f} mm off centre); lengthen the clip",
            param="clip_width",
        )

    deck = _deck()
    keep_r = deck.keep / 2.0 - 0.4
    if radius + ro > keep_r:
        reject(
            "coil_diameter plus the clip wall hangs past the 92 mm deck footprint; lower coil_diameter or clip_wall",
            param="coil_diameter",
        )
    wire_r = math.hypot(deck.wire_x, deck.wire_y)
    opening_r = deck.wire_d / 2.0 + 2.0
    if abs(radius - wire_r) > opening_r - 1.0:
        reject(
            "coil_diameter moves the tube off the deck's wire hole; keep the tube centre near 34 mm from the bolt centre",
            param="coil_diameter",
        )

    ring_in = min(radius - 12.0, wire_r - opening_r - 2.5)
    ring_out = max(radius + ro + 1.2, wire_r + opening_r + 2.2)
    if ring_out > keep_r:
        reject(
            "the base ring would hang past the 92 mm footprint; lower coil_diameter",
            param="coil_diameter",
        )
    # Spoke tips stay inside the square, including the half-width of the spoke.
    spoke_r_out = keep_r * math.sqrt(2.0) - SPOKE_W / 2.0 - 0.4
    bolt_r = deck.bolt_sq / math.sqrt(2.0)
    if spoke_r_out < bolt_r + deck.hole_d / 2.0 + 3.0:
        reject("spokes cannot reach the M3 holes and stay inside the 92 mm square", param="coil_diameter")

    z0 = _lowest_clip_z(ro, alpha, clip_width)
    arc = math.hypot(2.0 * math.pi * radius, pitch)
    strip = deck.strip
    rise = strip / arc * pitch
    return SimpleNamespace(
        tube_od=tube_od,
        coil_diameter=coil_diameter,
        gap=gap,
        clip_wall=clip_wall,
        snap_opening=snap_opening,
        pad_thickness=pad_thickness,
        clip_width=clip_width,
        pitch=pitch,
        radius=radius,
        rb=rb,
        ro=ro,
        alpha=alpha,
        saddle_dist=saddle_dist,
        half_chord=half_chord,
        beta=math.asin(half_chord / (tube_od / 2.0)),
        along=along,
        neck=neck,
        z0=z0,
        arc=arc,
        turns=strip / arc,
        rise=rise,
        ring_in=ring_in,
        ring_out=ring_out,
        spoke_r_out=spoke_r_out,
        bolt_r=bolt_r,
        opening_r=opening_r,
        wire_r=wire_r,
        deck=deck,
        angles=clip_angles(deck),
    )


def _deck():
    return SimpleNamespace(
        bolt_sq=measured("top_deck_bolt_spacing"),
        hole_d=measured("m3_clearance"),
        wire_x=measured("top_deck_wire_offset_x"),
        wire_y=measured("top_deck_wire_offset_y"),
        wire_d=measured("top_deck_wire_hole_dia"),
        keep=measured("top_deck_flange_size"),
        strip=measured("strip_length"),
    )


def _lowest_clip_z(ro, alpha, clip_width):
    """Tube-centre height of the lowest base clip.

    The flat under the clip is tilted with the helix, so the downhill corner is
    the low point. It embeds 0.3 mm into the ring top, which fuses the tower
    to the ring without reaching the bore.
    """
    return FLANGE_T + ro * math.cos(alpha) + (clip_width / 2.0) * math.sin(alpha) - 0.3


def clip_angles(deck):
    """Four angles, lowest first, climbing counterclockwise off the wire hole."""
    wire = math.degrees(math.atan2(deck.wire_y, deck.wire_x))
    return tuple((wire + LEAD_IN_DEG + 90.0 * k) % 360.0 for k in range(4))


def frame_at(spec, theta, z):
    """Plane at a tube centre. Local +X is radial out, +Y is up across the tube, +Z runs along it."""
    radial = (math.cos(theta), math.sin(theta), 0.0)
    tangent = _tangent_pitched(theta, spec.radius, spec.pitch)
    up = _up(tangent)
    z_axis = _cross(radial, up)
    origin = (spec.radius * radial[0], spec.radius * radial[1], z)
    return Plane(origin=Vector(*origin), x_dir=Vector(*radial), z_dir=Vector(*z_axis))


def _tangent_pitched(theta, radius, pitch):
    c = pitch / (2.0 * math.pi)
    tx, ty, tz = -radius * math.sin(theta), radius * math.cos(theta), c
    n = math.hypot(tx, ty, tz)
    return (tx / n, ty / n, tz / n)


def _up(tangent):
    """World up, flattened into the plane normal to the tube, then normalised."""
    tz = tangent[2]
    ux, uy, uz = -tangent[0] * tz, -tangent[1] * tz, 1.0 - tz * tz
    n = math.hypot(ux, uy, uz)
    return (ux / n, uy / n, uz / n)


def _cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def clip_profile(spec):
    """Cross-section of the standalone clip, in the plane normal to the tube.

    +X opens outward (the snap). +Y is toward the next turn. The sleeve is a
    band around the middle tube. A stem of ``pad_thickness`` rises and falls
    into saddle heads. Each saddle is a circular cradle of the tube radius,
    centred exactly one installed pitch away.
    """
    ro = spec.ro
    sd = spec.saddle_dist
    rt = spec.tube_od / 2.0
    # Stop the cradle short of a knife edge. `half_chord` is as wide as the
    # base opening allows; the last 1.2 mm of that width is a flat land beside
    # the arc so the lip prints as a real wall. The arc's crown is unchanged,
    # so the turn still sits at exactly one pitch.
    land = 1.2
    reach = spec.half_chord - land
    phi = math.asin(reach / rt)
    lip = sd - rt * math.cos(phi)
    crown = sd - rt
    sleeve = Circle(ro)
    # The saddle head is only the cradle itself. The stem between the sleeve
    # and that cradle is `pad_thickness` wide, which is the vertical bar in
    # the side view. The head is wider so the cradle can be a real arc, and
    # it still fits through the base clip's upward opening.
    head_h = lip - (crown - 0.6)
    head_c = (lip + crown - 0.6) / 2.0
    head = Pos(0, head_c) * Rectangle(2.0 * spec.half_chord, head_h)
    head += Pos(0, -head_c) * Rectangle(2.0 * spec.half_chord, head_h)
    stem = Rectangle(spec.pad_thickness, 2.0 * (crown + 0.4))
    bore = Circle(spec.rb)
    # Slot opens the bore to +X. It starts at the bore centre so the lips remain.
    slot = Pos(ro, 0) * Rectangle(2.0 * ro + 2.0, spec.snap_opening)
    top = Pos(0, sd) * Circle(spec.tube_od / 2.0)
    bot = Pos(0, -sd) * Circle(spec.tube_od / 2.0)
    return sleeve + head + stem - bore - slot - top - bot


def clip_centered(spec):
    """Clip with the sleeve centre at the origin and the tube axis on Z, from -w/2 to w/2."""
    return extrude(clip_profile(spec), amount=spec.clip_width / 2.0, both=True)


def base_clip_profile(spec):
    """Upward-opening C with a flat foot and 45 degree sides, so the base prints support-free.

    The top is open. The standalone clip's lower saddle nests in that opening,
    which is why the base clip does not wrap over the tube.
    """
    ro = spec.ro
    ped = PED_HALF
    if ped >= ro - 1.0:
        reject("clip_wall is too small for the 45 degree foot under the base clip", param="clip_wall")
    y_bot = -ro
    y_shoulder = y_bot + (ro - ped)  # 45 degrees: run equals rise
    y_top = ro
    outer = Polygon(
        (-ped, y_bot),
        (ped, y_bot),
        (ro, y_shoulder),
        (ro, y_top),
        (-ro, y_top),
        (-ro, y_shoulder),
        align=None,
    )
    # Foot blank. The bed plane cuts it flat after the clip is stood on the helix.
    foot = Pos(0, y_bot - FOOT_DROP / 2.0 + 0.4) * Rectangle(2.0 * ped, FOOT_DROP)
    bore = Circle(spec.rb)
    slot = Pos(0, ro) * Rectangle(spec.snap_opening, 2.0 * ro + 2.0)
    return outer + foot - bore - slot


def base_clip_centered(spec):
    return extrude(base_clip_profile(spec), amount=spec.clip_width / 2.0, both=True)


def above_bed():
    """Half-space z >= 0, large enough to contain the base."""
    return Pos(0, 0, 400) * Box(900, 900, 800)


def ring_and_spokes(spec):
    deck = spec.deck
    ring = extrude(Circle(spec.ring_out) - Circle(spec.ring_in), amount=FLANGE_T)
    body = ring
    length = spec.spoke_r_out - (spec.ring_in + 2.0)
    mid = spec.ring_in + 2.0 + length / 2.0
    for ang in (45.0, 135.0, 225.0, 315.0):
        spoke = Pos(mid, 0, FLANGE_T / 2.0) * Box(length, SPOKE_W, FLANGE_T)
        body += Rot(0, 0, ang) * spoke
    half = deck.bolt_sq / 2.0
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            body -= Pos(sx * half, sy * half, FLANGE_T / 2.0) * Cylinder(deck.hole_d / 2.0, FLANGE_T + 4.0)
    # Round window over the deck's wire hole. The 13 mm tube stays above the
    # flange; the leads drop through this window into the Ø10 hole.
    body -= Pos(deck.wire_x, deck.wire_y, FLANGE_T / 2.0) * Cylinder(spec.opening_r, FLANGE_T + 4.0)
    return body


def clipped_turn_numbers(spec):
    """Even turns whose four in-line clips all still land on the 1000 mm strip.

    Turn 1 is the base. Turn 2 is the first standalone set. The turn between
    two clipped turns sits in both saddles, so the clips do not touch.
    """
    found = []
    n = 2
    while True:
        # Distance along the strip from the lowest clip to this turn at the highest base angle.
        along = 3.0 * (spec.pitch / 4.0) + (n - 1) * spec.pitch
        if along > spec.rise + 1.0:
            break
        if n % 2 == 0:
            found.append(n)
        n += 1
        if n > 24:
            break
    return tuple(found)


def placement_angles(spec):
    """(theta radians, base tube-centre z) for the four base clips, lowest first."""
    out = []
    for i, deg in enumerate(spec.angles):
        out.append((math.radians(deg), spec.z0 + i * (spec.pitch / 4.0)))
    return tuple(out)
