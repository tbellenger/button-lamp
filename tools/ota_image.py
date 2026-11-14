#!/usr/bin/env python3
"""Build and check an Uno R4 WiFi .ota image.

The bytes match ArduinoIoTCloud extras/tools/bin2ota.py for board UNOR4WIFI
plus lzss.py --encode (Okumura EI=11, EJ=4):

  uint32 little-endian  length of (magic + version + lzss payload)
  uint32 little-endian  CRC-32 of those bytes (ISO-HDLC / zlib)
  uint32 little-endian  magic 0x23411002
  8-byte version        zeros, except the last byte 0x40 (compression flag)
  LZSS payload

0x23411002 is ARDUINO_RA4M1_OTA_MAGIC in uno-r4-wifi-usb-bridge OTA.h
(USB VID 0x2341, PID 0x1002). Bridge firmware 0.6.0 passes that magic into
OTA.begin() for every RA4M1 update. Core 1.6.0 OTAUpdate does not follow
HTTP redirects; the sketch resolves GitHub's 302 before startDownload.

check decompresses with a decoder that follows Arduino_ESP32_OTA 0.3.1
(the copy linked by bridge 0.6.0) and also with the C decoder, and requires
both to match the original .bin.
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import zlib

MAGIC = 0x23411002
VERSION = bytes([0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40])
HERE = os.path.dirname(os.path.abspath(__file__))
LZSS_C = os.path.join(HERE, "ota", "lzss.c")
LZSS_MAIN = os.path.join(HERE, "ota", "lzss_main.c")


def lzss_exe():
    exe = os.path.join(tempfile.gettempdir(), "matrixlamp-lzss")
    subprocess.check_call(
        ["gcc", "-O2", "-o", exe, LZSS_MAIN, LZSS_C],
        stdout=subprocess.DEVNULL,
    )
    return exe


def lzss_run(mode, src, dst):
    exe = lzss_exe()
    subprocess.check_call([exe, mode, src, dst])


def pack_bytes(blob):
    # Length and CRC cover magic + version + payload, not the 8-byte prefix.
    body = struct.pack("<I", MAGIC) + VERSION + blob
    crc = zlib.crc32(body) & 0xFFFFFFFF
    return struct.pack("<II", len(body), crc) + body


def arduino_lzss_decode(data):
    """Arduino_ESP32_OTA LZSSDecoder: EI=11, EJ=4, MSB-first bits."""
    ei, ej = 11, 4
    n = 1 << ei
    ring = bytearray(n * 2)
    for i in range(n - ((1 << ej) + 1)):
        ring[i] = 0x20
    r = n - ((1 << ej) + 1)
    acc = 0
    acc_n = 0
    pos = 0
    out = bytearray()

    def getbit(bits):
        nonlocal acc, acc_n, pos
        while acc_n < bits:
            if pos >= len(data):
                return None
            acc = ((acc << 8) | data[pos]) & 0xFFFFFFFFFFFFFFFF
            pos += 1
            acc_n += 8
        val = acc >> (acc_n - bits)
        acc &= (1 << (acc_n - bits)) - 1
        acc_n -= bits
        return val

    while True:
        flag = getbit(1)
        if flag is None:
            break
        if flag:
            lit = getbit(8)
            if lit is None:
                break
            out.append(lit)
            ring[r] = lit
            r = (r + 1) & (n - 1)
        else:
            idx = getbit(ei)
            if idx is None:
                break
            length = getbit(ej)
            if length is None:
                break
            for k in range(length + 2):
                b = ring[(idx + k) & (n - 1)]
                out.append(b)
                ring[r] = b
                r = (r + 1) & (n - 1)
    return bytes(out)


def cmd_pack(bin_path, ota_path):
    with tempfile.TemporaryDirectory() as td:
        lz = os.path.join(td, "sketch.lzss")
        lzss_run("--encode", bin_path, lz)
        payload = open(lz, "rb").read()
    if not payload:
        raise SystemExit("lzss encode produced an empty file")
    blob = pack_bytes(payload)
    with open(ota_path, "wb") as f:
        f.write(blob)
    print(f"ota {len(blob)} bytes from bin {os.path.getsize(bin_path)}")


def cmd_check(ota_path, bin_path):
    raw = open(ota_path, "rb").read()
    want = open(bin_path, "rb").read()
    if len(raw) < 20:
        raise SystemExit("ota header is short")
    length, crc, magic = struct.unpack_from("<III", raw, 0)
    version = raw[12:20]
    payload = raw[20:]
    if magic != MAGIC:
        raise SystemExit(f"magic {magic:#x} is not {MAGIC:#x} (Uno R4 WiFi)")
    if version != VERSION:
        raise SystemExit(f"version bytes {version.hex()} are not the LZSS flag")
    body = raw[8:]
    if length != len(body):
        raise SystemExit(f"length field {length} != {len(body)}")
    got = zlib.crc32(body) & 0xFFFFFFFF
    if crc != got:
        raise SystemExit(f"crc {crc:#010x} != {got:#010x}")
    decoded = arduino_lzss_decode(payload)
    if decoded != want:
        raise SystemExit(
            f"arduino decoder {len(decoded)} bytes != bin {len(want)}"
        )
    with tempfile.TemporaryDirectory() as td:
        lz = os.path.join(td, "p.lzss")
        out = os.path.join(td, "out.bin")
        open(lz, "wb").write(payload)
        lzss_run("--decode", lz, out)
        c_decoded = open(out, "rb").read()
    if c_decoded != want:
        raise SystemExit(f"c decoder {len(c_decoded)} bytes != bin {len(want)}")
    print(
        f"ota ok magic {magic:#x} bin {len(want)} ota {len(raw)} lzss {len(payload)}"
    )


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("pack")
    a.add_argument("bin")
    a.add_argument("ota")
    b = sub.add_parser("check")
    b.add_argument("ota")
    b.add_argument("bin")
    args = p.parse_args()
    if args.cmd == "pack":
        cmd_pack(args.bin, args.ota)
    else:
        cmd_check(args.ota, args.bin)


if __name__ == "__main__":
    main()
