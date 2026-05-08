# rp2040-autoflash

Automatically flash any number of RP2040 boards the moment they're plugged in
holding BOOTSEL. Drop a `.uf2` into a folder, connect a board, and it gets
programmed — no host software, no button-mashing, no per-board commands.

Multiple boards connected at the same time are flashed in parallel, each
isolated from the others.

## How it works

1. A **udev rule** matches the RP2040's BOOTSEL USB IDs (`2e8a:0003`) when it
   appears as a block device.
2. udev hands the event off to a **systemd template service**
   (`rp2040-flash@sdX.service`) so the work runs asynchronously and udev
   doesn't time out. Each connected board gets its own service instance.
3. The service runs a **bash script** that mounts the BOOTSEL drive, copies
   the UF2 from the firmware directory, and cleans up. The RP2040 reboots
   itself the moment the UF2 finishes writing.

## Requirements

- Linux with systemd and udev (Ubuntu, Debian, Fedora, Arch, Raspberry Pi OS, etc.)
- `mount`, `vfat` filesystem support (standard on every desktop distro)
- root access for installation

## Files

| File | Destination |
|---|---|
| `99-rp2040-flash.rules` | `/etc/udev/rules.d/` |
| `rp2040-flash@.service` | `/etc/systemd/system/` |
| `flash-rp2040.sh` | `/usr/local/bin/` |

## Installation

```bash
sudo install -m 755 flash-rp2040.sh        /usr/local/bin/flash-rp2040.sh
sudo install -m 644 99-rp2040-flash.rules  /etc/udev/rules.d/
sudo install -m 644 rp2040-flash@.service  /etc/systemd/system/

sudo mkdir -p /opt/rp2040-firmware

sudo systemctl daemon-reload
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Usage

1. Place a UF2 file in `/opt/rp2040-firmware/`.
   - If a file named `firmware.uf2` exists, it is used.
   - Otherwise, the most recently modified `*.uf2` in the directory is used.
2. Connect an RP2040 while holding BOOTSEL (or with `BOOTSEL` already engaged).
3. The board flashes and reboots automatically — typically within a second
   or two of being detected.
4. Repeat with as many boards as your USB ports/hubs can supply. They flash
   independently and in parallel.

### Updating the firmware

Just replace the file:

```bash
sudo cp new-build.uf2 /opt/rp2040-firmware/firmware.uf2
```

No service restart or udev reload is needed — the script reads the directory
on every flash.

## Configuration

### Firmware directory

Edit `rp2040-flash@.service` and change:

```ini
Environment=RP2040_FIRMWARE_DIR=/opt/rp2040-firmware
```

Then reload:

```bash
sudo systemctl daemon-reload
```

### Supporting RP2350

The default rule matches `ID_MODEL_ID==0003` (RP2040). To also match RP2350,
edit `99-rp2040-flash.rules` and replace the model match with:

```
ENV{ID_MODEL_ID}=="0003|000f"
```

…or add a second rule line for `000f`.

### Avoiding desktop auto-mount races

If you run a desktop environment (GNOME, KDE, etc.) that auto-mounts
removable storage, it may grab the BOOTSEL drive before the script does.
Add this to the udev rule to suppress that:

```
ENV{UDISKS_IGNORE}="1"
```

## Monitoring

Live tail of all flash activity:

```bash
journalctl -t 'rp2040-flash*' -f
```

Or the dedicated log file:

```bash
tail -f /var/log/rp2040-flash.log
```

Status of a specific in-flight flash (replace `sdb`):

```bash
systemctl status 'rp2040-flash@sdb.service'
```

## Troubleshooting

**Nothing happens when I plug in a board.**
Check that the board enumerates as `2e8a:0003`:

```bash
lsusb | grep 2e8a
```

If the VID/PID is correct but no service starts, verify the rule loaded:

```bash
udevadm monitor --udev --subsystem-match=block
```

then plug the board in. You should see an `add` event and a
`SYSTEMD_WANTS=rp2040-flash@...` property.

**The service starts but flashing fails.**
Look at the log:

```bash
tail -n 50 /var/log/rp2040-flash.log
```

Common causes: the firmware directory is empty, the `.uf2` is corrupt, or
the desktop auto-mounter grabbed the drive first (see configuration above).

**`cp` / `sync` errors in the log.**
These are usually harmless. The RP2040 disconnects from USB the instant the
UF2 has been written, so the kernel sees an unclean unmount even though the
flash succeeded. The "Flash sequence finished" line is what matters.

**Board flashes but doesn't run my code.**
That's a firmware problem, not a flashing problem — the same UF2 dropped
manually onto the BOOTSEL drive will fail in the same way. Verify the build.

## Uninstallation

```bash
sudo rm /etc/udev/rules.d/99-rp2040-flash.rules
sudo rm /etc/systemd/system/rp2040-flash@.service
sudo rm /usr/local/bin/flash-rp2040.sh
sudo udevadm control --reload-rules
sudo systemctl daemon-reload
```


