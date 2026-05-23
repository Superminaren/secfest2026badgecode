# RP2040 Connection List — Securityfest Badge 2026

A programmer-friendly breakdown of how the RP2040 (U2) is wired on the badge.

## 🔌 Programming & Debug

| Function | RP2040 Pin | Notes |
|---|---|---|
| **USB-C (J2)** | USB_DM (46), USB_DP (47) | Primary way to flash firmware (UF2 bootloader / picotool / PlatformIO) |
| **BOOTSEL button (SW3)** | Pulls QSPI_CS low | Hold while plugging in USB or while pressing RESET to enter UF2 bootloader |
| **RESET button (SW1)** | RUN (26) | Pulls RUN to GND through R6 (330 Ω) |
| **SWCLK test point** | SWCLK (24) | For SWD debugging (e.g. with a Picoprobe / J-Link) |
| **SWDIO test point** | SWDIO (25) | Same — these are exposed as test pads, not a header |
| **Crystal X1** | XIN (20), XOUT (21) | External clock source |

To flash: hold `BOOTSEL`, press and release `RESET`, release `BOOTSEL` → device shows up as `RPI-RP2` mass storage.

## 🎮 Buttons (all active-LOW, enable internal pull-ups)

| GPIO | Pin | Function | Schematic Net |
|---|---|---|---|
| GPIO8 | 12 | **A** button (SW2) | RP_BTN_A |
| GPIO9 | 13 | **B** button (SW4) | RP_BTN_B |
| GPIO10 | 14 | **Up** (SW5) | RP_BTN_UP |
| GPIO11 | 15 | **Down** (SW8) | RP_BTN_DWN |
| GPIO12 | 16 | **Left** (SW6) | RP_BTN_LFT |
| GPIO13 | 17 | **Right** (SW7) | RP_BTN_RGT |

All buttons short to GND when pressed — there are no external pull-ups, so configure with `gpio_pull_up()` (or equivalent in your SDK).

## 💡 LEDs

| GPIO | Pin | Function | Notes |
|---|---|---|---|
| GPIO14 | 18 | **Flashlight LEDs** (FLED1–3) | Drives MOSFET Q1 gate (FL_LED net). Set HIGH to turn on. |
| GPIO20 | 35 | Front LED 1 | Direct drive |
| GPIO21 | 36 | Front LED 2 | Direct drive |
| GPIO22 | 37 | Front LED 3 | Direct drive |
| GPIO23 / ADC0 | 38 | Front LED 4 | Direct drive (ADC capable but used as LED) |

## 🟦 LED Matrix (9×9 Charlieplex via IS31FL3731)

The 9×9 matrix is driven by the IS31FL3731 (U1). You don't talk to the LEDs directly — you talk to the chip over a **dedicated I²C bus**:

| GPIO | Pin | Function |
|---|---|---|
| GPIO4 | 4 | **SDA_LED** → IS31FL3731 SDA |
| GPIO5 | 5 | **SCL_LED** → IS31FL3731 SCL |

The IS31FL3731 has its `AD` pin tied to GND, so its I²C address is `0x74`. There are existing libraries for this chip (CircuitPython, Adafruit, etc.).

## 🧩 Main I²C bus (shared with SAO + expansion header)

| GPIO | Pin | Function |
|---|---|---|
| GPIO4 | 6 | **SDA** (4.7 kΩ pull-up via R29) |
| GPIO5 | 7 | **SCL** (4.7 kΩ pull-up via R28) |

## 🔧 SAO Connector (J3 — DEFCON Simple Add-On)

| RP2040 GPIO | SAO Pin |
|---|---|
| 3V3 | Power |
| GND | Ground |
| GPIO4 (SDA) | SDA |
| GPIO5 (SCL) | SCL |
| GPIO0 | SAO_GPIO1 |
| GPIO1 | SAO_GPIO2 |

GPIO0 and GPIO1 are also UART0 TX/RX by default — handy for a SAO that wants serial.

## 📍 Expansion Header (J1, 8-pin)

Exposes spare GPIOs and the main I²C bus:

| Pin | Net | RP2040 |
|---|---|---|
| 1 | +3V3 | — |
| 2 | GND | — |
| 3 | GP19 | GPIO19 (pin 30) |
| 4 | GP18 | GPIO18 (pin 29) |
| 5 | GP17 | GPIO17 (pin 28) |
| 6 | GP16 | GPIO16 (pin 27) |
| 7 | SCL | GPIO5 |
| 8 | SDA | GPIO4 |

GPIO16–19 are great for SPI (SPI0: SCK=GP18, TX=GP19, RX=GP16, CS=GP17) if you want to add peripherals.

## 💾 External Flash (W25Q128JVS — 16 MB)

You normally don't touch these — the bootrom and SDK handle them — but for completeness:

| Function | RP2040 Pin |
|---|---|
| QSPI_CS | 56 |
| QSPI_SCLK | 52 |
| QSPI_SD0 | 53 |
| QSPI_SD1 | 55 |
| QSPI_SD2 | 54 |
| QSPI_SD3 | 51 |

## ⚡ Power

- **+3V3** rail comes from either the TPS63020 buck-boost (battery side) or the AP2112K-3.3 LDO (USB-C side), combined through a Schottky diode.
- All RP2040 power pins (IOVDD, DVDD, VREG_VIN, ADC_AVDD, USB_VDD) tie to +3V3 with local decoupling.

## 🚫 Unused / available GPIOs

These pins aren't wired to anything on the schematic — fair game for hacks, mods, or bodges:

`GPIO6, GPIO7, GPIO8, GPIO20, GPIO21, GPIO22, GPIO27 (ADC1), GPIO28 (ADC2), GPIO29 (ADC3)`

---

## Quick programming cheat-sheet

- USB CDC serial works out of the box on the USB-C port.
- Use `gpio_pull_up()` on all 6 button pins.
- Two I²C buses available — `i2c0`/`i2c1` mapping depends on your firmware, but **GPIO2/3 = LED matrix**, **GPIO4/5 = user/SAO bus**.
- ADC0 is already taken (front LED 4); ADC1–3 are free if you want analog inputs.
