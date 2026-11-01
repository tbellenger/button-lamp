#!/usr/bin/env bash
# Compile and upload sketches/MatrixLamp_R4_Wifi to an Arduino Uno R4 WiFi.
# From the repo root, or from anywhere:
#   tools/upload_matrixlamp.sh                  # find the board
#   tools/upload_matrixlamp.sh /dev/cu.usbmodem1101
#   tools/upload_matrixlamp.sh COM5 --monitor
#   tools/upload_matrixlamp.sh --monitor
#   tools/upload_matrixlamp.sh ota
#   tools/upload_matrixlamp.sh --ota 192.168.1.20
# --monitor opens the serial monitor at 115200 (the sketch's Serial.begin)
# after a successful USB upload. Ctrl-C closes it.
# ota / --ota [host] compiles and sends the .bin over WiFi. The host defaults
# to matrixlamp.local. USB upload stays the default when ota is omitted.

set -u

FQBN="arduino:renesas_uno:unor4wifi"
SKETCH="sketches/MatrixLamp_R4_Wifi"
BAUD="115200"

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1

PORT=""
MONITOR=0
OTA=0
HOST=""
usage() {
  echo "Usage: tools/upload_matrixlamp.sh [PORT] [--monitor]"
  echo "       tools/upload_matrixlamp.sh ota [HOST]"
  echo "       tools/upload_matrixlamp.sh --ota [HOST]"
  echo "  PORT       serial port (COM5, /dev/cu.usbmodem1101, /dev/ttyACM0)."
  echo "             If omitted, the script looks for one Uno R4 WiFi."
  echo "  --monitor  open the serial monitor at ${BAUD} baud after a USB upload."
  echo "  ota, --ota compile and send the firmware over WiFi."
  echo "  HOST       lamp address. Default matrixlamp.local. An IP works too."
  exit "${1:-0}"
}
while [ $# -gt 0 ]; do
  case "$1" in
    --monitor|-m) MONITOR=1 ;;
    --help|-h) usage 0 ;;
    --ota)
      OTA=1
      if [ $# -ge 2 ] && [ "${2#-}" = "$2" ]; then
        HOST=$2
        shift
      fi
      ;;
    ota)
      OTA=1
      ;;
    --*)
      echo "Unknown option: $1"
      usage 1
      ;;
    *)
      if [ "$OTA" -eq 1 ] && [ -z "$HOST" ]; then
        HOST=$1
      elif [ -z "$PORT" ]; then
        PORT=$1
      else
        echo "Unexpected extra argument: $1"
        usage 1
      fi
      ;;
  esac
  shift
done
if [ "$OTA" -eq 1 ] && [ "$MONITOR" -eq 1 ]; then
  echo "--monitor is only for a USB upload."
  exit 1
fi
if [ "$OTA" -eq 1 ] && [ -n "$PORT" ]; then
  echo "Pass either a serial port or ota, not both."
  exit 1
fi
if [ "$OTA" -eq 1 ] && [ -z "$HOST" ]; then
  HOST=matrixlamp.local
fi

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

echo "Checking the Uno R4 WiFi core, Adafruit NeoPixel, and ArduinoOTA..."
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
if ! arduino-cli lib install "ArduinoOTA"; then
  echo "Could not install the ArduinoOTA library (JAndrassy, 1.1.0 or newer)."
  exit 1
fi

SECRETS="$SKETCH/secrets.h"
if [ ! -f "$SECRETS" ]; then
  echo "WARNING: $SECRETS does not exist."
  echo "A USB build will not contain the WiFi name. A blank board opens MatrixLamp-Setup."
  echo "A board that already stored a network keeps using it."
  echo "  cp $SKETCH/secrets.example.h $SECRETS"
  echo "Then put the home network in WIFI_SSID and WIFI_PASS, and set OTA_PASSWORD."
  echo "secrets.h is gitignored. Do not commit it."
fi

run_ota() {
  if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 is required for the WiFi upload (CRC and the result check)."
    exit 1
  fi
  local pass=""
  if [ -n "${OTA_PASSWORD:-}" ]; then
    pass=$OTA_PASSWORD
  elif [ -f "$SECRETS" ]; then
    pass=$(sed -n 's/^[[:space:]]*#define[[:space:]]\{1,\}OTA_PASSWORD[[:space:]]\{1,\}"\([^"]*\)".*/\1/p' "$SECRETS" | head -n 1)
  fi
  if [ -z "$pass" ]; then
    if [ -t 0 ]; then
      printf "OTA password: "
      read -r pass
    fi
  fi
  if [ -z "$pass" ]; then
    echo "No OTA password. Set OTA_PASSWORD in the environment or in $SECRETS."
    exit 1
  fi
  local out="$ROOT/build/matrixlamp"
  rm -rf "$out"
  mkdir -p "$out"
  echo "Compiling $SKETCH"
  if ! arduino-cli compile -b "$FQBN" --output-dir "$out" "$SKETCH"; then
    echo "FAILED: compile failed."
    exit 1
  fi
  local bin="$out/MatrixLamp_R4_Wifi.ino.bin"
  if [ ! -f "$bin" ]; then
    echo "FAILED: no .bin was produced."
    exit 1
  fi
  local crc mark
  crc=$(python3 -c 'import sys,zlib; print("%08x" % (zlib.crc32(open(sys.argv[1],"rb").read()) & 0xffffffff))' "$bin")
  mark=$(python3 -c '
import sys
d=open(sys.argv[1],"rb").read()
i=d.find(b"MLFW:")
if i < 0:
    sys.exit(2)
s=d[i:i+96].split(b"\0",1)[0].decode("ascii","replace")
sys.stdout.write(s[5:])
' "$bin") || {
    echo "FAILED: the .bin has no MLFW version marker."
    exit 1
  }
  echo "Sending $(wc -c < "$bin" | tr -d " ") bytes to http://$HOST/update"
  local body rc
  body="$out/ota-body.txt"
  set +e
  curl -fS --http1.1 --max-time 180 \
    -H "X-OTA-Password: $pass" \
    -H "X-OTA-Crc32: $crc" \
    -H "Content-Type: application/octet-stream" \
    --data-binary @"$bin" \
    -o "$body" \
    "http://$HOST/update"
  rc=$?
  set -e
  if [ -f "$body" ]; then
    echo "Lamp said: $(tr -d "\r" < "$body")"
  fi
  if [ "$rc" -ne 0 ]; then
    echo "curl exited $rc. The lamp may be rebooting; checking the version."
  fi
  local i
  for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do
    sleep 2
    if python3 -c '
import json, sys, urllib.request
ver, built, url = sys.argv[1], sys.argv[2], sys.argv[3]
try:
    with urllib.request.urlopen(url, timeout=4) as r:
        s = json.load(r)
except Exception:
    sys.exit(1)
if s.get("fw") == ver and s.get("built") == built:
    print("SUCCESS: lamp is running %s built %s" % (ver, built))
    sys.exit(0)
sys.exit(1)
' "${mark%%|*}" "${mark#*|}" "http://$HOST/api/state"; then
      exit 0
    fi
  done
  echo "FAILED: the lamp did not report ${mark%%|*} built ${mark#*|}."
  echo "If matrixlamp.local does not resolve, rerun with the IP from the web page or serial."
  exit 1
}

if [ "$OTA" -eq 1 ]; then
  echo "OTA target http://$HOST/"
  run_ota
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
