# Spiral Lamp: plywood base + printed spiral holder

A hand-cut 3 mm (1/8") plywood replacement for the printed Spiral Lamp enclosure (Alexjulness plans). It uses the original printed switch panel and a printed cage that holds a JARBIH 13 mm silicone neon tube.

- **Strip:** BTF-LIGHTING FCOB WS2812B-compatible, 5 V, 160 LEDs/m, 5 mm wide. It's cut to **900 mm = 144 LEDs**, so `LED_COUNT` stays 144.
- **Tube:** JARBIH "for 5MM", 13 mm OD, 6.5 mm slot (B0CKZ2WWJF).
- **Box:** 250 × 210 × 120 mm with butt joints and 1/4" glue strips. The panel sits on the slope. The bottom is removable and doubles as the electronics tray. The back has a USB-C power inlet and a USB-B window for the Arduino.

| Path | What |
|---|---|
| `PLAN.md` / `PLAN.pdf` | Full build plan: specs, cut list, printing, build order, wiring |
| `cutlist.csv` | Cut list for 3 mm and 6 mm ply (mm + inches) |
| `templates/ply3mm/` | Full-size SVG templates. `templates_letter_tiled_ply3mm.pdf` is the one to print on Letter at 100%. It's marked for hand cutting: drill sizes and blade-entry holes. |
| `templates/ply6mm/` | Same for 6 mm (1/4") ply |
| `stl/` | Cage (13 mm tube), end cap, clip fit test, top backer plate, 6 bottom blocks. Watertight, fits a CR-10, no supports. |
| `render/` | Assembled, exploded, cut-away and tray renders |
| `src/` | Python generators. `xvfb-run -a python3 src/build_all.py` rebuilds STLs, templates and renders. `python3 src/make_pdf.py` rebuilds `PLAN.pdf`. |

**Rebuilding:**
- Install python3, trimesh, manifold3d, shapely, numpy, reportlab, cairosvg, pyvista, Pillow and markdown. You also need Chrome for the PDF.
- The renders and the panel mockup need the purchased plan STLs. They're not included here, so set `SPIRAL_PLANS_DIR` to their folder.
- Key settings:
  - Tube size: `TUBES` in `src/build_all.py`.
  - Coil and strip: `src/spiral.py` (`default_params`).
  - Box and ply: `src/box.py` (`params`).
