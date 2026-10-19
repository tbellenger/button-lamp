import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle, Circle, FancyArrowPatch

RED="#d62728"; BLK="#222222"; GRN="#2ca02c"; BLU="#1f77b4"; ORG="#e8710a"; PUR="#8e44ad"; YEL="#e0b000"
fig, ax = plt.subplots(figsize=(17, 10.5), dpi=110)
ax.set_xlim(0, 162); ax.set_ylim(-2, 100); ax.axis("off")
ax.set_title("Spiral Lamp — BENCH TEST wiring  (Arduino Uno/Nano + WS2812B strip + 2 buttons)",
             fontsize=18, fontweight="bold", pad=10)

def wire(pts, color, lw=3.2, z=2):
    xs, ys = zip(*pts); ax.plot(xs, ys, color=color, lw=lw, solid_capstyle="round", zorder=z)
def dot(x, y, c=BLK): ax.add_patch(Circle((x, y), 0.85, color=c, zorder=5))
def T(x, y, t, **kw):
    kw.setdefault("fontsize", 11); ax.text(x, y, t, zorder=7, **kw)

Y5, YG = 92.5, 5.5
# rails
ax.add_patch(Rectangle((40, Y5-1.5), 120, 3, fc="#ffe3e3", ec=RED, lw=1.5, zorder=0))
T(41, Y5+2.3, "+5 V rail (breadboard)", color=RED, fontsize=11.5, fontweight="bold")
ax.add_patch(Rectangle((30, YG-1.5), 130, 3, fc="#e6e6e6", ec=BLK, lw=1.5, zorder=0))
T(31, YG-5.5, "GND rail — every ground meets here: power supply, LED strip, Arduino, both buttons",
  fontsize=11.5, fontweight="bold")

# Arduino
ax.add_patch(FancyBboxPatch((4, 24), 28, 54, boxstyle="round,pad=0.6", fc="#0b6e8f", ec=BLK, lw=2, zorder=1))
T(18, 75, "ARDUINO\nUno / Nano", ha="center", va="top", color="white", fontsize=15, fontweight="bold")
T(16, 50, "USB to computer\nSerial Monitor\n115200 baud", ha="center", color="white", fontsize=9.5)
pins = {"5V": 68, "D9": 60, "D8": 44, "D4": 36, "GND": 30}
for n, y in pins.items():
    ax.add_patch(Rectangle((30.4, y-1.3), 3.4, 2.6, fc="#f2f2f2", ec=BLK, zorder=3))
    T(29.4, y, n, ha="right", va="center", color="white", fontsize=12, fontweight="bold")
PX = 33.8

# Arduino 5V: not connected when external supply is used
wire([(PX, 68), (40, 68)], RED, lw=2.2)
ax.plot([41], [68], marker="x", color=RED, ms=13, mew=3, zorder=6)
T(43, 68, "Arduino 5V: NOT connected to the +5V rail\nwhen you use the external supply\n(USB-only option: see notes)",
  va="center", fontsize=9.5, color=RED)

# Arduino GND
wire([(PX, 30), (37, 30), (37, YG)], BLK); dot(37, YG)

# Data line with resistor
wire([(PX, 60), (50, 60)], GRN)
ax.add_patch(Rectangle((50, 58.3), 13, 3.4, fc="#d9b38c", ec=BLK, lw=1.5, zorder=3))
for k, c in enumerate(["#ff7f0e", "#ff7f0e", "#8c564b"]):
    ax.add_patch(Rectangle((52.5+k*2.7, 58.3), 1.2, 3.4, fc=c, ec="none", zorder=4))
T(56.5, 63.2, "330–470 Ω resistor", ha="center", fontsize=10.5, fontweight="bold", color=GRN)
wire([(63, 60), (96, 60)], GRN)
T(70, 56.3, "data (keep short)", color=GRN, fontsize=10)

# LED strip
sx0, sx1, sy = 96, 134, 60
ax.add_patch(Rectangle((sx0, sy-5), sx1-sx0, 10, fc="#f4f4f4", ec=BLK, lw=2, zorder=1))
for i in range(6):
    x = sx0 + 11 + i*4.2
    ax.add_patch(Rectangle((x-1.5, sy-1.5), 3, 3, fc="white", ec="#888", zorder=2))
    ax.add_patch(Circle((x, sy), 0.85, color=[RED, GRN, BLU][i % 3], zorder=3))
T(sx0, sy+6.3, "WS2812B strip (60 LEDs, 5 V)", fontsize=12, fontweight="bold")
for n, y in {"+5V": sy+3, "DIN": sy, "GND": sy-3}.items():
    ax.add_patch(Rectangle((sx0-0.6, y-0.9), 2.6, 1.8, fc=YEL, ec=BLK, zorder=3))
    T(sx0+2.4, y, n, va="center", fontsize=9.5, fontweight="bold")
