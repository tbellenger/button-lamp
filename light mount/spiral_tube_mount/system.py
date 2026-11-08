"""Stackable cross mount for the 13 mm silicone LED tube.

Each level is two diameter pieces that half-lap at the centre, printed flat so
the C of each clip lies in the layer plane. A centre spacer sits on that lap,
grips a pin, and holds the next level one pitch higher. The base plate bolts
to the existing top-deck holes and carries the first pin.

All lengths are millimetres.
"""

import math
from types import SimpleNamespace

from nurb import (
    Box,
    Circle,
    Cone,
    Cylinder,
    Plane,
    Polygon,
    Pos,
    Rectangle,
    Rot,
    Vector,
    extrude,
    loft,
    measured,
    reject,
)

# Bore is a hair over the tube so the silicone is not crushed. The mouth holds it.
BORE_CLEAR = 0.2
# Half-lap slot is this much wider than the other piece's thickness. Provisional.
LAP_CLEAR = 0.3
# Pin hole is this much larger than the pin, on the diameter. Provisional snug fit.
PIN_CLEAR = 0.2
PIN_R = 3.0
FLAT_DEPTH = 1.5
# Extra room past the pin flat so the flat locates without becoming a press fit.
FLAT_CLEAR = 0.15
HUB_H = 14.0
BOSS_R = 14.0
PLATE = 100.0
FLANGE_T = 4.5
# How far the pin continues past the hub into the next spacer or cap.
PIN_ENGAGE = 4.5
LEAD_IN_DEG = 40.0
# Lowest clip centre. Keeps the clip body above the plate.
HUB_Z0 = 24.0


def specs(tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width):
    """Derived sizes, or a refusal when a combination cannot be printed."""
    if tube_od < 8.0 or tube_od > 20.0:
        reject("tube_od must stay between 8 and 20 mm", param="tube_od")
    if gap < 6.0:
        reject("gap under 6 mm leaves no room between turns", param="gap")
    if clip_wall < 2.0:
        reject("clip_wall under 2 mm is under five perimeters on a 0.4 mm nozzle", param="clip_wall")
    if clip_width < 12.0 or clip_width > 18.0:
        reject("clip_width must stay between 12 and 18 mm along the tube", param="clip_width")
    if snap_opening >= tube_od - 0.4:
        reject(
            f"snap_opening {snap_opening:.1f} mm is not narrower than the {tube_od:.1f} mm tube",
            param="snap_opening",
        )
    pitch = tube_od + gap
    radius = (coil_diameter - tube_od) / 2.0
    if radius < 36.0 or radius > 55.0:
        reject("coil centre-line radius must stay between 36 and 55 mm", param="coil_diameter")
    rb = tube_od / 2.0 + BORE_CLEAR
    if snap_opening / 2.0 >= rb:
        reject("snap_opening is wider than the bore", param="snap_opening")
    ro = rb + clip_wall
    # The curved bore shifts outward at the ends of the clip by about this much.
    sagitta = radius * (1.0 - math.cos((clip_width / 2.0) / radius))
    if clip_wall - sagitta < 1.6:
        reject(
            "the helix bow thins the outer wall under 1.6 mm; raise clip_wall or the coil diameter",
            param="clip_wall",
        )
    hole_r = PIN_R + PIN_CLEAR / 2.0
    # 45-degree teardrop apex, measured from the hole centre along print +Z.
    apex = hole_r * math.sqrt(2.0)
    if apex + 1.0 > clip_width / 2.0:
        reject(
            "clip_width is too thin for a 45 degree teardrop over the centre pin hole",
            param="clip_width",
        )
    if HUB_H / 2.0 < hole_r + 2.4:
        reject("the half-lap tongue is too short to keep a wall past the pin hole", param="clip_width")
    quarter = pitch / 4.0
    return SimpleNamespace(
        tube_od=tube_od,
        coil_diameter=coil_diameter,
        gap=gap,
        clip_wall=clip_wall,
        snap_opening=snap_opening,
        clip_width=clip_width,
        pitch=pitch,
        radius=radius,
        rb=rb,
        ro=ro,
        hole_r=hole_r,
        quarter=quarter,
        # Clip centres relative to the hub, lowest piece first.
        low_v=(-1.5 * quarter, 0.5 * quarter),
        high_v=(-0.5 * quarter, 1.5 * quarter),
        climb=pitch / (2.0 * math.pi),
        slot_w=clip_width + LAP_CLEAR,
        web=pitch - HUB_H,
        pin_len=HUB_H + PIN_ENGAGE,
        shoulder=HUB_Z0 - HUB_H / 2.0,
    )


