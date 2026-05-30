# Security Fest 2026 Badge

Welcome to the official firmware repository for the **Security Fest 2026 conference badge**. The badge is a small, hackable piece of hardware you can take home, customise, and build on top of.

---

## Hardware

| Component | Details |
|---|---|
| MCU | Raspberry Pi RP2040 (dual-core ARM Cortex-M0+, 133 MHz) |
| RAM | 264 KB SRAM |
| Flash | 16 MB |
| LED matrix | 9 × 9 IS31FL3731 charlieplexed driver (I²C) |
| Front LEDs | 4 × RGB-capable front LEDs |
| Flashlight | High-power white/IR LED on GPIO 15 (PWM) |
| IR | Software-modulated 38/40 kHz IR transmitter |
| Buttons | 6 — A, B, Up, Down, Left, Right |
| SAO port | GPIO 0 (PWM sin-wave) + GPIO 1 (constant 10 % PWM) |
| Unused GPIO port | Marked with GPIO pinouts |
| USB | USB-CDC serial at 115200 baud, capable of MIDI, HID, and more. |

---

## Firmware — Welcome Demo

The `welcome_demo` folder contains the main badge firmware. See [`welcome_demo/README.md`](welcome_demo/README.md) for flashing instructions.

## Games controller mode

Hold A when connecting the badge to the PC or Hold A then quickly press RST, the badge will show up as a games controller! Press RST to go back to badge original mode!


### Boot sequence

1. **LED sweep** — the four front LEDs fan out while a growing line crosses the matrix.
2. **Welcome scroll** — a message scrolls across the 9 × 9 matrix. If your name is set, there is a chance it will greet you personally.
3. **Fireworks** — a particle-physics fireworks display runs for up to 10 seconds.
4. **Menu** — the main badge menu begins scrolling.

Press any button during steps 1–3 to skip straight to the menu.

### Menu items

| Item | Description |
|---|---|
| **SNAKE** | Classic snake on the 9 × 9 grid. A speeds up, B exits. |
| **SIMON** | Simon Says — repeat the growing LED sequence. B exits. |
| **WELCOME** | Replay the boot welcome animation. |
| **ANIMATIONS** | Live-preview and select the idle screensaver animation. Left/Right cycles, A saves, B cancels. |
| **SETTINGS** | Adjust matrix brightness. The entire display dims and brightens as you click the UP / DOWN buttons. Brightness setting is saved when B is pressed. |
| **FLASHLIGHT** | Turns on the high-power LED if installed. Up/Down adjusts brightness. B exits. |
| **IR REMOTE** | Sends configured IR codes on button press. A+B exits. |
| **IR HAVOC** | Cycles through power and source codes for 15 TV brands, blasting them over the IR LED. B exits. Configure targets and timing via the serial CLI. |
| **NAME BADGE** | Scrolls your name continuously on the matrix. B exits. |

### Idle screensaver

After a configurable period of inactivity the badge enters a screensaver. Choose from:

- **Scroll** — rotating hacker messages
- **Wave** — a sine wave sweeping across all nine rows
- **Twinkle** — random pixels flaring and fading
- **Rain** — pixels falling from the top with a fading trail
- **Pulse** — the whole matrix breathing in and out
- **Plasma** — three overlapping sine waves creating an interference pattern

### Personalising your badge

Flash with your name baked in:

```bash
cd welcome_demo
bash flash.sh "YOUR NAME"
```

Or set it at any time over the serial CLI without reflashing:

```
name CARL
```

Once set, the boot animation has a chance of greeting you by name and the **NAME BADGE** mode scrolls your name in a loop.

---

## Serial CLI

Connect at **115200 baud** (USB-CDC). On macOS/Linux:

```bash
pio device monitor          # inside the welcome_demo folder
screen /dev/tty.usbmodem*   # or directly
```

On Windows use PuTTY pointed at the badge COM port.

Type `help` inside **CONFIG** for a full command reference. Key commands:

```
name <yourname>             — set your name (max 15 chars)
bright matrix <1-8>         — matrix brightness
bright flash <1-8>          — flashlight brightness
led <mode>                  — front LED animation
idle on|off                 — enable/disable screensaver
idle timeout <5-60>         — seconds before screensaver kicks in
ir <btn> <proto> <addr> <cmd>  — configure IR remote buttons
havoc codes power|input|all — what IR HAVOC transmits
havoc delay <100-2000>      — ms between IR HAVOC sends
default                     — restore factory defaults
```

The CLI also has four text games: **Mastermind**, **Hangman** (security edition), **Minesweeper**, and **Zero Day** — a hacker text adventure.

---

## IR HAVOC

IR HAVOC cycles through power and source/input codes for 15 common TV brands (Samsung, LG, Sony, Philips, Panasonic, Toshiba, Sharp, Vizio, Hisense, TCL, JVC, Hitachi, Grundig, Funai, Beko). The matrix displays a rotating swirl animation between sends, cycling through single swirl, counter-swirl, and plasma patterns. Front LEDs flash on each transmission.

> **Note:** codes are best-effort approximations from documented IR databases. Effectiveness varies by TV model. Use responsibly.

---

## Hacking the badge

The firmware is plain Arduino/C++ built with PlatformIO. Everything lives in `welcome_demo/src/`:

| File | Contents |
|---|---|
| `main.cpp` | All display states, animations, games, IR, and the main loop |
| `badge_config.h` | EEPROM layout, config struct, IR and animation constants |
| `serial_games.cpp/h` | USB serial CLI, Mastermind, Hangman, Minesweeper, text adventure |
| `text_adventure.cpp/h` | Zero Day — the hacker text adventure engine |
| `font4x7.h` / `font3x5.h` | Pixel fonts for the matrix scroller |

Good starting points:

- **Add a welcome message** — drop a string into `WELCOME_MSGS[]` in `main.cpp`. Include `%s` anywhere to have your name substituted at runtime.
- **Add an idle animation** — write a `matrixXxxStep()` function, add a `MATRIX_ANIM_*` constant in `badge_config.h`, and wire it into `matrixAnimStep()`.
- **Add a menu item** — add a string to `MENU_ITEMS[]`, a value to the `AppState` enum, a `reset` + `step` function pair, and wire them into `enterState()` and the `loop()` switch.
- **Add an IR code** — add a row to `TV_CODES[]` in `main.cpp` with brand name, protocol, address, power command, and input command.

---

## License

Do whatever you want with it. Have fun, stay curious, and patch your stuff.

**Security Fest 2026** — [securityfest.com](https://securityfest.com)

