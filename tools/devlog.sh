#!/usr/bin/env bash
# Reset the board and capture its boot log.
#
# The console is on the native USB-JTAG port, which is part of the chip, so it
# disappears and re-enumerates across a reset. Reopening has to be retried
# rather than assumed, and the /dev/ttyACMx number can change while doing it -
# always resolve by id.
#
# Use tools/devmon.sh instead when you need to watch the board react to
# something without restarting it.
#
# usage: tools/devlog.sh [seconds] [grep-pattern]
set -uo pipefail

SECONDS_TO_CAPTURE="${1:-25}"
PATTERN="${2:-}"

exec python3 - "$SECONDS_TO_CAPTURE" "$PATTERN" <<'PY'
import glob, re, subprocess, sys, time

seconds = float(sys.argv[1])
pattern = sys.argv[2] if len(sys.argv) > 2 else ""

try:
    import serial
except ImportError:
    sys.exit("pyserial missing: source the ESP-IDF export script first")

def find_port():
    ports = glob.glob("/dev/serial/by-id/*Espressif*")
    return ports[0] if ports else None

port = find_port()
if not port:
    sys.exit("board not found")

subprocess.run(["esptool.py", "--chip", "esp32p4", "-p", port,
                "--before", "default_reset", "--after", "hard_reset", "run"],
               capture_output=True, timeout=60)

# Wait for the USB-JTAG device to come back after the reset.
s = None
deadline = time.time() + 20
while time.time() < deadline and s is None:
    p = find_port()
    if p:
        try:
            ser = serial.Serial()
            ser.port = p
            ser.baudrate = 115200
            ser.timeout = 1
            # RTS drives EN on this port: asserting it holds the chip in reset
            # and the capture comes back silent. DTR must be on or the CDC
            # console withholds output.
            ser.rts = False
            ser.dtr = True
            ser.open()
            ser.rts = False
            ser.dtr = True
            s = ser
        except Exception:
            time.sleep(0.3)
    else:
        time.sleep(0.3)

if s is None:
    sys.exit("port never came back after reset")

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
