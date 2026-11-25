"""Four-tower spiral with the hooks printed in the towers.

Same ring, same cap, same numbered slots as light mount/four_tower_spiral.
The hooks are not separate clips and there is no clip-to-arm joint: each
tower is one solid. It prints lying on its side, so the C of every hook is
the layer outline and the flat face on the bed is a real plane.

The 965.2 mm tube is not all helix. A straight tangent runs from the outer
edge of the ring into the first hook. Hooks are placed only where that helix
still has tube. Lengths are millimetres. Fits are design allowances, not
measured.
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
    Text,
    Vector,
    extrude,
    fillet,
    loft,
    measured,
    reject,
)

BORE_CLEAR = 0.2
# Dry-fit opening, snug. Tom glues after the trial fit. Not measured.
JOINT_CLEAR = 0.2
MARK_DEPTH = 0.6
MARK_FONT = 5.0
# Index 0 was on +X. The set is turned 45° counterclockwise so tower 2
# (yellow) and tower 3 (green) land on the front corners.
# +Y is the back, toward the cable window. -Y is the front, toward the buttons.
TOWER_NAMES = ("0", "90", "180", "270")
TOWER_PHASE = math.pi / 4.0
# Lowest hook is on tower 1. Same helix, shifted with the towers.
THETA0 = math.pi / 2.0 + TOWER_PHASE
R_PIN = 22.0
R_OUT = 64.0
R_IN = 8.0
FLANGE_T = 4.5
RIB_RADIAL = 6.0
RIB_TANGENTIAL = 3.2
FOOT_CLEAR = 16.0
CAP_T = 8.0
# Neck of the hook where it leaves the rib. Wider than the old 4.29 mm spoke
# so the root fillet has a 3 mm radius (half the neck) and the neck itself
# clears the 5 mm snap-root rule.
CLIP_ARM = 6.0
TUBE_OD = 13.0
COIL_DIAMETER = 100.0
PITCH = 26.0
TUBE_LENGTH = 965.2
CLIP_WALL = 2.4
SNAP_OPENING = 11.5
CLIP_WIDTH = 10.0

# Helix order, counterclockwise and up. 1 is the lowest hook.
TOWER_MARK = {1: "1", 2: "2", 3: "3", 0: "4"}


def specs(
    tube_od, coil_diameter, pitch, clip_wall, snap_opening, clip_width,
    rib_radial, rib_tangential, clip_arm, joint_clear, tube_length, extra_stations,
):
    """Derived sizes, or a refusal when a combination cannot be printed."""
    if tube_od < 8.0 or tube_od > 20.0:
        reject("tube_od must stay between 8 and 20 mm", param="tube_od")
    if pitch <= tube_od:
        reject("pitch must be greater than the tube diameter", param="pitch")
    gap = pitch - tube_od
    if gap < 6.0:
        reject("gap under 6 mm leaves no room between turns", param="pitch")
    if clip_wall < 2.0:
        reject("clip_wall under 2 mm is under five perimeters on a 0.4 mm nozzle", param="clip_wall")
    if clip_width < 8.0 or clip_width > 18.0:
        reject("clip_width must stay between 8 and 18 mm along the tube", param="clip_width")
    if rib_tangential < 2.4 or rib_tangential > clip_width:
        reject(
            "rib_tangential must stay between 2.4 mm and clip_width. "
            "The hook is thicker than the rib and both sit on the bed.",
            param="rib_tangential",
        )
    if rib_radial < 4.0 or rib_radial > 16.0:
        reject("rib_radial must stay between 4 and 16 mm", param="rib_radial")
    if clip_arm < 5.0 or clip_arm > 12.0:
        reject(
            "clip_arm must stay between 5 and 12 mm. 5 mm is the neck at the root; thinner, the fillet has nothing to land on.",
            param="clip_arm",
        )
    if joint_clear < 0.2 or joint_clear > 1.0:
        reject(
            "joint_clear must stay between 0.2 and 1.0 mm. 0.2 mm is the snug dry-fit.",
            param="joint_clear",
        )
    if snap_opening >= tube_od - 0.4:
        reject(
            f"snap_opening {snap_opening:.1f} mm is not narrower than the {tube_od:.1f} mm tube",
            param="snap_opening",
        )
    if tube_length < 200.0 or tube_length > 2500.0:
        reject("tube_length must stay between 200 and 2500 mm", param="tube_length")
    if extra_stations < 0 or extra_stations > 12:
        reject("extra_stations must stay between 0 and 12", param="extra_stations")

    radius = (coil_diameter - tube_od) / 2.0
    if radius < 36.0 or radius > 55.0:
        reject("coil centre-line radius must stay between 36 and 55 mm", param="coil_diameter")
    rb = tube_od / 2.0 + BORE_CLEAR
    if snap_opening / 2.0 >= rb:
        reject("snap_opening is wider than the bore", param="snap_opening")
    ro = rb + clip_wall

    climb = pitch / (2.0 * math.pi)
    ds = math.hypot(radius, climb)
    turn_len = 2.0 * math.pi * ds
    quarter_arc = turn_len / 4.0
    if quarter_arc <= 0:
        reject("pitch and coil diameter do not make a helix", param="pitch")

    # The hook sits on the bed, so its bore centre is clip_width/2 off the
    # tower's radial plane. Pull the arm in until that offset still lands on
    # the 43.5 mm centreline, and shift the helix phase to match.
    bore_z = clip_width / 2.0
    if bore_z >= radius - 1.0:
        reject("clip_width is too long for the coil radius", param="clip_width")
    radial = math.sqrt(radius * radius - bore_z * bore_z)
    tube_x = radial - R_PIN
    if tube_x < rib_radial / 2.0 + 4.0:
        reject("the hook has no room between the rib and the tube", param="coil_diameter")
    bore_phase = math.atan2(-bore_z, radial)

    # Straight entry: tangent from the first hook out to the ring's outer edge.
    # That run is part of the 965.2 mm and is not helix. There is no straight
    # tail past the cut end; whatever remains after the lead is the coil.
    if radius >= R_OUT - 1.0:
        reject("the coil meets the outside of the ring, so the straight entry has nowhere to land", param="coil_diameter")
    lead_in = math.sqrt(R_OUT * R_OUT - radius * radius)
    if lead_in >= tube_length - turn_len:
        reject(
            f"the straight entry is {lead_in:.1f} mm, which leaves less than one turn of helix",
            param="tube_length",
        )
    helix_length = tube_length - lead_in
    turns = helix_length / turn_len

    # A hook is kept when its grip (clip_width centred on the station) still
    # touches tube. Stations past the cut end are not printed.
    limit = (helix_length + clip_width / 2.0) / quarter_arc
    k_top = int(math.floor(limit + 1e-9))
    if k_top > 60:
        reject("tube_length asks for more than 60 hooks", param="tube_length")
    stations = []
    for k in range(k_top + 1):
        centre = k * quarter_arc
        if centre - clip_width / 2.0 <= helix_length + 1e-6:
            stations.append(k)
    if len(stations) < 4:
        reject("the helix is shorter than one turn, so the four towers have nothing to share", param="tube_length")
    for _ in range(int(extra_stations)):
        stations.append(stations[-1] + 1)
    k_last = stations[-1]
    tail = helix_length - (k_last - int(extra_stations)) * quarter_arc
    # The tail past the last real station, before extra_stations pads it.
    real_last = k_last - int(extra_stations)
    tail = helix_length - real_last * quarter_arc

    cap_y = FOOT_CLEAR + k_last * (pitch / 4.0) + ro + 4.0
    tower_len = cap_y + CAP_T
    bed_length = tower_len + FLANGE_T
    if bed_length > 290.0:
        reject(
            f"the tower would be {bed_length:.0f} mm long on the bed; the CR-10 bed is 300 mm. Shorten the tube.",
            param="tube_length",
        )

    arm_bevel = clip_arm / 2.0
    x_face = rib_radial / 2.0
    if x_face + arm_bevel + 1.0 >= tube_x - ro:
        reject("the root fillet reaches the hook bore; narrow clip_arm", param="clip_arm")
    # Fillets on one tower must not meet the next hook a pitch higher.
    if clip_arm / 2.0 + arm_bevel > pitch / 2.0 - 1.0:
        reject("the root fillet meets the next hook on this tower", param="clip_arm")

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
        arm_bevel=arm_bevel,
        joint_clear=joint_clear,
        tube_length=tube_length,
        extra_stations=int(extra_stations),
        pitch=pitch,
        radius=radius,
        rb=rb,
        ro=ro,
        quarter=pitch / 4.0,
        climb=climb,
        ds=ds,
        turn_len=turn_len,
        quarter_arc=quarter_arc,
        stations=tuple(stations),
        k_last=k_last,
        lead_in=lead_in,
        helix_length=helix_length,
        tail=tail,
        turns=turns,
        coil_height=turns * pitch,
        bore_z=bore_z,
        bore_phase=bore_phase,
        tube_x=tube_x,
        cap_y=cap_y,
        tower_len=tower_len,
        bed_length=bed_length,
        bed_z=0.0,
        rib_z_center=rib_tangential / 2.0,
    )


def default_specs(**overrides):
    """The coil Tom asked for. Sliders on the parts override the same names."""
    values = dict(
        tube_od=TUBE_OD,
        coil_diameter=COIL_DIAMETER,
        pitch=PITCH,
        clip_wall=CLIP_WALL,
        snap_opening=SNAP_OPENING,
        clip_width=CLIP_WIDTH,
        rib_radial=RIB_RADIAL,
        rib_tangential=RIB_TANGENTIAL,
        clip_arm=CLIP_ARM,
        joint_clear=JOINT_CLEAR,
        tube_length=TUBE_LENGTH,
        extra_stations=0,
    )
    values.update(overrides)
    return specs(**values)


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
    """World angle of tower index, 45° off the axes. 0° is +X, 90° is +Y (back)."""
    return index * math.pi / 2.0 + TOWER_PHASE


def station_theta(k):
    """Tower angle of station k. The coil starts at THETA0 and climbs counterclockwise."""
    return THETA0 + k * math.pi / 2.0


def stations_for(spec, index):
    """Hook stations whose tower angle lands on this tower."""
    target = tower_angle(index)
    found = []
    for k in spec.stations:
        ang = station_theta(k) % (2.0 * math.pi)
        delta = abs(ang - target)
        if min(delta, 2.0 * math.pi - delta) < 1e-6:
            found.append(k)
    return tuple(found)


def clip_y(spec, k):
    """Height of station k above the foot, which sits on the ring."""
    return FOOT_CLEAR + k * spec.quarter


def clip_world_z(spec, k):
    """World Z of station k. The ring top is FLANGE_T above the deck."""
    return FLANGE_T + clip_y(spec, k)


def helix_phi(spec, theta):
    """Geometric angle of the tube centreline at a tower angle theta."""
    return theta + spec.bore_phase


def helix_point(spec, theta):
    """Tube centreline through the hook bores. theta is the tower angle."""
    phi = helix_phi(spec, theta)
    z = FLANGE_T + clip_y(spec, 0) + spec.climb * (theta - THETA0)
    return (
        spec.radius * math.cos(phi),
        spec.radius * math.sin(phi),
        z,
    )


def lead_in_points(spec):
    """Straight tangent from the ring's outer edge to the first hook centre.

    The point at the ring is where the tube enters from the side. The other
    end is the first bore. Both sit at the first hook's height.
    """
    theta = station_theta(0)
    bore = helix_point(spec, theta)
    phi = helix_phi(spec, theta)
    # Increasing phi walks the helix. The entry is the other way.
    incoming = (math.sin(phi), -math.cos(phi))
    start = (
        bore[0] + spec.lead_in * incoming[0],
        bore[1] + spec.lead_in * incoming[1],
        bore[2],
    )
    return start, bore


def _bore_void(spec, cy):
    """Curved bore and outward mouth. Sections stay parallel to the bed.

    The grip matches the side-printed clip: bore radius tube/2 + 0.2, mouth
    11.5 mm, wall 2.4 mm. The loft follows the helix so the tube is not
    asked to be straight inside the hook.
    """
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
        reject("a hook bore did not loft as a single solid", param="clip_width")
    return solid


def _digit(char):
    return Text(str(char), font_size=MARK_FONT, font="DejaVu Sans")


def _tower_mark(spec, index):
    """Recess the numeral into the top face of the rib, just above the ring."""
    face = _digit(TOWER_MARK[index])
    return Pos(0, 8.0, spec.rib_tangential - MARK_DEPTH) * extrude(face, MARK_DEPTH + 0.2)


def _slab(face, z0, height):
    return Pos(0, 0, z0) * extrude(face, height)


def _root_fillet(spec, y, sign):
    """Fillet in the layer plane where the neck leaves the rib.

    sign is +1 above the neck and -1 below it. The arc sits outside the rib
    and overlaps the neck, so the union is one face with the normal pointing
    up. Extrude then builds on the bed instead of through it. The fillet adds
    material at the root: that is the gusset.
    """
    half = spec.clip_arm / 2.0
    radius = spec.arm_bevel
    x0 = spec.rib_radial / 2.0
    # Overlap the neck, not the rib. A bite into the rib becomes a fin once
    # the hook is taller than the spine.
    into = 0.2
    corner_y = y + sign * half
    cy = y + sign * (half + radius)
    cx = x0 + radius
    steps = 10
    # CCW, seen from +Z: corner, out along the neck, arc back to the rib face.
    pts = [
        (x0, corner_y - sign * into),
        (x0 + radius, corner_y - sign * into),
    ]
    # Upper fillet: arc from the neck-side tangent (angle 3π/2) back to the
    # rib face (angle π). Lower fillet is the mirror, π/2 back to π.
    if sign > 0:
        a0, a1 = 1.5 * math.pi, math.pi
    else:
        a0, a1 = 0.5 * math.pi, math.pi
    for i in range(steps + 1):
        a = a0 + (a1 - a0) * i / steps
        pts.append((cx + radius * math.cos(a), cy + radius * math.sin(a)))
    pts.append((x0, cy))
    # A clockwise outline extrudes through the bed. Keep the material on +Z.
    area = 0.0
    for i, (x1, y1) in enumerate(pts):
        x2, y2 = pts[(i + 1) % len(pts)]
        area += x1 * y2 - x2 * y1
    if area < 0.0:
        pts.reverse()
    return Polygon(pts)


def _hook_plan(spec, y):
    """Layer outline of one hook: neck, C, and the two root fillets."""
    half = spec.clip_arm / 2.0
    x_face = spec.rib_radial / 2.0
    x_end = spec.tube_x - spec.rb - 0.4
    if x_face + 1.0 >= x_end:
        reject("the hook neck has no room between the rib and the bore", param="clip_arm")
    neck = Pos((x_face + x_end) / 2.0, y) * Rectangle(x_end - x_face, spec.clip_arm)
    clip = Pos(spec.tube_x, y) * Circle(spec.ro)
    plan = neck + clip
    junctions = [v for v in plan.vertices() if v.X > x_face + 1.0]
    if len(junctions) >= 2:
        plan = fillet(junctions, min(2.0, spec.arm_bevel))
    plan = plan + _root_fillet(spec, y, 1.0) + _root_fillet(spec, y, -1.0)
    return plan


def build_tower(spec, index):
    """One tower, in its print frame. +X out, +Y up the tower, +Z print-up.

    The flat face is z = 0, the whole way: rib, neck, fillets, and hook.
    Nothing hangs below that plane, so the part lies on the bed with no support.
    The C is the outline in XY, which is the layer plane.
    """
    if index not in TOWER_MARK:
        reject("tower index must be 0, 1, 2 or 3", param="extra_stations")
    rib_h = spec.cap_y + FLANGE_T
    rib = Pos(0, (spec.cap_y - FLANGE_T) / 2.0) * Rectangle(spec.rib_radial, rib_h)
    tenon = Pos(0, spec.cap_y + CAP_T / 2.0) * Rectangle(spec.rib_radial, CAP_T)
    solid = _slab(rib + tenon, spec.bed_z, spec.rib_tangential)
    solid -= _tower_mark(spec, index)
    found = stations_for(spec, index)
    if not found:
        reject("this tower has no hook on the tube", param="tube_length")
    for k in found:
        y = clip_y(spec, k)
        solid += _slab(_hook_plan(spec, y), spec.bed_z, spec.clip_width)
        solid -= Pos(0, 0, spec.bed_z + spec.bore_z) * _bore_void(spec, y)
    if len(solid.solids()) != 1:
        reject("the tower did not come back as a single solid", param="rib_radial")
    return solid


def _slot(spec, angle):
    """Through-slot the rib drops into. Same size at every tower."""
    radial = spec.rib_radial + spec.joint_clear
    tangent = spec.rib_tangential + spec.joint_clear
    x = R_PIN * math.cos(angle)
    y = R_PIN * math.sin(angle)
    return (
        Pos(x, y, FLANGE_T / 2.0)
        * Rot(0, 0, math.degrees(angle))
        * Pos(0, -spec.rib_z_center, 0)
        * Box(radial, tangent, FLANGE_T + 2.0)
    )


def _base_mark(spec, index, angle):
    """Recessed numeral on the top of the ring, just outboard of the slot."""
    face = Text(TOWER_MARK[index], font_size=MARK_FONT, font="DejaVu Sans")
    pocket = extrude(face, MARK_DEPTH + 0.2)
    radius = R_PIN + spec.rib_radial / 2.0 + 4.5
    x = radius * math.cos(angle)
    y = radius * math.sin(angle)
    return Pos(x, y, FLANGE_T - MARK_DEPTH) * pocket


def build_base(spec, holes):
    """Ring. Four identical slots, a recessed number by each, bolts, cable window.

    The slot is the same 6.2 by 3.4 mm opening as the earlier four-tower ring,
    so a tower from this set drops into that ring.
    """
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
    """Thin hoop. Pockets match the rib, the same hoop as the earlier cap."""
    wall = 2.4
    outer = R_PIN + spec.rib_radial / 2.0 + wall
    inner = R_PIN - spec.rib_radial / 2.0 - wall
    if inner < 8.0:
        reject("rib_radial leaves no hole in the middle of the cap", param="rib_radial")
    ring = extrude(Circle(outer) - Circle(inner), CAP_T)
    body = ring
    radial = spec.rib_radial + spec.joint_clear
    tangent = spec.rib_tangential + spec.joint_clear
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
    z_dir = Vector(math.sin(angle), -math.cos(angle), 0)
    origin = Vector(R_PIN * math.cos(angle), R_PIN * math.sin(angle), FLANGE_T)
    return Plane(origin=origin, x_dir=radial, z_dir=z_dir) * solid


def bore_world(spec, index, k):
    """World point of this hook's bore centre, from the print frame."""
    angle = tower_angle(index)
    radial = Vector(math.cos(angle), math.sin(angle), 0)
    z_dir = Vector(math.sin(angle), -math.cos(angle), 0)
    up = Vector(0, 0, 1)
    origin = Vector(R_PIN * math.cos(angle), R_PIN * math.sin(angle), FLANGE_T)
    local = Vector(spec.tube_x, clip_y(spec, k), spec.bore_z)
    return origin + local.X * radial + local.Y * up + local.Z * z_dir


def build_coupon(spec):
    """A short rib, one hook, and a block with the same slot.

    Print it on its side, like a tower. Snap the link. The tab should drop
    into the gauge. The opening is joint_clear larger than the rib.
    """
    z0 = spec.bed_z
    y_clip = 22.0
    rib_h = 28.0 + FLANGE_T
    rib = Pos(0, (28.0 - FLANGE_T) / 2.0) * Rectangle(spec.rib_radial, rib_h)
    solid = _slab(rib, z0, spec.rib_tangential)
    solid -= _tower_mark(spec, 1)
    solid += _slab(_hook_plan(spec, y_clip), z0, spec.clip_width)
    solid -= Pos(0, 0, z0 + spec.bore_z) * _bore_void(spec, y_clip)
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


def counts(spec):
    """Hooks on each tower, in number order 1..4."""
    order = (1, 2, 3, 0)
    return tuple(len(stations_for(spec, index)) for index in order)
