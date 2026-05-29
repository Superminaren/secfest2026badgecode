// ================================================================
// serial_games.cpp  —  Secfest 2026 Badge
// ================================================================
#include "serial_games.h"

// ── VT100 / ANSI helpers ─────────────────────────────────────────
#define CLS "\033[2J\033[H"   // clear screen + home cursor
#define RST "\033[0m"         // reset all attributes
#define BLD "\033[1m"         // bold
#define DIM "\033[2m"         // dim
#define RED "\033[91m"        // bright red
#define GRN "\033[92m"        // bright green
#define YLW "\033[93m"        // bright yellow
#define BLU "\033[94m"        // bright blue
#define MAG "\033[95m"        // bright magenta
#define CYN "\033[96m"        // bright cyan
#define GRY "\033[90m"        // dark grey

// ── Hangman word list (security / hacking themed) ───────────────
const char* const SerialGames::_hmWords[] = {
  "firewall",    "exploit",     "malware",    "phishing",   "rootkit",
  "payload",     "sandbox",     "fuzzing",    "honeypot",   "cipher",
  "botnet",      "spoofing",    "backdoor",   "shellcode",  "keylogger",
  "pivoting",    "injection",   "sniffing",   "ransomware", "escalation",
  "obfuscation", "enumeration", "stealthy",   "exfiltrate", "wireshark",
  "shodan",      "zerotrust",   "darknet",    "privilege",  "pentest",
};
const uint8_t SerialGames::_hmWordCount =
    sizeof(SerialGames::_hmWords) / sizeof(SerialGames::_hmWords[0]);

// Gallows rows [stage 0..6][row 0..6] — stage = number of wrong guesses
static const char* const GALLOWS[7][7] = {
  {"  +---+", "  |   |", "      |", "      |", "      |", "      |", "========="},
  {"  +---+", "  |   |", "  O   |", "      |", "      |", "      |", "========="},
  {"  +---+", "  |   |", "  O   |", "  |   |", "      |", "      |", "========="},
  {"  +---+", "  |   |", "  O   |", " /|   |", "      |", "      |", "========="},
  {"  +---+", "  |   |", "  O   |", " /|\\  |", "      |", "      |", "========="},
  {"  +---+", "  |   |", "  O   |", " /|\\  |", " /    |", "      |", "========="},
  {"  +---+", "  |   |", "  O   |", " /|\\  |", " / \\  |", "      |", "========="},
};

// ================================================================
// Public API
// ================================================================

void SerialGames::begin() {
  _st       = St::IDLE;
  _prevConn = false;
  _clearLine();
}

void SerialGames::update() {
  bool conn = (bool)Serial;

  // ── detect connect / disconnect ──────────────────────────
  if (conn && !_prevConn) {
    delay(200);           // let the terminal finish its handshake
    _drawMenu();
    _st       = St::MENU;
    _lineMode = false;
  }
  if (!conn) {
    _prevConn = false;
    _st       = St::IDLE;
    return;
  }
  _prevConn = conn;
  if (_st == St::IDLE) return;

  // ── drain available bytes ────────────────────────────────
  while (Serial.available()) {
    char c = (char)Serial.read();

    // Swallow VT100 escape sequences (arrow keys etc.)
    if (c == '\033') {
      unsigned long t = millis();
      while (Serial.available() && (millis() - t) < 25) Serial.read();
      continue;
    }

    if (_lineMode) {
      _feedChar(c);
    } else {
      if (c >= 'A' && c <= 'Z') c += 32;  // normalise to lower-case
      _charDispatch(c);
    }
  }
}

// ================================================================
// Input helpers
// ================================================================

void SerialGames::_clearLine() {
  memset(_line, 0, sizeof(_line));
  _lineLen = 0;
}

// Called for every byte in line-mode.  Echoes characters, handles
// backspace, and dispatches to the relevant game on Enter.
void SerialGames::_feedChar(char c) {
  if (c == '\r' || c == '\n') {
    // Ignore the bare \n of a \r\n pair
    if (c == '\n' && _lineLen == 0) return;

    Serial.print("\r\n");
    _line[_lineLen] = '\0';

    St prevSt = _st;
    bool redraw = false;

    if      (_st == St::MASTERMIND)  redraw = _mmOnLine();
    else if (_st == St::MINESWEEPER) redraw = _msOnLine();
    else if (_st == St::ADVENTURE) {
      _adv.feedLine(_line);
      if (_adv.quitToMenu) {
        _adv.quitToMenu = false;
        _st = St::MENU;
        _lineMode = false;
        _drawMenu();
      }
    }
    else if (_st == St::CONFIG) {
      _cfgOnLine();
    }

    _clearLine();

    // Only redraw if still in the same state (handlers that exit to menu
    // have already redrawn the menu themselves)
    if (_st == prevSt && redraw) {
      if (_st == St::MASTERMIND)  _mmDraw();
      if (_st == St::MINESWEEPER) _msDraw();
    }

  } else if ((c == '\b' || c == 127) && _lineLen > 0) {
    _lineLen--;
    Serial.print("\b \b");    // erase the echoed character

  } else if (c >= 32 && _lineLen < 31) {
    _line[_lineLen++] = c;
    Serial.print(c);           // local echo
  }
}

