# WifiFirmwareCheck

Prints the Arduino Uno R4 WiFi ESP32-S3 bridge (WiFi) firmware version. The sketch does not join a network and does not need a WiFi name or password.

The sketch is `sketches/WifiFirmwareCheck/WifiFirmwareCheck.ino`, next to `sketches/MatrixLamp_R4_Wifi`. Serial is 115200. It waits up to about 5 seconds for the monitor, prints `WiFi.firmwareVersion()`, `WIFI_FIRMWARE_LATEST_VERSION`, whether the bridge is at least 0.5.0, `WiFi.status()`, and the Renesas core version when the script was able to pass it in. The same version line repeats every 5 seconds, so opening the monitor late still shows it.

`99.99.99` is what WiFiS3 returns when `AT+FWVERSION?` fails. It is not a real version. `WiFi.status() == WL_NO_MODULE` means the bridge did not answer. On this core a failed status read comes back as `WL_IDLE_STATUS` (0), not `WL_NO_MODULE`; the sketch prints both.

## Run

Install [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/) (`brew install arduino-cli` on macOS). From the repo:

```text
tools/WifiFirmwareCheck/check_wifi_fw.sh
tools/WifiFirmwareCheck/check_wifi_fw.sh /dev/cu.usbmodem1101
PORT=/dev/ttyACM0 tools/WifiFirmwareCheck/check_wifi_fw.sh
```

The script installs the `arduino:renesas_uno` core if it is missing, looks for one Uno R4 WiFi in `arduino-cli board list` (FQBN `arduino:renesas_uno:unor4wifi`) unless you pass a port, compiles, uploads, and opens:

```text
arduino-cli monitor -p <port> -c baudrate=115200
```

This upload replaces the sketch on the board. The lamp page stays down until you flash MatrixLamp again:

```text
tools/upload_matrixlamp.sh
tools/upload_matrixlamp.sh /dev/cu.usbmodem1101
tools\upload_matrixlamp.bat COM5
```

That is the USB flash documented in `sketches/MatrixLamp_R4_Wifi/README.md` (Upload) and in the project README. Double-tap reset until the onboard LED breathes if the board does not take a new sketch.

## Version needed

**0.5.0 or newer.** Arduino's `OTAUpdate` non-blocking download (`startDownload` / `downloadProgress`, the `OTANonBlocking` example) sends `AT+OTADOWNLOADSTART` and `AT+OTADOWNLOADPROGRESS`. Those commands were added in [arduino/uno-r4-wifi-usb-bridge PR 53](https://github.com/arduino/uno-r4-wifi-usb-bridge/pull/53) and shipped in bridge firmware 0.5.0. Older firmware only has the blocking `AT+OTADOWNLOAD`.

`WIFI_FIRMWARE_LATEST_VERSION` is whatever the installed Renesas core was built with (0.6.0 in core 1.6.0). The sketch also says when the bridge is older than that.

The Renesas core does not compile its own version into sketches. The script reads `arduino-cli core list` and passes that version in, so the monitor line can include `core 1.6.0`.

## If the firmware is old

Updating the bridge also replaces the sketch on the RA4M1. Flash the lamp again afterwards with `tools/upload_matrixlamp.sh`. Unplug the board and plug it back in after the bridge update, before uploading a sketch. Left in ESP download mode, a sketch upload can erase the USB bridge firmware.

Arduino IDE 2.2.1 or newer: close the Serial Monitor, then **Tools > Firmware Updater**. Choose UNO R4 WiFi, check for updates, install the newest version, then unplug and plug the board back in. Steps: [Update the connectivity module firmware on UNO R4 WiFi](https://support.arduino.cc/hc/en-us/articles/9670986058780-Update-the-connectivity-module-firmware-on-UNO-R4-WiFi).

Or [arduino-fwuploader](https://github.com/arduino/arduino-fwuploader):

```text
arduino-fwuploader firmware flash -b arduino:renesas_uno:unor4wifi -a /dev/ttyACM0
```

On Linux the ESP32 sometimes enters download mode only through a USB hub.

Or Arduino's espflash updater from the bridge repo. Download `unor4wifi-update-linux.zip`, `unor4wifi-update-macos.zip`, or `unor4wifi-update-windows.zip` from the [uno-r4-wifi-usb-bridge releases](https://github.com/arduino/uno-r4-wifi-usb-bridge/releases), extract it, and run `./update.sh` (Linux), `update.command` (macOS), or `update.bat` (Windows). That package uses `espflash` after putting the board in download mode. Notes: [unor4wifi-updater](https://github.com/arduino/uno-r4-wifi-usb-bridge/tree/main/unor4wifi-updater).

If the board is no longer detected as a UNO R4 WiFi, restore the bridge with espflash: [Restore the USB connectivity firmware on UNO R4 WiFi with espflash](https://support.arduino.cc/hc/en-us/articles/16379769332892-Restore-the-USB-connectivity-firmware-on-UNO-R4-WiFi-with-espflash).