def deck():
    """Deck holes. Values live in measurements.toml."""
    cable = measured("light_cable_hole_dia")
    window = measured("mount_cable_window_dia")
    if window < cable + 4.0 - 1e-6:
        reject("the mount window must stay at least 4 mm larger than the light-cable hole")
    return SimpleNamespace(
        bolt=measured("top_deck_bolt_spacing"),
        bolt_d=measured("m3_clearance"),
        wire_x=measured("top_deck_wire_offset_x"),
        wire_y=measured("top_deck_wire_offset_y"),
        cable_d=cable,
        window_d=window,
        as_cut=measured("top_deck_wire_hole_dia"),
    )


def levels_for_strip(strip_mm, radius, pitch):
    """How many 4-clip levels a strip of this length needs.

    Stations are a quarter turn apart. The count includes station 0. A level
    is one low cross and one high cross (four clips). The last level is printed
    whole even when the strip ends on its third clip.
    """
    step = 0.5 * math.pi * math.hypot(radius, pitch / (2.0 * math.pi))
    k = 0
    while (k + 1) * step <= strip_mm + 1e-6:
        k += 1
    stations = k + 1
    return max(1, math.ceil(stations / 4.0)), stations


def lowest_angle(wire_x, wire_y):
    """Angle of the lowest clip: wire hole, then a lead-in along the helix."""
    return math.atan2(wire_y, wire_x) + math.radians(LEAD_IN_DEG)


def _teardrop_prism(hole_r, length):
    """Horizontal hole along Y, 45 degree roof toward print +Z, centred on the origin."""
    a = hole_r * math.sqrt(2.0) / 2.0
    apex = hole_r * math.sqrt(2.0)
    face = Circle(hole_r) + Polygon([(a, a), (-a, a), (0.0, apex)])
    prism = extrude(face, length)
    # Extrude runs +Z. A +90 degree turn about X sends that axis to -Y and the
    # apex (+Y in the sketch) to +Z.
    return Pos(0, length / 2.0, 0) * Rot(90, 0, 0) * prism


def hole_void(spec, with_flat):
    """Pin hole through the hub. Optional flat on local +X clocks the low piece."""
    void = _teardrop_prism(spec.hole_r, HUB_H + 4.0)
    if with_flat:
        flat_x = PIN_R - FLAT_DEPTH + FLAT_CLEAR
        blocker = Pos(flat_x + 20.0, 0, 0) * Box(40, HUB_H + 8, spec.clip_width + 8)
        void -= blocker
    return void


def _bore_void(spec, cx, cy, outward):
    """Curved bore and mouth. Sections stay parallel to the bed so the walls only tilt a few degrees."""
    radius = spec.radius
    half = spec.clip_width / 2.0 + 1.4
    alpha = math.asin(min(0.9, half / radius))
    sign = 1.0 if outward[0] > 0 else -1.0
    sections = []
    for i in range(5):
        dth = -alpha + 2.0 * alpha * i / 4.0
        x = cx - radius * (1.0 - math.cos(dth))
        y = cy + spec.climb * dth
        z = -radius * math.sin(dth)
        bore = Pos(x, y, z) * Circle(spec.rb)
        mouth = Pos(x + sign * spec.ro, y, z) * Rectangle(2.0 * spec.ro + 4.0, spec.snap_opening)
        sections.append(bore + mouth)
    solid = loft(sections)
    if len(solid.solids()) != 1:
        reject("the curved bore did not loft as a single solid", param="clip_width")
    return solid


