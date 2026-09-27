#!/usr/bin/env bash
# Read the console without resetting the board.
#
# Use this when you need to watch the device react to something happening on the
# screen; tools/devlog.sh resets first, which would throw away the state you are
# trying to observe.
#
# Modem lines matter, and differently per port:
#   - USB-JTAG: RTS drives EN. Asserting it (which is what opening the port
#     normally does) holds the chip in reset and you get silence. DTR must be
#     asserted though, or the device's CDC console withholds output.
#   - CH34x: DTR/RTS are wired to the auto-reset circuit, so touching either
#     reboots the board into download mode.
#
# usage: tools/devmon.sh [seconds] [grep-pattern]
set -uo pipefail

SECONDS_TO_CAPTURE="${1:-30}"
PATTERN="${2:-}"

exec python3 - "$SECONDS_TO_CAPTURE" "$PATTERN" <<'PY'
import glob, sys, time

seconds = float(sys.argv[1])
pattern = sys.argv[2] if len(sys.argv) > 2 else ""

try:
    import serial
except ImportError:
    sys.exit("pyserial missing: source the ESP-IDF export script first")

ports = glob.glob("/dev/serial/by-id/*Espressif*") or glob.glob("/dev/serial/by-id/*1a86*")
if not ports:
    sys.exit("board not found")

port = ports[0]
s = serial.Serial()
s.port = port
s.baudrate = 115200
s.timeout = 1
# Set before opening so the chip is never held in reset even briefly.
s.rts = False
s.dtr = True
s.open()
s.rts = False
s.dtr = True

print(f"== monitoring {port}", flush=True)

import re
rx = re.compile(pattern) if pattern else None

end = time.time() + seconds
while time.time() < end:
    line = s.readline()
    if not line:
        continue
    text = line.decode("utf-8", "replace").rstrip()
    if rx is None or rx.search(text):
        print(text, flush=True)
PY
