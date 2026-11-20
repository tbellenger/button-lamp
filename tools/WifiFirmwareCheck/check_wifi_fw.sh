#!/usr/bin/env bash
# Compile, upload, and monitor sketches/WifiFirmwareCheck on an Uno R4 WiFi.
# The sketch prints the ESP32-S3 bridge firmware version. It does not join WiFi.
#
# This replaces the sketch on the board. MatrixLamp_R4_Wifi stays off until
# you flash it again with tools/upload_matrixlamp.sh
# (Windows: tools\upload_matrixlamp.bat). See sketches/MatrixLamp_R4_Wifi/README.md
# (Upload) and the project README.
#
# From the repo root, or from anywhere:
#   tools/WifiFirmwareCheck/check_wifi_fw.sh
#   tools/WifiFirmwareCheck/check_wifi_fw.sh /dev/cu.usbmodem1101
#   tools/WifiFirmwareCheck/check_wifi_fw.sh COM5
#   PORT=/dev/ttyACM0 tools/WifiFirmwareCheck/check_wifi_fw.sh
# A port argument wins over PORT.

set -u

FQBN="arduino:renesas_uno:unor4wifi"
SKETCH="sketches/WifiFirmwareCheck"
BAUD="115200"
CORE_ID="arduino:renesas_uno"

ROOT=$(CDPATH='' cd -- "$(dirname "$0")/../.." && pwd)
cd "$ROOT" || exit 1

HEADER="$ROOT/sketches/WifiFirmwareCheck/renesas_core_version.h"
BUILD=""

cleanup() {
  if [ -n "$BUILD" ]; then
    rm -rf "$BUILD"
  fi
  if [ -f "$HEADER" ]; then
    rm -f "$HEADER"
  fi
}
trap cleanup EXIT

PORT="${PORT:-}"

usage() {
  echo "Usage: tools/WifiFirmwareCheck/check_wifi_fw.sh [PORT]"
  echo "  PORT       serial port (COM5, /dev/cu.usbmodem1101, /dev/ttyACM0)."
  echo "             If omitted, PORT is used, then one Uno R4 WiFi is chosen."
  echo "  The script compiles, uploads, and opens the serial monitor at ${BAUD}."
  exit "${1:-0}"
}

ARG_PORT=""
while [ $# -gt 0 ]; do
  case "$1" in
    --help|-h) usage 0 ;;
    --*)
      echo "Unknown option: $1"
      usage 1
      ;;
    *)
      if [ -n "$ARG_PORT" ]; then
        echo "Unexpected extra argument: $1"
        usage 1
      fi
      ARG_PORT=$1
      ;;
  esac
  shift
done
if [ -n "$ARG_PORT" ]; then
  PORT=$ARG_PORT
fi

echo "WifiFirmwareCheck: repo is $ROOT"
echo "This upload replaces the sketch on the Uno R4 WiFi."
echo "MatrixLamp_R4_Wifi stops until you flash it again:"
echo "  tools/upload_matrixlamp.sh [PORT]"
echo "  tools\\upload_matrixlamp.bat [PORT]"
echo "See sketches/MatrixLamp_R4_Wifi/README.md (Upload) and the project README."

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli is not installed, or it is not on PATH."
  echo "Install it, then open a new terminal so PATH picks it up:"
  echo "  macOS:  brew install arduino-cli"
  echo "  Linux:  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
  echo "  Docs:   https://arduino.github.io/arduino-cli/latest/installation/"
  exit 1
fi

core_version() {
  arduino-cli core list | awk -v id="$CORE_ID" '$1 == id { print $2; exit }'
}

CORE_VER=$(core_version || true)
if [ -z "$CORE_VER" ]; then
  echo "Installing ${CORE_ID}..."
  if ! arduino-cli core update-index; then
    echo "WARNING: could not refresh the board index. Continuing if the core is already installed."
  fi
  if ! arduino-cli core install "$CORE_ID"; then
    echo "Could not install the ${CORE_ID} core."
    echo "In the Arduino IDE this is Boards Manager -> Arduino UNO R4 Boards."
    exit 1
  fi
  CORE_VER=$(core_version || true)
fi

