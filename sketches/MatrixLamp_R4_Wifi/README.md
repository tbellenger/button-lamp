# MatrixLamp_R4_Wifi

Uno R4 WiFi sketch for Tom's BTF-LIGHTING WS2812B 16×16 panel (256 LEDs). The lamp is the panel and the Uno only: there is no knob, fader, or button. The phone page has a brightness slider, a speed slider, and preset buttons (fire, rainbow, meatballs, plasma, sun, spiral, fluid, heart). While Meatballs is showing, + Ball and − Ball change the count from 1 to 8. A new ball gets a random position and velocity. On Spiral, Speed shifts the colour and the arm keeps a fixed spin. The last preset, brightness, speed, and ball count are stored in the data-flash block after the WiFi credentials. The board's 12×8 red matrix scrolls the WiFi IP. After one USB flash, firmware updates go over WiFi.

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

Copy `secrets.example.h` to `secrets.h` in this folder. `secrets.h` is gitignored. Do not commit it. Put the network in `WIFI_SSID` / `WIFI_PASS` and set `OTA_PASSWORD`. A USB build stores those in data flash when they differ. A GitHub build has no `secrets.h` and does not overwrite them. With no stored network, the board opens WiFi `MatrixLamp-Setup` so a phone can enter them.

The lamp is online only when `WiFi.status()` is `WL_CONNECTED` and `WiFi.localIP()` is not `0.0.0.0`. Boot soft-resets the ESP32-S3 twice before joining, because an OTA reboot resets only the RA4M1 and a stale OK leaves the old port-80 socket in place. If the listen socket still will not open, or joining fails twice, the modem is reset again. mDNS: `matrixlamp.local` and `matrix-lamp.local`. DHCP name: `matrixlamp`.

## If the picture is backwards

Edit only these, at the top of `MatrixLamp_R4_Wifi.ino`:

| Setting | Meaning |
|---|---|
| `PANEL_SERPENTINE` | `true` when rows alternate direction (these panels usually do). |
| `PANEL_SERPENTINE_ODD` | `true`: row 0 left to right, row 1 right to left. |
| `PANEL_FLIP_X`, `PANEL_FLIP_Y` | Mirror. Both are `false`. |
| `PANEL_ROTATE` | Clockwise quarter-turns, 0–3. This sketch uses **3**. |

In the desk stand, housing +Y is up. `PANEL_ROTATE` 1 left the fire upside down (flames from the top). `3` is that turn plus 180 degrees, so the ember row is the bottom edge. Every preset uses the same map.

## Upload

Install [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/), then from the repo root:

```text
tools/upload_matrixlamp.sh
tools/upload_matrixlamp.sh /dev/cu.usbmodem1101 --monitor
tools\upload_matrixlamp.bat COM5
```

Windows can double-click `tools/upload_matrixlamp.bat`. The window waits at the end. The port is the first argument; leave it off and the script looks for one Uno R4 WiFi. `--monitor` opens serial at 115200 after a USB upload. The script installs the `arduino:renesas_uno` core, Adafruit NeoPixel, and ArduinoOTA if they are missing, and warns if `secrets.h` is missing.

The first time, use USB. After this firmware is running:

```text
tools\upload_matrixlamp.bat ota
tools\upload_matrixlamp.bat ota 192.168.1.20
tools/upload_matrixlamp.sh --ota
```

That compiles a `.bin` and POSTs it to `http://matrixlamp.local/update` (or the IP you pass). The password comes from the `OTA_PASSWORD` environment variable, then `secrets.h`, then a prompt. Success is printed only when the lamp reboots into that build. If `matrixlamp.local` does not resolve on Windows, use the IP shown on the web page and on the serial monitor.

On the phone, open the lamp's page, enter the OTA password under Firmware, and either pick a `.bin` or tap Update from GitHub. The file picker sends the raw file, not multipart. The lamp answers `100 Continue` before it erases staging (122880 bytes on this core, so a 122672 byte image fits) and then fills the panel with orange as the bytes land. White means it is copying into the sketch. Red is a rejected file or a failed GitHub fetch. The firmware line shows `ota` / `otaPct` / `otaMsg` from `/api/state`. `closed before reply` means the browser never got a status line. That is not a reboot.

If the panel stays white, or the board never comes back, double-tap reset until the onboard LED breathes, then `tools\upload_matrixlamp.bat` (or the `.sh`) over USB. That recovery does not need WiFi. The project README, section 5, has the size limit (122880 bytes) and the GitHub release tags.

```text
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi sketches/MatrixLamp_R4_Wifi
```

Core 1.6.0, NeoPixel 1.15.5, ArduinoOTA 1.1.1, no `secrets.h`: 114824 B flash (43% of 262144), 17032 B globals (51% of 32768), 15736 B left for locals. The `.bin` is 114832 B. OTA accepts at most 122880 B. NeoPixel mallocs another 768 B for the 256 LEDs.
