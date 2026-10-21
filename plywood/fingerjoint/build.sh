#!/bin/sh
# Regenerate both sets (as ordered + v2), run every check, re-render v2, write SVGs / sheet layout / cutlist.
# T (sheet thickness) lives at the top of src/fj.py; the control cut-outs in src/controls.py.
set -e
cd "$(dirname "$0")"
FJ_VARIANT=ordered python3 src/fj.py >/dev/null && FJ_VARIANT=ordered python3 src/check.py | grep FAILS
FJ_VARIANT=v2 python3 src/fj.py >/dev/null && FJ_VARIANT=v2 python3 src/check.py | grep FAILS
FJ_VARIANT=v2 python3 src/export.py
FJ_VARIANT=v2 xvfb-run -a python3 src/render.py
