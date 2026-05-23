// ================================================================
// text_adventure.cpp  —  Secfest 2026 Badge
// ================================================================
// ZERO DAY — A hacker text adventure
// ================================================================
#include "text_adventure.h"

// ── VT100 helpers (local copies so this file is self-contained) ──
#define TA_RST  "\033[0m"
#define TA_BLD  "\033[1m"
#define TA_DIM  "\033[2m"
#define TA_RED  "\033[91m"
#define TA_GRN  "\033[92m"
#define TA_YLW  "\033[93m"
#define TA_BLU  "\033[94m"
#define TA_MAG  "\033[95m"
#define TA_CYN  "\033[96m"
#define TA_GRY  "\033[90m"
#define TA_CLS  "\033[2J\033[H"

// ================================================================
// Static print helpers
// ================================================================
void TextAdventure::_p(const char* s)  { Serial.print(s); }
void TextAdventure::_pl(const char* s) { Serial.print(s); Serial.print("\r\n"); }
void TextAdventure::_pr()              { Serial.print("\r\n"); }

// ================================================================
// Room / item / NPC name tables
// ================================================================
const char* TextAdventure::_roomName(Room r) {
  switch (r) {
    case PARKING:        return "Underground Parking";
    case LOBBY:          return "NovaCorp Lobby";
    case STAIRWELL:      return "Emergency Stairwell";
    case BREAKROOM:      return "2nd Floor Break Room";
    case OFFICE:         return "Open-Plan Office";
    case SERVERCLOSET:   return "Server Closet";
    case HALL3:          return "3rd Floor Corridor";
    case MEETINGROOM:    return "Glass-Walled Meeting Room";
    case MANAGERSOFFICE: return "Director's Office";
    case SECURITYOFFICE: return "Security Control Room";
    case DATACENTER:     return "Data Centre — Cold Aisle";
    case ROOF:           return "Rooftop Access";
    case BASEMENT:       return "Sub-Basement Utility Corridor";
    case POWERROOM:      return "Main Power Room";
    case TUNNEL:         return "Service Tunnel";
    case STREET:         return "Outside — You're free.";
    default:             return "???";
  }
}

const char* TextAdventure::_itemName(Item it) {
  switch (it) {
    case BADGE:        return "visitor badge";
    case LOCKPICK:     return "lockpick set";
    case MULTITOOL:    return "multitool";
    case STICKYNOTE:   return "sticky note";
    case USB_DRIVE:    return "USB drive";
    case KEYCARD:      return "keycard (level 2)";
    case MASTERKEY:    return "master keycard";
    case EXTINGUISHER: return "fire extinguisher";
    case THERMOS:      return "thermos of coffee";
    case DATA_CHIP:    return "data chip";
    case ROPE:         return "coil of rope";
    case PHOTO:        return "printed photograph";
    default:           return "unknown item";
  }
}

const char* TextAdventure::_itemShort(Item it) {
  switch (it) {
    case BADGE:        return "badge";
    case LOCKPICK:     return "lockpick";
    case MULTITOOL:    return "multitool";
    case STICKYNOTE:   return "note";
    case USB_DRIVE:    return "usb";
    case KEYCARD:      return "keycard";
    case MASTERKEY:    return "masterkey";
    case EXTINGUISHER: return "extinguisher";
    case THERMOS:      return "thermos";
    case DATA_CHIP:    return "chip";
    case ROPE:         return "rope";
    case PHOTO:        return "photo";
    default:           return "?";
  }
}

const char* TextAdventure::_npcName(NPC n) {
  switch (n) {
    case BOB:   return "Glenn";
    case JANET: return "Janet";
    case GHOST: return "Ghost";
    default:    return "???";
  }
}

// ================================================================
// Initialisation
// ================================================================
void TextAdventure::begin() {
  _room  = PARKING;
  _score = 0;
  _ended = false;
  quitToMenu = false;
  _active    = true;
  _bobTalks   = 0;
  _janetTalks = 0;
  _ghostTalks = 0;

  memset(_inv,      0, sizeof(_inv));
  memset(_roomItem, 0, sizeof(_roomItem));
  memset(_flags,    0, sizeof(_flags));

  // ── place starting items ──────────────────────────────────
  _roomItem[PARKING][BADGE]        = 1;  // near the drain pipe
  _roomItem[PARKING][LOCKPICK]     = 1;  // taped under a car
  _roomItem[LOBBY][STICKYNOTE]     = 1;  // on the reception desk
  _roomItem[BREAKROOM][THERMOS]    = 1;  // on the counter
  _roomItem[BREAKROOM][KEYCARD]    = 1;  // on the fridge door (magnet)
  _roomItem[OFFICE][USB_DRIVE]     = 1;  // in a desk drawer
  _roomItem[OFFICE][PHOTO]         = 1;  // pinned to a corkboard
  _roomItem[SERVERCLOSET][MULTITOOL]   = 1;
  _roomItem[SERVERCLOSET][DATA_CHIP]   = 1;
  _roomItem[MEETINGROOM][ROPE]         = 1;  // coiled by the window
  _roomItem[SECURITYOFFICE][MASTERKEY] = 1;  // guard's desk
  _roomItem[DATACENTER][EXTINGUISHER]  = 1;

  _printBanner();
  _look();
}

void TextAdventure::update() {
  // Nothing to poll — all logic is driven by feedLine()
}

// ================================================================
// Banner
// ================================================================
void TextAdventure::_printBanner() {
  _p(TA_CLS);
  _pl(TA_BLD TA_GRN
    "  ╔══════════════════════════════════════════════╗");
  _pl(
    "  ║          Z E R O   D A Y                    ║");
  _pl(
    "  ║     A NovaCorp Infiltration                 ║");
  _pl(
    "  ╚══════════════════════════════════════════════╝" TA_RST);
  _pr();
  _pl(TA_GRY "  You are CIPHER — freelance operative." TA_RST);
  _pl(TA_GRY "  Mission: extract stolen research data from NovaCorp HQ," TA_RST);
  _pl(TA_GRY "  and get out before the 02:00 lockdown." TA_RST);
  _pl(TA_GRY "  Someone inside may need your help too." TA_RST);
  _pr();
  _pl(TA_DIM "  Type  " TA_RST TA_BLD "help" TA_RST TA_DIM "  for commands.  " TA_RST
             TA_BLD "q" TA_RST TA_DIM " or " TA_RST TA_BLD "quit" TA_RST TA_DIM " to return to the badge menu." TA_RST);
  _pr();
}

