// ================================================================
// badge_config.h  —  Secfest 2026 Badge
// ================================================================
// Shared badge configuration: EEPROM layout, types, and globals
// accessed by both main.cpp and serial_games.cpp.
//
// EEPROM map (64 bytes):
//   Byte  0    : matrix/global brightness (1-8)
//   Byte  1    : flashlight brightness (1-8)
//   Bytes 2-25 : IR codes for 6 buttons × 4 bytes each
//                Layout per button: [protocol][addr_lo][addr_hi][command]
//   Bytes 26-41: owner name (null-terminated, uppercase, 15 chars max)
//   Byte  42   : front LED animation mode (LED_ANIM_*)
//   Byte  43   : idle screensaver enabled (0=off 1=on)
//   Byte  44   : idle timeout in seconds (5-60)
//   Byte  45   : menu scroll speed ms/pixel (20-150)
//   Byte  46   : idle message scroll speed ms/pixel (10-100)
//   Byte  47   : name badge scroll speed ms/pixel (20-150)
//   Byte  48   : matrix idle animation mode (MATRIX_ANIM_*)
//   Byte  49   : IR havoc code-type bitmask (HAVOC_SEND_*)
//   Byte  50   : IR havoc send interval (×100 ms, 1-20)
// ================================================================
#pragma once
#include <Arduino.h>

// ── EEPROM ──────────────────────────────────────────────────────
#define EEPROM_CFG_SIZE          64
#define EEPROM_ADDR_BRIGHT        0
#define EEPROM_ADDR_FLASH         1
#define EEPROM_ADDR_IR_BASE       2   // 4 bytes × 6 buttons = 24 bytes
#define EEPROM_ADDR_NAME         26   // 16 bytes, null-terminated
#define NAME_MAX_LEN             16   // 15 visible chars + null terminator
#define EEPROM_ADDR_LED_ANIM     42
#define EEPROM_ADDR_IDLE_ENABLE  43
#define EEPROM_ADDR_IDLE_TIMEOUT 44
#define EEPROM_ADDR_MENU_SCROLL  45
#define EEPROM_ADDR_IDLE_SCROLL  46
#define EEPROM_ADDR_NAME_SCROLL  47
#define EEPROM_ADDR_MATRIX_ANIM  48
#define EEPROM_ADDR_HAVOC_CODES  49
#define EEPROM_ADDR_HAVOC_DELAY  50

// ── IR havoc bitmask ─────────────────────────────────────────────
#define HAVOC_SEND_POWER  0x01   // transmit power-toggle codes
#define HAVOC_SEND_INPUT  0x02   // transmit source/input codes
#define HAVOC_SEND_ALL    0x03

// ── Matrix idle animation modes ──────────────────────────────────
#define MATRIX_ANIM_SCROLL   0  // scrolling hacker messages (default)
#define MATRIX_ANIM_WAVE     1  // sine wave sweeping across the screen
#define MATRIX_ANIM_TWINKLE  2  // random pixels flaring and fading
#define MATRIX_ANIM_RAIN     3  // pixels falling from the top
#define MATRIX_ANIM_PULSE    4  // whole screen breathing in and out
#define MATRIX_ANIM_PLASMA   5  // interference pattern of three sine waves
#define MATRIX_ANIM_COUNT    6

// ── IR protocol IDs ─────────────────────────────────────────────
#define IR_PROTO_NEC      0
#define IR_PROTO_SAMSUNG  1
#define IR_PROTO_SONY     2
#define IR_PROTO_RC5      3

// ── Front LED animation modes ────────────────────────────────────
#define LED_ANIM_KNIGHT    0  // soft sweep back and forth (default)
#define LED_ANIM_PULSE     1  // all LEDs breathe together
#define LED_ANIM_STROBE    2  // fast on/off flash
#define LED_ANIM_ALTERNATE 3  // 1&3 vs 2&4 alternating
#define LED_ANIM_CHASE     4  // hard dot bouncing left-right
#define LED_ANIM_ON        5  // all constant on
#define LED_ANIM_OFF       6  // all off
#define LED_ANIM_COUNT     7