if [ -n "$CORE_VER" ]; then
  echo "Using ${CORE_ID} ${CORE_VER}"
else
  echo "WARNING: ${CORE_ID} is installed, but its version was not read."
  echo "The sketch will say the Renesas core version is not available."
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
    echo "Pass the port yourself: tools/WifiFirmwareCheck/check_wifi_fw.sh COM5" >&2
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
  case "$HITS" in
    ''|*[!0-9]*) HITS=0 ;;
  esac
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
    case "$ALL" in
      ''|*[!0-9]*) ALL=0 ;;
    esac
    if [ "$ALL" -eq 0 ]; then
      echo "arduino-cli did not see any serial ports."
    else
      echo "Ports arduino-cli can see:"
      printf '%s\n' "$LIST" | awk '$1=="PORT"{ $1=""; sub(/^ /,""); print "  " $0 }'
    fi
    echo "Pass the port, for example:"
    echo "  tools/WifiFirmwareCheck/check_wifi_fw.sh /dev/cu.usbmodem1101"
    echo "  tools/WifiFirmwareCheck/check_wifi_fw.sh COM5"
    echo "  PORT=/dev/ttyACM0 tools/WifiFirmwareCheck/check_wifi_fw.sh"
    if [ -t 0 ]; then
      printf "Port (or Enter to cancel): "
      read -r PORT
      if [ -z "$PORT" ]; then
        echo "Cancelled. The board was not changed."
        exit 1
      fi
    else
      exit 1
    fi
  fi
fi

if [ -z "$PORT" ]; then
  echo "No serial port was given."
  exit 1
fi

if printf '%s\n' "$CORE_VER" | grep -Eq '^[0-9]+(\.[0-9]+){1,3}$'; then
  printf '%s\n' \
    '#pragma once' \
    '/* Written by tools/WifiFirmwareCheck/check_wifi_fw.sh. Not committed. */' \
    "#define RENESAS_CORE_VERSION \"${CORE_VER}\"" \
    > "$HEADER"
fi

if [ -z "${TMPDIR:-}" ]; then
  BUILD="/tmp/wifi-fw-check.$$"
else
  BUILD="${TMPDIR}/wifi-fw-check.$$"
fi
mkdir -p "$BUILD" || exit 1

echo "Compiling ${SKETCH} for ${FQBN}"
if ! arduino-cli compile -b "$FQBN" --build-path "$BUILD" "$SKETCH"; then
  echo "Compile failed. The board was not changed."
  exit 1
fi

echo "Uploading to ${PORT}."
echo "This replaces the sketch currently on the board."
if ! arduino-cli upload -b "$FQBN" -p "$PORT" --input-dir "$BUILD" "$SKETCH"; then
  echo "Upload failed."
  echo "If the upload started, MatrixLamp_R4_Wifi may already be gone."
  echo "Reflash the lamp with:"
  echo "  tools/upload_matrixlamp.sh ${PORT}"
  echo "  tools\\upload_matrixlamp.bat ${PORT}"
  exit 1
fi

echo "Uploaded WifiFirmwareCheck to ${PORT}."
echo "The lamp sketch is no longer running. Reflash it when you are done:"
echo "  tools/upload_matrixlamp.sh ${PORT}"
echo "  tools\\upload_matrixlamp.bat ${PORT}"
echo "See sketches/MatrixLamp_R4_Wifi/README.md (Upload) and the project README."
echo "Serial monitor at ${BAUD} baud. The version line repeats every 5 seconds."
echo "  arduino-cli monitor -p ${PORT} -c baudrate=${BAUD}"

if [ -t 0 ] && [ -t 1 ]; then
  echo "Opening the monitor. Ctrl-C to stop."
  sleep 2
  if arduino-cli monitor -p "$PORT" -c "baudrate=${BAUD}"; then
    exit 0
  fi
  echo "The serial monitor stopped."
  echo "Open it again with:"
  echo "  arduino-cli monitor -p ${PORT} -c baudrate=${BAUD}"
  exit 1
fi

echo "This shell is not a terminal, so the monitor was not opened."
echo "Open it with:"
echo "  arduino-cli monitor -p ${PORT} -c baudrate=${BAUD}"
