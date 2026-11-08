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
    Pos,
    Rectangle,
    Rot,
    Vector,
    Text,
    extrude,
    loft,
    measured,
    reject,
)

BORE_CLEAR = 0.2
# Dry-fit opening, snug. Tom glues after the trial fit. Not measured.
JOINT_CLEAR = 0.2
# Recessed digit. Shallow so it does not eat the rib or the ring.
MARK_DEPTH = 0.6
MARK_FONT = 5.0
# 0 = +X (right), 1 = +Y (back), 2 = -X, 3 = -Y (front).
TOWER_NAMES = ("0", "90", "180", "270")
# Lowest clip is on the back tower. Cable hole is about 20 degrees before it.
THETA0 = math.pi / 2.0
R_PIN = 22.0
R_OUT = 64.0
R_IN = 8.0
FLANGE_T = 4.5
# Running rib between the foot, the clips, and the cap. Tunable.
# Radial is the in-plane depth (the uncoil bend). Tangential is the visual width.
RIB_RADIAL = 6.0
RIB_TANGENTIAL = 3.2
# First clip centre above the ring. The tab below it fills the slot.
FOOT_CLEAR = 16.0
CAP_T = 8.0
# Vertical size of the spoke at each clip. Local. Not a full-height web.
CLIP_ARM = 4.0


