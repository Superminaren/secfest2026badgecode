# rp2040-badge-flasher

Flash Secfest 2026 badges (RP2040) from a root terminal. Plug in a badge
while holding BOOTSEL — it is detected and flashed automatically. Plug in
the next one when ready. No udev rules or systemd services required.

## Quick start

```bash
# Copy your firmware into place (once)
sudo mkdir -p /opt/rp2040-firmware
sudo cp .pio/build/*/firmware.uf2 /opt/rp2040-firmware/firmware.uf2

# Run the flasher
sudo ./flash-badge.sh
```

Then just plug badges in one at a time holding BOOTSEL. Each board is
detected, flashed, and reboots itself. The script prints a running count
and waits for the next board. Press **Ctrl-C** to quit.

### Custom firmware directory

```bash
sudo ./flash-badge.sh /path/to/dir
# or
sudo RP2040_FIRMWARE_DIR=/path/to/dir ./flash-badge.sh
```

### Updating the firmware mid-session

Replace the file while the script is running — it re-reads the directory
on every flash:

```bash
sudo cp new-build.uf2 /opt/rp2040-firmware/firmware.uf2
```

## Firmware selection

- If `firmware.uf2` exists in the directory, it is always used.
- Otherwise the most recently modified `*.uf2` in the directory is used.

## Troubleshooting

**Board not detected.**  
Confirm it enumerates as `2e8a:0003`:

```bash
lsusb | grep 2e8a
```

**Mount fails.**  
A desktop environment (GNOME, KDE, etc.) may grab the BOOTSEL drive before
the script does. Disable auto-mounting for removable drives, or add a udev
rule to suppress it:

```
# /etc/udev/rules.d/99-rp2040-nomount.rules
SUBSYSTEM=="block", ENV{ID_VENDOR_ID}=="2e8a", ENV{ID_MODEL_ID}=="0003", ENV{UDISKS_IGNORE}="1"
```

Then `sudo udevadm control --reload-rules`.

**`cp` / `sync` errors.**  
Expected and harmless — the RP2040 disconnects from USB the instant the UF2
is fully received, which looks like an unclean unmount to the kernel. If the
script printed "Flash complete", the board got the firmware.

**Board flashes but doesn't run the code.**  
That is a firmware build problem, not a flashing problem. Verify the `.uf2`
by dropping it manually onto the BOOTSEL drive.

## Files

| File | Purpose |
|---|---|
| `flash-badge.sh` | Interactive terminal flasher (main script) |
| `flash-rp2040.sh` | Low-level flash helper (used by the old udev/systemd approach) |
| `99-rp2040-flash.rules` | udev rule for the old fully-automatic approach |
| `rp2040-flash@.service` | systemd service for the old fully-automatic approach |
