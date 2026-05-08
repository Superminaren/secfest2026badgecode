#!/bin/bash
# Flash an RP2040 in BOOTSEL mode with a .uf2 from $RP2040_FIRMWARE_DIR.
# Picks firmware.uf2 if present, otherwise the most recently modified .uf2 in the dir.

set -uo pipefail

DEVICE="${1:?Usage: $0 <block-device>}"
FIRMWARE_DIR="${RP2040_FIRMWARE_DIR:-/opt/rp2040-firmware}"
MOUNT_POINT="/run/rp2040-flash/$(basename "$DEVICE")"
LOG_TAG="rp2040-flash[$(basename "$DEVICE")]"

log() {
    logger -t "$LOG_TAG" "$*"
    echo "$(date '+%F %T') $LOG_TAG $*" >> /var/log/rp2040-flash.log
}

cleanup() {
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        umount "$MOUNT_POINT" 2>/dev/null || true
    fi
    rmdir "$MOUNT_POINT" 2>/dev/null || true
}
trap cleanup EXIT

log "Triggered for $DEVICE"

# Pick a UF2: prefer firmware.uf2, else newest *.uf2 in the directory
if [[ -f "$FIRMWARE_DIR/firmware.uf2" ]]; then
    UF2_FILE="$FIRMWARE_DIR/firmware.uf2"
else
    UF2_FILE=$(find "$FIRMWARE_DIR" -maxdepth 1 -type f -name '*.uf2' \
               -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -n1 | cut -d' ' -f2-)
fi

if [[ -z "${UF2_FILE:-}" || ! -f "$UF2_FILE" ]]; then
    log "ERROR: no .uf2 file in $FIRMWARE_DIR"
    exit 1
fi
log "Using firmware: $UF2_FILE"

# Wait briefly for the block device to settle
for _ in {1..15}; do
    [[ -b "$DEVICE" ]] && break
    sleep 0.2
done
[[ -b "$DEVICE" ]] || { log "ERROR: $DEVICE not present"; exit 1; }

# Mount
mkdir -p "$MOUNT_POINT"
if ! mount -t vfat -o rw,sync,noatime,flush "$DEVICE" "$MOUNT_POINT"; then
    log "ERROR: failed to mount $DEVICE"
    exit 1
fi

# Sanity-check it's really a Pico in BOOTSEL
if [[ ! -f "$MOUNT_POINT/INFO_UF2.TXT" ]]; then
    log "ERROR: $DEVICE is not an RP2 boot drive (no INFO_UF2.TXT)"
    exit 1
fi

# Copy UF2; the device reboots itself once the write completes
log "Writing firmware..."
cp "$UF2_FILE" "$MOUNT_POINT/" 2>/dev/null
sync 2>/dev/null || true
# cp/sync may report errors because the device disconnects mid-write — that's normal.

log "Flash sequence finished for $DEVICE"