// ================================================================
// Help
// ================================================================
void TextAdventure::_printHelp() {
  _pr();
  _pl(TA_BLD TA_CYN "  — COMMANDS ——————————————————————————————————" TA_RST);
  _pl("  " TA_BLD "go / move" TA_RST " DIRECTION   north south east west up down");
  _pl("  " TA_BLD "look" TA_RST "                 describe current room");
  _pl("  " TA_BLD "examine" TA_RST " THING         look closely at item or feature");
  _pl("  " TA_BLD "take / get" TA_RST " ITEM        pick up an item");
  _pl("  " TA_BLD "drop" TA_RST " ITEM             leave item in room");
  _pl("  " TA_BLD "use" TA_RST " ITEM              use item on its own");
  _pl("  " TA_BLD "use" TA_RST " ITEM on TARGET    use item on something");
  _pl("  " TA_BLD "talk" TA_RST " NAME             speak to an NPC");
  _pl("  " TA_BLD "inventory / inv" TA_RST "       list carried items");
  _pl("  " TA_BLD "score" TA_RST "                 current score");
  _pl("  " TA_BLD "help" TA_RST "                  this screen");
  _pl("  " TA_BLD "q / quit" TA_RST "              exit to badge menu");
  _pr();
}

// ================================================================
// Score / Inventory
// ================================================================
void TextAdventure::_printScore() {
  _pr();
  Serial.print("  Score: " TA_BLD TA_YLW);
  Serial.print(_score);
  _pl(" / 100" TA_RST);
  _pr();
}

void TextAdventure::_printInventory() {
  _pr();
  _p(TA_BLD "  Inventory: " TA_RST);
  bool any = false;
  for (uint8_t i = 0; i < ITEM_COUNT; i++) {
    if (_inv[i]) {
      if (any) _p(", ");
      _p(_itemName((Item)i));
      any = true;
    }
  }
  if (!any) _p(TA_DIM "empty" TA_RST);
  _pr(); _pr();
}

// ================================================================
// Room descriptions
// ================================================================
void TextAdventure::_look() {
  _describeRoom();
}

void TextAdventure::_describeRoom() {
  _pr();
  _p("  " TA_BLD TA_CYN);
  _p(_roomName(_room));
  _pl(TA_RST);
  _pl(TA_DIM "  ─────────────────────────────────────" TA_RST);

  switch (_room) {
    case PARKING:
      _pl("  Concrete pillars cast long shadows under stuttering fluorescent");
      _pl("  tubes. The smell of motor oil and old cigarettes. A BADGE and a");
      _pl("  LOCKPICK SET are visible near the drain pipe and a tire.");
      _pl("  Exits: " TA_BLD "north" TA_RST " → lobby.");
      break;
    case LOBBY:
      _pl("  Polished marble floors, a corporate logo gleaming on the wall.");
      if (_flag(FL_GUARD_DISTRACT)) {
        _pl("  " TA_BLD "Glenn" TA_RST " is hunched over his thermos, back to the monitors.");
        _pl("  Exits: " TA_BLD "north" TA_RST " → stairwell,  " TA_BLD "south" TA_RST " → parking.");
      } else if (_flag(FL_BADGE_SHOWN)) {
        _pl("  " TA_BLD "Glenn" TA_RST " gives you a nod. Your badge is on your chest.");
        _pl("  Exits: " TA_BLD "north" TA_RST " → stairwell,  " TA_BLD "south" TA_RST " → parking.");
      } else {
        _pl("  " TA_BLD "Glenn" TA_RST ", the security guard, watches you from behind his desk.");
        _pl("  He looks bored, but his hand rests near an alarm button.");
        _pl("  Exits: " TA_BLD "north" TA_RST " → stairwell  (Glenn blocks unescorted access).");
      }
      if (_roomHasItem(STICKYNOTE))
        _pl("  A " TA_BLD "STICKY NOTE" TA_RST " is visible on the reception desk.");
      break;
    case STAIRWELL:
      _pl("  Echoing concrete shaft, emergency lighting strips lining the walls.");
      _pl("  The stairwell smells of dust and urgency.");
      _pl("  Exits: " TA_BLD "south" TA_RST " → lobby,  " TA_BLD "up" TA_RST " → 2nd floor,  " TA_BLD "down" TA_RST " → basement.");
      break;
    case BREAKROOM:
      _pl("  A kitchenette. Half-empty coffee pot. Motivational poster.");
      _pl("  NovaCorp pens litter the table.");
      if (_roomHasItem(THERMOS)) _pl("  A " TA_BLD "THERMOS" TA_RST " sits on the counter.");
      if (_roomHasItem(KEYCARD)) _pl("  A " TA_BLD "KEYCARD" TA_RST " is stuck to the fridge with a magnet.");
      _pl("  Exits: " TA_BLD "east" TA_RST " → office,  " TA_BLD "down" TA_RST " → stairwell.");
      break;
    case OFFICE:
      _pl("  Rows of identical workstations. A few screens still glow.");
      _pl("  " TA_BLD "Janet" TA_RST ", a tired-looking analyst, types without looking up.");
      _pl("  A corkboard holds schedules and a " TA_BLD "PHOTO" TA_RST ".");
      if (_roomHasItem(USB_DRIVE)) _pl("  A " TA_BLD "USB DRIVE" TA_RST " peeks from a desk drawer.");
      if (_flag(FL_LAPTOP_OPEN)) _pl("  One laptop screen shows an unlocked session.");
      _pl("  Exits: " TA_BLD "west" TA_RST " → break room,  " TA_BLD "north" TA_RST " → server closet,  " TA_BLD "up" TA_RST " → 3rd floor hall.");
      break;
    case SERVERCLOSET:
      _pl("  Racks of blinking hardware, cable spaghetti, the roar of fans.");
      _pl("  An access terminal glows green. Someone left a MULTITOOL here,");
      _pl("  and there's a DATA CHIP on a rack shelf.");
      if (_roomHasItem(MULTITOOL)) _pl("  The " TA_BLD "MULTITOOL" TA_RST " is hooked on a cable tray.");
      if (_roomHasItem(DATA_CHIP)) _pl("  The " TA_BLD "DATA CHIP" TA_RST " sits on a rack shelf.");
      _pl("  Exits: " TA_BLD "south" TA_RST " → office.");
      break;
    case HALL3:
      _pl("  A long corridor. Doors line both sides. A security camera");
      _pl("  blinks red at the far end — watching.");
      if (_flag(FL_CAMERAS_OFF)) _pl("  The camera's red light is " TA_BLD TA_GRN "dark" TA_RST ". Blind spot.");
      _pl("  Exits: " TA_BLD "east" TA_RST " → meeting room,  " TA_BLD "west" TA_RST " → director's office,  " TA_BLD "down" TA_RST " → office.");
      break;
    case MEETINGROOM:
      _pl("  A glass-walled room visible from the corridor. Eight chairs,");
      _pl("  a whiteboard covered in diagrams you don't understand.");
      _pl("  A " TA_BLD "ROPE" TA_RST " coil leans against the exterior window frame.");
      if (_roomHasItem(ROPE)) _pl("  The " TA_BLD "ROPE" TA_RST " is coiled near the window.");
      _pl("  Exits: " TA_BLD "west" TA_RST " → 3rd floor hall.");
      break;
    case MANAGERSOFFICE:
      if (!_flag(FL_MANAGER_OPEN)) {
        _pl("  Heavy oak door. Keycard lock. This is the director's office.");
        _pl("  The lock requires a " TA_BLD "master keycard" TA_RST " or some skill with a lockpick.");
        _pl("  Exits: " TA_BLD "east" TA_RST " → 3rd floor hall.");
      } else {
        _pl("  A corner office with a panoramic view. A laptop is open on the");
        _pl("  desk, its screen dark. A safe is ajar behind the bookshelf.");
        if (_flag(FL_GHOST_FOUND)) {
          _pl("  " TA_BLD TA_MAG "Ghost" TA_RST " — your contact — is here, looking haggard but alive.");
        }
        _pl("  Exits: " TA_BLD "east" TA_RST " → 3rd floor hall.");
      }
      break;
    case SECURITYOFFICE:
      _pl("  Banks of monitors showing camera feeds. A rack of radio equipment.");
      if (!_flag(FL_GUARD_AWAKE)) {
        _pl("  A guard is slumped over the desk, " TA_BLD "asleep" TA_RST ". Lucky.");
        _pl("  A " TA_BLD "MASTER KEYCARD" TA_RST " is visible on the desk beside him.");
      } else {
        _pl("  The guard is awake and looks at you with narrowed eyes.");
      }
      _pl("  Exits: " TA_BLD "south" TA_RST " → stairwell,  " TA_BLD "east" TA_RST " → data centre.");
      break;
    case DATACENTER:
      _pl("  Sub-zero air, deafening server noise. Blue LED status lights");
      _pl("  pulse like a heartbeat. A workstation is mounted at the end");
      _pl("  of the cold aisle. A " TA_BLD "FIRE EXTINGUISHER" TA_RST " is wall-mounted.");
      if (_roomHasItem(EXTINGUISHER)) _pl("  The " TA_BLD "EXTINGUISHER" TA_RST " is mounted on the wall.");
      _pl("  Exits: " TA_BLD "west" TA_RST " → security control room,  " TA_BLD "north" TA_RST " → roof access.");
      break;
    case ROOF:
      _pl("  Night sky. City lights below. A " TA_BLD "cooling unit" TA_RST " throbs.");
      _pl("  There's a fire-escape ladder bolted to the parapet. Looking over");
      _pl("  the edge — the meeting-room windows are three floors down.");
      if (_flag(FL_ROPE_USED)) _pl("  Your " TA_BLD "rope" TA_RST " is secured to the parapet railing.");
      _pl("  Exits: " TA_BLD "down" TA_RST " → data centre.");
      if (_flag(FL_ROPE_USED) && _flag(FL_GHOST_FREED))
        _pl("  You could " TA_BLD "climb down" TA_RST " the rope with Ghost.");
      break;
    case BASEMENT:
      _pl("  Bare concrete, pipes, the hum of machinery. Emergency lighting");
      _pl("  casts everything in sickly orange. Water drips somewhere.");
      _pl("  Exits: " TA_BLD "north" TA_RST " → stairwell,  " TA_BLD "east" TA_RST " → power room.");
      break;
    case POWERROOM:
      _pl("  Massive transformer units. Warning signs in three languages.");
      _pl("  A master breaker panel dominates one wall — cutting it would");
      _pl("  kill cameras, alarms, and door locks. Also lights.");
      _pl("  Exits: " TA_BLD "west" TA_RST " → basement,  " TA_BLD "south" TA_RST " → service tunnel.");
      break;
    case TUNNEL:
      _pl("  A low concrete tunnel smelling of rain and earth. It runs");
      _pl("  under the street to a maintenance exit on the other side.");
      if (_flag(FL_POWER_CUT))
        _pl("  The tunnel door lock is dead — power is cut.");
      else
        _pl("  The far door is magnetically locked. Needs power cut or keycard.");
      _pl("  Exits: " TA_BLD "north" TA_RST " → power room.");
      if (_flag(FL_POWER_CUT) || _flag(FL_DATA_COPIED))
        _pl("  " TA_BLD "south" TA_RST " → outside (escape route open).");
      break;
    default:
      _pl("  Somewhere unfamiliar. Look around.");
      break;
  }

  // List items on floor (if not already described inline)
  // (we only auto-list items not mentioned per-room above)
  _pr();
  _p("  " TA_DIM "> " TA_RST);
}

