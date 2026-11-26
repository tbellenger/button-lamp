"""Four-tower spiral. The openings are cut into the tower bar.

The bar is a slim spine. Each clip is a C-shaped ring on the outer edge,
joined by a short root, opening away from the lamp. The foot is a rectangular
tab that drops through a slot in a lobe on the base ring. The top of the
spine carries the curved slot for the plain disc.

The base ring is only as wide as the four M3 holes. A lobe reaches inward
under each tower. The middle is open, so the light cable still comes up
through the deck hole. The coil and the clip centres are unchanged.

Each tower prints lying on its side. The ring and the disc print flat.
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
    Text,
    Vector,
    extrude,
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
FLANGE_T = 4.5
# 45° relief on the outer bed corner of the bar. Kept with the bar section.
BOLT_CHAMFER = 2.0
FOOT_CLEAR = 16.0
# Base ring. 2.4 mm is twelve layers at 0.2 mm. The top disc is separate.
BASE_T = 2.4
# Top disc. 1.2 mm is six layers at 0.2 mm. Thinner than that and a ring
# this size flops in the hand. 2.4 mm of radial width is six perimeters
# on a 0.4 mm nozzle, centred on the coil.
DISC_T = 1.2
DISC_RADIAL = 2.4
DISC_MID_R = 43.5
DISC_R = DISC_MID_R + DISC_RADIAL / 2.0
DISC_HOLE_R = DISC_MID_R - DISC_RADIAL / 2.0
# Spine. 4 mm radial is the narrow beam that still leaves a solid root on the
# clip wall without entering the bore. 8 mm matches the clip thickness, so
# the tower is one slab on the bed.
SPINE_RADIAL = 4.0
ROOT_GAP = 0.3
TAB_LEN = 4.0
LOBE_WALL = 1.6
# Band around each M3 hole, from the hole edge to the ring edge.
BAND_WALL = 1.6
# Solid spine between the outside of the top clip and the floor of the disc slot.
SLOT_ABOVE_CLIP = 1.4
# PLA outside an M3 clearance hole, measured from the hole edge. 3 mm covers
# the M3 socket head (Ø5.5) and leaves a rim the screw can clamp.
BOLT_MARGIN = 3.0
# Tabs on the base ring only, perpendicular to the ring, one each side of a tower.
GUIDE_T = 2.0
GUIDE_H = 4.0
BASE_GUIDE_RADIAL = 2.4
TUBE_OD = 13.0
COIL_DIAMETER = 100.0
PITCH = 26.0
TUBE_LENGTH = 965.2
# 1.6 mm is four perimeters on a 0.4 mm nozzle. The lips only open 0.75 mm
# each to pass the 13 mm tube, then sit slack in the 13.4 mm bore.
CLIP_WALL = 1.6
SNAP_OPENING = 11.5
# Grip along the tube. The bar is this thick, so the opening is this long.
CLIP_WIDTH = 8.0

# Helix order, counterclockwise and up. 1 is the lowest hook.
TOWER_MARK = {1: "1", 2: "2", 3: "3", 0: "4"}


def _base_ring_radii():
    """Inner and outer radius of the base band, from the deck bolt square.

    The band is the M3 hole plus BAND_WALL on each side. The lobes that hold
    the towers are added inside this edge.
    """
    bolt = measured("top_deck_bolt_spacing")
    hole = measured("m3_clearance")
    bolt_r = bolt / math.sqrt(2.0)
    return bolt_r - hole / 2.0 - BAND_WALL, bolt_r + hole / 2.0 + BAND_WALL


def _span_x(r0, r1, z_a, z_b):
    """Local X whose world radius stays between r0 and r1 over z_a..z_b."""
    z_lo = min(abs(z_a), abs(z_b))
    z_hi = max(abs(z_a), abs(z_b))
    if r0 <= z_hi or r1 <= z_hi:
        reject("a guide radius falls inside its own offset", param="joint_clear")
    x0 = math.sqrt(r0 * r0 - z_lo * z_lo) - R_PIN
    x1 = math.sqrt(r1 * r1 - z_hi * z_hi) - R_PIN
    return x0, x1


def disc_world_z(spec):
    """World Z of the underside of the top disc, seated in the slots."""
    return FLANGE_T + spec.cap_y - spec.slot_depth


def specs(
    tube_od, coil_diameter, pitch, clip_wall, snap_opening, clip_width,
    joint_clear, tube_length, extra_stations,
):
    """Derived sizes, or a refusal when a combination cannot be printed."""
    if tube_od < 8.0 or tube_od > 20.0:
        reject("tube_od must stay between 8 and 20 mm", param="tube_od")
    if pitch <= tube_od:
        reject("pitch must be greater than the tube diameter", param="pitch")
    gap = pitch - tube_od
    if gap < 6.0:
        reject("gap under 6 mm leaves no room between turns", param="pitch")
    if clip_wall < 1.6 or clip_wall > 4.0:
        reject(
            "clip_wall must stay between 1.6 and 4 mm. 1.6 mm is four perimeters on a 0.4 mm nozzle.",
            param="clip_wall",
        )
    if clip_width < 7.0 or clip_width > 18.0:
        reject("clip_width must stay between 7 and 18 mm along the tube", param="clip_width")
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

    # The bar lies on the bed, so the bore centre is clip_width/2 off the
    # tower's radial plane. Shift the bar until that offset is still on the
    # 43.5 mm centreline. The phase follows, so the tube is not asked to kink.
    bore_z = clip_width / 2.0
    if bore_z >= radius - 1.0:
        reject("clip_width is too long for the coil radius", param="clip_width")
    radial = math.sqrt(radius * radius - bore_z * bore_z)
    tube_x = radial - R_PIN
    x_in = tube_x - ro
    x_out = tube_x + ro
    if x_out - x_in < 8.0:
        reject("the bar is too narrow to plug into a slot", param="clip_wall")
    bore_phase = math.atan2(-bore_z, radial)
    helix_angle = math.atan2(pitch, 2.0 * math.pi * radius)

    # The rim tangent, from the first clip out to the ring edge, is fixed by
    # the two radii. At 26 mm pitch that leaves the cut end 26 mm past the
    # last clip. A pitch that pulled the end back onto a tower would be about
    # 72 mm, and the coil would be more than twice as tall. The straight run
    # is lengthened instead, so the helix is an exact number of quarter turns:
    # it starts in the first clip and the cut end sits in the last.
    if radius >= R_OUT - 1.0:
        reject("the coil meets the outside of the ring, so the straight entry has nowhere to land", param="coil_diameter")
    rim_lead = math.sqrt(R_OUT * R_OUT - radius * radius)
    quarters_at_rim = (tube_length - rim_lead) / quarter_arc
    n_quarters = int(math.floor(quarters_at_rim + 1e-9))
    if n_quarters < 4:
        reject("the tube is shorter than one turn", param="tube_length")
    helix_length = n_quarters * quarter_arc
    lead_in = tube_length - helix_length
    if lead_in < rim_lead - 1e-6:
        reject(
            "landing the cut end on a clip would start the helix inside the ring",
            param="tube_length",
        )
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

    # The slot is cut down from the top. 1.4 mm of solid stays between the
    # clip and the slot floor. The slot itself is the disc plus 0.2 mm.
    slot_depth = DISC_T + joint_clear
    cap_y = FOOT_CLEAR + k_last * (pitch / 4.0) + ro + SLOT_ABOVE_CLIP + slot_depth
    slot_r_outer = DISC_R + joint_clear / 2.0
    slot_r_inner = DISC_HOLE_R - joint_clear / 2.0
    # The bar's outer face is closest to the axis at the bed edge (z = 0).
    # The inner face is farthest at the top edge (z = clip_width).
    face_out = R_PIN + x_out
    face_in = math.hypot(R_PIN + x_in, clip_width)
    if slot_r_outer > face_out - 2.4:
        reject("the disc rim leaves under 2.4 mm of bar outside the slot", param="clip_wall")
    if slot_r_inner < face_in + 2.4:
        reject("the disc rim leaves under 2.4 mm of bar inside the slot", param="clip_wall")
    if slot_depth >= 4.0:
        reject("the disc slot would cut more than 4 mm off the tower top", param="joint_clear")
    base_r_in, base_r_out = _base_ring_radii()
    spine_out = tube_x - rb - ROOT_GAP
    spine_in = spine_out - SPINE_RADIAL
    if spine_out <= spine_in:
        reject("the spine has no radial thickness", param="clip_wall")
    tower_len = cap_y
    bed_length = tower_len + FLANGE_T
    if bed_length > 290.0:
        reject(
            f"the tower would be {bed_length:.0f} mm long on the bed; the CR-10 bed is 300 mm. Shorten the tube.",
            param="tube_length",
        )
    bar_w = x_out - x_in
    print_x = bar_w
    print_y = bed_length
    print_z = clip_width

    return SimpleNamespace(
        tube_od=tube_od,
        coil_diameter=coil_diameter,
        gap=gap,
        clip_wall=clip_wall,
        snap_opening=snap_opening,
        clip_width=clip_width,
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
        rim_lead=rim_lead,
        helix_length=helix_length,
        tail=tail,
        turns=turns,
        coil_height=turns * pitch,
        bore_z=bore_z,
        bore_phase=bore_phase,
        helix_angle_deg=math.degrees(helix_angle),
        tube_x=tube_x,
        x_in=x_in,
        x_out=x_out,
        spine_in=spine_in,
        spine_out=spine_out,
        bar_w=bar_w,
        slot_radial=bar_w + joint_clear,
        slot_tangent=clip_width + joint_clear,
        cap_y=cap_y,
        disc_r=DISC_R,
        disc_hole_r=DISC_HOLE_R,
        disc_t=DISC_T,
        slot_depth=slot_depth,
        slot_r_outer=slot_r_outer,
        slot_r_inner=slot_r_inner,
        base_r_in=base_r_in,
        base_r_out=base_r_out,
        base_slot_depth=BASE_T + joint_clear,
        base_slot_r_inner=base_r_in - joint_clear,
        base_slot_r_outer=base_r_out + joint_clear,
        tower_len=tower_len,
        bed_length=bed_length,
        print_x=print_x,
        print_y=print_y,
        print_z=print_z,
        bed_z=0.0,
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
    """Bore through the bar, mouth on the outer face.

    Sections stay parallel to the bed and follow the helix, so the opening
    climbs at the coil's tangent instead of kinking the tube. The mouth opens
    toward +X, away from the lamp, so the tube presses in from outside after
    the cap is on. The slot is 11.5 mm, narrower than the 13 mm tube, and it
    stops inside the bore, so the lips on either side keep the tube. The
    inner face of the bar stays solid. The foot and cap arms are on that side
    and do not cross the mouth.
    """
    radius = spec.radius
    cx = spec.tube_x
    half = spec.clip_width / 2.0 + 2.0
    alpha = math.asin(min(0.5, half / radius))
    mouth_w = 2.0 * spec.ro + 8.0
    sections = []
    for i in range(5):
        dth = -alpha + 2.0 * alpha * i / 4.0
        x = cx - radius * (1.0 - math.cos(dth))
        y = cy + spec.climb * dth
        z = -radius * math.sin(dth)
        bore = Pos(x, y, z) * Circle(spec.rb)
        mouth = Pos(x + spec.ro, y, z) * Rectangle(mouth_w, spec.snap_opening)
        sections.append(bore + mouth)
    solid = loft(sections, ruled=True)
    if len(solid.solids()) != 1:
        reject("a clip bore did not loft as a single solid", param="clip_width")
    return solid


def _digit(char):
    return Text(str(char), font_size=MARK_FONT, font="DejaVu Sans")


def _tower_mark(spec, index):
    """Recess the numeral into the top face of the spine, just above the tab."""
    face = _digit(TOWER_MARK[index])
    x = (spec.spine_in + spec.spine_out) / 2.0
    return Pos(x, 6.0, spec.clip_width - MARK_DEPTH) * extrude(face, MARK_DEPTH + 0.2)


def _bed_corner_chamfer(spec, y0, y1, leg):
    """45° triangle off the outer bed corner, running along the tower."""
    x1 = spec.x_out
    face = Polygon([(x1 + 0.4, -0.4), (x1 - leg, -0.4), (x1 + 0.4, leg + 0.2)])
    length = y1 - y0
    solid = extrude(face, length)
    # Sketch Y becomes print +Z. The extrusion, which was +Z, becomes -Y,
    # so a shift by y1 puts the cut on y0..y1.
    return Pos(0, y1, 0) * Rot(90, 0, 0) * solid


def _clip_disk(spec, cy):
    """Solid disk of the clip ring, following the same helix as the bore.

    The spine overlaps the inner edge of this disk. Subtracting the bore
    and the mouth leaves the C and the short root.
    """
    radius = spec.radius
    cx = spec.tube_x
    reach = spec.clip_width / 2.0 + 2.0
    alpha = math.asin(min(0.5, reach / radius))
    sections = []
    for i in range(5):
        dth = -alpha + 2.0 * alpha * i / 4.0
        x = cx - radius * (1.0 - math.cos(dth))
        y = cy + spec.climb * dth
        z = -radius * math.sin(dth)
        sections.append(Pos(x, y, z) * Circle(spec.ro))
    disk = loft(sections, ruled=True)
    if len(disk.solids()) != 1:
        reject("a clip ring did not loft as a single solid", param="clip_width")
    return disk


def _top_lug_bounds(spec):
    """Local X and Y of the pad that holds the disc slot, with 1.6 mm walls."""
    z_far = spec.clip_width
    x_in = math.sqrt(spec.slot_r_inner ** 2 - z_far * z_far) - R_PIN - CLIP_WALL
    x_out = spec.slot_r_outer - R_PIN + CLIP_WALL
    y0 = spec.cap_y - spec.slot_depth - SLOT_ABOVE_CLIP
    return min(spec.spine_in, x_in), max(spec.spine_out, x_out), y0


def build_tower(spec, index):
    """One tower, in its print frame. +X out, +Y up the tower, +Z print-up.

    A 4 mm spine carries a C-ring at each clip. The foot is the tab. The top
    pad holds the disc slot. Nothing hangs below z = 0.
    """
    if index not in TOWER_MARK:
        reject("tower index must be 0, 1, 2 or 3", param="extra_stations")
    found = stations_for(spec, index)
    if not found:
        reject("this tower has no opening on the tube", param="tube_length")
    z0 = spec.bed_z
    y0 = -FLANGE_T
    y1 = spec.cap_y
    solid = _box(spec.spine_in, spec.spine_out, y0, y1, z0, spec.clip_width)
    for k in found:
        cy = clip_y(spec, k)
        disk = Pos(0, 0, z0 + spec.bore_z) * _clip_disk(spec, cy)
        slab = _box(
            spec.tube_x - spec.ro - 1.0, spec.tube_x + spec.ro + 1.0,
            cy - spec.ro - 2.0, cy + spec.ro + 2.0,
            z0, spec.clip_width,
        )
        ring = disk.intersect(slab)
        if ring is None or len(ring.solids()) < 1:
            reject("a clip ring missed the print slab", param="clip_width")
        solid += ring
    solid -= _tower_mark(spec, index)
    for k in found:
        solid -= Pos(0, 0, z0 + spec.bore_z) * _bore_void(spec, clip_y(spec, k))
    lug_x0, lug_x1, lug_y0 = _top_lug_bounds(spec)
    solid += _box(lug_x0, lug_x1, lug_y0, y1, z0, spec.clip_width)
    solid -= _arc_band(
        spec.slot_r_inner, spec.slot_r_outer,
        y1 - spec.slot_depth, y1 + 0.4,
    )
    if len(solid.solids()) != 1:
        reject("the tower did not come back as a single solid", param="clip_width")
    low = solid.bounding_box().min.Z
    if low < -0.05:
        reject("the tower hangs below the bed", param="clip_width")
    return solid


def build_base(spec, holes):
    """Narrow bolt ring. Each tower has an inward lobe with a through-slot.

    The band is the M3 hole plus 1.6 mm of wall. The lobe is 4 mm tall so
    the tab can be 4 mm long without sticking through the deck.
    """
    width = holes.bolt_d + 2.0 * BAND_WALL
    if abs((spec.base_r_out - spec.base_r_in) - width) > 0.05:
        reject("the base ring does not match the deck bolt holes", param="joint_clear")
    ring = extrude(Circle(spec.base_r_out) - Circle(spec.base_r_in), BASE_T)
    body = ring
    c = spec.joint_clear / 2.0
    y0 = -FLANGE_T
    y1 = -FLANGE_T + TAB_LEN
    # Stop the lobe at the inner edge of the band so the M3 hole keeps its wall.
    lobe_x1 = spec.base_r_in - R_PIN + 0.25
    lobe_z0 = -LOBE_WALL
    lobe_z1 = spec.clip_width + LOBE_WALL
    for index in range(4):
        ang = tower_angle(index)
        lobe = _box(spec.spine_in - LOBE_WALL, lobe_x1, y0, y1, lobe_z0, lobe_z1)
        slot = _box(
            spec.spine_in - c, spec.spine_out + c,
            y0 - 0.4, y1 + 0.4,
            -c, spec.clip_width + c,
        )
        body += place_tower(lobe - slot, ang)
    half = holes.bolt / 2.0
    span = TAB_LEN + 2.0
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            body -= Pos(sx * half, sy * half, span / 2.0) * Cylinder(holes.bolt_d / 2.0, span)
    if len(body.solids()) != 1:
        reject("the base ring did not come back as a single solid", param="coil_diameter")
    return body


def _box(x0, x1, y0, y1, z0, z1):
    return Pos((x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0) * Box(x1 - x0, y1 - y0, z1 - z0)


def _arc_band(r_in, r_out, y0, y1):
    """Annulus along local Y, centred on the lamp axis.

    In the print frame the axis is at x = -R_PIN, z = 0. The band is the
    curved slot the plate rim occupies.
    """
    if r_out <= r_in:
        reject("a curved slot has no radial width", param="joint_clear")
    height = y1 - y0
    mid = (y0 + y1) / 2.0
    outer = Pos(-R_PIN, mid, 0) * Rot(90, 0, 0) * Cylinder(r_out, height)
    inner = Pos(-R_PIN, mid, 0) * Rot(90, 0, 0) * Cylinder(r_in, height + 0.4)
    band = outer - inner
    if len(band.solids()) < 1:
        reject("a curved slot did not cut as a solid", param="joint_clear")
    return band


def _guide(spec, side, r0, r1, y0, y1):
    """Tab just outside one tangential face of the bar."""
    c = spec.joint_clear / 2.0
    if side < 0:
        z_a = -c - GUIDE_T
        z_b = -c
    else:
        z_a = spec.clip_width + c
        z_b = z_a + GUIDE_T
    x0, x1 = _span_x(r0, r1, z_a, z_b)
    if x1 - x0 < 1.0:
        reject("a guide has no radial length", param="joint_clear")
    return _box(x0, x1, y0, y1, z_a, z_b)


def build_cap(spec):
    """Plain annulus. Prints flat, and drops into the top slots as printed."""
    return extrude(Circle(spec.disc_r) - Circle(spec.disc_hole_r), spec.disc_t)


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
    """A short spine, one outward clip, and the foot tab.

    Print it on its side. Try the tab in a base slot before the towers.
    """
    z0 = spec.bed_z
    y_clip = 18.0
    y0 = -FLANGE_T
    solid = _box(spec.spine_in, spec.spine_out, y0, 32.0, z0, spec.clip_width)
    disk = Pos(0, 0, z0 + spec.bore_z) * _clip_disk(spec, y_clip)
    slab = _box(
        spec.tube_x - spec.ro - 1.0, spec.tube_x + spec.ro + 1.0,
        y_clip - spec.ro - 2.0, y_clip + spec.ro + 2.0,
        z0, spec.clip_width,
    )
    ring = disk.intersect(slab)
    if ring is None or len(ring.solids()) < 1:
        reject("the coupon clip missed the print slab", param="clip_width")
    solid += ring
    solid -= _tower_mark(spec, 1)
    solid -= Pos(0, 0, z0 + spec.bore_z) * _bore_void(spec, y_clip)
    if len(solid.solids()) != 1:
        reject("the fit coupon did not come back as a single solid", param="joint_clear")
    low = solid.bounding_box().min.Z
    if low < -0.05:
        reject("the coupon hangs below the bed", param="clip_width")
    return solid


def counts(spec):
    """Hooks on each tower, in number order 1..4."""
    order = (1, 2, 3, 0)
    return tuple(len(stations_for(spec, index)) for index in order)