// ── IR code record (4 bytes on EEPROM) ──────────────────────────
struct IrCode {
  uint8_t  protocol;   // IR_PROTO_*
  uint8_t  addr_lo;    // address low byte
  uint8_t  addr_hi;    // address high byte
  uint8_t  command;    // command byte
};

static inline uint16_t irCodeAddr(const IrCode& c) {
  return (uint16_t)c.addr_lo | ((uint16_t)c.addr_hi << 8);
}

// ── Button indices (must match BtnIdx in main.cpp) ───────────────
// BI_A=0, BI_B=1, BI_UP=2, BI_DOWN=3, BI_LEFT=4, BI_RIGHT=5
#define IR_BTN_COUNT  6

// ── Full badge configuration ─────────────────────────────────────
struct BadgeConfig {
  uint8_t brightness;         // matrix + front LED depth, 1-8
  uint8_t flashBright;        // flashlight brightness, 1-8
  IrCode  ir[IR_BTN_COUNT];   // per-button IR codes
  char    name[NAME_MAX_LEN]; // owner name (null-terminated, uppercase)
  uint8_t ledAnim;            // LED_ANIM_* front-LED animation
  uint8_t idleEnable;         // 1 = screensaver on, 0 = off
  uint8_t idleTimeoutSec;     // seconds of inactivity before screensaver (5-60)
  uint8_t menuScrollMs;       // menu item scroll speed ms/pixel (20-150)
  uint8_t idleScrollMs;       // idle message scroll speed ms/pixel (10-100)
  uint8_t nameScrollMs;       // name-badge scroll speed ms/pixel (20-150)
  uint8_t matrixAnim;         // MATRIX_ANIM_* idle matrix animation
  uint8_t havocCodes;         // HAVOC_SEND_* bitmask — which code types to blast
  uint8_t havocDelay;         // send interval in ×100 ms (1-20 → 100 ms – 2 s)
};

// Default IR codes — Samsung TV (NEC-compatible)
// Power=0x02, Mute=0x0F, VolumeUp=0x07, VolDown=0x0B, ChUp=0x12, ChDn=0x10
static const BadgeConfig BADGE_CONFIG_DEFAULT = {
  8,   // brightness
  8,   // flashBright
  {
    { IR_PROTO_NEC, 0x07, 0x07, 0x07 }, // A     → Volume Up
    { IR_PROTO_NEC, 0x07, 0x07, 0x0B }, // B     → Volume Down
    { IR_PROTO_NEC, 0x07, 0x07, 0x12 }, // Up    → Channel Up
    { IR_PROTO_NEC, 0x07, 0x07, 0x10 }, // Down  → Channel Down
    { IR_PROTO_NEC, 0x07, 0x07, 0x0F }, // Left  → Mute
    { IR_PROTO_NEC, 0x07, 0x07, 0x02 }, // Right → Power
  },
  "",                // name
  LED_ANIM_KNIGHT,   // ledAnim
  1,                 // idleEnable
  8,                 // idleTimeoutSec
  90,                // menuScrollMs
  45,                // idleScrollMs
  70,                // nameScrollMs
  MATRIX_ANIM_SCROLL, // matrixAnim
  HAVOC_SEND_ALL,     // havocCodes  — blast both power and input by default
  5,                  // havocDelay  — 500 ms between sends
};

// ── Global config — defined in main.cpp ──────────────────────────
extern BadgeConfig g_cfg;

// ── Persistence helpers — defined in main.cpp ────────────────────
void configLoad();
void configSave();
void configSaveBrightness();
void configSaveFlash();
void configSaveIr(uint8_t btnIdx);
void configSaveName();
void configSaveSettings();  // saves ledAnim, idle*, scroll* fields
