#!/usr/bin/env bash
# Build and flash, resolving the port by id.
#
# Port choice matters. The native USB-JTAG port intermittently dies partway
# through a write with "chip stopped responding", and the bigger the image the
# more likely it is: the 7 MB SPIFFS partition failed on it every single time,
# at every baud, while the CH34x UART bridge wrote it in 62 seconds with the
# hash verified. So the CH34x is preferred for flashing when it is plugged in.
#
# The console is a separate matter - that is on the USB-JTAG port. See
# tools/devlog.sh and tools/devmon.sh.
#
# usage: tools/devflash.sh [extra idf.py build args...]
set -uo pipefail

cd "$(dirname "$0")/.."

PORT=$(ls /dev/serial/by-id/*1a86* 2>/dev/null | head -1)
if [[ -z "$PORT" ]]; then
    PORT=$(ls /dev/serial/by-id/*Espressif* 2>/dev/null | head -1)
    echo "--- CH34x not present, falling back to USB-JTAG (large writes may fail)"
fi
if [[ -z "$PORT" ]]; then
    echo "board not found" >&2
    exit 1
fi

idf.py "$@" build || exit 1

# One attempt, deliberately.
#
# Retrying makes things worse rather than better. A write that dies partway
# leaves the chip wedged, and every further attempt fails EARLIER than the last
# - an observed run went 47%, then 29%, then 18%, then 9%. The board only
# recovers on a power cycle, and the first attempt after one is the good one, so
# a retry loop spends that attempt on a chip that cannot accept it.
#
# So: fail loudly, and let the operator power cycle before trying again.
# app-flash by default: the app is ~4 MB and the SPIFFS partition another 5, and
# every extra megabyte is another chance for the write to die partway. The WAD
# only needs writing once, so set FLASH_ALL=1 for that (or to provision a board).
TARGET=app-flash
[[ "${FLASH_ALL:-0}" == "1" ]] && TARGET=flash

echo "--- $TARGET on $(basename "$PORT")"
if idf.py -p "$PORT" -b "${FLASH_BAUD:-460800}" "$TARGET" 2>&1 | tail -5 | grep -q "Hash of data verified\|Done"; then
    echo "--- flashed"
    exit 0
fi

cat >&2 <<'MSG'
--- flash FAILED.

    Use tools/chunkflash.sh - that is what this board usually needs.

    A single 4 MB write here dies partway with "chip stopped responding" (seen
    at 48% compressed, 83% uncompressed), while a 4 MB *read* of the same flash
    completes at 100% every time. So the link is fine and long writes are not.
    chunkflash.sh writes the image in 512 KB pieces, retries any piece that
    fails on its own, and verifies the whole thing by reading it back.

    Note also: with both the CH34x and USB-JTAG cables plugged in at once,
    transfers were far less reliable. One cable at a time.

    A partial write leaves the app partition unbootable, so the board will sit
    in a "No bootable app partitions" loop until a write succeeds. Only the app
    partition is touched - the SPIFFS WAD is not part of this write.
MSG
exit 1
