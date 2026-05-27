#!/bin/bash
# Written by Carl Vargklint 2026-05-19
# Install platformio CLI then run the script from this folder.
#
# Optional: pass a name as the first argument to bake a default badge name
# into the firmware (used on first boot if no name has been set via serial).
#
#   ./flash.sh "CARL"
#
# The name is saved to EEPROM on first boot, so future reflashes without a
# name argument will still display the stored name.

if [ -n "$1" ]; then
  PLATFORMIO_BUILD_FLAGS="-DBADGE_DEFAULT_NAME=\\\"$1\\\"" pio run -t upload
else
  pio run -t upload
fi