// Called for every byte in char-mode (Menu, Hangman).
void SerialGames::_charDispatch(char c) {
  switch (_st) {
    case St::MENU:
      if      (c == '1') _mmStart();
      else if (c == '2') _hmStart();
      else if (c == '3') _msStart();
      else if (c == '4') _advStart();
      else if (c == '5') _cfgStart();
      break;

    case St::HANGMAN:
      if (c == 'q')                  { _drawMenu(); _st = St::MENU; }
      else if (c >= 'a' && c <= 'z')   _hmOnChar(c);
      break;

    default: break;
  }
}

// ================================================================
// Screen helpers
// ================================================================

void SerialGames::_cls() { Serial.print(CLS); }

void SerialGames::_drawMenu() {
  _cls();
  Serial.print(
    BLD CYN
    "  ╔══════════════════════════════════════╗\r\n"
    "  ║      SECFEST 2026  BADGE GAMES       ║\r\n"
    "  ╚══════════════════════════════════════╝\r\n"
    RST "\r\n"
    "  " BLD "[1]" RST "  MASTERMIND    — Crack the 4-digit code\r\n"
    "  " BLD "[2]" RST "  HANGMAN       — Security vocabulary edition\r\n"
    "  " BLD "[3]" RST "  MINESWEEPER   — 8 × 8 grid, 10 mines\r\n"
    "  " BLD "[4]" RST "  ZERO DAY      — Text adventure\r\n"
    "  " BLD "[5]" RST "  CONFIG        — Badge settings & IR codes\r\n"
    "\r\n"
    GRY "  Press 1–5 to start.  Q inside any game returns here.\r\n" RST
  );
}

// ================================================================
// MASTERMIND
// ================================================================

void SerialGames::_mmStart() {
  for (uint8_t i = 0; i < 4; i++) _mm.secret[i] = (uint8_t)random(1, 7);
  _mm.nGuess = 0;
  _mm.won    = false;
  _mm.over   = false;
  _st        = St::MASTERMIND;
  _lineMode  = true;
  _clearLine();
  _mmDraw();
}

void SerialGames::_mmDraw() {
  _cls();
  Serial.print(
    BLD YLW
    "  ╔══════════════════════════════════════╗\r\n"
    "  ║           CRACK  THE  CODE           ║\r\n"
    "  ║   Guess the 4-digit code (1 – 6)     ║\r\n"
    "  ╚══════════════════════════════════════╝\r\n"
    RST "\r\n"
    "   #   GUESS      " BLD GRN "●" RST " bulls   " BLD CYN "○" RST " cows\r\n"
    GRY "  ──────────────────────────────────────\r\n" RST
  );

  for (uint8_t i = 0; i < 10; i++) {
    if (i < _mm.nGuess) {
      Serial.printf("  %2u   ", i + 1);
      for (uint8_t j = 0; j < 4; j++) Serial.printf("%u ", _mm.guess[i][j]);
      Serial.printf("        " BLD GRN "%u" RST "         " BLD CYN "%u" RST "\r\n",
                    _mm.bulls[i], _mm.cows[i]);
    } else {
      Serial.print(GRY "   .   _ _ _ _\r\n" RST);
    }
  }
  Serial.print("\r\n");

  if (_mm.won) {
    Serial.printf(BLD GRN "  ✓  Cracked in %u guess%s!  Well played.\r\n" RST,
                  _mm.nGuess, _mm.nGuess == 1 ? "" : "es");
    Serial.print(GRY "  Type Q + Enter to return to the menu.\r\n" RST);
    return;
  }
  if (_mm.over) {
    Serial.printf(BLD RED "  ✗  Out of guesses.  Code was:  %u %u %u %u\r\n" RST,
                  _mm.secret[0], _mm.secret[1], _mm.secret[2], _mm.secret[3]);
    Serial.print(GRY "  Type Q + Enter to return to the menu.\r\n" RST);
    return;
  }

  Serial.printf("  Attempts remaining: " BLD "%u" RST "\r\n\r\n", 10 - _mm.nGuess);
  Serial.print("  Enter 4 digits (1-6), e.g. 1234.  Q = quit: ");
}

