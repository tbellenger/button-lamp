#!/usr/bin/env python3
"""Replay the lamp's GitHub header reader against the live release.

The sketch stops a 302 when the Location line ends. This checks that cut
lands after the signed URL and before the Content-Security-Policy, then
follows that URL and checks the manifest the lamp would parse.
"""

import json
import subprocess
import sys

MANIFEST = (
    "https://github.com/tbellenger/button-lamp/releases/download/"
    "matrixlamp-latest/manifest.json"
)
BIN_URL = (
    "https://github.com/tbellenger/button-lamp/releases/download/"
    "matrixlamp-latest/MatrixLamp_R4_Wifi.ino.bin"
)
PATH_MAX = 1400
OTA_MAX = 122880


def curl(url, extra):
    return subprocess.check_output(
        ["curl", "-sD", "-", "--http1.0", "-A", "MatrixLamp",
         "--max-redirs", "0", "--max-time", "30"] + extra + [url]
    )


def split_response(raw):
    sep = raw.find(b"\r\n\r\n")
    if sep < 0:
        raise SystemExit("no header end")
    return raw[:sep + 4], raw[sep + 4:]


class Head:
    def __init__(self):
        self.code = 0
        self.length = -1
        self.chunked = 0
        self.loc = 0
        self.path = bytearray()
        self.stopped = 0
        self.err = ""


def cieq(buf, n, lit):
    if len(buf) < n or len(lit) < n:
        return False
    for i in range(n):
        a = buf[i]
        b = lit[i]
        if 65 <= a <= 90:
            a += 32
        if 65 <= b <= 90:
            b += 32
        if a != b:
            return False
    return True


def read_headers(raw):
    """Same cuts as ghReadResponse: 90-byte lines, Location into a 1400-byte path,
    return as soon as a 3xx Location line ends."""
    h = Head()
    line = bytearray()
    ln = 0
    cap = 0
    phase = 0
    for b in raw:
        if phase:
            break
        if b == 10:
            if ln and line[ln - 1] == 13:
                ln -= 1
            if cap:
                cap = 0
                if 300 <= h.code < 400 and h.loc:
                    h.stopped = 1
                    return h
            if ln == 0:
                phase = 1
                if 300 <= h.code < 400:
                    h.stopped = 2
                    return h
                continue
            text = bytes(line[:ln])
            if h.code == 0:
                sp = text.find(b" ")
                if sp >= 0:
                    h.code = int(text[sp + 1:].split()[0])
            elif text.lower().startswith(b"content-length:"):
                h.length = int(text[15:].strip() or b"0")
            elif text.lower().startswith(b"transfer-encoding:") and b"chunked" in text.lower():
                h.chunked = 1
            ln = 0
            line = bytearray()
        elif cap:
            if b != 13:
                if len(h.path) >= PATH_MAX - 1:
                    h.err = "url too long"
                    return h
                h.path.append(b)
        elif ln < 90:
            line.append(b)
            ln += 1
            if ln == 9 and cieq(line, 9, b"Location:"):
                cap = 1
                h.loc = 1
    h.err = h.err or "headers not finished"
    return h


def take_location(path):
    p = bytes(path).decode("latin1").lstrip(" \t")
    if p.startswith("https://"):
        rest = p[8:]
        slash = rest.find("/")
        if slash <= 0:
            return None
        return rest[:slash], rest[slash:]
    if p.startswith("/"):
        return "", p
    return None


def check(name, url):
    raw = curl(url, ["-o", "/dev/null"])
    head, _ = split_response(raw + b"\r\n\r\n")
    # curl -D - with -o /dev/null still puts headers on stdout only when -D -
    # is used; -o /dev/null does not strip -D. raw is headers.
    h = read_headers(raw)
    loc_at = raw.lower().find(b"\nlocation:")
    csp_at = raw.lower().find(b"\ncontent-security-policy:")
    print(f"{name}: status bytes {len(raw)} location-at {loc_at} csp-at {csp_at}")
    print(f"  parser code={h.code} loc={h.loc} stopped={h.stopped} err={h.err!r} path={len(h.path)}")
    if h.code != 302 or not h.loc or h.stopped != 1:
        raise SystemExit(f"{name}: redirect was not cut at Location")
    if csp_at >= 0:
        # Bytes the old reader still had to pull after Location's newline.
        loc_end = raw.find(b"\n", loc_at + 1) + 1
        if loc_end <= 0 or loc_end > csp_at:
            raise SystemExit(f"{name}: Location is not before the policy header")
        print(f"  stopped at {loc_end}, skipped {len(raw) - loc_end} header bytes")
    got = take_location(h.path)
    if not got or not got[0] or not got[1].startswith("/"):
        raise SystemExit(f"{name}: Location did not split")
    host, path = got
    print(f"  host {host} ({len(host)}) path {len(path)}")
    if len(host) >= 64:
        raise SystemExit("host does not fit g_ghHost")
    if host != "release-assets.githubusercontent.com":
        raise SystemExit(f"unexpected host {host}")
    asset = "https://" + host + path
    body_raw = curl(asset, [])
    ahead, body = split_response(body_raw)
    status = ahead.split(b"\r\n", 1)[0]
    cl = None
    for line in ahead.split(b"\r\n"):
        if line.lower().startswith(b"content-length:"):
            cl = int(line.split(b":", 1)[1].strip())
    print(f"  asset {status.decode()} content-length {cl} body {len(body)}")
    if b"200" not in status:
        raise SystemExit("asset was not 200")
    if cl != len(body):
        raise SystemExit("content-length does not match body")
    return body, cl


def main():
    manifest, mlen = check("manifest", MANIFEST)
    doc = json.loads(manifest.decode())
    need = ("version", "file", "crc32", "size")
    for k in need:
        if k not in doc:
            raise SystemExit(f"manifest missing {k}")
    print(f"manifest {doc['version']} file {doc['file']} size {doc['size']} crc {doc['crc32']}")
    if int(doc["size"]) > OTA_MAX or int(doc["size"]) < 4096:
        raise SystemExit("manifest size out of range")
    blob, blen = check("bin", BIN_URL)
    if blen != int(doc["size"]):
        raise SystemExit(f"bin length {blen} != manifest size {doc['size']}")
    if blen > OTA_MAX:
        raise SystemExit("bin over ota max")
    print("harness ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
