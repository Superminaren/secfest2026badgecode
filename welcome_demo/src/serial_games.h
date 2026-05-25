// ================================================================
// serial_games.h  —  Secfest 2026 Badge
// ================================================================
// Text-based games over USB-CDC serial.
// Connect with PuTTY or Minicom; the terminal must support
// VT100 / ANSI escape codes (colour + clear-screen).
//
// Usage in main.cpp:
//   #include "serial_games.h"
//   static SerialGames serialGames;
//   // in setup():  serialGames.begin();
//   // in loop():   serialGames.update();
// ================================================================
#pragma once
#include <Arduino.h>
#include "badge_config.h"
#include "text_adventure.h"

class SerialGames {
public:
  void begin();
  void update();   // call every loop() iteration — fully non-blocking

private:
  // ── top-level state ─────────────────────────────────────
  enum class St : uint8_t { IDLE, MENU, MASTERMIND, HANGMAN, MINESWEEPER, ADVENTURE, CONFIG };
  St   _st        = St::IDLE;
  bool _prevConn  = false;

  // ── shared line-input buffer ────────────────────────────
  // lineMode=true  → buffer chars until Enter, then dispatch
  // lineMode=false → dispatch each printable char immediately
  bool    _lineMode = false;
  char    _line[32] = {};
  uint8_t _lineLen  = 0;

  void _clearLine();
  void _feedChar(char c);      // line-mode byte handler
  void _charDispatch(char c);  // char-mode byte handler

  // ── screen helpers ──────────────────────────────────────
  static void _cls();
  void        _drawMenu();

  // ── MASTERMIND ──────────────────────────────────────────
  // Guess a secret 4-digit code (each digit 1-6) in ≤10 tries.
  // Feedback: ● (bull) = right digit, right place;
  //           ○ (cow)  = right digit, wrong place.
  struct MMState {
    uint8_t secret[4];
    uint8_t guess[10][4];
    uint8_t bulls[10];
    uint8_t cows[10];
    uint8_t nGuess;
    bool    won, over;
  } _mm;

  void _mmStart();
  bool _mmOnLine();   // returns true if a full redraw is needed
  void _mmDraw();

  // ── HANGMAN ─────────────────────────────────────────────
  // Guess a security-themed word before the figure is complete
  // (≤6 wrong guesses).  Single-keystroke input — no Enter needed.
  struct HMState {
    const char* word;
    bool        guessed[26];
    uint8_t     nWrong;
    bool        won, over;
  } _hm;

  static const char* const _hmWords[];
  static const uint8_t     _hmWordCount;

  void _hmStart();
  void _hmOnChar(char c);
  void _hmDraw();

  // ── MINESWEEPER ─────────────────────────────────────────
  // 8 × 8 grid, 10 mines.  First reveal is always safe.
  // Commands:  r ROW COL  (reveal)
  //            f ROW COL  (flag / unflag)
  //            q          (quit to menu)
  static constexpr uint8_t MSR = 8;
  static constexpr uint8_t MSC = 8;
  static constexpr uint8_t MSN = 10;

  struct MSState {
    bool    mine    [MSR][MSC];
    bool    revealed[MSR][MSC];
    bool    flagged [MSR][MSC];
    uint8_t count   [MSR][MSC];
    bool    seeded, won, over;
    uint8_t nRevealed;
  } _ms;

  void _msStart();
  void _msSeed(uint8_t safeR, uint8_t safeC);
  void _msReveal(int r, int c);
  bool _msOnLine();   // returns true if a full redraw is needed
  void _msDraw();

  // ── TEXT ADVENTURE ──────────────────────────────────────
  // ZERO DAY — hacker infiltration story.
  // Full line-based input; adventure manages its own prompts.
  TextAdventure _adv;

  void _advStart();

  // ── CONFIG ──────────────────────────────────────────────
  // Line-mode CLI for editing badge_config (brightness, IR codes).
  // Commands (not case-sensitive):
  //   show                       — print current config
  //   bright matrix <1-8>        — set matrix brightness
  //   bright flash <1-8>         — set flashlight brightness
  //   ir <btn> <proto> <addr> <cmd>
  //     btn   = a b up down left right
  //     proto = nec samsung sony rc5
  //     addr  = 0x…  (16-bit hex)
  //     cmd   = 0x…  (8-bit hex)
  //   help                       — show commands
  //   q / back                   — return to main menu
  void _cfgStart();
  void _cfgOnLine();       // called from _feedChar on Enter
  void _cfgDraw();
  void _cfgShowConfig();
  static uint8_t     _cfgParseBtn(const char* s);    // BI_* or 0xFF
  static uint8_t     _cfgParseProto(const char* s);  // IR_PROTO_* or 0xFF
  static uint8_t     _cfgParseAnim(const char* s);   // LED_ANIM_* or 0xFF
  static const char* _animName(uint8_t a);
};
