#!/usr/bin/env bash
# Compile and upload sketches/MatrixLamp_R4_Wifi to an Arduino Uno R4 WiFi.
# From the repo root, or from anywhere:
#   tools/upload_matrixlamp.sh                  # find the board
#   tools/upload_matrixlamp.sh /dev/cu.usbmodem1101
#   tools/upload_matrixlamp.sh COM5 --monitor
#   tools/upload_matrixlamp.sh --monitor
# --monitor opens the serial monitor at 115200 (the sketch's Serial.begin)
# after a successful upload. Ctrl-C closes it.
# There is no OTA upload.

set -u

FQBN="arduino:renesas_uno:unor4wifi"
SKETCH="sketches/MatrixLamp_R4_Wifi"
BAUD="115200"

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1

PORT=""
MONITOR=0
for arg in "$@"; do
  case "$arg" in
    --monitor|-m) MONITOR=1 ;;
    --help|-h)
      echo "Usage: tools/upload_matrixlamp.sh [PORT] [--monitor]"
      echo "  PORT      serial port (COM5, /dev/cu.usbmodem1101, /dev/ttyACM0)."
      echo "            If omitted, the script looks for one Uno R4 WiFi."
      echo "  --monitor open the serial monitor at ${BAUD} baud after upload."
      exit 0
      ;;
    --*)
      echo "Unknown option: $arg"
      echo "Usage: tools/upload_matrixlamp.sh [PORT] [--monitor]"
      exit 1
      ;;
    *)
      if [ -n "$PORT" ]; then
        echo "Unexpected extra argument: $arg"
        echo "Usage: tools/upload_matrixlamp.sh [PORT] [--monitor]"
        exit 1
      fi
      PORT=$arg
      ;;
  esac
done

echo "Matrix Lamp: repo is $ROOT"
echo "Updating from git..."
if ! git pull; then
  echo "WARNING: git pull failed (offline, or this checkout has local changes)."
  echo "Continuing with the copy that is already here."
fi

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli is not installed, or it is not on PATH."
  echo "Install it, then open a new terminal so PATH picks it up:"
  echo "  macOS:  brew install arduino-cli"
  echo "  Linux:  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
  echo "  Docs:   https://arduino.github.io/arduino-cli/latest/installation/"
  exit 1
fi

echo "Checking the Uno R4 WiFi core and Adafruit NeoPixel..."
if ! arduino-cli core update-index; then
  echo "WARNING: could not refresh the board index. Continuing if the core is already installed."
fi
if ! arduino-cli core install arduino:renesas_uno; then
  echo "Could not install the arduino:renesas_uno core."
  echo "In the Arduino IDE this is Boards Manager -> Arduino UNO R4 Boards."
  exit 1
fi
if ! arduino-cli lib install "Adafruit NeoPixel"; then
  echo "Could not install the Adafruit NeoPixel library (1.12 or newer)."
  exit 1
fi

SECRETS="$SKETCH/secrets.h"
if [ ! -f "$SECRETS" ]; then
  echo "WARNING: $SECRETS does not exist. WiFi will stay off until you create it."
  echo "  cp $SKETCH/secrets.example.h $SECRETS"
  echo "Then put the home network in WIFI_SSID and WIFI_PASS."
  echo "secrets.h is gitignored. Do not commit it."
fi

# Print HITS / HIT / PORT lines from `arduino-cli board list --json`.
list_boards() {
  local json py
  if ! json=$(arduino-cli board list --json --discovery-timeout 5s); then
    echo "arduino-cli board list failed." >&2
    return 1
  fi
  if command -v python3 >/dev/null 2>&1; then
    py=python3
  elif command -v python >/dev/null 2>&1; then
    py=python
  else
    echo "python3 is not installed, so the port cannot be picked automatically." >&2
    echo "Pass the port yourself: tools/upload_matrixlamp.sh COM5" >&2
    printf '%s\n' "$json" >&2
    return 1
  fi
  printf '%s\n' "$json" | "$py" -c '
import json, sys
fqbn = "arduino:renesas_uno:unor4wifi"
try:
    data = json.load(sys.stdin)
except Exception as exc:
    sys.stderr.write("Could not read arduino-cli board list: %s\n" % exc)
    sys.exit(1)
hits = []
shown = []
for item in data.get("detected_ports") or []:
    port = item.get("port") or {}
    addr = port.get("address") or ""
    names = []
    matched = False
    for board in item.get("matching_boards") or []:
        name = board.get("name") or ""
        if name:
            names.append(name)
        if board.get("fqbn") == fqbn:
            matched = True
    if not addr:
        continue
    if names:
        shown.append("%s  (%s)" % (addr, ", ".join(names)))
    else:
        shown.append(addr)
    if matched:
        hits.append(addr)
print("HITS %d" % len(hits))
for h in hits:
    print("HIT %s" % h)
print("ALL %d" % len(shown))
for s in shown:
    print("PORT %s" % s)
'
}

if [ -z "$PORT" ]; then
  echo "Looking for an Arduino Uno R4 WiFi..."
  LIST=$(list_boards) || exit 1
  HITS=$(printf '%s\n' "$LIST" | awk '$1=="HITS"{print $2; exit}')
  HITS=${HITS:-0}
  if [ "$HITS" -eq 1 ]; then
    PORT=$(printf '%s\n' "$LIST" | awk '$1=="HIT"{print $2; exit}')
    echo "Using $PORT"
  else
    if [ "$HITS" -eq 0 ]; then
      echo "No Arduino Uno R4 WiFi was found."
    else
      echo "More than one Arduino Uno R4 WiFi was found:"
      printf '%s\n' "$LIST" | awk '$1=="HIT"{print "  " $2}'
    fi
    ALL=$(printf '%s\n' "$LIST" | awk '$1=="ALL"{print $2; exit}')
    ALL=${ALL:-0}
    if [ "$ALL" -eq 0 ]; then
      echo "arduino-cli did not see any serial ports."
    else
      echo "Ports arduino-cli can see:"
      printf '%s\n' "$LIST" | awk '$1=="PORT"{ $1=""; sub(/^ /,""); print "  " $0 }'
    fi
    echo "Pass the port, for example:"
    echo "  tools/upload_matrixlamp.sh /dev/cu.usbmodem1101"
    echo "  tools/upload_matrixlamp.sh COM5"
    if [ -t 0 ]; then
      printf "Port (or Enter to cancel): "
      read -r PORT
      if [ -z "$PORT" ]; then
        echo "Cancelled."
        exit 1
      fi
    else
      exit 1
    fi
  fi
fi

echo "Compiling and uploading to $PORT"
if ! arduino-cli compile --upload -b "$FQBN" -p "$PORT" "$SKETCH"; then
  echo "Compile or upload failed."
  exit 1
fi

echo "Uploaded MatrixLamp_R4_Wifi to $PORT"
if [ "$MONITOR" -eq 1 ]; then
  echo "Serial monitor at $BAUD baud. Ctrl-C to stop."
  exec arduino-cli monitor -p "$PORT" -b "$FQBN" -c "baudrate=$BAUD"
fi