// ================================================================
// Direction parsing → destination room
// ================================================================
TextAdventure::Room TextAdventure::_parseDir(const char* w) const {
  // Abbreviations
  bool n = (strncmp(w,"north",5)==0 || strcmp(w,"n")==0);
  bool s = (strncmp(w,"south",5)==0 || strcmp(w,"s")==0);
  bool e = (strncmp(w,"east", 4)==0 || strcmp(w,"e")==0);
  bool ww= (strncmp(w,"west", 4)==0 || strcmp(w,"w")==0);
  bool u = (strncmp(w,"up",   2)==0);
  bool d = (strncmp(w,"down", 4)==0);
  bool cl= (strcmp(w,"climb")==0);

  switch (_room) {
    case PARKING:        if (n) return LOBBY;          break;
    case LOBBY:
      if (n && _flag(FL_BADGE_SHOWN)) return STAIRWELL;
      if (s) return PARKING;
      break;
    case STAIRWELL:
      if (s) return LOBBY;
      if (u) return BREAKROOM;
      if (d) return BASEMENT;
      break;
    case BREAKROOM:
      if (e) return OFFICE;
      if (d) return STAIRWELL;
      break;
    case OFFICE:
      if (ww) return BREAKROOM;
      if (n) return SERVERCLOSET;
      if (u) return HALL3;
      break;
    case SERVERCLOSET:   if (s) return OFFICE;         break;
    case HALL3:
      if (e) return MEETINGROOM;
      if (ww && _flag(FL_MANAGER_OPEN)) return MANAGERSOFFICE;
      if (d) return OFFICE;
      break;
    case MEETINGROOM:    if (ww) return HALL3;          break;
    case MANAGERSOFFICE: if (e) return HALL3;           break;
    case SECURITYOFFICE:
      if (s) return STAIRWELL;
      if (e) return DATACENTER;
      break;
    case DATACENTER:
      if (ww) return SECURITYOFFICE;
      if (n) return ROOF;
      break;
    case ROOF:
      if (d) return DATACENTER;
      if ((cl || d || s) && _flag(FL_ROPE_USED) && _flag(FL_GHOST_FREED)) return STREET;
      break;
    case BASEMENT:
      if (n) return STAIRWELL;
      if (e) return POWERROOM;
      break;
    case POWERROOM:
      if (ww) return BASEMENT;
      if (s) return TUNNEL;
      break;
    case TUNNEL:
      if (n) return POWERROOM;
      if (s && (_flag(FL_POWER_CUT) || _hasItem(MASTERKEY))) return STREET;
      break;
    default: break;
  }
  return ROOM_COUNT; // invalid
}