// Returns true if the screen needs a full redraw.
bool SerialGames::_mmOnLine() {
  // Quit
  if (_line[0] == 'q' || _line[0] == 'Q') {
    _drawMenu();
    _st       = St::MENU;
    _lineMode = false;
    return false;
  }

  // Allow new game when already over
  if (_mm.won || _mm.over) return false;

  // Collect exactly 4 digits 1-6, ignoring spaces and other noise
  uint8_t digits[4], n = 0;
  for (uint8_t i = 0; _line[i] && n < 4; i++) {
    if (_line[i] >= '1' && _line[i] <= '6') digits[n++] = (uint8_t)(_line[i] - '0');
  }
  if (n < 4) {
    Serial.print(RED "  ✗  Need exactly 4 digits in range 1-6 (e.g. 1234).\r\n" RST);
    Serial.print("  Enter 4 digits (1-6), e.g. 1234.  Q = quit: ");
    return false;   // bad input — don't redraw; we already printed a message
  }

  uint8_t r = _mm.nGuess;
  for (uint8_t i = 0; i < 4; i++) _mm.guess[r][i] = digits[i];

  // Score: bulls (right digit + right position), cows (right digit, wrong position)
  uint8_t bulls = 0, cows = 0;
  uint8_t sc[7] = {}, gc[7] = {};
  for (uint8_t i = 0; i < 4; i++) {
    if (_mm.secret[i] == digits[i]) {
      bulls++;
    } else {
      sc[_mm.secret[i]]++;
      gc[digits[i]]++;
    }
  }
  for (uint8_t d = 1; d <= 6; d++) cows += min(sc[d], gc[d]);

  _mm.bulls[r] = bulls;
  _mm.cows[r]  = cows;
  _mm.nGuess++;

  if (bulls == 4)           _mm.won  = true;
  else if (_mm.nGuess >= 10) _mm.over = true;

  return true;   // valid input processed — caller will redraw
}

// ================================================================
// HANGMAN
// ================================================================

void SerialGames::_hmStart() {
  _hm.word   = _hmWords[random(_hmWordCount)];
  _hm.nWrong = 0;
  _hm.won    = false;
  _hm.over   = false;
  memset(_hm.guessed, 0, sizeof(_hm.guessed));
  _st       = St::HANGMAN;
  _lineMode = false;
  _hmDraw();
}

void SerialGames::_hmDraw() {
  _cls();
  Serial.print(
    BLD MAG
    "  ╔══════════════════════════════════════╗\r\n"
    "  ║      HANGMAN  —  Security Edition    ║\r\n"
    "  ╚══════════════════════════════════════╝\r\n"
    RST "\r\n"
  );

  // Gallows — figure turns red at maximum wrong guesses
  uint8_t stage = (_hm.nWrong > 6) ? 6 : _hm.nWrong;
  for (uint8_t row = 0; row < 7; row++) {
    bool isBody = (row >= 2 && row <= 5);
    const char* col = (isBody && _hm.nWrong >= 6) ? RED : GRY;
    Serial.printf("    %s%s" RST "\r\n", col, GALLOWS[stage][row]);
  }
  Serial.print("\r\n");

  // Word — revealed letters in green, blanks in white
  Serial.print("  ");
  uint8_t missing = 0;
  for (uint8_t i = 0; _hm.word[i]; i++) {
    uint8_t idx = (uint8_t)(_hm.word[i] - 'a');
    if (_hm.guessed[idx]) {
      Serial.printf(BLD GRN "%c " RST, _hm.word[i]);
    } else {
      Serial.print(BLD "_ " RST);
      missing++;
    }
  }
  Serial.print("\r\n\r\n");

  // Wrong-guess list
  Serial.print("  Wrong: ");
  bool any = false;
  for (uint8_t i = 0; i < 26; i++) {
    if (!_hm.guessed[i]) continue;
    bool inWord = false;
    for (uint8_t j = 0; _hm.word[j]; j++)
      if (_hm.word[j] == (char)('a' + i)) { inWord = true; break; }
    if (!inWord) { Serial.printf(RED "%c " RST, 'a' + i); any = true; }
  }
  if (!any) Serial.print(GRY "none" RST);
  Serial.printf("   " GRY "(%u / 6 wrong)" RST "\r\n\r\n", _hm.nWrong);

  // Win / lose check
  if (missing == 0 && !_hm.over) {
    _hm.won  = true;
    _hm.over = true;
  }
  if (_hm.nWrong >= 6 && !_hm.won) {
    _hm.over = true;
  }

  if (_hm.won) {
    Serial.print(BLD GRN "  ✓  You survived!  Well played, hacker.\r\n" RST);
    Serial.print(GRY "  Press any key to return to the menu.\r\n" RST);
    return;
  }
  if (_hm.over) {
    Serial.printf(BLD RED "  ✗  Caught!  The word was: " YLW "%s\r\n" RST, _hm.word);
    Serial.print(GRY "  Press any key to return to the menu.\r\n" RST);
    return;
  }

  Serial.print("  Guess a letter (Q = quit): ");
}

void SerialGames::_hmOnChar(char c) {
  // After game over, any key returns to menu
  if (_hm.over) { _drawMenu(); _st = St::MENU; return; }

  uint8_t idx = (uint8_t)(c - 'a');
  if (_hm.guessed[idx]) return;    // already guessed — ignore silently
  _hm.guessed[idx] = true;

  // Count miss
  bool hit = false;
  for (uint8_t i = 0; _hm.word[i]; i++)
    if (_hm.word[i] == c) { hit = true; break; }
  if (!hit) _hm.nWrong++;

  _hmDraw();
}

