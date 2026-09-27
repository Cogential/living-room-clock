#!/usr/bin/env bash
# Flash the app in chunks, retrying each chunk on its own.
#
# Why this exists: on this board a single 4 MB write reliably dies partway with
# "chip stopped responding" - at 48% compressed, 83% uncompressed - while a 4 MB
# *read* of the same flash completes at 100% every time. Reads are fine, long
# writes are not, so the problem is how long a single erase/program run lasts
# rather than the USB link.
#
# Splitting the image into sector-aligned chunks means no single esptool run has
# to survive that long, and a chunk that does fail can be retried by itself
# instead of restarting the whole image. The result is verified by reading the
# flash back and comparing, which is the operation this board is good at.
#
# usage: tools/chunkflash.sh [chunk-bytes]
set -uo pipefail

cd "$(dirname "$0")/.."

IMAGE=build/living_room_clock.bin
BASE=$((0x10000))
CHUNK=${1:-524288}          # 512 KiB, a multiple of the 4096-byte sector
RETRIES=4

[[ -f "$IMAGE" ]] || { echo "no $IMAGE - build first" >&2; exit 1; }

PORT=$(ls /dev/serial/by-id/*1a86* 2>/dev/null | head -1)
[[ -z "$PORT" ]] && PORT=$(ls /dev/serial/by-id/*Espressif* 2>/dev/null | head -1)
[[ -z "$PORT" ]] && { echo "board not found" >&2; exit 1; }

SIZE=$(stat -c%s "$IMAGE")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "--- $IMAGE: $SIZE bytes in $CHUNK-byte chunks on $(basename "$PORT")"

offset=0
while (( offset < SIZE )); do
    part="$TMP/part.bin"
    dd if="$IMAGE" of="$part" bs=1 skip="$offset" count="$CHUNK" \
       iflag=skip_bytes,count_bytes status=none
    addr=$((BASE + offset))
    got=$(stat -c%s "$part")

    ok=0
    for try in $(seq 1 $RETRIES); do
        printf -- "--- 0x%06x (+%d) try %d ... " "$addr" "$got" "$try"
        if esptool.py --chip esp32p4 -p "$PORT" -b 460800 \
             --before default_reset --after no_reset \
             write_flash --flash_mode dio --flash_size 32MB --flash_freq 80m \
             --no-compress "$addr" "$part" 2>&1 | grep -q "Wrote"; then
            echo "ok"; ok=1; break
        fi
        echo "failed"
        sleep 2
    done

    (( ok )) || { echo "chunk at $addr failed after $RETRIES tries" >&2; exit 1; }
    offset=$((offset + CHUNK))
done

echo "--- verifying"
if esptool.py --chip esp32p4 -p "$PORT" -b 460800 --after hard_reset \
     verify_flash "$BASE" "$IMAGE" 2>&1 | grep -q "verify OK"; then
    echo "--- flashed and verified"
    exit 0
fi

echo "--- WROTE, BUT VERIFY FAILED - do not trust this image" >&2
exit 1