// ================================================================
// Item / NPC word parsers
// ================================================================
TextAdventure::Item TextAdventure::_parseItem(const char* w) const {
  if (strstr(w,"badge"))       return BADGE;
  if (strstr(w,"lockpick"))    return LOCKPICK;
  if (strstr(w,"multi"))       return MULTITOOL;
  if (strstr(w,"note") || strstr(w,"sticky")) return STICKYNOTE;
  if (strstr(w,"usb") || strstr(w,"drive"))   return USB_DRIVE;
  if (strstr(w,"keycard") || strstr(w,"key card")) return KEYCARD;
  if (strstr(w,"master"))      return MASTERKEY;
  if (strstr(w,"extinguish") || strstr(w,"fire")) return EXTINGUISHER;
  if (strstr(w,"thermos") || strstr(w,"coffee")) return THERMOS;
  if (strstr(w,"chip") || strstr(w,"data chip")) return DATA_CHIP;
  if (strstr(w,"rope"))        return ROPE;
  if (strstr(w,"photo") || strstr(w,"photograph")) return PHOTO;
  return ITEM_NONE;
}

TextAdventure::NPC TextAdventure::_parseNPC(const char* w) const {
  if (strstr(w,"glenn")|| strstr(w,"guard"))  return BOB;
  if (strstr(w,"janet")|| strstr(w,"analyst"))return JANET;
  if (strstr(w,"ghost")|| strstr(w,"marcus")) return GHOST;
  return NPC_COUNT;
}

// ================================================================
// Command: go
// ================================================================
void TextAdventure::_cmdGo(const char* dir) {
  Room dest = _parseDir(dir);
  if (dest == ROOM_COUNT) {
    // Special cases
    if (_room == LOBBY && (strcmp(dir,"north")==0 || strcmp(dir,"n")==0)
        && !_flag(FL_BADGE_SHOWN)) {
      _pl("  " TA_RED "Glenn steps in front of you." TA_RST);
      _pl("  \"Hold on there. Visitors need to sign in first.\"");
      _pl("  You'll need to show him your badge or find another way.");
      _pr();
      _p("  " TA_DIM "> " TA_RST);
      return;
    }
    if (_room == HALL3 && (strcmp(dir,"west")==0||strcmp(dir,"w")==0)
        && !_flag(FL_MANAGER_OPEN)) {
      _pl("  " TA_RED "The door is locked." TA_RST
          " The keycard slot blinks red.");
      _pl("  You need the master keycard or a lockpick.");
      _pr();
      _p("  " TA_DIM "> " TA_RST);
      return;
    }
    if (_room == TUNNEL && (strcmp(dir,"south")==0||strcmp(dir,"s")==0)) {
      _pl("  " TA_RED "The tunnel door is magnetically locked." TA_RST);
      _pl("  Cut the power, or use the master keycard.");
      _pr();
      _p("  " TA_DIM "> " TA_RST);
      return;
    }
    _pl("  " TA_DIM "You can't go that way." TA_RST);
    _pr();
    _p("  " TA_DIM "> " TA_RST);
    return;
  }

  // Stairwell → security office: special — requires level 2 keycard
  if (_room == STAIRWELL && dest == SECURITYOFFICE) {
    if (!_hasItem(KEYCARD) && !_hasItem(MASTERKEY)) {
      _pl("  " TA_RED "A keycard reader bars the security wing." TA_RST
          " Level 2 access required.");
      _pr();
      _p("  " TA_DIM "> " TA_RST);
      return;
    }
  }

  if (dest == STREET) {
    _triggerEnding();
    return;
  }

  _room = dest;
  _look();
}

// ================================================================
// Command: look
// ================================================================
void TextAdventure::_cmdLook() { _look(); }

