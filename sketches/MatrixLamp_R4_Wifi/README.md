# MatrixLamp_R4_Wifi

Uno R4 WiFi sketch for Tom's BTF-LIGHTING WS2812B 16×16 panel (256 LEDs). The lamp is the panel and the Uno only: there is no knob, fader, or button. The phone page has a brightness slider, a speed slider, and preset buttons (fire, rainbow, meatballs, plasma, sun, spiral, fluid, heart, matrix, lightning, off, radar). Radar plays a live NEXRAD loop from the owner's radar host (default `http://api.drunkwallaby.com/radar.bin`, polled in the background 20 seconds after WiFi is up and then every 5 minutes). The page does not edit that address. Auto radar is off unless the radar preset is selected or the page toggle is turned on. A saved on value from older firmware does not start the poll. Matrix is green digital rain: a saturated green head (80, 255, 80), a short pure-green tail, and a field that fades toward black. Speed is how fast it falls. Lightning is a jagged bolt with a scrolling colour ramp; Speed is how fast that ramp moves. While Meatballs is showing, + Ball and − Ball change the count from 1 to 8. A new ball gets a random position and velocity. Off is a black panel except one very dim yellow LED at logical (0, 15), which sits at the bottom-left of the lamp. The colour is raw (6, 6, 0). NeoPixel brightness would round that to black, so each lit channel is raised just enough that show() keeps it at 1 or more. Speed does not move it. On Spiral, Speed shifts the colour and the arm keeps a fixed spin. The last preset, brightness, speed, and ball count are stored in the data-flash block after the WiFi credentials. The board's 12×8 red matrix scrolls the WiFi IP. After one USB flash, firmware updates go over WiFi.

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

Copy `secrets.example.h` to `secrets.h` in this folder. `secrets.h` is gitignored. Do not commit it. Put the network in `WIFI_SSID` / `WIFI_PASS`. A USB build stores those in data flash when they differ. A GitHub build has no `secrets.h` and does not overwrite them. With no stored network, the board opens WiFi `MatrixLamp-Setup` so a phone can enter them.

The lamp is online only when `WiFi.localIP()` is a real station address. `WL_CONNECTED` alone is not enough: `0.0.0.0`, a `169.254.x.x` link-local address, and anything on `192.168.4.x` (the ESP32 SoftAP subnet, including `192.168.4.1`) are not treated as joined. The setup AP `MatrixLamp-Setup` is the exception, and it does use `192.168.4.1`. `ota.update()` programs the RA4M1 through BOSSA and resets that chip only. It does not restart the ESP32-S3. Boot therefore calls `WiFi.end()` and then `AT+RESET` before any `WiFi.begin()`, which restarts the bridge. `SOFTRESETWIFI` alone leaves the default access point running. Serial marks the steps `boot`, `bridge ok`, `wifi joined <ip>`, and `server started`. If no real station address arrives within 22 seconds, the modem is reset and the join is tried again. Credentials are the first 1 KB of data flash at `0x40100000` (preferences are the next block, `0x40100400`). Preferences are read a byte at a time and written again just before `ota.update()`, so the lamp comes back on the saved preset. The setup page writes the credentials and does not report success unless the bytes read back. It then reboots even if the phone drops the socket. mDNS: `matrixlamp.local` and `matrix-lamp.local`. DHCP name: `matrixlamp`.

## If the picture is backwards

The rotation and the serpentine map live in `lamp_core.cpp` (`FX_ROTATE`, `FX_SERPENTINE`, `FX_SERPENTINE_ODD`, `FX_FLIP_X`, `FX_FLIP_Y`). The sketch and the browser preview both call that map.

| Setting | Meaning |
|---|---|
| `FX_SERPENTINE` | `true` when rows alternate direction (these panels usually do). |
| `FX_SERPENTINE_ODD` | `true`: row 0 left to right, row 1 right to left. |
| `FX_FLIP_X`, `FX_FLIP_Y` | Mirror. Both are `false`. |
| `FX_ROTATE` | Clockwise quarter-turns, 0–3. This sketch uses **3**. |

In the desk stand, housing +Y is up. Rotate 1 left the fire upside down (flames from the top). `3` is that turn plus 180 degrees, so the ember row is the bottom edge. Every preset uses the same map.

A browser preview of these presets is in `web/simulator`. See that folder's README.

## Upload

Install [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/), then from the repo root:

```text
tools/upload_matrixlamp.sh
tools/upload_matrixlamp.sh /dev/cu.usbmodem1101 --monitor
tools\upload_matrixlamp.bat COM5
```

Windows can double-click `tools/upload_matrixlamp.bat`. The window waits at the end. The port is the first argument; leave it off and the script looks for one Uno R4 WiFi. `--monitor` opens serial at 115200 after a USB upload. The script installs the `arduino:renesas_uno` core and Adafruit NeoPixel if they are missing, and warns if `secrets.h` is missing. Over-the-air updates use `OTAUpdate`, which is part of that core.

The first time, and the first time after this OTA change, use USB. The previous firmware staged a raw `.bin` in the top half of flash. It cannot install the new `.ota` file, and a board still running that firmware will not understand this format. After this firmware is running, the page has one button, **Install update**. It reads `manifest.json` from the `matrixlamp-latest` release. The same version reports `Up to date (vX)` and does not download. A newer version downloads `MatrixLamp_R4_Wifi.ino.ota`. The picture keeps running; the page and the serial line show the percent. The sketch image has to fit in 245760 bytes (256 KB of code flash minus the 16 KB bootloader).

If the panel stays white, or the board never comes back, double-tap reset until the onboard LED breathes, then `tools\upload_matrixlamp.bat` (or the `.sh`) over USB. That recovery does not need WiFi. The project README, section 5, has the size limit and the GitHub release tags.

```text
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi sketches/MatrixLamp_R4_Wifi
```

Core 1.6.0, NeoPixel 1.15.5, `OTAUpdate` from that core, no `secrets.h`: 123616 B flash (47% of the 262144 figure in `boards.txt`), 18488 B globals (56% of 32768), 14280 B left for locals. The `.bin` is 123624 B. The `.ota` is 99652 B. The linker stops the image at 245760 B. NeoPixel mallocs another 768 B for the 256 LEDs. The radar frames are 26×128 B and share that RAM with the other presets' scratch. A watchdog reboots the board if the main loop stays stuck.
