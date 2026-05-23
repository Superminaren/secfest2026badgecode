// ================================================================
// text_adventure.h  —  Secfest 2026 Badge
// ================================================================
// ZERO DAY — A hacker text adventure.
//
// You are a security operative sent to extract stolen research data
// from NovaCorp HQ before the building goes into full lockdown.
// Uncover the truth, free a trapped contact, and vanish without
// a trace — or blow your cover trying.
//
// Play over USB-CDC serial (PuTTY / Minicom).
// All interaction is line-based: type a command and press Enter.
//
// Commands:  go/move DIRECTION, look, examine ITEM,
//            take/get ITEM, drop ITEM, use ITEM,
//            use ITEM on TARGET, talk NPC,
//            inventory/inv, score, help, quit
// ================================================================
#pragma once
#include <Arduino.h>

class TextAdventure {
public:
  void begin();               // reset to start state
  void update();              // call from SerialGames::update() — non-blocking
  void feedLine(const char* line);  // dispatch a completed input line

  bool isActive() const { return _active; }
  // Called when 'q'/'quit' should return to the SerialGames menu
  // — caller checks _quitToMenu and resets it after reading
  bool quitToMenu = false;

private:
  bool _active = false;

  // ── Rooms ────────────────────────────────────────────────────────
  enum Room : uint8_t {
    PARKING=0, LOBBY, STAIRWELL, BREAKROOM, OFFICE,
    SERVERCLOSET, HALL3, MEETINGROOM, MANAGERSOFFICE, SECURITYOFFICE,
    DATACENTER, ROOF, BASEMENT, POWERROOM, TUNNEL,
    STREET,          // win-condition room — not truly reachable as normal room
    ROOM_COUNT
  };

  // ── Items ────────────────────────────────────────────────────────
  enum Item : uint8_t {
    BADGE=0, LOCKPICK, MULTITOOL, STICKYNOTE, USB_DRIVE,
    KEYCARD, MASTERKEY, EXTINGUISHER, THERMOS, DATA_CHIP,
    ROPE, PHOTO,
    ITEM_COUNT,
    ITEM_NONE = 0xFF
  };

  // ── NPCs ─────────────────────────────────────────────────────────
  enum NPC : uint8_t { BOB=0, JANET, GHOST, NPC_COUNT };

  // ── Flags ────────────────────────────────────────────────────────
  enum Flag : uint8_t {
    FL_BADGE_SHOWN=0, FL_GUARD_DISTRACT, FL_CAMERAS_OFF, FL_ALARM,
    FL_MANAGER_OPEN, FL_TERMINAL_USED,  FL_GUARD_AWAKE,  FL_DATA_COPIED,
    FL_GHOST_FOUND,  FL_GHOST_FREED,    FL_ROPE_USED,    FL_POWER_CUT,
    FL_JANET_TALKED, FL_LAPTOP_OPEN,
    FLAG_COUNT
  };

  // ── State ────────────────────────────────────────────────────────
  Room    _room      = PARKING;
  uint8_t _inv[ITEM_COUNT];     // 1 = in inventory
  uint8_t _roomItem[ROOM_COUNT][ITEM_COUNT]; // items on floor per room
  bool    _flags[FLAG_COUNT];
  uint8_t _bobTalks  = 0;       // how many times player talked to Glenn
  uint8_t _janetTalks = 0;
  uint8_t _ghostTalks = 0;
  int16_t _score     = 0;
  bool    _ended     = false;

  // ── Helpers ──────────────────────────────────────────────────────
  void _printBanner();
  void _printHelp();
  void _printScore();
  void _printInventory();
  void _look();
  void _describeRoom();
  const char* _roomName(Room r);
  const char* _itemName(Item it);
  const char* _itemShort(Item it);
  const char* _npcName(NPC n);

  bool _hasItem(Item it) const  { return _inv[it]; }
  bool _roomHasItem(Item it) const { return _roomItem[_room][it]; }
  bool _flag(Flag f) const      { return _flags[f]; }
  void _setFlag(Flag f)         { if (!_flags[f]) { _flags[f] = true; } }
  void _addScore(int16_t pts)   { _score += pts; }

  Item  _parseItem(const char* word) const;
  NPC   _parseNPC(const char* word) const;
  Room  _parseDir(const char* word) const; // returns ROOM_COUNT on fail

  void _cmdGo(const char* dir);
  void _cmdLook();
  void _cmdExamine(const char* what);
  void _cmdTake(const char* what);
  void _cmdDrop(const char* what);
  void _cmdUse(const char* what, const char* on);
  void _cmdTalk(const char* who);

  void _triggerEnding();
  void _endingGhostMode();
  void _endingOperative();
  void _endingBurned();

  static void _p(const char* s);   // Serial.print wrapper
  static void _pl(const char* s);  // print + \r\n
  static void _pr();               // print \r\n
};