// ================================================================
// MINESWEEPER
// ================================================================

void SerialGames::_msStart() {
  memset(&_ms, 0, sizeof(_ms));
  _st       = St::MINESWEEPER;
  _lineMode = true;
  _clearLine();
  _msDraw();
}

void SerialGames::_msSeed(uint8_t safeR, uint8_t safeC) {
  _ms.seeded = true;
  uint8_t placed = 0;
  while (placed < MSN) {
    uint8_t r = (uint8_t)random(MSR);
    uint8_t c = (uint8_t)random(MSC);
    if (_ms.mine[r][c]) continue;
    // Keep a 1-cell safety radius around the first click
    if (abs((int)r - safeR) <= 1 && abs((int)c - safeC) <= 1) continue;
    _ms.mine[r][c] = true;
    placed++;
  }
  // Pre-compute neighbour mine counts
  for (uint8_t r = 0; r < MSR; r++)
    for (uint8_t c = 0; c < MSC; c++) {
      uint8_t n = 0;
      for (int8_t dr = -1; dr <= 1; dr++)
        for (int8_t dc = -1; dc <= 1; dc++) {
          int nr = r + dr, nc = c + dc;
          if (nr >= 0 && nr < MSR && nc >= 0 && nc < MSC && _ms.mine[nr][nc]) n++;
        }
      _ms.count[r][c] = n;
    }
}

// Recursive flood-fill reveal — safe on an 8×8 grid
void SerialGames::_msReveal(int r, int c) {
  if (r < 0 || r >= MSR || c < 0 || c >= MSC) return;
  if (_ms.revealed[r][c] || _ms.flagged[r][c])  return;
  if (_ms.mine[r][c]) { _ms.over = true; return; }
  _ms.revealed[r][c] = true;
  _ms.nRevealed++;
  if (_ms.count[r][c] == 0) {
    for (int8_t dr = -1; dr <= 1; dr++)
      for (int8_t dc = -1; dc <= 1; dc++)
        if (dr || dc) _msReveal(r + dr, c + dc);
  }
}

void SerialGames::_msDraw() {
  _cls();

  uint8_t flags = 0;
  for (uint8_t r = 0; r < MSR; r++)
    for (uint8_t c = 0; c < MSC; c++)
      if (_ms.flagged[r][c]) flags++;

  Serial.print(
    BLD GRN
    "  ╔══════════════════════════════════════╗\r\n"
    "  ║     MINESWEEPER  —  8×8, 10 mines    ║\r\n"
    "  ╚══════════════════════════════════════╝\r\n"
    RST "\r\n"
  );

  // Column header + top border
  Serial.print("       1 2 3 4 5 6 7 8\r\n");
  Serial.print("     +-----------------+\r\n");

  for (uint8_t r = 0; r < MSR; r++) {
    Serial.printf("   %u | ", r + 1);
    for (uint8_t c = 0; c < MSC; c++) {
      if (_ms.flagged[r][c]) {
        Serial.print(YLW "F" RST " ");
      } else if (!_ms.revealed[r][c]) {
        // On game over, show all mine locations
        if (_ms.over && _ms.mine[r][c])
          Serial.print(RED "*" RST " ");
        else
          Serial.print(GRY "?" RST " ");
      } else if (_ms.count[r][c] == 0) {
        Serial.print("  ");              // clear / empty cell
      } else {
        // Colour-code by danger: 1-2 cyan, 3-4 yellow, 5+ red
        const char* nc =
            (_ms.count[r][c] <= 2) ? CYN :
            (_ms.count[r][c] <= 4) ? YLW : RED;
        Serial.printf("%s%u" RST " ", nc, _ms.count[r][c]);
      }
    }
    Serial.print("|\r\n");
  }

  Serial.print("     +-----------------+\r\n\r\n");

  Serial.printf("  " BLD "Mines left: %d" RST
                GRY "   (F=flag  ?=hidden  blank=safe)" RST "\r\n\r\n",
                (int)MSN - (int)flags);

  if (_ms.won) {
    Serial.print(BLD GRN "  ✓  Cleared!  Every mine avoided.\r\n" RST);
    Serial.print(GRY "  Type Q + Enter to return to the menu.\r\n" RST);
    return;
  }
  if (_ms.over) {
    Serial.print(BLD RED "  ✗  BOOM!  You hit a mine.\r\n" RST);
    Serial.print(GRY "  Type Q + Enter to return to the menu.\r\n" RST);
    return;
  }

  Serial.print(
    "  " GRY "r ROW COL" RST " = reveal   "
    GRY "f ROW COL" RST " = flag/unflag   "
    GRY "Q" RST " = quit\r\n"
    "  > "
  );
}