ax.add_patch(FancyArrowPatch((sx0+6, sy-8), (sx1-2, sy-8), arrowstyle="-|>", mutation_scale=22, lw=2.6, color=ORG))
T(sx0, sy-12.3, "Printed arrows must point AWAY from the\nArduino (wire the DIN end, not DOUT).",
  color=ORG, fontsize=10, fontweight="bold", va="center")
# strip power
wire([(sx0, sy+3), (92, sy+3), (92, Y5)], RED); dot(92, Y5, RED)
wire([(sx0, sy-3), (89, sy-3), (89, YG)], BLK); dot(89, YG)

# capacitor between rails, right next to the strip
cx = 139
wire([(cx, Y5), (cx, 79)], RED); dot(cx, Y5, RED)
ax.add_patch(Rectangle((cx-2.6, 69), 5.2, 10, fc="#3b6fb6", ec=BLK, lw=1.5, zorder=3))
ax.add_patch(Rectangle((cx+1.2, 69), 1.4, 10, fc="#dddddd", ec="none", zorder=4))
T(cx-1.2, 77, "+", color="white", fontsize=12, fontweight="bold", ha="center", va="center")
T(cx+1.9, 71, "–", color=BLK, fontsize=12, fontweight="bold", ha="center", va="center")
wire([(cx, 69), (cx, YG)], BLK); dot(cx, YG)
T(cx-3.5, 80.5, "1000 µF, 6.3 V+ electrolytic\nlong leg (+) to +5V,\nstriped leg (–) to GND", fontsize=9.5, ha="right", va="top")

# power supply
ax.add_patch(FancyBboxPatch((143, 36), 15, 24, boxstyle="round,pad=0.5", fc="#fff3c4", ec=BLK, lw=2))
T(150.5, 57.5, "5 V\nPOWER\nSUPPLY", ha="center", va="top", fontsize=11.5, fontweight="bold")
T(150.5, 45.5, "5 V 3 A\n160 LEDs\ncap 50 ~1.9 A", ha="center", va="center", fontsize=8.8)
T(146, 61.3, "+5V", color=RED, fontsize=10, fontweight="bold")
T(146, 32.2, "GND", fontsize=10, fontweight="bold")
wire([(150.5, 60.5), (150.5, Y5)], RED); dot(150.5, Y5, RED)
wire([(150.5, 35.5), (150.5, YG)], BLK); dot(150.5, YG)

# buttons (signal on top-right leg, GND on the DIAGONAL bottom-left leg)
def button(cx, cy, pin_y, title, sub, cap):
    ax.add_patch(Rectangle((cx-4, cy-4), 8, 8, fc="#cfcfcf", ec=BLK, lw=1.8, zorder=3))
    ax.add_patch(Circle((cx, cy), 2.5, fc=cap, ec=BLK, zorder=4))
    for dx, dy in [(-4, 4), (4, 4), (-4, -4), (4, -4)]:
        ax.add_patch(Rectangle((cx+dx-0.75, cy+dy-0.75), 1.5, 1.5, fc="#999", ec=BLK, zorder=5))
    wire([(PX, pin_y), (cx+4, pin_y), (cx+4, cy+4)], PUR)
    wire([(cx-4, cy-4), (cx-4, YG)], BLK); dot(cx-4, YG)
    T(cx-1.5, cy-6.3, title, fontsize=11, fontweight="bold", va="top")
    T(cx-1.5, cy-9.3, sub, fontsize=9.5, va="top")
button(50, 24, 36, "Button 1 → D4", "next pattern\n(white arcade button)", "white")
button(72, 24, 44, "Button 2 → D8", "next brightness\n(red arcade button)", RED)
T(42, 49.5, "4-pin tactile buttons: use two DIAGONALLY opposite legs.\nNo resistors needed (sketch uses INPUT_PULLUP).",
  fontsize=9.5, color=PUR, va="center")

# notes
notes = ("Checklist\n"
         "1. Supply GND, strip GND and Arduino GND all joined.\n"
         "2. Resistor in the data wire, near the strip's DIN pad.\n"
         "3. Capacitor across +5V/GND at the strip, polarity right.\n"
         "4. USB-only test: brightness 5 or 10 only\n"
         "    (160 LEDs, ~0.4 A at 10). Otherwise the strip\n"
         "    +5V stays on the 5 V 3 A supply.\n"
         "5. Change pins at the top of bench_test.ino.")
ax.text(91, 11, notes, fontsize=9.6, va="bottom", ha="left",
        bbox=dict(boxstyle="round,pad=0.6", fc="#f4f6ff", ec=BLU, lw=1.5), zorder=6)

for i, (c, t) in enumerate([(RED, "+5 V"), (BLK, "GND"), (GRN, "LED data"), (PUR, "button signal")]):
    ax.plot([5, 10], [18-i*3.3]*2, color=c, lw=4); T(11.5, 18-i*3.3, t, va="center", fontsize=10)
plt.savefig("/workspace/buttonbox/out/wiring_diagram.png", bbox_inches="tight", facecolor="white")
