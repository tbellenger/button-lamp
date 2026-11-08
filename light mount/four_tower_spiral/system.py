"""Four-tower spiral mount.

A ring bolts to the deck. Four towers stand at 0, 90, 180 and 270 degrees.
Each tower is printed on its side, so the clip C lies in the layer plane.
A cap drops onto the four tops and holds that radius.

The helix starts at the 90 degree tower (the back, +Y). That is 20 degrees
past the light-cable hole, going counterclockwise and up. The other towers
pick up the same helix a quarter turn later.

Lengths are millimetres. Fits are design allowances, not measured.
"""

import math
from types import SimpleNamespace

from nurb import (
    Box,
    Circle,
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

BORE_CLEAR = 0.2
# Across the flats of the diamond pin. Just over the 5 mm printed-pin floor.
# Diagonal is this times sqrt(2). Not a measured load; it is the smallest pin
# the print rules will treat as a real pin rather than a perimeter tube.
PIN_ACROSS = 5.2
# Extra on the across-flats opening. Snug. Not measured.
PIN_CLEAR = 0.2
TOOTH_W = 1.2
TOOTH_H = 3.0
# One lug per tower, close to the pin, so the foot can stay a small pad.
# Offsets are along the tower's radial axis, from the pin.
LUG_OFFSET = {0: -4.5, 1: -1.5, 2: 1.5, 3: 4.5}
LUG_W = 2.0
LUG_DROP = 4.0
# 0 = +X (right), 1 = +Y (back), 2 = -X, 3 = -Y (front).
TOWER_NAMES = ("0", "90", "180", "270")
# Lowest clip is on the back tower. Cable hole is about 20 degrees before it.
THETA0 = math.pi / 2.0
R_PIN = 22.0
R_OUT = 64.0
R_IN = 8.0
FLANGE_T = 4.5
PIN_H = 8.0
# Running rib between the foot, the clips, and the cap. Tunable.
# Radial is the in-plane depth (the uncoil bend). Tangential is the visual width.
RIB_RADIAL = 6.0
RIB_TANGENTIAL = 3.2
# First clip centre above the foot. Clears the socket and the ring.
FOOT_CLEAR = 16.0
CAP_T = 8.0
# Vertical size of the spoke at each clip. Local. Not a full-height web.
CLIP_ARM = 4.0


def specs(
    tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width,
    rib_radial, rib_tangential, clip_arm, strip_length, extra_stations,
):
    """Derived sizes, or a refusal when a combination cannot be printed."""
    if tube_od < 8.0 or tube_od > 20.0:
        reject("tube_od must stay between 8 and 20 mm", param="tube_od")
    if gap < 6.0:
        reject("gap under 6 mm leaves no room between turns", param="gap")
    if clip_wall < 2.0:
        reject("clip_wall under 2 mm is under five perimeters on a 0.4 mm nozzle", param="clip_wall")
    if clip_width < 8.0 or clip_width > 18.0:
        reject("clip_width must stay between 8 and 18 mm along the tube", param="clip_width")
    if rib_tangential < 2.4 or rib_tangential > clip_width:
        reject(
            "rib_tangential must stay between 2.4 mm and clip_width. "
            "2.4 mm is six perimeters on a 0.4 mm nozzle; wider than the clip, the rib no longer hides behind it.",
            param="rib_tangential",
        )
    if rib_radial < 4.0 or rib_radial > 16.0:
        reject("rib_radial must stay between 4 and 16 mm", param="rib_radial")
    if clip_arm < 3.0 or clip_arm > 12.0:
        reject("clip_arm must stay between 3 and 12 mm", param="clip_arm")
    if snap_opening >= tube_od - 0.4:
        reject(
            f"snap_opening {snap_opening:.1f} mm is not narrower than the {tube_od:.1f} mm tube",
            param="snap_opening",
        )
    if strip_length < 200.0 or strip_length > 2500.0:
        reject("strip_length must stay between 200 and 2500 mm", param="strip_length")
    if extra_stations < 0 or extra_stations > 12:
        reject("extra_stations must stay between 0 and 12", param="extra_stations")

    pitch = tube_od + gap
    radius = (coil_diameter - tube_od) / 2.0
    if radius < 36.0 or radius > 55.0:
        reject("coil centre-line radius must stay between 36 and 55 mm", param="coil_diameter")
    rb = tube_od / 2.0 + BORE_CLEAR
    if snap_opening / 2.0 >= rb:
        reject("snap_opening is wider than the bore", param="snap_opening")
    ro = rb + clip_wall
    pin_half = PIN_ACROSS * math.sqrt(2.0) / 2.0
    hole_half = pin_half + PIN_CLEAR * math.sqrt(2.0) / 2.0
    # Local foot only. The running rib does not have to contain the diamond.
    foot_half = hole_half + 2.4
    foot_t = 2.0 * foot_half

    climb = pitch / (2.0 * math.pi)
    quarter_arc = 0.5 * math.pi * math.hypot(radius, climb)
    stations = []
    k = 0
    while k * quarter_arc <= strip_length + 1e-6:
        stations.append(k)
        k += 1
        if k > 60:
            reject("strip_length asks for more than 60 clips", param="strip_length")
    for _ in range(int(extra_stations)):
        stations.append(stations[-1] + 1)
    if len(stations) < 4:
        reject("the strip is shorter than one turn, so the four towers have nothing to share", param="strip_length")

    k_max = stations[-1]
    cap_y = FOOT_CLEAR + k_max * (pitch / 4.0) + ro + 4.0
    tower_len = cap_y + CAP_T
    if tower_len > 280.0:
        reject(
            f"the tower would be {tower_len:.0f} mm long on the bed; the CR-10 bed is 300 mm. Shorten the strip.",
            param="strip_length",
        )
    return SimpleNamespace(
        tube_od=tube_od,
        coil_diameter=coil_diameter,
        gap=gap,
        clip_wall=clip_wall,
        snap_opening=snap_opening,
        clip_width=clip_width,
        rib_radial=rib_radial,
        rib_tangential=rib_tangential,
        clip_arm=clip_arm,
        strip_length=strip_length,
        extra_stations=int(extra_stations),
        pitch=pitch,
        radius=radius,
        rb=rb,
        ro=ro,
        quarter=pitch / 4.0,
        climb=climb,
        quarter_arc=quarter_arc,
        stations=tuple(stations),
        k_max=k_max,
        cap_y=cap_y,
        tower_len=tower_len,
        pin_half=pin_half,
        hole_half=hole_half,
        foot_half=foot_half,
        foot_t=foot_t,
        # Bed side of the foot. The rib and the clips share this face.
        bed_z=-foot_half,
        rib_z_center=-foot_half + rib_tangential / 2.0,
        # Tube centre in the tower frame. The pin is at local x = 0.
        tube_x=radius - R_PIN,
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
    )


def tower_angle(index):
    """World angle of tower index. 0 is +X, 1 is +Y (back)."""
    return index * math.pi / 2.0


def stations_for(spec, index):
    """Clip stations whose helix angle lands on this tower."""
    target = tower_angle(index)
    found = []
    for k in spec.stations:
        ang = (THETA0 + k * math.pi / 2.0) % (2.0 * math.pi)
        delta = abs(ang - target)
        if min(delta, 2.0 * math.pi - delta) < 1e-6:
            found.append(k)
    return tuple(found)


def clip_y(spec, k):
    """Height of station k above the foot, which sits on the ring."""
    return FOOT_CLEAR + k * spec.quarter


def _diamond_face(half):
    """Square diamond. Points on the axes, so every side is 45 degrees."""
    return Polygon([(half, 0.0), (0.0, half), (-half, 0.0), (0.0, -half)])


def _diamond_along_y(half, length):
    """Diamond prism along Y, centred on the origin. Sketch +Y becomes print +Z."""
    prism = extrude(_diamond_face(half), length)
    return Pos(0, length / 2.0, 0) * Rot(90, 0, 0) * prism


def _bore_void(spec, cy):
    """Curved bore and outward mouth. Sections stay parallel to the bed."""
    radius = spec.radius
    cx = spec.tube_x
    half = spec.clip_width / 2.0 + 1.4
    alpha = math.asin(min(0.9, half / radius))
    sections = []
    for i in range(5):
        dth = -alpha + 2.0 * alpha * i / 4.0
        x = cx - radius * (1.0 - math.cos(dth))
        y = cy + spec.climb * dth
        z = -radius * math.sin(dth)
        bore = Pos(x, y, z) * Circle(spec.rb)
        mouth = Pos(x + spec.ro, y, z) * Rectangle(2.0 * spec.ro + 4.0, spec.snap_opening)
        sections.append(bore + mouth)
    solid = loft(sections, ruled=True)
    if len(solid.solids()) != 1:
        reject("a clip bore did not loft as a single solid", param="clip_width")
    return solid


def _tooth(spec):
    """45 degree tooth on the outward side of the socket. Blocks a 180 degree turn."""
    w = TOOTH_W
    x_wall = spec.hole_half + 0.5
    x_tip = spec.hole_half - w
    face = Polygon([(x_wall, w), (x_wall, -w), (x_tip, 0.0)])
    prism = extrude(face, TOOTH_H)
    # Sketch +Y becomes print +Z. The extrude runs to -Y, then shifts up into the foot.
    return Pos(0, TOOTH_H, 0) * Rot(90, 0, 0) * prism


def _notches(spec, index):
    """1, 2, 3 or 4 shallow bites on the inboard edge of the rib. Count is the label."""
    count = index + 1
    width = 1.4
    gap = 0.8
    y0 = spec.cap_y - 6.0 - count * (width + gap)
    edge = -spec.rib_radial / 2.0
    cut = None
    for n in range(count):
        y = y0 + n * (width + gap)
        bite = Pos(edge + 0.35, y + width / 2.0) * Rectangle(1.0, width)
        cut = bite if cut is None else cut + bite
    return cut


def _slab(face, z0, height):
    return Pos(0, 0, z0) * extrude(face, height)


def build_tower(spec, index):
    """One tower, in its print frame. +X out, +Y up, +Z print-up (tangent once stood).

    The running member is a narrow rib on the bed face. The foot, each clip, and
    the spoke into that clip are local and thicker. They share the bed face, so
    nothing hangs below the rib.
    """
    if index not in LUG_OFFSET:
        reject("tower index must be 0, 1, 2 or 3", param="extra_stations")
    z0 = spec.bed_z
    rib = Pos(0, spec.cap_y / 2.0) * Rectangle(spec.rib_radial, spec.cap_y)
    tenon = Pos(0, spec.cap_y + CAP_T / 2.0) * Rectangle(spec.rib_radial, CAP_T)
    # The foot is a local pad around the diamond. It is thicker than the rib.
    foot = Pos(0, FOOT_CLEAR / 2.0) * Rectangle(spec.foot_t, FOOT_CLEAR)
    lug = Pos(LUG_OFFSET[index], -LUG_DROP / 2.0 + 0.3) * Rectangle(LUG_W, LUG_DROP + 0.6)
    solid = _slab(rib + tenon - _notches(spec, index), z0, spec.rib_tangential)
    solid += _slab(foot + lug, z0, spec.foot_t)
    for k in stations_for(spec, index):
        y = clip_y(spec, k)
        clip = Pos(spec.tube_x, y) * Circle(spec.ro)
        arm_x0 = spec.rib_radial / 2.0 - 1.5
        arm_x1 = spec.tube_x - spec.rb - 0.4
        arm = Pos((arm_x0 + arm_x1) / 2.0, y) * Rectangle(max(arm_x1 - arm_x0, 1.0), spec.clip_arm)
        solid += _slab(clip + arm, z0, spec.clip_width)
        solid -= Pos(0, 0, z0 + spec.clip_width / 2.0) * _bore_void(spec, y)
    socket = Pos(0, PIN_H / 2.0, 0) * _diamond_along_y(spec.hole_half, PIN_H + 2.0)
    solid -= socket
    solid += _tooth(spec)
    if len(solid.solids()) != 1:
        reject("the tower did not come back as a single solid", param="rib_radial")
    return solid


def _pin_groove(spec):
    """Flat cut on local +X, open at the top of the pin. That side faces outward."""
    depth = TOOTH_W + PIN_CLEAR + 0.4
    span = spec.pin_half * 2.0 + 4.0
    return Pos(spec.pin_half - depth / 2.0 + 0.3, 0, PIN_H) * Box(depth + 0.6, span, PIN_H * 2.0 + 2.0)


def _placed_pin(spec, angle):
    pin = extrude(_diamond_face(spec.pin_half), PIN_H + 0.8)
    pin -= _pin_groove(spec)
    x = R_PIN * math.cos(angle)
    y = R_PIN * math.sin(angle)
    return Pos(x, y, FLANGE_T - 0.6) * Rot(0, 0, math.degrees(angle)) * pin


def _lug_pocket(spec, index, angle):
    lug_x = LUG_OFFSET[index]
    radius = R_PIN + lug_x
    px = radius * math.cos(angle)
    py = radius * math.sin(angle)
    # Local X is radial, local Y is the tangent. The lug is clip_width thick along the tangent.
    return Pos(px, py, FLANGE_T / 2.0) * Rot(0, 0, math.degrees(angle)) * Box(
        LUG_W + PIN_CLEAR, spec.foot_t + PIN_CLEAR, FLANGE_T + 2.0
    )


def build_base(spec, holes):
    """Ring. Four diamond pins, four lug pockets, bolts, cable window."""
    ring = extrude(Circle(R_OUT) - Circle(R_IN), FLANGE_T)
    body = ring
    for index in range(4):
        ang = tower_angle(index)
        body += _placed_pin(spec, ang)
        body -= _lug_pocket(spec, index, ang)
    half = holes.bolt / 2.0
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            body -= Pos(sx * half, sy * half, FLANGE_T / 2.0) * Cylinder(holes.bolt_d / 2.0, FLANGE_T + 2.0)
    body -= Pos(holes.wire_x, holes.wire_y, FLANGE_T / 2.0) * Cylinder(holes.window_d / 2.0, FLANGE_T + 2.0)
    # Keep the cable window off the pins. A closer coil or a bigger pin would eat it.
    for index in range(4):
        ang = tower_angle(index)
        px = R_PIN * math.cos(ang)
        py = R_PIN * math.sin(ang)
        dist = math.hypot(px - holes.wire_x, py - holes.wire_y)
        if dist < spec.pin_half + holes.window_d / 2.0 + 1.5:
            reject("a tower pin is too close to the light-cable window", param="coil_diameter")
    if len(body.solids()) != 1:
        reject("the base ring did not come back as a single solid", param="coil_diameter")
    return body


def build_cap(spec):
    """Thin hoop. Pockets match the rib, not the old full-height slab."""
    wall = 2.4
    outer = R_PIN + spec.rib_radial / 2.0 + wall
    inner = R_PIN - spec.rib_radial / 2.0 - wall
    if inner < 8.0:
        reject("rib_radial leaves no hole in the middle of the cap", param="rib_radial")
    ring = extrude(Circle(outer) - Circle(inner), CAP_T)
    body = ring
    radial = spec.rib_radial + PIN_CLEAR
    tangent = spec.rib_tangential + PIN_CLEAR
    # Tower +Z is the clockwise tangent. Cap local +Y is the opposite direction.
    pocket_y = -spec.rib_z_center
    for index in range(4):
        ang = tower_angle(index)
        x = R_PIN * math.cos(ang)
        y = R_PIN * math.sin(ang)
        pocket = (
            Pos(x, y, CAP_T / 2.0)
            * Rot(0, 0, math.degrees(ang))
            * Pos(0, pocket_y, 0)
            * Box(radial, tangent, CAP_T + 2.0)
        )
        body -= pocket
    if len(body.solids()) != 1:
        reject("the cap did not come back as a single solid", param="clip_width")
    return body


def place_tower(solid, angle):
    """Stand a print-frame tower on its pin. Local +X is radial, +Y is up."""
    radial = Vector(math.cos(angle), math.sin(angle), 0)
    # radial × up, so local +Y maps to world up. This +Z is the clockwise tangent.
    z_dir = Vector(math.sin(angle), -math.cos(angle), 0)
    origin = Vector(R_PIN * math.cos(angle), R_PIN * math.sin(angle), FLANGE_T)
    return Plane(origin=origin, x_dir=radial, z_dir=z_dir) * solid


def build_coupon(spec):
    """Foot, a diamond gauge, and one clip, in the tower's print pose.

    Snap the tab. The gauge should slide into the socket. This lug is the
    0 degree tower's lug. Clearances are design choices.
    """
    return _coupon_solid(spec)


def _coupon_solid(spec):
    """Short foot and one clip, plus a diamond gauge on the same bed face."""
    z0 = spec.bed_z
    y = 24.0
    rib = Pos(0, 16.0) * Rectangle(spec.rib_radial, 32.0)
    foot = Pos(0, 8.0) * Rectangle(spec.foot_t, 16.0)
    lug = Pos(LUG_OFFSET[0], -LUG_DROP / 2.0 + 0.3) * Rectangle(LUG_W, LUG_DROP + 0.6)
    solid = _slab(rib, z0, spec.rib_tangential)
    solid += _slab(foot + lug, z0, spec.foot_t)
    arm_x0 = spec.rib_radial / 2.0 - 1.5
    arm_x1 = spec.tube_x - spec.rb - 0.4
    arm = Pos((arm_x0 + arm_x1) / 2.0, y) * Rectangle(max(arm_x1 - arm_x0, 1.0), spec.clip_arm)
    clip = Pos(spec.tube_x, y) * Circle(spec.ro)
    solid += _slab(clip + arm, z0, spec.clip_width)
    solid -= Pos(0, 0, z0 + spec.clip_width / 2.0) * _bore_void(spec, y)
    solid -= Pos(0, PIN_H / 2.0, 0) * _diamond_along_y(spec.hole_half, PIN_H + 2.0)
    solid += _tooth(spec)
    # Gauge sits with its bottom point just under the bed, then the tip is shaved flat.
    gauge = Pos(0, 12.0, spec.bed_z - 1.0 + spec.pin_half) * _diamond_along_y(spec.pin_half, 14.0)
    gauge -= Pos(spec.pin_half, 12.0, 0) * Box(spec.pin_half * 4.0, 20.0, TOOTH_W * 2.0 + PIN_CLEAR)
    # Shave the gauge flush with the foot's bed face, leaving about a 2 mm flat.
    gauge -= Pos(0, 12.0, spec.bed_z - 8.0) * Box(24.0, 30.0, 16.0)
    gauge = Pos(-28.0, 0, 0) * gauge
    tab = Pos(-16.0, 8.0, spec.bed_z + 1.0) * Box(20.0, 3.0, 2.0)
    body = solid + gauge + tab
    if len(body.solids()) != 1:
        reject("the fit coupon did not come back as a single solid", param="rib_radial")
    return body