// Returns true if the screen needs a full redraw.
bool SerialGames::_msOnLine() {
  // Quit (works mid-game and on win/lose screen)
  if (_line[0] == 'q' || _line[0] == 'Q') {
    _drawMenu();
    _st       = St::MENU;
    _lineMode = false;
    return false;
  }

  if (_ms.won || _ms.over) return false;

  char cmd = _line[0];
  if (cmd != 'r' && cmd != 'R' && cmd != 'f' && cmd != 'F') {
    Serial.print(RED "  ✗  Unknown command.  Use: r ROW COL  or  f ROW COL\r\n" RST "  > ");
    return false;
  }

  // Parse two integers following the command letter
  const char* p = _line + 1;
  while (*p == ' ') p++;
  int row = 0;
  while (*p >= '0' && *p <= '9') row = row * 10 + (*p++ - '0');
  while (*p == ' ') p++;
  int col = 0;
  while (*p >= '0' && *p <= '9') col = col * 10 + (*p++ - '0');
  row--; col--;   // convert 1-indexed user input to 0-indexed

  if (row < 0 || row >= MSR || col < 0 || col >= MSC) {
    Serial.print(RED "  ✗  Row and column must each be 1–8.\r\n" RST "  > ");
    return false;
  }

  if (cmd == 'f' || cmd == 'F') {
    if (!_ms.revealed[row][col])
      _ms.flagged[row][col] = !_ms.flagged[row][col];
  } else {
    if (!_ms.seeded) _msSeed((uint8_t)row, (uint8_t)col);
    _msReveal(row, col);
  }

  if (_ms.nRevealed == MSR * MSC - MSN) _ms.won = true;
  return true;
}

// ================================================================
// TEXT ADVENTURE
// ================================================================
void SerialGames::_advStart() {
  _st       = St::ADVENTURE;
  _lineMode = true;
  _clearLine();
  _adv.begin();
}

// ================================================================
// CONFIG  — badge settings over serial CLI
// ================================================================

static const char* _protoName(uint8_t p) {
  switch (p) {
    case IR_PROTO_NEC:     return "NEC";
    case IR_PROTO_SAMSUNG: return "SAMSUNG";
    case IR_PROTO_SONY:    return "SONY";
    case IR_PROTO_RC5:     return "RC5";
  }
  return "?";
}

static const char* _btnName(uint8_t i) {
  switch (i) {
    case 0: return "A";
    case 1: return "B";
    case 2: return "UP";
    case 3: return "DOWN";
    case 4: return "LEFT";
    case 5: return "RIGHT";
  }
  return "?";
}

uint8_t SerialGames::_cfgParseBtn(const char* s) {
  if (strcmp(s,"a")==0)     return 0;
  if (strcmp(s,"b")==0)     return 1;
  if (strcmp(s,"up")==0)    return 2;
  if (strcmp(s,"down")==0)  return 3;
  if (strcmp(s,"left")==0)  return 4;
  if (strcmp(s,"right")==0) return 5;
  return 0xFF;
}

uint8_t SerialGames::_cfgParseProto(const char* s) {
  if (strcmp(s,"nec")==0)     return IR_PROTO_NEC;
  if (strcmp(s,"samsung")==0) return IR_PROTO_SAMSUNG;
  if (strcmp(s,"sony")==0)    return IR_PROTO_SONY;
  if (strcmp(s,"rc5")==0)     return IR_PROTO_RC5;
  return 0xFF;
}

uint8_t SerialGames::_cfgParseAnim(const char* s) {
  if (strcmp(s,"knight")==0)    return LED_ANIM_KNIGHT;
  if (strcmp(s,"pulse")==0)     return LED_ANIM_PULSE;
  if (strcmp(s,"strobe")==0)    return LED_ANIM_STROBE;
  if (strcmp(s,"alt")==0)       return LED_ANIM_ALTERNATE;
  if (strcmp(s,"alternate")==0) return LED_ANIM_ALTERNATE;
  if (strcmp(s,"chase")==0)     return LED_ANIM_CHASE;
  if (strcmp(s,"on")==0)        return LED_ANIM_ON;
  if (strcmp(s,"off")==0)       return LED_ANIM_OFF;
  return 0xFF;
}

const char* SerialGames::_animName(uint8_t a) {
  switch (a) {
    case LED_ANIM_KNIGHT:    return "KNIGHT";
    case LED_ANIM_PULSE:     return "PULSE";
    case LED_ANIM_STROBE:    return "STROBE";
    case LED_ANIM_ALTERNATE: return "ALTERNATE";
    case LED_ANIM_CHASE:     return "CHASE";
    case LED_ANIM_ON:        return "ON";
    case LED_ANIM_OFF:       return "OFF";
  }
  return "?";
}

void SerialGames::_cfgStart() {
  _st       = St::CONFIG;
  _lineMode = true;
  _clearLine();
  _cfgDraw();
}

