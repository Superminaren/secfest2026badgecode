# Secfest 2026 Badge — Welcome Demo

The welcome demo firmware for the Secfest 2026 badge. It runs a boot animation, scrolls a randomised welcome message, fires off some fireworks, then drops into a menu with Snake, Simon Says, a flashlight, an IR remote, and a name badge mode.

## Prerequisites

Install the [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html):

```bash
pip install platformio
```

## Flashing

Connect your badge via USB, then run from this folder:

```bash
bash flash.sh
```

To personalise the badge with your name:

```bash
bash flash.sh "YOUR NAME"
```

The name is saved to the badge's EEPROM on first boot, so you don't need to pass it again on future reflashes unless you want to change it. It will show up on the boot animation and in the Name Badge menu option.

## Changing your name later

Connect the badge over USB and open a serial monitor at 115200 baud (e.g. `pio device monitor`). The badge runs a small terminal — type `help` to see available commands including how to update the name without reflashing.

You can alternatively use Putty towards the COM port in Windows, or minicom: `minicom -D /dev/ttyUSB0` / screen `screen /dev/ttyUSB0` in Linux.