def _cross_blank(spec, vs, with_flat, keep_top):
    """One diameter piece, in its print frame.

    Profile in XY, thickness along Z. Local +X runs toward the first clip of
    this piece. Local +Y is up once the piece is stood on the pin.
    """
    radius = spec.radius
    bar = Rectangle(2.0 * (radius - spec.ro + 3.0), HUB_H)
    body = bar
    for v in vs:
        body += Pos(radius, v) * Circle(spec.ro)
        body += Pos(-radius, v) * Circle(spec.ro)
    # keep_top means this piece is the upper half of the lap, so the slot
    # opens downward. The low piece keeps the bottom and the slot opens up.
    if keep_top:
        slot_y = -HUB_H / 4.0
    else:
        slot_y = HUB_H / 4.0
    body -= Pos(0, slot_y) * Rectangle(spec.slot_w, HUB_H / 2.0 + 0.2)
    solid = Pos(0, 0, -spec.clip_width / 2.0) * extrude(body, spec.clip_width)
    solid -= hole_void(spec, with_flat)
    solid -= _bore_void(spec, radius, vs[0], (1.0, 0.0, 0.0))
    solid -= _bore_void(spec, -radius, vs[1], (-1.0, 0.0, 0.0))
    if len(solid.solids()) != 1:
        reject("the cross did not come back as a single solid", param="clip_width")
    return solid


def cross_low(spec):
    """Lower half of the lap. Its +X clip is the lowest clip of the level."""
    return _cross_blank(spec, spec.low_v, with_flat=True, keep_top=False)


def cross_high(spec):
    """Upper half of the lap. Sits in the low piece's slot, turned 90 degrees."""
    return _cross_blank(spec, spec.high_v, with_flat=False, keep_top=True)


def _pin(length):
    """Vertical pin with a flat on +X. Base of the pin is z=0. Tip is chamfered."""
    pin = Pos(0, 0, length / 2.0) * Cylinder(PIN_R, length)
    flat_x = PIN_R - FLAT_DEPTH
    pin -= Pos(flat_x + 8.0, 0, length / 2.0) * Box(16, 2.0 * PIN_R, length)
    return pin


def _socket_void(spec, web):
    """Hole up from z=0. A 45 degree cone closes it so the spacer prints without a bridge."""
    # Stop the cone at 0.6 mm instead of a point so the mesh stays watertight.
    # The remaining ceiling is under 1.2 mm across and sits inside the pin.
    tip = 0.6
    cone_h = spec.hole_r - tip
    grip = web - cone_h - 0.8
    if grip < 4.0:
        reject("the spacer socket is under 4 mm deep; lower the hub or the pin", param="clip_width")
    void = Pos(0, 0, grip / 2.0) * Cylinder(spec.hole_r, grip + 0.2)
    void += Pos(0, 0, grip + cone_h / 2.0) * Cone(spec.hole_r, tip, cone_h)
    flat_x = PIN_R - FLAT_DEPTH + FLAT_CLEAR
    void -= Pos(flat_x + 15.0, 0, web / 2.0) * Box(30, 40, web + 4)
    return void, grip


def spacer(spec):
    """Disk that clamps one level and a pin that carries the next. Print upright, socket down."""
    web = spec.web
    void, grip = _socket_void(spec, web)
    disk = Pos(0, 0, web / 2.0) * Cylinder(BOSS_R, web)
    # Root is buried in the disk, wider than the socket, so the pin is part of the disk.
    root = Pos(0, 0, (grip + web) / 2.0) * Cylinder(PIN_R + 2.2, web - grip)
    body = disk + root + Pos(0, 0, web - 0.6) * _pin(spec.pin_len + 0.6) - void
    if len(body.solids()) != 1:
        reject("the spacer did not come back as a single solid", param="clip_width")
    return body


def cap(spec):
    """Top clamp. A through hole, so it prints without a bridge. No pin above it."""
    web = spec.web
    disk = Pos(0, 0, web / 2.0) * Cylinder(BOSS_R, web)
    hole = Pos(0, 0, web / 2.0) * Cylinder(spec.hole_r, web + 2.0)
    flat_x = PIN_R - FLAT_DEPTH + FLAT_CLEAR
    hole -= Pos(flat_x + 15.0, 0, web / 2.0) * Box(30, 30, web + 4)
    body = disk - hole
    if len(body.solids()) != 1:
        reject("the cap did not come back as a single solid", param="clip_width")
    return body