void SerialGames::_cfgDraw() {
  _cls();
  Serial.print(
    BLD CYN
    "  ╔══════════════════════════════════════════════╗\r\n"
    "  ║           BADGE  CONFIG                     ║\r\n"
    "  ╚══════════════════════════════════════════════╝\r\n"
    RST "\r\n"
    GRY "  Commands (case-insensitive):\r\n\r\n"
    "  " RST BLD "show" RST GRY "                        — print all current settings\r\n"
    "  " RST BLD "name <yourname>" RST GRY "             — set badge owner name (matrix name-badge)\r\n"
    "  " RST BLD "bright matrix <1-8>" RST GRY "        — matrix brightness / LED sweep depth\r\n"
    "  " RST BLD "bright flash <1-8>" RST GRY "         — flashlight brightness\r\n"
    "  " RST BLD "led <mode>" RST GRY "                 — front LED animation\r\n"
    "      " RST "modes: knight  pulse  strobe  alt  chase  on  off\r\n" GRY
    "  " RST BLD "idle on" RST " / " BLD "off" RST GRY "               — enable/disable screensaver\r\n"
    "  " RST BLD "idle timeout <5-60>" RST GRY "        — seconds of inactivity before screensaver\r\n"
    "  " RST BLD "scroll menu <20-150>" RST GRY "       — menu item scroll speed (ms/pixel)\r\n"
    "  " RST BLD "scroll idle <10-100>" RST GRY "       — screensaver message speed (ms/pixel)\r\n"
    "  " RST BLD "scroll name <20-150>" RST GRY "       — name-badge scroll speed (ms/pixel)\r\n"
    "  " RST BLD "ir <btn> <proto> <addr> <cmd>" RST GRY "\r\n"
    "      btn=A/B/UP/DOWN/LEFT/RIGHT  proto=NEC/SAMSUNG/SONY/RC5\r\n"
    "  " RST BLD "havoc codes power|input|all" RST GRY " — which code types IR HAVOC transmits\r\n"
    "  " RST BLD "havoc delay <100-2000>" RST GRY "     — ms between IR HAVOC sends\r\n"
    "  " RST BLD "default" RST GRY "                    — restore factory defaults\r\n"
    "  " RST BLD "q / back" RST GRY "                  — return to main menu\r\n"
    RST "\r\n"
    "  > "
  );
}

void SerialGames::_cfgShowConfig() {
  Serial.print("\r\n");
  Serial.printf("  Name              : " BLD "%s" RST "\r\n",
                g_cfg.name[0] ? g_cfg.name : "(not set)");
  Serial.printf("  Matrix brightness : " BLD "%d" RST "/8\r\n", g_cfg.brightness);
  Serial.printf("  Flash brightness  : " BLD "%d" RST "/8\r\n", g_cfg.flashBright);
  Serial.printf("  LED animation     : " BLD "%s" RST "\r\n", _animName(g_cfg.ledAnim));
  Serial.print("\r\n");
  Serial.printf("  Screensaver       : " BLD "%s" RST "\r\n", g_cfg.idleEnable ? "ON" : "OFF");
  Serial.printf("  Idle timeout      : " BLD "%d" RST " s\r\n", g_cfg.idleTimeoutSec);
  Serial.printf("  Scroll  menu      : " BLD "%d" RST " ms/px\r\n", g_cfg.menuScrollMs);
  Serial.printf("  Scroll  idle      : " BLD "%d" RST " ms/px\r\n", g_cfg.idleScrollMs);
  Serial.printf("  Scroll  name      : " BLD "%d" RST " ms/px\r\n", g_cfg.nameScrollMs);
  Serial.print("\r\n");
  const char* havocCodesStr =
    (g_cfg.havocCodes == HAVOC_SEND_ALL)   ? "power + input" :
    (g_cfg.havocCodes & HAVOC_SEND_POWER)  ? "power only"    :
    (g_cfg.havocCodes & HAVOC_SEND_INPUT)  ? "input only"    : "none";
  Serial.printf("  IR havoc codes    : " BLD "%s" RST "\r\n", havocCodesStr);
  Serial.printf("  IR havoc delay    : " BLD "%d" RST " ms\r\n",
                (int)g_cfg.havocDelay * 100);
  Serial.print("\r\n");
  Serial.print("  " BLD "Button  Protocol  Address   Command\r\n" RST);
  Serial.print(GRY "  ────────────────────────────────────\r\n" RST);
  for (uint8_t i = 0; i < IR_BTN_COUNT; i++) {
    const IrCode& c = g_cfg.ir[i];
    Serial.printf("  %-6s  %-8s  0x%04X    0x%02X\r\n",
      _btnName(i), _protoName(c.protocol),
      (unsigned)(irCodeAddr(c)), (unsigned)c.command);
  }
  Serial.print("\r\n  > ");
}