def specs(
    tube_od, coil_diameter, gap, clip_wall, snap_opening, clip_width,
    rib_radial, rib_tangential, clip_arm, joint_clear, strip_length, extra_stations,
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
    if joint_clear < 0.2 or joint_clear > 1.0:
        reject(
            "joint_clear must stay between 0.2 and 1.0 mm. 0.2 mm is the snug dry-fit; tighter is a press, not a trial fit.",
            param="joint_clear",
        )
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
        joint_clear=joint_clear,
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
        # Rib sits on z=0. Its centre is what the slot and the cap line up with.
        bed_z=0.0,
        rib_z_center=rib_tangential / 2.0,
        # Tube centre in the tower frame. The slot is at local x = 0.
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


# Helix order, counterclockwise and up. 1 is the lowest clip.
# Index 0 is +X, 1 is +Y (back), 2 is -X, 3 is -Y (front).
TOWER_MARK = {1: "1", 2: "2", 3: "3", 0: "4"}


def _digit(char):
    """A small numeral, centred on the origin, in the sketch plane."""
    return Text(str(char), font_size=MARK_FONT, font="DejaVu Sans")


def _tower_mark(spec, index):
    """Recess the numeral into the top face of the rib, just above the ring."""
    face = _digit(TOWER_MARK[index])
    return Pos(0, 8.0, spec.rib_tangential - MARK_DEPTH) * extrude(face, MARK_DEPTH + 0.2)


def _slab(face, z0, height):
    return Pos(0, 0, z0) * extrude(face, height)


def build_tower(spec, index):
    """One tower, in its print frame. +X out, +Y up, +Z print-up (tangent once stood).

    The running member is a narrow rib on the bed face. The foot, each clip, and
    the spoke into that clip are local and thicker. They share the bed face, so
    nothing hangs below the rib.
    """
    if index not in TOWER_MARK:
        reject("tower index must be 0, 1, 2 or 3", param="extra_stations")
    # The tab is the same rib, long enough to fill the ring. No separate pin.
    rib_h = spec.cap_y + FLANGE_T
    rib = Pos(0, (spec.cap_y - FLANGE_T) / 2.0) * Rectangle(spec.rib_radial, rib_h)
    tenon = Pos(0, spec.cap_y + CAP_T / 2.0) * Rectangle(spec.rib_radial, CAP_T)
    solid = _slab(rib + tenon, spec.bed_z, spec.rib_tangential)
    solid -= _tower_mark(spec, index)
    for k in stations_for(spec, index):
        y = clip_y(spec, k)
        clip = Pos(spec.tube_x, y) * Circle(spec.ro)
        arm_x0 = spec.rib_radial / 2.0 - 1.5
        arm_x1 = spec.tube_x - spec.rb - 0.4
        arm = Pos((arm_x0 + arm_x1) / 2.0, y) * Rectangle(max(arm_x1 - arm_x0, 1.0), spec.clip_arm)
        solid += _slab(clip + arm, spec.bed_z, spec.clip_width)
        solid -= Pos(0, 0, spec.bed_z + spec.clip_width / 2.0) * _bore_void(spec, y)
    if len(solid.solids()) != 1:
        reject("the tower did not come back as a single solid", param="rib_radial")
    return solid


def _slot(spec, angle):
    """Through-slot the rib drops into. Same size at every tower."""
    radial = spec.rib_radial + spec.joint_clear
    tangent = spec.rib_tangential + spec.joint_clear
    x = R_PIN * math.cos(angle)
    y = R_PIN * math.sin(angle)
    # Same tangential shift as the cap pocket, so the rib and the slot share a centre.
    return (
        Pos(x, y, FLANGE_T / 2.0)
        * Rot(0, 0, math.degrees(angle))
        * Pos(0, -spec.rib_z_center, 0)
        * Box(radial, tangent, FLANGE_T + 2.0)
    )


def _base_mark(spec, index, angle):
    """Recessed numeral on the top of the ring, just outboard of the slot."""
    # Upright when looking down on the ring, so 1–4 read the same way at every slot.
    face = Text(TOWER_MARK[index], font_size=MARK_FONT, font="DejaVu Sans")
    pocket = extrude(face, MARK_DEPTH + 0.2)
    radius = R_PIN + spec.rib_radial / 2.0 + 4.5
    x = radius * math.cos(angle)
    y = radius * math.sin(angle)
    return Pos(x, y, FLANGE_T - MARK_DEPTH) * pocket


def build_base(spec, holes):
    """Ring. Four identical slots, a recessed number by each, bolts, cable window."""
    ring = extrude(Circle(R_OUT) - Circle(R_IN), FLANGE_T)
    body = ring
    for index in range(4):
        ang = tower_angle(index)
        body -= _slot(spec, ang)
        body -= _base_mark(spec, index, ang)
    half = holes.bolt / 2.0
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            body -= Pos(sx * half, sy * half, FLANGE_T / 2.0) * Cylinder(holes.bolt_d / 2.0, FLANGE_T + 2.0)
    body -= Pos(holes.wire_x, holes.wire_y, FLANGE_T / 2.0) * Cylinder(holes.window_d / 2.0, FLANGE_T + 2.0)
    # Keep every slot off the cable window.
    half_slot = 0.5 * math.hypot(spec.rib_radial + spec.joint_clear, spec.rib_tangential + spec.joint_clear)
    for index in range(4):
        ang = tower_angle(index)
        px = R_PIN * math.cos(ang)
        py = R_PIN * math.sin(ang)
        dist = math.hypot(px - holes.wire_x, py - holes.wire_y)
        if dist < half_slot + holes.window_d / 2.0 + 1.0:
            reject("a tower slot is too close to the light-cable window", param="coil_diameter")
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
    radial = spec.rib_radial + spec.joint_clear
    tangent = spec.rib_tangential + spec.joint_clear
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
    """Stand a print-frame tower on its slot. Local +X is radial, +Y is up."""
    radial = Vector(math.cos(angle), math.sin(angle), 0)
    # radial × up, so local +Y maps to world up. This +Z is the clockwise tangent.
    z_dir = Vector(math.sin(angle), -math.cos(angle), 0)
    origin = Vector(R_PIN * math.cos(angle), R_PIN * math.sin(angle), FLANGE_T)
    return Plane(origin=origin, x_dir=radial, z_dir=z_dir) * solid


def build_coupon(spec):
    """A short rib with its tab, one clip, and a block with the same slot.

    Snap the link. The tab should drop into the slot. The opening is joint_clear
    larger than the rib. That allowance is a design choice.
    """
    return _coupon_solid(spec)


def _coupon_solid(spec):
    z0 = spec.bed_z
    y_clip = 22.0
    rib_h = 28.0 + FLANGE_T
    rib = Pos(0, (28.0 - FLANGE_T) / 2.0) * Rectangle(spec.rib_radial, rib_h)
    solid = _slab(rib, z0, spec.rib_tangential)
    solid -= _tower_mark(spec, 1)
    arm_x0 = spec.rib_radial / 2.0 - 1.5
    arm_x1 = spec.tube_x - spec.rb - 0.4
    arm = Pos((arm_x0 + arm_x1) / 2.0, y_clip) * Rectangle(max(arm_x1 - arm_x0, 1.0), spec.clip_arm)
    clip = Pos(spec.tube_x, y_clip) * Circle(spec.ro)
    solid += _slab(clip + arm, z0, spec.clip_width)
    solid -= Pos(0, 0, z0 + spec.clip_width / 2.0) * _bore_void(spec, y_clip)
    # Two cheeks and a back wall. The gap is the ring slot, open on top so it needs no support.
    radial = spec.rib_radial + spec.joint_clear
    tall = spec.rib_tangential + spec.joint_clear
    block = Pos(-22.0, 4.0, tall / 2.0) * Box(16.0, 12.0, tall)
    channel = Pos(-22.0, 7.0, tall / 2.0) * Box(radial, 14.0, tall + 2.0)
    gauge = block - channel
    link = Pos(-9.0, 2.0, spec.rib_tangential / 2.0) * Box(18.0, 2.4, 2.0)
    body = solid + gauge + link
    if len(body.solids()) != 1:
        reject("the fit coupon did not come back as a single solid", param="rib_radial")
    return body