def base_plate(spec, holes):
    """Square plate, centre pin, bolt holes, and the light-cable window."""
    plate = extrude(Rectangle(PLATE, PLATE), FLANGE_T)
    boss_h = spec.shoulder - FLANGE_T
    if boss_h < 6.0:
        reject("the centre boss is under 6 mm; the lowest clip would hit the plate", param="coil_diameter")
    boss = Pos(0, 0, FLANGE_T) * extrude(Circle(BOSS_R), boss_h)
    # Flat faces the lowest clip, not world +X. Bolts and the cable window stay put.
    theta = math.degrees(lowest_angle(holes.wire_x, holes.wire_y))
    pin = Pos(0, 0, spec.shoulder - 1.0) * Rot(0, 0, theta) * _pin(spec.pin_len + 1.0)
    body = plate + boss + pin
    half = holes.bolt / 2.0
    for sx in (-1, 1):
        for sy in (-1, 1):
            body -= Pos(sx * half, sy * half, FLANGE_T / 2.0) * Cylinder(holes.bolt_d / 2.0, FLANGE_T + 2)
    body -= Pos(holes.wire_x, holes.wire_y, FLANGE_T / 2.0) * Cylinder(holes.window_d / 2.0, FLANGE_T + 2)
    if len(body.solids()) != 1:
        reject("the base did not come back as a single solid", param="coil_diameter")
    return body


def place_cross(solid, theta, z_hub):
    """Stand a print-frame cross on the pin. theta is the angle of its +X clip."""
    radial = Vector(math.cos(theta), math.sin(theta), 0)
    z_dir = Vector(math.sin(theta), -math.cos(theta), 0)
    plane = Plane(origin=Vector(0, 0, z_hub), x_dir=radial, z_dir=z_dir)
    return plane * solid


def fit_coupon(spec):
    """Lap tongues, a pin, and a socket, joined by break tabs. Clearances are design choices."""
    return _coupon_solid(spec)


def _coupon_solid(spec):
    """Tongues lie in their print pose. A 2 mm floor ties them to a pin and a socket."""
    def hub_slice():
        return extrude(Rectangle(36, HUB_H), spec.clip_width)

    low = hub_slice()
    low -= Pos(0, HUB_H / 4.0, spec.clip_width / 2.0) * Box(spec.slot_w, HUB_H / 2.0 + 0.2, spec.clip_width + 2)
    low -= Pos(0, 0, spec.clip_width / 2.0) * hole_void(spec, True)
    high = hub_slice()
    high -= Pos(0, -HUB_H / 4.0, spec.clip_width / 2.0) * Box(spec.slot_w, HUB_H / 2.0 + 0.2, spec.clip_width + 2)
    high -= Pos(0, 0, spec.clip_width / 2.0) * hole_void(spec, False)
    high = Pos(48, 0, 0) * high
    floor = Pos(24, 0, 1.0) * Box(20, 6, 2.0)
    # The lap slot removes the middle of each tongue, so the spine sits off to one side.
    puck = Pos(-12, 30, spec.web / 2.0) * Cylinder(12, spec.web)
    hole = Pos(-12, 30, spec.web / 2.0) * Cylinder(spec.hole_r, spec.web + 2)
    flat_x = PIN_R - FLAT_DEPTH + FLAT_CLEAR
    hole -= Pos(-12 + flat_x + 10.0, 30, spec.web / 2.0) * Box(20, 20, spec.web + 4)
    puck -= hole
    pin = Pos(14, 30, 0) * _pin(spec.pin_len)
    spine = Pos(-12, 16, 1.0) * Box(6, 24, 2.0)
    tie = Pos(2, 30, 1.0) * Box(22, 6, 2.0)
    body = low + high + floor + puck + pin + spine + tie
    if len(body.solids()) != 1:
        reject("the fit coupon did not come back as a single solid", param="clip_width")
    return body