void SerialGames::_cfgOnLine() {
  // Lower-case the input into a working buffer
  char buf[64];
  uint8_t len = 0;
  while (_line[len] && len < 63) { buf[len] = _line[len]; len++; }
  buf[len] = '\0';
  for (uint8_t i = 0; i < len; i++)
    if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] += 32;

  // Tokenise (up to 5 tokens)
  char* toks[5] = {nullptr,nullptr,nullptr,nullptr,nullptr};
  uint8_t tc = 0;
  char* p = buf;
  while (*p && tc < 5) {
    while (*p == ' ') p++;
    if (!*p) break;
    toks[tc++] = p;
    while (*p && *p != ' ') p++;
    if (*p) { *p = '\0'; p++; }
  }

  if (tc == 0) { Serial.print("  > "); return; }

  // ── q / back ───────────────────────────────────────────────
  if (strcmp(toks[0],"q")==0 || strcmp(toks[0],"back")==0) {
    _st = St::MENU; _lineMode = false; _drawMenu(); return;
  }

  // ── help ───────────────────────────────────────────────────
  if (strcmp(toks[0],"help")==0) { _cfgDraw(); return; }

  // ── show ───────────────────────────────────────────────────
  if (strcmp(toks[0],"show")==0) { _cfgShowConfig(); return; }

  // ── default ────────────────────────────────────────────────
  if (strcmp(toks[0],"default")==0) {
    g_cfg = BADGE_CONFIG_DEFAULT;
    configSave();
    Serial.print("  " GRN "Factory defaults restored and saved.\r\n" RST "\r\n  > ");
    return;
  }

  // ── name <yourname> ────────────────────────────────────────
  if (strcmp(toks[0],"name")==0) {
    if (tc < 2) {
      Serial.print("  " RED "Usage: name <yourname>  (max 15 chars, no spaces)\r\n" RST "  > ");
      return;
    }
    strncpy(g_cfg.name, toks[1], NAME_MAX_LEN - 1);
    g_cfg.name[NAME_MAX_LEN - 1] = '\0';
    for (uint8_t i = 0; g_cfg.name[i]; i++) {
      uint8_t b = (uint8_t)g_cfg.name[i];
      if (b >= 'a' && b <= 'z')
        g_cfg.name[i] = (char)(b - 32);
      else if (b == 0xE5 || b == 0xE4 || b == 0xF6 || b == 0xFC)
        g_cfg.name[i] = (char)(b - 0x20);
    }
    configSaveName();
    Serial.printf("  " GRN "Name set to: %s — saved.\r\n" RST "  > ", g_cfg.name);
    return;
  }

  // ── bright matrix|flash <1-8> ──────────────────────────────
  if (strcmp(toks[0],"bright")==0 && tc >= 3) {
    uint8_t val = (uint8_t)atoi(toks[2]);
    if (val < 1 || val > 8) {
      Serial.print("  " RED "Value must be 1-8.\r\n" RST "  > "); return;
    }
    if (strcmp(toks[1],"matrix")==0) {
      g_cfg.brightness = val; configSaveBrightness();
      Serial.printf("  " GRN "Matrix brightness set to %d.\r\n" RST "  > ", val);
    } else if (strcmp(toks[1],"flash")==0) {
      g_cfg.flashBright = val; configSaveFlash();
      Serial.printf("  " GRN "Flashlight brightness set to %d.\r\n" RST "  > ", val);
    } else {
      Serial.print("  " RED "Unknown target. Use 'matrix' or 'flash'.\r\n" RST "  > ");
    }
    return;
  }

  // ── led <mode> ─────────────────────────────────────────────
  if (strcmp(toks[0],"led")==0) {
    if (tc < 2) {
      Serial.print("  " RED "Usage: led <mode>  (knight pulse strobe alt chase on off)\r\n" RST "  > ");
      return;
    }
    uint8_t anim = _cfgParseAnim(toks[1]);
    if (anim == 0xFF) {
      Serial.print("  " RED "Unknown mode. Choose: knight pulse strobe alt chase on off\r\n" RST "  > ");
      return;
    }
    g_cfg.ledAnim = anim;
    configSaveSettings();
    Serial.printf("  " GRN "LED animation set to %s — saved.\r\n" RST "  > ", _animName(anim));
    return;
  }

  // ── idle on|off  /  idle timeout <sec> ────────────────────
  if (strcmp(toks[0],"idle")==0 && tc >= 2) {
    if (strcmp(toks[1],"on")==0) {
      g_cfg.idleEnable = 1; configSaveSettings();
      Serial.print("  " GRN "Screensaver enabled — saved.\r\n" RST "  > ");
    } else if (strcmp(toks[1],"off")==0) {
      g_cfg.idleEnable = 0; configSaveSettings();
      Serial.print("  " GRN "Screensaver disabled — saved.\r\n" RST "  > ");
    } else if (strcmp(toks[1],"timeout")==0 && tc >= 3) {
      uint8_t sec = (uint8_t)atoi(toks[2]);
      if (sec < 5 || sec > 60) {
        Serial.print("  " RED "Timeout must be 5-60 seconds.\r\n" RST "  > "); return;
      }
      g_cfg.idleTimeoutSec = sec; configSaveSettings();
      Serial.printf("  " GRN "Idle timeout set to %d s — saved.\r\n" RST "  > ", sec);
    } else {
      Serial.print("  " RED "Usage: idle on  /  idle off  /  idle timeout <5-60>\r\n" RST "  > ");
    }
    return;
  }

  // ── scroll menu|idle|name <ms> ─────────────────────────────
  if (strcmp(toks[0],"scroll")==0 && tc >= 3) {
    int ms = atoi(toks[2]);
    if (strcmp(toks[1],"menu")==0) {
      if (ms < 20 || ms > 150) {
        Serial.print("  " RED "Menu scroll must be 20-150 ms/pixel.\r\n" RST "  > "); return;
      }
      g_cfg.menuScrollMs = (uint8_t)ms; configSaveSettings();
      Serial.printf("  " GRN "Menu scroll set to %d ms/px — saved.\r\n" RST "  > ", ms);
    } else if (strcmp(toks[1],"idle")==0) {
      if (ms < 10 || ms > 100) {
        Serial.print("  " RED "Idle scroll must be 10-100 ms/pixel.\r\n" RST "  > "); return;
      }
      g_cfg.idleScrollMs = (uint8_t)ms; configSaveSettings();
      Serial.printf("  " GRN "Idle scroll set to %d ms/px — saved.\r\n" RST "  > ", ms);
    } else if (strcmp(toks[1],"name")==0) {
      if (ms < 20 || ms > 150) {
        Serial.print("  " RED "Name scroll must be 20-150 ms/pixel.\r\n" RST "  > "); return;
      }
      g_cfg.nameScrollMs = (uint8_t)ms; configSaveSettings();
      Serial.printf("  " GRN "Name scroll set to %d ms/px — saved.\r\n" RST "  > ", ms);
    } else {
      Serial.print("  " RED "Usage: scroll menu|idle|name <ms>\r\n" RST "  > ");
    }
    return;
  }

  // ── ir <btn> <proto> <addr> <cmd> ──────────────────────────
  if (strcmp(toks[0],"ir")==0) {
    if (tc < 5) {
      Serial.print("  " RED "Usage: ir <btn> <proto> <addr> <cmd>\r\n" RST "  > "); return;
    }
    uint8_t btn   = _cfgParseBtn(toks[1]);
    uint8_t proto = _cfgParseProto(toks[2]);
    if (btn == 0xFF) {
      Serial.print("  " RED "Unknown button. Use A B UP DOWN LEFT RIGHT.\r\n" RST "  > "); return;
    }
    if (proto == 0xFF) {
      Serial.print("  " RED "Unknown protocol. Use NEC SAMSUNG SONY RC5.\r\n" RST "  > "); return;
    }
    uint32_t addr = (uint32_t)strtoul(toks[3], nullptr, 16);
    uint32_t cmd  = (uint32_t)strtoul(toks[4], nullptr, 16);
    if (addr > 0xFFFF || cmd > 0xFF) {
      Serial.print("  " RED "Address must be ≤ 0xFFFF, command ≤ 0xFF.\r\n" RST "  > "); return;
    }
    g_cfg.ir[btn].protocol = proto;
    g_cfg.ir[btn].addr_lo  = (uint8_t)(addr & 0xFF);
    g_cfg.ir[btn].addr_hi  = (uint8_t)(addr >> 8);
    g_cfg.ir[btn].command  = (uint8_t)cmd;
    configSaveIr(btn);
    Serial.printf("  " GRN "%s → %s addr=0x%04lX cmd=0x%02lX — saved.\r\n" RST "  > ",
      _btnName(btn), _protoName(proto), addr, cmd);
    return;
  }

  // ── havoc codes power|input|all  /  havoc delay <ms> ──────
  if (strcmp(toks[0],"havoc")==0 && tc >= 2) {
    if (strcmp(toks[1],"codes")==0 && tc >= 3) {
      uint8_t mask = 0;
      if (strcmp(toks[2],"power")==0) mask = HAVOC_SEND_POWER;
      else if (strcmp(toks[2],"input")==0) mask = HAVOC_SEND_INPUT;
      else if (strcmp(toks[2],"all")==0)   mask = HAVOC_SEND_ALL;
      else {
        Serial.print("  " RED "Usage: havoc codes power|input|all\r\n" RST "  > ");
        return;
      }
      g_cfg.havocCodes = mask;
      configSaveSettings();
      Serial.printf("  " GRN "Havoc code types set to: %s — saved.\r\n" RST "  > ",
        (mask == HAVOC_SEND_ALL) ? "power + input" :
        (mask & HAVOC_SEND_POWER) ? "power only" : "input only");
    } else if (strcmp(toks[1],"delay")==0 && tc >= 3) {
      int ms = atoi(toks[2]);
      if (ms < 100 || ms > 2000) {
        Serial.print("  " RED "Delay must be 100-2000 ms.\r\n" RST "  > "); return;
      }
      g_cfg.havocDelay = (uint8_t)(ms / 100);
      configSaveSettings();
      Serial.printf("  " GRN "Havoc delay set to %d ms — saved.\r\n" RST "  > ", ms);
    } else {
      Serial.print("  " RED "Usage: havoc codes power|input|all  |  havoc delay <100-2000>\r\n" RST "  > ");
    }
    return;
  }

  Serial.print("  " DIM "Unrecognised command. Type 'help'.\r\n" RST "  > ");
}
