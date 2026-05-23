// ================================================================
// badge_config.h  —  Secfest 2026 Badge
// ================================================================
// Shared badge configuration: EEPROM layout, types, and globals
// accessed by both main.cpp and serial_games.cpp.
//
// EEPROM map (32 bytes):
//   Byte  0    : matrix/global brightness (1-8)
//   Byte  1    : flashlight brightness (1-8)
//   Bytes 2-25 : IR codes for 6 buttons × 4 bytes each
//                Layout per button: [protocol][addr_lo][addr_hi][command]
// ================================================================
#pragma once
#include <Arduino.h>

// ── EEPROM ──────────────────────────────────────────────────────
#define EEPROM_CFG_SIZE      32
#define EEPROM_ADDR_BRIGHT    0
#define EEPROM_ADDR_FLASH     1
#define EEPROM_ADDR_IR_BASE   2   // 4 bytes × 6 buttons = 24 bytes

// ── IR protocol IDs ─────────────────────────────────────────────
#define IR_PROTO_NEC      0
#define IR_PROTO_SAMSUNG  1
#define IR_PROTO_SONY     2
#define IR_PROTO_RC5      3

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
  uint8_t brightness;      // matrix + front LED depth, 1-8
  uint8_t flashBright;     // flashlight brightness, 1-8
  IrCode  ir[IR_BTN_COUNT];// per-button IR codes
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
  }
};

// ── Global config — defined in main.cpp ──────────────────────────
extern BadgeConfig g_cfg;

// ── Persistence helpers — defined in main.cpp ────────────────────
void configLoad();
void configSave();
void configSaveBrightness();
void configSaveFlash();
void configSaveIr(uint8_t btnIdx);