// ================================================================
// Command: examine
// ================================================================
void TextAdventure::_cmdExamine(const char* what) {
  // Room features first
  if (_room == LOBBY && strstr(what,"desk")) {
    _pl("  Neat stacks of visitor passes, a sign-in logbook, and a");
    _pl("  corporate phone. Nothing you need right now.");
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (_room == SERVERCLOSET && strstr(what,"terminal")) {
    if (_flag(FL_CAMERAS_OFF)) {
      _pl("  Already done — cameras are disabled.");
    } else {
      _pl("  The terminal shows a camera management UI. You know this system.");
      _pl("  " TA_GRY "  use usb on terminal" TA_RST " to deploy a loop script.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (_room == MANAGERSOFFICE && strstr(what,"safe")) {
    if (!_flag(FL_MANAGER_OPEN)) {
      _pl("  You're not inside yet."); _pr(); _p("  " TA_DIM "> " TA_RST); return;
    }
    _pl("  The safe is already ajar. Empty — someone got here first.");
    _pl("  But there's a handwritten " TA_BLD "note" TA_RST " taped inside:");
    _pl("  " TA_YLW "  'GHOST — datacenter rack B7 — they know. Stay hidden.'" TA_RST);
    if (!_flag(FL_GHOST_FOUND)) {
      _addScore(5);
      _setFlag(FL_GHOST_FOUND);
      _pl("  " TA_GRN "  (+5) You now know where Ghost is hiding." TA_RST);
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (_room == ROOF && strstr(what,"parapet")) {
    _pl("  A solid concrete railing. Good anchor point.");
    _pl("  " TA_GRY "  use rope on parapet" TA_RST " to set a rappel line.");
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (_room == POWERROOM && strstr(what,"breaker")) {
    _pl("  A huge panel — " TA_RED "DO NOT TOUCH — AUTHORISED PERSONNEL ONLY" TA_RST ".");
    _pl("  Flipping the master breaker would cut all electronic locks.");
    _pl("  " TA_GRY "  use multitool on breaker" TA_RST " to bypass the safety.");
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (_room == DATACENTER && strstr(what,"workstation")) {
    if (!_flag(FL_DATA_COPIED)) {
      _pl("  The workstation is logged in under a service account. You can see");
      _pl("  the NovaCorp research archive mounted as a network share.");
      _pl("  " TA_GRY "  use usb on workstation" TA_RST " to copy the data.");
    } else {
      _pl("  Copied. The exfiltrated files are on your USB drive.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (_room == DATACENTER && (strstr(what,"rack")||strstr(what,"b7"))) {
    if (_flag(FL_GHOST_FOUND) && !_flag(FL_GHOST_FREED)) {
      _pl("  Behind rack B7, wedged between two chassis units, is a person.");
      _pl("  " TA_MAG "Ghost" TA_RST " — Marcus Reeve, your contact. He looks dehydrated.");
      _pl("  " TA_MAG "\"Cipher…\"" TA_RST " he rasps. " TA_MAG "\"I knew you'd come. Get me out.\"" TA_RST);
      _addScore(10);
      _setFlag(FL_GHOST_FOUND);
      // Move Ghost to manager's office for story continuity:
      _pl("  He follows you cautiously out of the rack. You need a way out.");
      _setFlag(FL_GHOST_FREED);
      _addScore(15);
      _pl("  " TA_GRN "  (+25) Ghost is free." TA_RST);
    } else if (_flag(FL_GHOST_FREED)) {
      _pl("  Ghost stands nearby. He gives you a tired nod.");
    } else {
      _pl("  A dense wall of server racks. Nothing unusual at a glance.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // Item examination
  Item it = _parseItem(what);
  if (it != ITEM_NONE && (_hasItem(it) || _roomHasItem(it))) {
    switch (it) {
      case BADGE:
        _pl("  A NovaCorp visitor badge. Name reads \"C. VARGA — IT Contractor\".");
        _pl("  Genuine enough to fool a bored guard.");
        break;
      case LOCKPICK:
        _pl("  A compact set — tension wrench and three picks. You know how to use these.");
        _pl("  Works on simple pin-tumbler locks. Not electronic ones.");
        break;
      case MULTITOOL:
        _pl("  Twelve-in-one. Wire stripper, pliers, flathead — the works.");
        _pl("  Exactly what you'd need to bypass a breaker safety cover.");
        break;
      case STICKYNOTE:
        _pl("  Scrawled on the back of a visitor pass:");
        _pl("  " TA_YLW "  'wifi: NovaCorp-Guest  pw: secfest2026'" TA_RST);
        _pl("  Not directly useful, but whoever wrote this deserves a lecture.");
        break;
      case USB_DRIVE:
        _pl("  16 GB drive, blank label. Preloaded with a camera-loop script");
        _pl("  and a data-exfil tool. Your handler packed well.");
        break;
      case KEYCARD:
        _pl("  Green stripe, level 2 clearance. Access to security wing and");
        _pl("  some server rooms. Not the director's floor.");
        break;
      case MASTERKEY:
        _pl("  Black stripe, no name. Master clearance — opens every door");
        _pl("  in the building and unlocks the tunnel exit.");
        _addScore(5);
        break;
      case EXTINGUISHER:
        _pl("  Standard CO₂ unit. Could be used to create a cold-fog distraction,");
        _pl("  or just smash something. But why make noise?");
        break;
      case THERMOS:
        _pl("  Still warm. Smells like decent coffee. Stamped \"GLENN\" on the side.");
        _pl("  Glenn's thermos. He'd probably want it back.");
        break;
      case DATA_CHIP:
        _pl("  A compact flash module. Labelled " TA_RED "\"NOVACORP / PROJEKT LAZARUS\"" TA_RST ".");
        _pl("  This is partial data — the full copy needs the workstation upstairs.");
        break;
      case ROPE:
        _pl("  About 20 metres of dynamic rope. Rated to 200 kg. Someone was");
        _pl("  prepared for a window exit. Could anchor to the rooftop parapet.");
        break;
      case PHOTO:
        _pl("  A printed photo of a woman in a data-centre aisle. On the back:");
        _pl("  " TA_YLW "  'Dr. A. Reeve — DO NOT ALLOW ACCESS'" TA_RST ".");
        _pl("  Reeve — same surname as your contact Marcus. Interesting.");
        if (!_flag(FL_JANET_TALKED)) {
          _pl("  " TA_GRY "  Maybe Janet knows something about the Reeve family." TA_RST);
        }
        _addScore(3);
        break;
      default:
        _pl("  Nothing you didn't already know.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST);
    return;
  }

  _pl("  " TA_DIM "You don't see that here." TA_RST);
  _pr(); _p("  " TA_DIM "> " TA_RST);
}

// ================================================================
// Command: take
// ================================================================
void TextAdventure::_cmdTake(const char* what) {
  Item it = _parseItem(what);
  if (it == ITEM_NONE || !_roomHasItem(it)) {
    _pl("  " TA_DIM "There's no such item here." TA_RST);
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (_room == SECURITYOFFICE && it == MASTERKEY && _flag(FL_GUARD_AWAKE)) {
    _pl("  " TA_RED "The guard is awake and watching you." TA_RST
        " You can't just grab it.");
    _pl("  Distract him first.");
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  _roomItem[_room][it] = 0;
  _inv[it] = 1;
  _p("  Taken: "); _pl(_itemName(it));

  // Score milestones
  if (it == BADGE)     _addScore(5);
  if (it == KEYCARD)   _addScore(5);
  if (it == MASTERKEY) _addScore(10);
  if (it == DATA_CHIP) _addScore(5);

  _pr(); _p("  " TA_DIM "> " TA_RST);
}

// ================================================================
// Command: drop
// ================================================================
void TextAdventure::_cmdDrop(const char* what) {
  Item it = _parseItem(what);
  if (it == ITEM_NONE || !_hasItem(it)) {
    _pl("  " TA_DIM "You're not carrying that." TA_RST);
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  _inv[it] = 0;
  _roomItem[_room][it] = 1;
  _p("  Dropped: "); _pl(_itemName(it));
  _pr(); _p("  " TA_DIM "> " TA_RST);
}

// ================================================================
// Command: use ITEM [on TARGET]
// ================================================================
void TextAdventure::_cmdUse(const char* what, const char* on) {
  Item it = _parseItem(what);

  if (it == ITEM_NONE) {
    _pl("  " TA_DIM "Use what?" TA_RST);
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (!_hasItem(it)) {
    _pl("  " TA_DIM "You don't have that." TA_RST);
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── BADGE ────────────────────────────────────────────────
  if (it == BADGE) {
    if (_room == LOBBY && !_flag(FL_BADGE_SHOWN)) {
      _pl("  You clip the badge to your jacket and approach Glenn.");
      _pl("  Glenn squints, nods. " TA_GRN "\"All right. Sign in and head on up.\"" TA_RST);
      _setFlag(FL_BADGE_SHOWN);
      _addScore(10);
      _pl("  " TA_GRN "  (+10) Glenn lets you through." TA_RST);
    } else {
      _pl("  Your badge is already recognised here.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── THERMOS ─────────────────────────────────────────────
  if (it == THERMOS) {
    if (_room == LOBBY && !_flag(FL_GUARD_DISTRACT)) {
      _pl("  \"Hey Glenn — I think this is yours. Found it upstairs.\"");
      _pl("  Glenn's face lights up. " TA_GRN "\"Oh brilliant, I've been looking for that!\"" TA_RST);
      _pl("  He turns away, prying the lid open. His back is to the monitors.");
      _setFlag(FL_GUARD_DISTRACT);
      _setFlag(FL_GUARD_AWAKE); // wakes him — but distracted
      _addScore(5);
      _pl("  " TA_GRN "  (+5) Guard is distracted." TA_RST);
      _inv[THERMOS] = 0; // handed over
    } else {
      _pl("  Now's not the moment for coffee diplomacy.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── LOCKPICK ────────────────────────────────────────────
  if (it == LOCKPICK) {
    if (_room == HALL3 && !_flag(FL_MANAGER_OPEN)) {
      _pl("  You crouch at the director's door. The lock is a simple pin-");
      _pl("  tumbler behind the card reader. Two minutes of careful work…");
      _pl("  " TA_GRN "Click." TA_RST " The door swings open.");
      _setFlag(FL_MANAGER_OPEN);
      _addScore(10);
      _pl("  " TA_GRN "  (+10) Director's office unlocked." TA_RST);
    } else if (_room == MANAGERSOFFICE) {
      _pl("  The door's already open.");
    } else {
      _pl("  There's nothing to pick here.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── MASTERKEY ───────────────────────────────────────────
  if (it == MASTERKEY) {
    if (_room == HALL3 && !_flag(FL_MANAGER_OPEN)) {
      _pl("  You swipe the master keycard. The lock turns green.");
      _pl("  " TA_GRN "Access granted." TA_RST " The director's door swings open.");
      _setFlag(FL_MANAGER_OPEN);
      _addScore(8);
      _pl("  " TA_GRN "  (+8) Director's office opened." TA_RST);
    } else {
      _pl("  You swipe the card. Nothing new happens here.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── USB_DRIVE ───────────────────────────────────────────
  if (it == USB_DRIVE) {
    // use on terminal → disable cameras
    if (_room == SERVERCLOSET && (on == nullptr || strstr(on,"terminal"))) {
      if (_flag(FL_CAMERAS_OFF)) {
        _pl("  The camera loop is already running.");
      } else {
        _pl("  You slide the USB into the terminal. The camera script uploads");
        _pl("  in seconds — looping the last 10 minutes of footage across all");
        _pl("  channels. The red indicator on the hall camera goes dark.");
        _setFlag(FL_CAMERAS_OFF);
        _setFlag(FL_TERMINAL_USED);
        _addScore(15);
        _pl("  " TA_GRN "  (+15) Cameras are blind." TA_RST);
      }
      _pr(); _p("  " TA_DIM "> " TA_RST); return;
    }
    // use on workstation → copy data
    if (_room == DATACENTER && (on == nullptr || strstr(on,"workstation"))) {
      if (_flag(FL_DATA_COPIED)) {
        _pl("  The data is already copied.");
      } else {
        _pl("  You plug into the workstation. The exfil tool runs silently.");
        _pl("  Progress bar… 97%… " TA_GRN "Done." TA_RST " 4.7 GB of Projekt Lazarus.");
        _setFlag(FL_DATA_COPIED);
        _addScore(20);
        _pl("  " TA_GRN "  (+20) Data exfiltrated." TA_RST);
      }
      _pr(); _p("  " TA_DIM "> " TA_RST); return;
    }
    _pl("  " TA_DIM "Use the USB on something specific — terminal, workstation." TA_RST);
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── MULTITOOL ───────────────────────────────────────────
  if (it == MULTITOOL) {
    if (_room == POWERROOM && (on == nullptr || strstr(on,"breaker"))) {
      if (_flag(FL_POWER_CUT)) {
        _pl("  Already done. The building runs on emergency lighting only.");
      } else {
        _pl("  You pry off the safety cover and flip the master breaker.");
        _pl("  A deep " TA_RED "THUNK" TA_RST ". The building goes to emergency red lights.");
        _pl("  Electronic locks are dead. Cameras are dead. Alarms… dead.");
        _setFlag(FL_POWER_CUT);
        _addScore(10);
        _pl("  " TA_GRN "  (+10) Power cut. Everything is open." TA_RST);
      }
      _pr(); _p("  " TA_DIM "> " TA_RST); return;
    }
    _pl("  " TA_DIM "Use the multitool on what?" TA_RST
        " (try: " TA_BLD "use multitool on breaker" TA_RST ")");
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── ROPE ────────────────────────────────────────────────
  if (it == ROPE) {
    if (_room == ROOF && (on == nullptr || strstr(on,"parapet"))) {
      if (_flag(FL_ROPE_USED)) {
        _pl("  The rope is already secured.");
      } else {
        _pl("  You tie off the rope to the parapet railing with a figure-eight.");
        _pl("  It hangs down three floors — window level. Clean exit route.");
        _setFlag(FL_ROPE_USED);
        _addScore(5);
        _pl("  " TA_GRN "  (+5) Rappel line set." TA_RST);
      }
      _pr(); _p("  " TA_DIM "> " TA_RST); return;
    }
    _pl("  " TA_DIM "Anchor the rope somewhere — try: " TA_BLD "use rope on parapet" TA_RST TA_DIM " (on the roof)." TA_RST);
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── EXTINGUISHER ────────────────────────────────────────
  if (it == EXTINGUISHER) {
    if (_room == SECURITYOFFICE && _flag(FL_GUARD_AWAKE) && !_flag(FL_GUARD_DISTRACT)) {
      _pl("  You blast CO₂ at the guard. He stumbles back, blind and coughing.");
      _pl("  You have maybe 30 seconds. " TA_YLW "Grab the keycard and move." TA_RST);
      _setFlag(FL_GUARD_DISTRACT);
      _addScore(3);
      _pl("  " TA_GRN "  (+3) Guard is temporarily incapacitated." TA_RST);
    } else {
      _pl("  That would just make a mess — and noise.");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── PHOTO ───────────────────────────────────────────────
  if (it == PHOTO) {
    if (_room == OFFICE) {
      _cmdTalk("janet"); // show photo → talk event
      return;
    }
    _pl("  " TA_DIM "This isn't the right moment for this." TA_RST);
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  _pl("  " TA_DIM "Nothing happens." TA_RST);
  _pr(); _p("  " TA_DIM "> " TA_RST);
}

// ================================================================
// Command: talk
// ================================================================
void TextAdventure::_cmdTalk(const char* who) {
  NPC n = _parseNPC(who);
  if (n == NPC_COUNT) {
    _pl("  " TA_DIM "There's nobody here by that name." TA_RST);
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── BOB ─────────────────────────────────────────────────
  if (n == BOB) {
    if (_room != LOBBY) {
      _pl("  " TA_DIM "Glenn isn't here." TA_RST);
      _pr(); _p("  " TA_DIM "> " TA_RST); return;
    }
    _bobTalks++;
    if (_bobTalks == 1) {
      _pl("  " TA_GRN "Glenn:" TA_RST " \"Evening. You here for the maintenance window?\"");
      _pl("  " TA_GRY "  He seems tired. Hasn't had his coffee." TA_RST);
    } else if (_bobTalks == 2) {
      _pl("  " TA_GRN "Glenn:" TA_RST " \"You know, I've been on since noon. Still no sign of");
      _pl("  my thermos. Swear someone nicked it from the break room.\"");
      _pl("  " TA_GRY "  Interesting. You wonder if the thermos you saw upstairs is his." TA_RST);
    } else {
      _pl("  " TA_GRN "Glenn:" TA_RST " \"I'm going to need you to sign in or show me something.");
      _pl("  Building policy, mate. Sorry.\"");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── JANET ───────────────────────────────────────────────
  if (n == JANET) {
    if (_room != OFFICE) {
      _pl("  " TA_DIM "Janet isn't here." TA_RST);
      _pr(); _p("  " TA_DIM "> " TA_RST); return;
    }
    _janetTalks++;
    if (_janetTalks == 1) {
      _pl("  Janet looks up briefly.");
      _pl("  " TA_YLW "Janet:" TA_RST " \"Can I help you? I'm kind of in the middle of something.\"");
      _pl("  She glances at the photo on the corkboard nervously.");
    } else if (_janetTalks == 2 && _hasItem(PHOTO)) {
      _pl("  You show her the photograph of the woman labelled 'DO NOT ALLOW ACCESS'.");
      _pl("  Janet's face pales.");
      _pl("  " TA_YLW "Janet:" TA_RST " \"Where did you get that? That's Dr. Reeve. She was");
      _pl("  the lead researcher on Lazarus. They said she quit. But Marcus —");
      _pl("  her brother — he didn't believe it. He came looking…\"");
      _pl("  She swallows. " TA_YLW "\"The director has him locked somewhere. Be careful.\"" TA_RST);
      _setFlag(FL_JANET_TALKED);
      _setFlag(FL_GHOST_FOUND); // now player knows Ghost's identity
      _addScore(8);
      _pl("  " TA_GRN "  (+8) Critical intel on Ghost's location and identity." TA_RST);
    } else if (_janetTalks >= 3) {
      _pl("  " TA_YLW "Janet:" TA_RST " \"Please — just get Marcus out. And burn that data.\"");
      _pl("  " TA_GRY "  Or copy it first. Same difference, right?" TA_RST);
    } else {
      _pl("  " TA_YLW "Janet:" TA_RST " \"I already said too much. Just go.\"");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }

  // ── GHOST ───────────────────────────────────────────────
  if (n == GHOST) {
    if (!_flag(FL_GHOST_FREED)) {
      if (_flag(FL_GHOST_FOUND)) {
        _pl("  You know Ghost is in the data centre, but haven't reached him yet.");
      } else {
        _pl("  " TA_DIM "You don't know where Ghost is." TA_RST);
      }
      _pr(); _p("  " TA_DIM "> " TA_RST); return;
    }
    _ghostTalks++;
    if (_ghostTalks == 1) {
      _pl("  " TA_MAG "Ghost:" TA_RST " \"Cipher. God, I thought I was done. They have everything.");
      _pl("  Projekt Lazarus is a kill-switch implant. Distributed via firmware");
      _pl("  update. NovaCorp sells it to governments. My sister found out—\"");
      _pl("  He stops himself. " TA_MAG "\"We need to get the data out. And we need to leave.\"" TA_RST);
    } else if (_ghostTalks == 2) {
      _pl("  " TA_MAG "Ghost:" TA_RST " \"The roof — there's a rappel point on the parapet.");
      _pl("  I used to do a lot of urban exploration. If you have rope, we can");
      _pl("  drop to the alley and reach the service tunnel from there.\"");
    } else {
      _pl("  " TA_MAG "Ghost:" TA_RST " \"Stop chatting and get us out of here.\"");
    }
    _pr(); _p("  " TA_DIM "> " TA_RST); return;
  }
}

// ================================================================
// Endings
// ================================================================
void TextAdventure::_triggerEnding() {
  _ended = true;
  bool hasData  = _flag(FL_DATA_COPIED) || _hasItem(DATA_CHIP);
  bool savedGhost = _flag(FL_GHOST_FREED);
  bool stealthy = !_flag(FL_ALARM) && _flag(FL_CAMERAS_OFF);

  if (savedGhost && hasData && stealthy) {
    _addScore(20);
    _endingGhostMode();
  } else if (hasData) {
    _addScore(5);
    _endingOperative();
  } else {
    _endingBurned();
  }
}

void TextAdventure::_endingGhostMode() {
  _p(TA_CLS);
  _pl(TA_BLD TA_GRN
    "  ╔══════════════════════════════════════════════╗");
  _pl(
    "  ║      ENDING: G H O S T   M O D E           ║");
  _pl(
    "  ╚══════════════════════════════════════════════╝" TA_RST);
  _pr();
  _pl("  You and Ghost drop three floors on the rope, land in the alley");
  _pl("  without a sound. The service tunnel ahead is dark and silent.");
  _pl("  Four minutes later you're breathing city air, NovaCorp's towers");
  _pl("  receding behind you.");
  _pr();
  _pl("  Ghost uploads a fragment to a journalist's dead-drop before you");
  _pl("  even reach the safe house. The data chip in your pocket holds");
  _pl("  the rest. Projekt Lazarus will never ship.");
  _pr();
  _pl("  NovaCorp reviews their camera logs — and finds ten minutes of");
  _pl("  perfect, undisturbed footage. You were never there.");
  _pr();
  _pl(TA_BLD TA_GRN "  Perfect run. No trace. Mission complete." TA_RST);
  _printScore();
  _pl(TA_DIM "  Press Enter or type 'q' to return to the badge menu." TA_RST);
  _pr(); _p("  " TA_DIM "> " TA_RST);
}

void TextAdventure::_endingOperative() {
  _p(TA_CLS);
  _pl(TA_BLD TA_YLW
    "  ╔══════════════════════════════════════════════╗");
  _pl(
    "  ║      ENDING: O P E R A T I V E             ║");
  _pl(
    "  ╚══════════════════════════════════════════════╝" TA_RST);
  _pr();
  _pl("  You slip through the tunnel exit alone, USB drive warm in your");
  _pl("  pocket. The data is real — Projekt Lazarus, names, contracts,");
  _pl("  everything your handler needs.");
  _pr();
  _pl("  Behind you, somewhere on the third floor, Ghost is still waiting.");
  _pl("  You made a call. Operatives always make calls.");
  _pr();
  _pl("  The story breaks in forty-eight hours. NovaCorp denies everything.");
  _pl("  Three executives resign within a week. Your handler says 'well done'.");
  _pl("  Ghost is eventually released. He doesn't return your messages.");
  _pr();
  _pl(TA_BLD TA_YLW "  Data extracted. Objective complete. Some regrets." TA_RST);
  _printScore();
  _pl(TA_DIM "  Press Enter or type 'q' to return to the badge menu." TA_RST);
  _pr(); _p("  " TA_DIM "> " TA_RST);
}

void TextAdventure::_endingBurned() {
  _p(TA_CLS);
  _pl(TA_BLD TA_RED
    "  ╔══════════════════════════════════════════════╗");
  _pl(
    "  ║      ENDING: B U R N E D                   ║");
  _pl(
    "  ╚══════════════════════════════════════════════╝" TA_RST);
  _pr();
  _pl("  You stumble out the tunnel exit empty-handed. No data. No Ghost.");
  _pl("  Just adrenaline and the growing certainty that the camera footage");
  _pl("  was NOT looped, and someone has your face.");
  _pr();
  _pl("  Your handler's line goes to voicemail. The safe-house is dark.");
  _pl("  A black SUV idles at the corner of the street.");
  _pl("  You walk past it without making eye contact and keep walking.");
  _pr();
  _pl("  Projekt Lazarus ships six months later. It becomes a best-seller.");
  _pr();
  _pl(TA_BLD TA_RED "  Mission failed. You got out, barely. Start over?" TA_RST);
  _printScore();
  _pl(TA_DIM "  Press Enter or type 'q' to return to the badge menu." TA_RST);
  _pr(); _p("  " TA_DIM "> " TA_RST);
}

// ================================================================
// Main line dispatcher
// ================================================================
void TextAdventure::feedLine(const char* raw) {
  if (_ended) {
    // Any input after ending → quit
    quitToMenu = true;
    _active = false;
    return;
  }

  // Lower-case copy
  char buf[64];
  uint8_t len = 0;
  while (raw[len] && len < 63) { buf[len] = raw[len]; len++; }
  buf[len] = '\0';
  for (uint8_t i = 0; i < len; i++) {
    if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] += 32;
  }

  // Trim leading spaces
  char* line = buf;
  while (*line == ' ') line++;
  if (*line == '\0') { _p("  " TA_DIM "> " TA_RST); return; }

  // Tokenise verb + rest
  char verb[16] = {0};
  char rest[48] = {0};
  uint8_t vi = 0;
  uint8_t ri = 0;
  bool inVerb = true;
  for (uint8_t i = 0; line[i] && i < 63; i++) {
    if (inVerb) {
      if (line[i] == ' ') { inVerb = false; }
      else if (vi < 15) { verb[vi++] = line[i]; }
    } else {
      if (ri < 47) { rest[ri++] = line[i]; }
    }
  }

  // Parse "on" separator in rest → item + target
  char item_part[32] = {0};
  char on_part[32]   = {0};
  const char* onSep = strstr(rest, " on ");
  if (onSep) {
    uint8_t il = (uint8_t)(onSep - rest);
    if (il > 31) il = 31;
    strncpy(item_part, rest, il);
    strncpy(on_part, onSep + 4, 31);
  } else {
    strncpy(item_part, rest, 31);
  }

  // Dispatch
  if (strcmp(verb,"q")==0 || strcmp(verb,"quit")==0) {
    _pl("  You melt into the city. Another day, another op.");
    quitToMenu = true;
    _active = false;
    return;
  }
  if (strcmp(verb,"help")==0 || strcmp(verb,"?")==0) {
    _printHelp(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (strcmp(verb,"score")==0) {
    _printScore(); _p("  " TA_DIM "> " TA_RST); return;
  }
  if (strcmp(verb,"inventory")==0 || strcmp(verb,"inv")==0 || strcmp(verb,"i")==0) {
    _printInventory(); return;
  }
  if (strcmp(verb,"look")==0 || strcmp(verb,"l")==0) {
    _cmdLook(); return;
  }
  if (strcmp(verb,"go")==0 || strcmp(verb,"move")==0 || strcmp(verb,"walk")==0) {
    _cmdGo(rest); return;
  }
  // bare direction words
  if (strcmp(verb,"north")==0||strcmp(verb,"n")==0||
      strcmp(verb,"south")==0||strcmp(verb,"s")==0||
      strcmp(verb,"east")==0 ||strcmp(verb,"e")==0||
      strcmp(verb,"west")==0 ||strcmp(verb,"w")==0||
      strcmp(verb,"up")==0   ||strcmp(verb,"down")==0||
      strcmp(verb,"climb")==0) {
    _cmdGo(verb); return;
  }
  if (strcmp(verb,"examine")==0||strcmp(verb,"x")==0||
      strcmp(verb,"look")==0||strcmp(verb,"inspect")==0||
      strcmp(verb,"read")==0) {
    _cmdExamine(rest[0] ? rest : verb); return;
  }
  if (strcmp(verb,"take")==0||strcmp(verb,"get")==0||strcmp(verb,"pick")==0) {
    _cmdTake(rest); return;
  }
  if (strcmp(verb,"drop")==0||strcmp(verb,"leave")==0||strcmp(verb,"put")==0) {
    _cmdDrop(rest); return;
  }
  if (strcmp(verb,"use")==0||strcmp(verb,"apply")==0||strcmp(verb,"insert")==0) {
    _cmdUse(item_part[0] ? item_part : rest,
            on_part[0]   ? on_part   : nullptr);
    return;
  }
  if (strcmp(verb,"talk")==0||strcmp(verb,"speak")==0||
      strcmp(verb,"ask")==0 ||strcmp(verb,"chat")==0) {
    _cmdTalk(rest); return;
  }
  // NPC names as verbs
  if (strcmp(verb,"glenn")==0) { _cmdTalk("glenn"); return; }
  if (strcmp(verb,"janet")==0) { _cmdTalk("janet"); return; }
  if (strcmp(verb,"ghost")==0) { _cmdTalk("ghost"); return; }

  _pl("  " TA_DIM "Unrecognised command. Type " TA_RST TA_BLD "help" TA_RST TA_DIM " for commands." TA_RST);
  _pr(); _p("  " TA_DIM "> " TA_RST);
}
