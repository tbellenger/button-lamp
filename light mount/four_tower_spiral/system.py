"""Four-tower spiral mount.

A ring bolts to the deck. Four towers stand on the diagonals, 45 degrees
off the axes, so the front pair sits at the corners instead of one tower
blocking the view from the buttons. Each tower is printed on its side, so
the clip C lies in the layer plane. A cap drops onto the four tops and
holds that radius.

The helix starts on tower 1, the back-left tower. It climbs counterclockwise
and up. The other towers pick up the same helix a quarter turn later.
Mounting holes and the cable window stay put: +Y is still the back.

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
# Index 0 was on +X. The whole set is turned 45° counterclockwise so tower 2
# (yellow, number 2) and tower 3 (green, number 3) land on the front corners.
# +Y stays the back, toward the cable window. -Y is the front, toward the buttons.
TOWER_NAMES = ("0", "90", "180", "270")
TOWER_PHASE = math.pi / 4.0
# Lowest clip is on tower 1. Same helix, shifted with the towers.
THETA0 = math.pi / 2.0 + TOWER_PHASE
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
# Spoke along the tower. The earlier joint was a 6 mm spoke; it scales with the
# clip, 6 * 10/14 = 4.29 mm. The 45° gusset is half of that, 2.15 mm, the same
# proportion as the 3 mm gusset on the 6 mm spoke.
CLIP_ARM = 4.29
# Coil inputs. pitch and tube_length are also recorded in measurements.toml.
TUBE_OD = 13.0
COIL_DIAMETER = 100.0
# Exactly twice the tube OD, so the clear gap equals the tube OD.
PITCH = 26.0
# 38 inches along the tube centreline. 38 * 25.4 = 965.2.
TUBE_LENGTH = 965.2
CLIP_WALL = 2.4
SNAP_OPENING = 11.5
# Along the tube. Was 14 mm. 10 mm is the original cage clip length for this tube,
# and it still covers the short tail past the last quarter-turn station.
CLIP_WIDTH = 10.0
JOINT_CLEAR = 0.2


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
    quarter_arc = 0.5 * math.pi * ds
    # Clips sit on the four towers, so they are a quarter turn apart. The cut end
    # is placed on the last of those clips. Whatever is left of the tube length
    # lies before the first clip, still inside that clip's incoming grip.
    # The outgoing side of a clip is only the bed face of the bore, so a tail
    # past the last centre would leave the plastic.
    if quarter_arc <= 0:
        reject("pitch and coil diameter do not make a helix", param="pitch")
    k_max = int(math.floor(tube_length / quarter_arc + 1e-9))
    if k_max > 60:
        reject("tube_length asks for more than 60 clips", param="tube_length")
    if k_max < 3:
        reject("the tube is shorter than one turn, so the four towers have nothing to share", param="tube_length")
    lead = tube_length - k_max * quarter_arc
    # Incoming grip is about one clip-width of arc. The entry has to land in it.
    incoming = clip_width * ds / radius
    if lead > incoming - 0.5:
        reject(
            f"the tube entry is {lead:.1f} mm of arc before the first clip, "
            f"past the {incoming:.1f} mm the clip holds. Widen clip_width.",
            param="tube_length",
        )
    stations = list(range(k_max + 1))
    for _ in range(int(extra_stations)):
        stations.append(stations[-1] + 1)
    cap_y = FOOT_CLEAR + k_max * (pitch / 4.0) + ro + 4.0
    tower_len = cap_y + CAP_T
    # The foot continues FLANGE_T below the ring top, and that length lies on the bed.
    bed_length = tower_len + FLANGE_T
    if bed_length > 290.0:
        reject(
            f"the tower would be {bed_length:.0f} mm long on the bed; the CR-10 bed is 300 mm. Shorten the tube.",
            param="tube_length",
        )
    turns = tube_length / (2.0 * math.pi * ds)
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
        # 45° gusset, both sides of the spoke, where it meets the rib's outer face.
        # Half the spoke: the same ratio as the 3 mm gusset on the 6 mm spoke.
        arm_bevel=clip_arm / 2.0,
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
        quarter_arc=quarter_arc,
        stations=tuple(stations),
        k_max=k_max,
        # Arc of tube before the first clip. The cut end is the last clip.
        lead=lead,
        entry_theta=THETA0 - lead / ds,
        end_theta=THETA0 + k_max * math.pi / 2.0,
        turns=turns,
        coil_height=turns * pitch,
        cap_y=cap_y,
        tower_len=tower_len,
        bed_length=bed_length,
        # Rib sits on z=0. Its centre is what the slot and the cap line up with.
        bed_z=0.0,
        rib_z_center=rib_tangential / 2.0,
        # Tube centre in the tower frame. The slot is at local x = 0.
        tube_x=radius - R_PIN,
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


def station_theta(k):
    """Helix angle of station k. The coil starts at THETA0 and climbs counterclockwise."""
    return THETA0 + k * math.pi / 2.0


def clip_world_z(spec, k):
    """World Z of station k. The ring top is FLANGE_T above the deck."""
    return FLANGE_T + clip_y(spec, k)


def helix_point(spec, theta):
    """Tube centreline. theta = 0 is +X. Z matches clip_world_z at every station."""
    z = FLANGE_T + clip_y(spec, 0) + spec.climb * (theta - THETA0)
    return (
        spec.radius * math.cos(theta),
        spec.radius * math.sin(theta),
        z,
    )


def arm_profile(spec):
    """Print-+Z top of the spoke. X is radial, Z is print-up.

    Across the rib the top stays at the rib height, so the root does not
    stand proud of the 6 × 3.2 mm section. Outside the outer face the top
    rises in one slope to the clip.
    """
    half = spec.clip_arm / 2.0
    x_far = -spec.rib_radial / 2.0
    x_face = spec.rib_radial / 2.0
    x_hi = spec.tube_x - math.sqrt(spec.ro * spec.ro - half * half)
    x_end = spec.tube_x - spec.rb - 0.4
    span = x_hi - x_face
    drop = spec.clip_width - spec.rib_tangential

    def z_at(x):
        if x <= x_face:
            return spec.rib_tangential
        if x >= x_hi:
            return spec.clip_width
        return spec.rib_tangential + drop * (x - x_face) / span

    return SimpleNamespace(
        half=half,
        x_far=x_far,
        x_face=x_face,
        x_hi=x_hi,
        x_end=x_end,
        z_lo=spec.rib_tangential,
        z_hi=spec.clip_width,
        z_at_face=z_at(x_face),
        drop=drop,
        span=span,
        angle_deg=math.degrees(math.atan2(drop, span)),
    )


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
# After the 45° turn: 1 back-left, 2 front-left, 3 front-right, 4 back-right.
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


def _arm_wedge(spec, y):
    """Spoke from the rib to the clip, in the print frame.

    The plan is a rectangle clip_arm long along the tower. Across the rib the
    top stays at the rib height, so the root is inside the 6 × 3.2 mm section.
    Outside the outer face the top rises to the clip. The underside stays on
    the bed. The 45° gussets sit inside the rib and are clipped to it.
    """
    half = spec.clip_arm / 2.0
    if half >= spec.ro - 0.4:
        reject("clip_arm is wide enough to reach through the clip wall", param="clip_arm")
    profile = arm_profile(spec)
    x_far = profile.x_far
    x_face = profile.x_face
    x_hi = profile.x_hi
    x_end = profile.x_end
    z_lo = profile.z_lo
    z_hi = profile.z_hi
    if x_face + 1.0 >= x_hi or x_hi + 0.4 >= x_end:
        reject("the spoke ramp has no room between the rib and the bore", param="clip_arm")
    # Sketch: X is radial, Y stands in for print +Z. Extrude the spoke thickness,
    # then Rot(90) about X lays that thickness along the tower and the sketch Y up.
    face = Polygon(
        [
            (x_far, 0.0),
            (x_end, 0.0),
            (x_end, z_hi),
            (x_hi, z_hi),
            (x_face, z_lo),
            (x_far, z_lo),
        ]
    )
    spoke = Pos(0, y + half, 0) * Rot(90, 0, 0) * extrude(face, spec.clip_arm)
    body = spoke + _gussets(spec, y)
    if len(body.solids()) != 1:
        reject("the spoke and its gussets did not fuse", param="clip_arm")
    return body


def _rib_envelope(spec, y):
    """The rib's 6 × 3.2 mm cross-section, long enough to cover this join.

    A hair larger than the rib so the gusset's face on the boundary is included,
    and still short of sticking past the real faces once the hair is inside.
    """
    reach = spec.clip_arm + 2.0 * spec.arm_bevel + 2.0
    face = Pos(0, y) * Rectangle(spec.rib_radial, reach)
    return _slab(face, spec.bed_z, spec.rib_tangential)


def _gussets(spec, y):
    """45° gussets inside the rib, one each side of the spoke.

    The leg is arm_bevel, half the spoke. Both triangles lie in the rib's
    radial width and only as tall as the rib. The intersection with the rib
    envelope drops anything that would pass a side face.
    """
    half = spec.clip_arm / 2.0
    bevel = spec.arm_bevel
    x_out = spec.rib_radial / 2.0
    x_in = x_out - bevel
    if x_in <= -x_out + 0.4:
        reject("arm_bevel is deeper than the rib", param="clip_arm")
    lower = Polygon(
        [
            (x_out, y - half),
            (x_in, y - half),
            (x_out, y - half - bevel),
        ]
    )
    upper = Polygon(
        [
            (x_out, y + half),
            (x_in, y + half),
            (x_out, y + half + bevel),
        ]
    )
    solid = _slab(lower + upper, spec.bed_z, spec.rib_tangential)
    clipped = solid & _rib_envelope(spec, y)
    if len(clipped.solids()) < 1:
        reject("the gussets fell outside the rib", param="clip_arm")
    return clipped


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
        solid += _slab(clip, spec.bed_z, spec.clip_width)
        solid += _arm_wedge(spec, y)
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
    clip = Pos(spec.tube_x, y_clip) * Circle(spec.ro)
    solid += _slab(clip, z0, spec.clip_width)
    solid += _arm_wedge(spec, y_clip)
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
