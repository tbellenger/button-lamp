"""Four-tower spiral. The openings are cut into the tower bar.

The bar is a slim spine. Each clip is a C-shaped ring on the outer edge,
opening away from the lamp. Between clips the spine widens out to the
tube centreline, following the inner arc of each bore, so the rings merge
into the spine. The foot is a rectangular tab that drops through a slot in
a rounded lobe on the base ring. The top of the spine carries a straight
notch the plain disc threads through.

The base ring is only as wide as the four M3 holes. A lobe reaches inward
under each tower, on the same ray as its bolt. The middle is open, so the
light cable still comes up through the deck hole. The coil and the clip
centres are unchanged.

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
# Top disc. 1.2 mm is six layers at 0.2 mm. 2.4 mm of radial width is six
# perimeters on a 0.4 mm nozzle. The radius is chosen in specs() so the
# ring sits on the spine, not out past it.
DISC_T = 1.2
DISC_RADIAL = 2.4
# Arc notch: 0.15 mm each side of the ring. 0.1 mm under a flush top.
NOTCH_SIDE = 0.15
NOTCH_DROP = 0.1
# Ring size fixed when it moved onto the spine (inner arc 1.6 mm clear of
# the old 8 mm spine's inner face). The 5 mm spine's shorter chord still
# clears that wall, so the ring is not resized.
_RING_INNER = math.hypot(34.1, 4.0)
DISC_HOLE_R = _RING_INNER + NOTCH_SIDE
DISC_R = DISC_HOLE_R + DISC_RADIAL
# Spine. 4 mm radial is the narrow beam that still leaves a solid root on the
# clip wall without entering the bore. 8 mm matches the clip thickness, so
# the tower is one slab on the bed.
SPINE_RADIAL = 4.0
# Tangential thickness. The C-clips stay 8 mm and share the bed face,
# so they stand 3 mm proud of this spine on the side away from the bed.
SPINE_T = 5.0
ROOT_GAP = 0.3
# Tab drops through the plate. 2.4 mm ends flush with the underside
# when the shoulder sits on the plate top.
TAB_LEN = 2.4
LOBE_WALL = 1.6
# Band around each M3 hole, from the hole edge to the ring edge.
BAND_WALL = 1.6
# Solid spine between the outside of the top clip and the floor of the disc notch.
SLOT_ABOVE_CLIP = 1.4
# Floor of plastic under the open top notch, so the disc has something to sit on.
NOTCH_FLOOR = 1.6
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
    """World Z of the underside of the top disc. The top is flush with the towers."""
    return FLANGE_T + spec.cap_y - spec.disc_t


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

    # The 8 mm clip shares the bed face with the 5 mm spine, so its centre
    # sits 1.5 mm off the spine midplane, on the side away from the bed.
    # The placement puts the spine midplane on the bolt ray. Shift local x
    # until that offset bore is still on the 43.5 mm helix.
    clip_center_z = clip_width / 2.0
    spine_mid_z = SPINE_T / 2.0
    bore_offset = clip_center_z - spine_mid_z
    if abs(bore_offset) >= radius - 1.0:
        reject("clip_width is too long for the coil radius", param="clip_width")
    radial_len = math.sqrt(radius * radius - bore_offset * bore_offset)
    tube_x = radial_len - R_PIN
    x_in = tube_x - ro
    x_out = tube_x + ro
    if x_out - x_in < 8.0:
        reject("the bar is too narrow to plug into a slot", param="clip_wall")
    bore_z = clip_center_z
    bore_phase = -math.atan2(bore_offset, radial_len)
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

    # 0.1 mm under the ring, so the notch is 1.3 mm deep and the ring top
    # is flush with the tower. 1.6 mm of floor stays under the notch.
    slot_depth = DISC_T + NOTCH_DROP
    cap_y = FOOT_CLEAR + k_last * (pitch / 4.0) + ro + SLOT_ABOVE_CLIP + slot_depth
    if slot_depth >= 4.0:
        reject("the disc notch would cut more than 4 mm off the tower top", param="joint_clear")
    base_r_in, base_r_out = _base_ring_radii()
    spine_out = tube_x - rb - ROOT_GAP
    spine_in = spine_out - SPINE_RADIAL
    if spine_out <= spine_in:
        reject("the spine has no radial thickness", param="clip_wall")
    # Same ring as before. Across the 5 mm spine the arc is a shorter
    # chord, so the inboard wall is at least the 1.6 mm it had at 8 mm.
    notch_r_in = DISC_HOLE_R - NOTCH_SIDE
    notch_r_out = DISC_R + NOTCH_SIDE
    disc_hole_r = DISC_HOLE_R
    disc_r = DISC_R
    x_inner = math.sqrt(notch_r_in * notch_r_in - spine_mid_z * spine_mid_z) - R_PIN
    if x_inner < spine_in + CLIP_WALL - 1e-6:
        reject("the top notch leaves under 1.6 mm inside the 5 mm spine", param="clip_wall")
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
        bore_offset=bore_offset,
        spine_t=SPINE_T,
        spine_mid_z=spine_mid_z,
        clip_center_z=clip_center_z,
        bore_phase=bore_phase,
        helix_angle_deg=math.degrees(helix_angle),
        tube_x=tube_x,
        x_in=x_in,
        x_out=x_out,
        spine_in=spine_in,
        spine_out=spine_out,
        bar_w=bar_w,
        slot_radial=bar_w + joint_clear,
        slot_tangent=SPINE_T + joint_clear,
        cap_y=cap_y,
        disc_r=disc_r,
        disc_hole_r=disc_hole_r,
        disc_t=DISC_T,
        slot_depth=slot_depth,
        slot_r_outer=notch_r_out,
        slot_r_inner=notch_r_in,
        notch_r_in=notch_r_in,
        notch_r_out=notch_r_out,
        head_x=(notch_r_out - R_PIN) + CLIP_WALL,
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


def axis_offset(spec):
    """No extra rotation. The placement origin centres the spine on the bolt ray.

    Rotating the tower by atan(4 mm / slot radius) put the slot centre on the
    bolt but left the 8 mm faces 6.65° off square to that ray. The origin
    shift in place_tower does the centring instead, so local X lies along
    the bolt ray and local Z is perpendicular to it.
    """
    return 0.0 * spec.clip_width


def theta0(spec):
    """Tower angle of the lowest hook. The whole set is shifted by axis_offset."""
    return math.pi / 2.0 + TOWER_PHASE + axis_offset(spec)


def tower_angle(index, spec=None):
    """World angle of the placement radial for tower index.

    The tab itself lands on the bolt diagonal, which is index * 90° + 45°.
    0° is +X, 90° is +Y (back).
    """
    if spec is None:
        spec = default_specs()
    return index * math.pi / 2.0 + TOWER_PHASE + axis_offset(spec)


def bolt_angle(index):
    """World angle of deck bolt index. The square is fixed; the towers move to it."""
    return index * math.pi / 2.0 + TOWER_PHASE


def station_theta(spec, k):
    """Tower angle of station k. The coil starts at theta0 and climbs counterclockwise."""
    return theta0(spec) + k * math.pi / 2.0


def stations_for(spec, index):
    """Hook stations whose tower angle lands on this tower."""
    target = tower_angle(index, spec)
    found = []
    for k in spec.stations:
        ang = station_theta(spec, k) % (2.0 * math.pi)
        delta = abs(ang - target)
        if min(delta, 2.0 * math.pi - delta) < 1e-6:
            found.append(k)
    return tuple(found)


def shoulder_y():
    """Local Y of the shoulder. In the assembly this is the top of the plate."""
    return BASE_T - FLANGE_T


def tab_bottom_y():
    """Local Y of the tab end. In the assembly this is the underside of the plate."""
    return shoulder_y() - TAB_LEN


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
    z = FLANGE_T + clip_y(spec, 0) + spec.climb * (theta - theta0(spec))
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
    theta = station_theta(spec, 0)
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
    return Pos(x, 6.0, spec.spine_t - MARK_DEPTH) * extrude(face, MARK_DEPTH + 0.2)


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


def _clearance_solid(spec, cy):
    """Bore grown by the 0.2 mm fit clearance, without the mouth.

    Subtracting this from the spine keeps the widened edge off the bore.
    The clip wall itself is not trimmed: the caller only cuts the spine.
    """
    radius = spec.radius
    cx = spec.tube_x
    reach = spec.clip_width / 2.0 + 2.0
    alpha = math.asin(min(0.5, reach / radius))
    sections = []
    grown = spec.rb + BORE_CLEAR
    for i in range(5):
        dth = -alpha + 2.0 * alpha * i / 4.0
        x = cx - radius * (1.0 - math.cos(dth))
        y = cy + spec.climb * dth
        z = -radius * math.sin(dth)
        sections.append(Pos(x, y, z) * Circle(grown))
    solid = loft(sections, ruled=True)
    if len(solid.solids()) != 1:
        reject("a bore clearance did not loft as a single solid", param="clip_width")
    return solid


def _extrude_xy(pts, z0, z1):
    """Extrude a polygon in the print plane through the bed thickness.

    The polygon is forced counter-clockwise. A clockwise face extrudes the
    wrong way and the union comes back as a compound.
    """
    area = 0.0
    for i, (x1, y1) in enumerate(pts):
        x2, y2 = pts[(i + 1) % len(pts)]
        area += x1 * y2 - x2 * y1
    if area < 0.0:
        pts = list(reversed(pts))
    face = Polygon(pts)
    return Pos(0, 0, z0) * extrude(face, z1 - z0)


def _spine_flesh(spec, stations):
    """Widen the spine out to the tube centreline between the end clips.

    The outer face follows each bore's inner arc and runs straight across
    the gap. It meets the 4 mm spine on a tangent at the first and last
    clip, which is the taper back to the spine at the two ends. The foot
    tab, below y = 0, stays the 4 mm section.
    """
    ys = [clip_y(spec, k) for k in stations]
    y0 = min(ys)
    y1 = max(ys)
    if y1 - y0 < 1.0:
        # One clip. Show the arc for a short way each side of it, still
        # clear of the bore, so the coupon has the same root.
        reach = spec.rb + BORE_CLEAR
        y0 = max(0.0, ys[0] - reach)
        y1 = ys[0] + reach
    block = _box(spec.spine_out - 0.05, spec.tube_x, y0, y1, 0.0, spec.spine_t)
    for k in stations:
        block -= Pos(0, 0, spec.bore_z) * _clearance_solid(spec, clip_y(spec, k))
    if block is None or len(block.solids()) < 1:
        reject("the widened spine disappeared", param="clip_width")
    return block


def _arc_notch(spec, y0, y1):
    """Arc slot the disc occupies. Axis is the lamp axis in the print frame.

    The placement origin is shifted, so that axis is at local x = -R_PIN
    and local z = bore_z, not at z = 0. The slot is the ring plus 0.15 mm
    each side, and it is open through the top of the tower.
    """
    if spec.notch_r_out <= spec.notch_r_in:
        reject("a curved slot has no radial width", param="joint_clear")
    height = y1 - y0
    mid = (y0 + y1) / 2.0
    centre = Pos(-R_PIN, mid, spec.spine_mid_z) * Rot(90, 0, 0)
    outer = centre * Cylinder(spec.notch_r_out, height)
    inner = Pos(-R_PIN, mid, spec.spine_mid_z) * Rot(90, 0, 0) * Cylinder(spec.notch_r_in, height + 0.4)
    band = outer - inner
    if len(band.solids()) < 1:
        reject("the top notch did not cut as a solid", param="joint_clear")
    return band


def _top_head(spec, stations):
    """Spine top, solid up to the highest clip, with an arc notch for the disc.

    The region under the notch is filled through, so there is no window
    between the head, the spine and the top clip. The fill stops 0.2 mm
    short of that clip's bore, and the mouth is left open.
    """
    x_out = spec.head_x
    y_top = spec.cap_y
    y_last = max(clip_y(spec, k) for k in stations)
    if y_top - spec.slot_depth - NOTCH_FLOOR < 0.0:
        reject("the top notch falls off the tower", param="tube_length")
    head = _box(spec.spine_in, x_out, y_last - spec.ro, y_top, 0.0, spec.spine_t)
    head -= _arc_notch(spec, y_top - spec.slot_depth, y_top + 0.6)
    for k in stations:
        cy = clip_y(spec, k)
        if cy + spec.ro + 1.0 < y_last - spec.ro:
            continue
        head -= Pos(0, 0, spec.bore_z) * _bore_void(spec, cy)
        head -= Pos(0, 0, spec.bore_z) * _clearance_solid(spec, cy)
    if head is None or len(head.solids()) < 1:
        reject("the top notch ate the head", param="joint_clear")
    return head


def build_tower(spec, index):
    """One tower, in its print frame. +X out, +Y up the tower, +Z print-up.

    A 4 mm spine carries a C-ring at each clip and widens between the clips.
    The foot is the tab. The top head holds the straight disc notch. Nothing
    hangs below z = 0.
    """
    if index not in TOWER_MARK:
        reject("tower index must be 0, 1, 2 or 3", param="extra_stations")
    found = stations_for(spec, index)
    if not found:
        reject("this tower has no opening on the tube", param="tube_length")
    z0 = spec.bed_z
    y0 = tab_bottom_y()
    y1 = spec.cap_y
    solid = _box(spec.spine_in, spec.spine_out, y0, y1, z0, spec.spine_t)
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
    solid += _spine_flesh(spec, found)
    solid += _top_head(spec, found)
    # The spine itself runs through the notch. Cut it from the union, or the
    # ring stops on the spine instead of threading it.
    solid -= _arc_notch(spec, spec.cap_y - spec.slot_depth, spec.cap_y + 0.6)
    if len(solid.solids()) != 1:
        reject("the tower did not come back as a single solid", param="clip_width")
    low = solid.bounding_box().min.Z
    if low < -0.05:
        reject("the tower hangs below the bed", param="clip_width")
    return solid


def _lobe_solid(spec, bolt_ang):
    """Inward lobe centred on a bolt ray, blending into the band.

    A stadium wide enough for the slot plus 1.6 mm of wall, with a round
    inner end. Fillet arcs carry the straight sides into the band's inner
    circle. The slot is cut afterwards, on the same ray.
    """
    x_mid = (spec.spine_in + spec.spine_out) / 2.0
    # The spine midplane is the bolt ray, so the slot centre radius is plain.
    r_slot = R_PIN + x_mid
    half_t = (spec.spine_t + spec.joint_clear) / 2.0
    half_r = (SPINE_RADIAL + spec.joint_clear) / 2.0
    rad = math.hypot(half_r, half_t) + LOBE_WALL
    fillet_r = 5.0
    band = spec.base_r_in
    # Fillet centre sits outside the straight side, so the joint flares
    # wider than the lobe body as it meets the band.
    vc = rad + fillet_r
    if band - fillet_r <= vc:
        reject("the lobe fillet does not meet the band", param="joint_clear")
    uc = math.sqrt((band - fillet_r) ** 2 - vc * vc)
    if uc <= r_slot + rad:
        reject("the lobe fillet runs into the slot", param="joint_clear")
    radial = (math.cos(bolt_ang), math.sin(bolt_ang))
    # Clockwise tangent, matching the tower's print +Z once the tower is stood up.
    tangent = (math.sin(bolt_ang), -math.cos(bolt_ang))

    def world(u, v):
        return (
            u * radial[0] + v * tangent[0],
            u * radial[1] + v * tangent[1],
        )

    pts = []
    # Inner semicircle, from the clockwise side around the tip to the other side.
    n_arc = 24
    for i in range(n_arc + 1):
        phi = -math.pi / 2.0 + math.pi * i / n_arc
        pts.append(world(r_slot + rad * math.cos(phi + math.pi), rad * math.sin(phi)))
    # phi from -90° to +90° with cos(phi+180) = -cos(phi): u from r_slot down to r_slot-rad and back.
    # At i=0, phi=-90, u=r_slot, v=-rad. At i=mid, phi=0, u=r_slot-rad, v=0. At end, u=r_slot, v=+rad.
    # Straight side, then a flare that grows wider than the body.
    pts.append(world(uc, rad))
    band_phi = math.atan2(vc, uc)
    n_fil = 16
    for i in range(1, n_fil + 1):
        phi = -math.pi / 2.0 + (band_phi + math.pi / 2.0) * i / n_fil
        pts.append(world(uc + fillet_r * math.cos(phi), vc + fillet_r * math.sin(phi)))
    overlap = band + 2.5
    pts.append(world(overlap * math.cos(band_phi), overlap * math.sin(band_phi)))
    pts.append(world(overlap * math.cos(-band_phi), overlap * math.sin(-band_phi)))
    for i in range(1, n_fil + 1):
        phi = -band_phi + (math.pi / 2.0 + band_phi) * i / n_fil
        pts.append(world(uc + fillet_r * math.cos(phi), -vc + fillet_r * math.sin(phi)))
    return _extrude_xy(pts, 0.0, BASE_T)


def build_base(spec, holes):
    """Narrow bolt ring. Each tower has a rounded inward lobe with a through-slot.

    The band and the lobes are one plate, 2.4 mm thick, with a flat top.
    The flare is only in the plan view. The slot and the bolt share the
    lobe's centre line. The bolt is the one further out.
    """
    width = holes.bolt_d + 2.0 * BAND_WALL
    if abs((spec.base_r_out - spec.base_r_in) - width) > 0.05:
        reject("the base ring does not match the deck bolt holes", param="joint_clear")
    ring = extrude(Circle(spec.base_r_out) - Circle(spec.base_r_in), BASE_T)
    body = ring
    c = spec.joint_clear / 2.0
    y0 = tab_bottom_y()
    y1 = shoulder_y()
    for index in range(4):
        body += _lobe_solid(spec, bolt_angle(index))
        slot = _box(
            spec.spine_in - c, spec.spine_out + c,
            y0 - 0.4, y1 + 0.4,
            -c, spec.spine_t + c,
        )
        body -= place_tower(slot, tower_angle(index, spec))
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


def place_tower(solid, angle, centre_z=None):
    """Stand a print-frame tower on its slot. Local +X is radial, +Y is up.

    The print frame still has the bed at local z = 0. The origin is shifted
    along the tangent so the spine midplane, local z = centre_z, lies on the
    ray. Local X then runs along that ray and the 5 mm side is square to it.
    """
    if centre_z is None:
        centre_z = SPINE_T / 2.0
    radial = Vector(math.cos(angle), math.sin(angle), 0)
    z_dir = Vector(math.sin(angle), -math.cos(angle), 0)
    origin = (
        Vector(R_PIN * math.cos(angle), R_PIN * math.sin(angle), FLANGE_T)
        - centre_z * z_dir
    )
    return Plane(origin=origin, x_dir=radial, z_dir=z_dir) * solid


def bore_world(spec, index, k):
    """World point of this hook's bore centre, from the print frame."""
    angle = tower_angle(index, spec)
    radial = Vector(math.cos(angle), math.sin(angle), 0)
    z_dir = Vector(math.sin(angle), -math.cos(angle), 0)
    up = Vector(0, 0, 1)
    origin = (
        Vector(R_PIN * math.cos(angle), R_PIN * math.sin(angle), FLANGE_T)
        - spec.spine_mid_z * z_dir
    )
    local = Vector(spec.tube_x, clip_y(spec, k), spec.clip_center_z)
    return origin + local.X * radial + local.Y * up + local.Z * z_dir


def build_coupon(spec):
    """A short spine, one outward clip, and the foot tab.

    Print it on its side. Try the tab in a base slot before the towers.
    """
    z0 = spec.bed_z
    y_clip = 18.0
    y0 = tab_bottom_y()
    solid = _box(spec.spine_in, spec.spine_out, y0, 32.0, z0, spec.spine_t)
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
    # Same widened root as a tower, around this one clip. The tab is the
    # 2.4 mm of spine below the plate line.
    reach = spec.rb + BORE_CLEAR
    clearance = Pos(0, 0, z0 + spec.bore_z) * _clearance_solid(spec, y_clip)
    block = _box(
        spec.spine_out - 0.05, spec.tube_x,
        max(0.0, y_clip - reach), y_clip + reach,
        z0, spec.spine_t,
    )
    block -= clearance
    solid += block
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
