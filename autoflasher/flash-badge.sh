#!/usr/bin/env bash
# flash-badge.sh — Interactive RP2040 badge flasher
#
# Run as root in a terminal:
#   sudo ./flash-badge.sh [/path/to/firmware/dir]
#
# Polls for RP2040 boards in BOOTSEL mode (VID=2e8a PID=0003), mounts each
# one, copies the UF2, and waits for the next. The board reboots itself the
# moment the copy finishes.
#
# Firmware selection (same as the old autoflasher):
#   - firmware.uf2 in the firmware dir, OR
#   - most recently modified *.uf2 if firmware.uf2 is absent.
#
# Default firmware dir: /opt/rp2040-firmware
# Override:  sudo RP2040_FIRMWARE_DIR=/my/path ./flash-badge.sh
#         or sudo ./flash-badge.sh /my/path

set -uo pipefail

FIRMWARE_DIR="${1:-${RP2040_FIRMWARE_DIR:-/opt/rp2040-firmware}}"
MOUNT_POINT="/run/rp2040-flash/interactive"
POLL_INTERVAL=0.5

# ── colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; DIM='\033[2m'; NC='\033[0m'

ts()       { date '+%H:%M:%S'; }
log_info() { echo -e "$(ts) ${CYAN}◆${NC}  $*"; }
log_ok()   { echo -e "$(ts) ${GREEN}✓${NC}  $*"; }
log_warn() { echo -e "$(ts) ${YELLOW}!${NC}  $*"; }
log_err()  { echo -e "$(ts) ${RED}✗${NC}  $*"; }

# ── helpers ───────────────────────────────────────────────────────────────────

cleanup_mount() {
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        umount "$MOUNT_POINT" 2>/dev/null || true
    fi
    rmdir "$MOUNT_POINT" 2>/dev/null || true
}

pick_uf2() {
    if [[ -f "$FIRMWARE_DIR/firmware.uf2" ]]; then
        echo "$FIRMWARE_DIR/firmware.uf2"
        return
    fi
    find "$FIRMWARE_DIR" -maxdepth 1 -type f -name '*.uf2' \
        -printf '%T@ %p\n' 2>/dev/null | sort -rn | head -n1 | cut -d' ' -f2-
}

# Print all /dev/sdX (or similar) block devices that belong to an RP2040
# BOOTSEL attachment (USB VID=2e8a PID=0003) right now.
find_rp2040_devs() {
    for usbdir in /sys/bus/usb/devices/*/; do
        [[ "$(cat "$usbdir/idVendor"  2>/dev/null)" == "2e8a" ]] || continue
        [[ "$(cat "$usbdir/idProduct" 2>/dev/null)" == "0003" ]] || continue
        # Walk the USB subtree; the block device sits under .../block/<name>/
        find "$usbdir" -maxdepth 9 -type d -name 'block' 2>/dev/null | \
        while read -r bdir; do
            for entry in "$bdir"/*/; do
                local name; name=$(basename "$entry")
                [[ -b "/dev/$name" ]] && echo "/dev/$name"
            done
        done
    done
}

do_flash() {
    local dev="$1"
    mkdir -p "$MOUNT_POINT"

    log_info "Mounting $dev …"
    if ! mount -t vfat -o rw,sync,noatime,flush "$dev" "$MOUNT_POINT" 2>/dev/null; then
        log_err "Mount failed — is a desktop auto-mounter racing us? (see README)"
        rmdir "$MOUNT_POINT" 2>/dev/null || true
        return 1
    fi

    if [[ ! -f "$MOUNT_POINT/INFO_UF2.TXT" ]]; then
        log_err "$dev has no INFO_UF2.TXT — not a BOOTSEL drive, skipping"
        umount "$MOUNT_POINT" 2>/dev/null || true
        rmdir  "$MOUNT_POINT" 2>/dev/null || true
        return 1
    fi

    local uf2; uf2=$(pick_uf2)
    if [[ -z "${uf2:-}" || ! -f "$uf2" ]]; then
        log_err "No .uf2 firmware found in $FIRMWARE_DIR"
        umount "$MOUNT_POINT" 2>/dev/null || true
        rmdir  "$MOUNT_POINT" 2>/dev/null || true
        return 1
    fi

    log_info "Writing ${BOLD}$(basename "$uf2")${NC} …"
    cp "$uf2" "$MOUNT_POINT/" 2>/dev/null || true
    sync 2>/dev/null || true
    # The RP2040 disconnects from USB the instant the UF2 is fully received,
    # so cp/sync often exit with an error — that is normal and expected.

    umount "$MOUNT_POINT" 2>/dev/null || true
    rmdir  "$MOUNT_POINT" 2>/dev/null || true
    log_ok  "Flash complete — board is rebooting"
    return 0
}

# ── main ──────────────────────────────────────────────────────────────────────

if [[ $EUID -ne 0 ]]; then
    echo "error: must be run as root  (sudo $0)" >&2
    exit 1
fi

count=0
trap 'echo -e "\n${BOLD}Stopped. Flashed $count badge(s) this session.${NC}"; cleanup_mount; exit 0' INT TERM

echo -e "\n${BOLD}=== RP2040 Badge Flasher ===${NC}"
echo -e "Firmware dir : ${CYAN}$FIRMWARE_DIR${NC}"

UF2=$(pick_uf2)
if [[ -z "${UF2:-}" ]]; then
    log_err "No .uf2 found in $FIRMWARE_DIR"
    echo    "Place your build there first:"
    echo -e "  ${DIM}cp /path/to/build.uf2 $FIRMWARE_DIR/firmware.uf2${NC}"
    exit 1
fi
log_ok "Firmware     : $(basename "$UF2")"
echo -e "\nPlug in a badge ${CYAN}while holding BOOTSEL${NC} — it will be flashed automatically."
echo -e "${DIM}Ctrl-C to quit.${NC}\n"

declare -A seen=()

while true; do
    mapfile -t devs < <(find_rp2040_devs 2>/dev/null)

    for dev in "${devs[@]+"${devs[@]}"}"; do
        [[ -z "$dev" ]] && continue
        if [[ -z "${seen[$dev]+x}" ]]; then
            seen["$dev"]=1
            echo -e "${BOLD}────────────────────────────────────────${NC}"
            echo -e "${BOLD} Badge detected: $dev${NC}"
            echo -e "${BOLD}────────────────────────────────────────${NC}"
            if do_flash "$dev"; then
                count=$((count + 1))
                echo -e "    ${GREEN}Total flashed this session: $count${NC}"
            fi
            echo
        fi
    done

    # Remove stale entries so a re-inserted board gets flashed again
    for dev in "${!seen[@]}"; do
        [[ -b "$dev" ]] || unset "seen[$dev]"
    done

    sleep "$POLL_INTERVAL"
done
