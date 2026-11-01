# MatrixLamp_R4_Wifi

Uno R4 WiFi sketch for Tom's BTF-LIGHTING WS2812B 16×16 panel (256 LEDs). The page is the spiral lamp's physical control panel. Switches at rest play the matrix fire; the other positions play the spiral modes (jumbo, classic, rainbow, the four waves, secret, lava, music). The board's 12×8 red matrix scrolls the WiFi IP. There is no animation upload and no OTA.

The project README, section 5, is the full note (page, `GET /api/state`, `GET /api/set`). This file is the wiring and the upload.

## Wiring

Data is **D6**, through a series resistor (330 Ω; 220–470 Ω is fine). Colour order is `NEO_GRB` at 800 kHz (`NEO_GRB + NEO_KHZ800`). Adafruit NeoPixel **1.12 or newer**.

The panel runs from its own 5 V supply. Share ground with the Arduino. Put a 1000 µF capacitor (6.3 V or higher) across the panel 5 V and GND, at the panel. Striped leg to GND.

| From | To |
|---|---|
| Panel DIN | 330 Ω → Arduino **D6** |
| Panel GND | Arduino **GND** and the supply **−** |
| Panel 5 V | Supply **+** |
| 1000 µF | Across supply **+ / −**, at the panel |

Do not power the panel from the Arduino 5 V pin while USB is plugged in.

Brightness starts at **20** of 255 (`BRIGHTNESS_DEFAULT`). The page cannot raise it past the cap, which is the lower of `BRIGHTNESS_CAP` (40) and the current budget `POWER_BUDGET_MA` (2500 mA). Full white at 40 is about 2.4 A. 20 is plenty on this panel.

## WiFi

Copy `secrets.example.h` to `secrets.h` in this folder. `secrets.h` is gitignored. Do not commit it. An empty `WIFI_SSID`, or no `secrets.h`, skips WiFi. The panel still runs.

The lamp is online only when `WiFi.status()` is `WL_CONNECTED` and `WiFi.localIP()` is not `0.0.0.0`. mDNS: `matrixlamp.local` and `matrix-lamp.local`. DHCP name: `matrixlamp`.

## If the picture is backwards

Edit only these, at the top of `MatrixLamp_R4_Wifi.ino`:

| Setting | Meaning |
|---|---|
| `PANEL_SERPENTINE` | `true` when rows alternate direction (these panels usually do). |
| `PANEL_SERPENTINE_ODD` | `true`: row 0 left to right, row 1 right to left. |
| `PANEL_FLIP_X`, `PANEL_FLIP_Y` | Mirror. |
| `PANEL_ROTATE` | Clockwise quarter-turns, 0–3. |

## Upload

Install [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/), then from the repo root:

```text
tools/upload_matrixlamp.sh
tools/upload_matrixlamp.sh /dev/cu.usbmodem1101 --monitor
tools\upload_matrixlamp.bat COM5
```

Windows can double-click `tools/upload_matrixlamp.bat`. The window waits at the end. The port is the first argument; leave it off and the script looks for one Uno R4 WiFi. `--monitor` opens serial at 115200 after upload. The script installs the `arduino:renesas_uno` core and Adafruit NeoPixel if they are missing, and warns if `secrets.h` is missing.

```text
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi sketches/MatrixLamp_R4_Wifi
```

Core 1.6.0, NeoPixel 1.15.5, no `secrets.h`: 95892 B flash (36% of 262144), 11368 B globals (34% of 32768), 21400 B left for locals. NeoPixel mallocs another 768 B for the 256 LEDs.
