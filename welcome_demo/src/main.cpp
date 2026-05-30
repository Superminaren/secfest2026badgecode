// Secfest 2026 Badge — Welcome Demo
//
// Boot sequence:
//   1. Sweep the four front LEDs
//   2. Scroll "WELCOME TO SECURITY FEST 2026" on the 9x9 matrix
//   3. Fireworks animation
//   4. Menu (Snake, Simon, Replay welcome)
//
// Any button press during steps 1-3 jumps straight to the menu.
//
// Pin map and orientation come from the verified hardware_test sketch.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_IS31FL3731.h>
#include <EEPROM.h>
#include <Joystick.h>
#include "font4x7.h"
#include "badge_config.h"
#include "serial_games.h"

static SerialGames serialGames;
static bool g_gamepadMode = false;   // set before USB init; true = controller boot

// ============================================================ pin map =======

#define MATRIX_BRIGHTNESS 20

// ============================================================ config / EEPROM =
// g_cfg is the live badge configuration. Load on boot, save whenever changed.
// Helpers declared here, defined below (badge_config.h has extern declarations).
BadgeConfig g_cfg = BADGE_CONFIG_DEFAULT;

// Convenience shorthand — used throughout the file
#define g_brightness  g_cfg.brightness
#define g_flashBright g_cfg.flashBright

void configLoad() {
  EEPROM.begin(EEPROM_CFG_SIZE);
  uint8_t bright = EEPROM.read(EEPROM_ADDR_BRIGHT);
  g_cfg.brightness  = (bright  >= 1 && bright  <= 8) ? bright  : 8;
  uint8_t flash  = EEPROM.read(EEPROM_ADDR_FLASH);
  g_cfg.flashBright = (flash   >= 1 && flash   <= 8) ? flash   : 8;
  for (uint8_t i = 0; i < IR_BTN_COUNT; i++) {
    uint16_t base = EEPROM_ADDR_IR_BASE + i * 4;
    uint8_t proto = EEPROM.read(base);
    if (proto > IR_PROTO_RC5) {
      // uninitialised — use default
      g_cfg.ir[i] = BADGE_CONFIG_DEFAULT.ir[i];
    } else {
      g_cfg.ir[i].protocol = proto;
      g_cfg.ir[i].addr_lo  = EEPROM.read(base + 1);
      g_cfg.ir[i].addr_hi  = EEPROM.read(base + 2);
      g_cfg.ir[i].command  = EEPROM.read(base + 3);
    }
  }
  // Load name — accept printable ASCII plus Å Ä Ö Ü (0xC4/C5/D6/DC)
  for (uint8_t i = 0; i < NAME_MAX_LEN; i++) {
    uint8_t b = EEPROM.read(EEPROM_ADDR_NAME + i);
    bool valid = (b >= 32 && b <= 126) ||
                 b == 0xC4 || b == 0xC5 || b == 0xD6 || b == 0xDC;
    if (valid) {
      g_cfg.name[i] = (char)b;
    } else {
      g_cfg.name[i] = '\0';
      break;
    }
  }
  g_cfg.name[NAME_MAX_LEN - 1] = '\0';

  // If no name is stored in EEPROM, fall back to the compile-time default
  // (supplied via ./flash.sh "NAME") and immediately persist it so future
  // reflashes without a name argument still display the stored name.
#ifdef BADGE_DEFAULT_NAME
  if (g_cfg.name[0] == '\0') {
    strncpy(g_cfg.name, BADGE_DEFAULT_NAME, NAME_MAX_LEN - 1);
    g_cfg.name[NAME_MAX_LEN - 1] = '\0';
    for (uint8_t i = 0; i < NAME_MAX_LEN; i++)
      EEPROM.write(EEPROM_ADDR_NAME + i, (uint8_t)g_cfg.name[i]);
    EEPROM.commit();
  }
#endif

  // Load new settings fields — validate each, fall back to default if uninitialised
  uint8_t ledAnim = EEPROM.read(EEPROM_ADDR_LED_ANIM);
  g_cfg.ledAnim = (ledAnim < LED_ANIM_COUNT) ? ledAnim : BADGE_CONFIG_DEFAULT.ledAnim;

  uint8_t idleEn = EEPROM.read(EEPROM_ADDR_IDLE_ENABLE);
  g_cfg.idleEnable = (idleEn <= 1) ? idleEn : BADGE_CONFIG_DEFAULT.idleEnable;

  uint8_t idleTo = EEPROM.read(EEPROM_ADDR_IDLE_TIMEOUT);
  g_cfg.idleTimeoutSec = (idleTo >= 5 && idleTo <= 60) ? idleTo : BADGE_CONFIG_DEFAULT.idleTimeoutSec;

  uint8_t mScroll = EEPROM.read(EEPROM_ADDR_MENU_SCROLL);
  g_cfg.menuScrollMs = (mScroll >= 20 && mScroll <= 150) ? mScroll : BADGE_CONFIG_DEFAULT.menuScrollMs;

  uint8_t iScroll = EEPROM.read(EEPROM_ADDR_IDLE_SCROLL);
  g_cfg.idleScrollMs = (iScroll >= 10 && iScroll <= 100) ? iScroll : BADGE_CONFIG_DEFAULT.idleScrollMs;

  uint8_t nScroll = EEPROM.read(EEPROM_ADDR_NAME_SCROLL);
  g_cfg.nameScrollMs = (nScroll >= 20 && nScroll <= 150) ? nScroll : BADGE_CONFIG_DEFAULT.nameScrollMs;

  uint8_t mAnim = EEPROM.read(EEPROM_ADDR_MATRIX_ANIM);
  g_cfg.matrixAnim = (mAnim < MATRIX_ANIM_COUNT) ? mAnim : BADGE_CONFIG_DEFAULT.matrixAnim;

  uint8_t hCode = EEPROM.read(EEPROM_ADDR_HAVOC_CODES);
  g_cfg.havocCodes = (hCode & HAVOC_SEND_ALL) ? hCode : BADGE_CONFIG_DEFAULT.havocCodes;

  uint8_t hDly = EEPROM.read(EEPROM_ADDR_HAVOC_DELAY);
  g_cfg.havocDelay = (hDly >= 1 && hDly <= 20) ? hDly : BADGE_CONFIG_DEFAULT.havocDelay;
}

void configSave() {
  EEPROM.write(EEPROM_ADDR_BRIGHT, g_cfg.brightness);
  EEPROM.write(EEPROM_ADDR_FLASH,  g_cfg.flashBright);
  for (uint8_t i = 0; i < IR_BTN_COUNT; i++) {
    uint16_t base = EEPROM_ADDR_IR_BASE + i * 4;
    EEPROM.write(base,     g_cfg.ir[i].protocol);
    EEPROM.write(base + 1, g_cfg.ir[i].addr_lo);
    EEPROM.write(base + 2, g_cfg.ir[i].addr_hi);
    EEPROM.write(base + 3, g_cfg.ir[i].command);
  }
  for (uint8_t i = 0; i < NAME_MAX_LEN; i++)
    EEPROM.write(EEPROM_ADDR_NAME + i, (uint8_t)g_cfg.name[i]);
  EEPROM.write(EEPROM_ADDR_LED_ANIM,     g_cfg.ledAnim);
  EEPROM.write(EEPROM_ADDR_IDLE_ENABLE,  g_cfg.idleEnable);
  EEPROM.write(EEPROM_ADDR_IDLE_TIMEOUT, g_cfg.idleTimeoutSec);
  EEPROM.write(EEPROM_ADDR_MENU_SCROLL,  g_cfg.menuScrollMs);
  EEPROM.write(EEPROM_ADDR_IDLE_SCROLL,  g_cfg.idleScrollMs);
  EEPROM.write(EEPROM_ADDR_NAME_SCROLL,  g_cfg.nameScrollMs);
  EEPROM.write(EEPROM_ADDR_MATRIX_ANIM,  g_cfg.matrixAnim);
  EEPROM.write(EEPROM_ADDR_HAVOC_CODES,  g_cfg.havocCodes);
  EEPROM.write(EEPROM_ADDR_HAVOC_DELAY,  g_cfg.havocDelay);
  EEPROM.commit();
}

void configSaveName() {
  for (uint8_t i = 0; i < NAME_MAX_LEN; i++)
    EEPROM.write(EEPROM_ADDR_NAME + i, (uint8_t)g_cfg.name[i]);
  EEPROM.commit();
}

void configSaveSettings() {
  EEPROM.write(EEPROM_ADDR_LED_ANIM,     g_cfg.ledAnim);
  EEPROM.write(EEPROM_ADDR_IDLE_ENABLE,  g_cfg.idleEnable);
  EEPROM.write(EEPROM_ADDR_IDLE_TIMEOUT, g_cfg.idleTimeoutSec);
  EEPROM.write(EEPROM_ADDR_MENU_SCROLL,  g_cfg.menuScrollMs);
  EEPROM.write(EEPROM_ADDR_IDLE_SCROLL,  g_cfg.idleScrollMs);
  EEPROM.write(EEPROM_ADDR_NAME_SCROLL,  g_cfg.nameScrollMs);
  EEPROM.write(EEPROM_ADDR_MATRIX_ANIM,  g_cfg.matrixAnim);
  EEPROM.write(EEPROM_ADDR_HAVOC_CODES,  g_cfg.havocCodes);
  EEPROM.write(EEPROM_ADDR_HAVOC_DELAY,  g_cfg.havocDelay);
  EEPROM.commit();
}

void configSaveBrightness() {
  EEPROM.write(EEPROM_ADDR_BRIGHT, g_cfg.brightness);
  EEPROM.commit();
}

void configSaveFlash() {
  EEPROM.write(EEPROM_ADDR_FLASH, g_cfg.flashBright);
  EEPROM.commit();
}

void configSaveIr(uint8_t idx) {
  if (idx >= IR_BTN_COUNT) return;
  uint16_t base = EEPROM_ADDR_IR_BASE + idx * 4;
  EEPROM.write(base,     g_cfg.ir[idx].protocol);
  EEPROM.write(base + 1, g_cfg.ir[idx].addr_lo);
  EEPROM.write(base + 2, g_cfg.ir[idx].addr_hi);
  EEPROM.write(base + 3, g_cfg.ir[idx].command);
  EEPROM.commit();
}

#define BTN_A     8
#define BTN_B     9
#define BTN_UP    10
#define BTN_DOWN  11
#define BTN_LEFT  12
#define BTN_RIGHT 13

#define SAO_GP1	  0
#define SAO_GP2   1

#define LED_FLASHLIGHT 14
#define LED_FRONT_1    20
#define LED_FRONT_2    21
#define LED_FRONT_3    22
#define LED_FRONT_4    23

#define LED_MATRIX_SDA 4
#define LED_MATRIX_SCL 5
#define IS31_ADDR      0x74

// Front LEDs are active-high (anode to GPIO, cathode to GND).
const int FRONT_LED_ON  = HIGH;
const int FRONT_LED_OFF = LOW;
const int FRONT_LEDS[]  = { LED_FRONT_1, LED_FRONT_2, LED_FRONT_3, LED_FRONT_4 };
const int NUM_FRONT_LEDS = 4;

// Button indices and tables
enum BtnIdx { BI_A=0, BI_B, BI_UP, BI_DOWN, BI_LEFT, BI_RIGHT, BI_COUNT };
const int BUTTON_PINS[BI_COUNT] = { BTN_A, BTN_B, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT };

Adafruit_IS31FL3731 matrix(9, 9);

// ============================================================ framebuffer ==

// 9x9 back buffer. The badge mounts the LEDs rotated 180 degrees relative
// to the schematic, so the helper mirrors both axes before writing to chip.
static const int W = 9;
static const int H = 9;
static uint8_t fb[H][W];

static inline void fbClear() {
  memset(fb, 0, sizeof(fb));
}

static inline void fbSet(int x, int y, uint8_t b) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  //if (x == y) return;   // charlieplex diagonal — no LED exists here
  fb[y][x] = b;
}

static inline void fbOr(int x, int y, uint8_t b) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  //if (x == y) return;   // charlieplex diagonal — no LED exists here
  if (b > fb[y][x]) fb[y][x] = b;
}

// Orientation pipeline (logical -> chip register).
//
// Edit these three flags to match how the matrix is physically mounted
// on your badge. They're applied in order: rotate, then flipH, then flipV.
// All combinations of rotation+mirror reach with these three knobs.
//
// Current setting: rotate 90° clockwise, then mirror horizontally
// (which together equal a transpose: logical (x,y) -> chip (y,x)).
#define MATRIX_ROTATE_90_CW   0
#define MATRIX_ROTATE_90_CCW  1
#define MATRIX_FLIP_H         1
#define MATRIX_FLIP_V         0

static inline void logicalToChip(int x, int y, int& cx, int& cy) {
  cx = x;
  cy = y;
#if MATRIX_ROTATE_90_CW
  // 90° CW on a (W x H) grid: (x, y) -> (H-1-y, x)
  int tx = (H - 1) - cy;
  int ty = cx;
  cx = tx;
  cy = ty;
#endif
#if MATRIX_ROTATE_90_CCW
  // 90° CCW on a (W x H) grid: (x, y) -> (H-1-y, x)
  int tx = cy;
  int ty = (H - 1) - cx;
  cx = tx;
  cy = ty;
#endif
#if MATRIX_FLIP_H
  cx = (W - 1) - cx;
#endif
#if MATRIX_FLIP_V
  cy = (H - 1) - cy;
#endif
}

static void fbPush() {
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      int cx, cy;
      logicalToChip(x, y, cx, cy);
      // Scale each pixel by the global brightness (1..8 → 1/8 … 8/8 of full)
      uint8_t val = (uint8_t)((int)fb[y][x] * g_brightness / 8);
      matrix.setLEDPWM(cx + cy * 16, val);
    }
  }
}

// ============================================================ input ========

struct Input {
  bool down[BI_COUNT];     // current state (true = pressed)
  bool pressed[BI_COUNT];  // edge: just went down this update
  bool released[BI_COUNT]; // edge: just went up this update
  unsigned long heldSince[BI_COUNT];
};

static Input input;

static void inputUpdate() {
  unsigned long now = millis();
  for (int i = 0; i < BI_COUNT; i++) {
    bool d = (digitalRead(BUTTON_PINS[i]) == LOW);
    input.pressed[i]  = d && !input.down[i];
    input.released[i] = !d && input.down[i];
    if (input.pressed[i]) input.heldSince[i] = now;
    input.down[i] = d;
  }
}

static bool anyPressed() {
  for (int i = 0; i < BI_COUNT; i++) if (input.pressed[i]) return true;
  return false;
}

// ============================================================ front LEDs ===

static void frontAll(int state) {
  for (int p : FRONT_LEDS) digitalWrite(p, state);
}

static void frontSet(int idx, int state) {
  if (idx >= 0 && idx < NUM_FRONT_LEDS) digitalWrite(FRONT_LEDS[idx], state);
}

// ============================================================ text scroll ==

// Source strings are UTF-8 (the .cpp file is UTF-8).  The font uses Latin-1
// single-byte values.  This buffer holds the converted copy so the scroller
// always sees one byte per glyph, including Å/Ä/Ö/Ü (U+00C4–U+00DC range).
// 64 bytes covers every string we scroll; longer strings are silently clipped.
#define SCROLLER_BUF_LEN 64
static char scrollerBuf[SCROLLER_BUF_LEN];

// Decode a UTF-8 string into a Latin-1 (ISO 8859-1) byte string.
// Characters outside U+00FF are dropped.  Three/four-byte sequences skipped.
static void utf8ToLatin1(char* dst, const char* src, uint8_t dstLen) {
  uint8_t di = 0;
  for (uint8_t si = 0; src[si] && di < dstLen - 1; si++) {
    uint8_t b = (uint8_t)src[si];
    if (b < 0x80) {
      dst[di++] = (char)b;
    } else if ((b & 0xE0) == 0xC0 && src[si + 1]) {
      uint8_t cont = (uint8_t)src[++si];
      uint16_t cp = ((uint16_t)(b & 0x1F) << 6) | (cont & 0x3F);
      if (cp >= 0x20 && cp <= 0xFF) dst[di++] = (char)(uint8_t)cp;
    }
    // 3/4-byte sequences not needed — skip continuation bytes
  }
  dst[di] = '\0';
}

struct Scroller {
  const char* text;
  int  textPxWidth;   // glyph cols + 1-px gap per char, total
  int  pos;           // leftmost on-screen column maps to text column = pos
  unsigned long lastTick;
  unsigned long stepMs;
  uint8_t brightness;
  bool done;
};

static Scroller scroller;

static void scrollStart(const char* t, unsigned long stepMs = 70, uint8_t b = 50) {
  utf8ToLatin1(scrollerBuf, t, SCROLLER_BUF_LEN);
  scroller.text = scrollerBuf;
  scroller.textPxWidth = (int)strlen(scrollerBuf) * (FONT_W + 1);
  scroller.pos = -W;      // start with text just off-screen right
  scroller.lastTick = 0;
  scroller.stepMs = stepMs;
  scroller.brightness = b;
  scroller.done = false;
}

// Render the scroller into fb. Returns true once the text has fully
// passed off-screen to the left (only meaningful for one-shot scrolls).
static bool scrollRenderTick() {
  if (!scroller.text) return false;
  unsigned long now = millis();
  if (now - scroller.lastTick < scroller.stepMs) return scroller.done;
  scroller.lastTick = now;

  fbClear();
  // Vertically center the 5-row glyph in the 9-row matrix.
  const int yOffset = (H - FONT_H) / 2;
  for (int sx = 0; sx < W; sx++) {
    int absCol = scroller.pos + sx;
    if (absCol < 0 || absCol >= scroller.textPxWidth) continue;
    int charIdx = absCol / (FONT_W + 1);
    int charCol = absCol % (FONT_W + 1);
    if (charCol >= FONT_W) continue;  // inter-character gap
    char c = scroller.text[charIdx];
    for (int row = 0; row < FONT_H; row++) {
      if (fontPixel(c, charCol, row)) {
        fbSet(sx, row + yOffset, scroller.brightness);
      }
    }
  }
  scroller.pos++;
  if (scroller.pos >= scroller.textPxWidth) scroller.done = true;
  return scroller.done;
}

// ============================================================ fireworks ====

struct Particle {
  int8_t x16, y16;     // position in 1/16 pixel units (so we can use small ints)
  int8_t vx, vy;       // velocity in 1/16 px per tick
  uint8_t life;        // ticks remaining; 0 = inactive
  uint8_t color;       // brightness 0..255
  uint8_t kind;        // 0 = rocket, 1 = spark
};

#define MAX_PARTICLES 48
static Particle particles[MAX_PARTICLES];

static Particle* spawnParticle() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life == 0) return &particles[i];
  }
  return nullptr;
}

static void fwInit() {
  memset(particles, 0, sizeof(particles));
}

static void fwSpawnRocket() {
  Particle* p = spawnParticle();
  if (!p) return;
  p->x16 = (random(W)) * 16 + 8;
  p->y16 = (H - 1) * 16;
  p->vx  = 0;
  p->vy  = -(int8_t)(random(8, 14));   // up
  p->life = (uint8_t)random(8, 14);
  p->color = 220;
  p->kind = 0;
}

static void fwExplode(int x16, int y16) {
  const int N = 9;
  for (int i = 0; i < N; i++) {
    Particle* p = spawnParticle();
    if (!p) return;
    p->x16 = x16;
    p->y16 = y16;
    // distribute velocity around a circle, deterministic-ish but jittered
    int angle = (i * 360 / N) + random(-15, 15);
    int spd   = random(6, 12);
    // tiny lookup: 8-direction sine/cosine in 1/16 units
    static const int8_t COS8[8] = { 16, 11,  0,-11,-16,-11,  0, 11};
    static const int8_t SIN8[8] = {  0, 11, 16, 11,  0,-11,-16,-11};
    int slot = ((angle % 360) + 360) % 360 / 45;
    p->vx = (COS8[slot] * spd) / 16;
    p->vy = (SIN8[slot] * spd) / 16;
    p->life = (uint8_t)random(8, 16);
    p->color = (uint8_t)random(150, 255);
    p->kind = 1;
  }
}

static void fwStep() {
  // Maybe launch a new rocket
  if (random(100) < 20) fwSpawnRocket();

  // Update particles
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle& p = particles[i];
    if (p.life == 0) continue;
    p.x16 += p.vx;
    p.y16 += p.vy;
    if (p.kind == 1) p.vy += 1;   // gravity on sparks
    p.life--;
    if (p.life == 0 && p.kind == 0) {
      // Rocket reached top of arc -> burst into sparks at this position
      fwExplode(p.x16, p.y16);
    }
  }

  // Render
  fbClear();
  for (int i = 0; i < MAX_PARTICLES; i++) {
    Particle& p = particles[i];
    if (p.life == 0) continue;
    int x = p.x16 / 16;
    int y = p.y16 / 16;
    uint8_t b = (p.kind == 0) ? p.color
                              : (uint8_t)((int)p.color * p.life / 16);
    fbOr(x, y, b);
  }
  fbPush();
}

// ============================================================ matrix anims ==
//
// Visual idle animations for the screensaver and the Animations menu.
// Each *Step function writes directly to fb and calls fbPush.
// MATRIX_ANIM_SCROLL uses the existing text scroller — handled by the caller.
// matrixAnimReset() clears per-animation buffers when switching modes.

static uint8_t s_twinkle[H][W];
static uint8_t s_rain[H][W];
static unsigned long s_rainLast = 0;

static void matrixAnimReset() {
  memset(s_twinkle, 0, sizeof(s_twinkle));
  memset(s_rain,    0, sizeof(s_rain));
  s_rainLast = 0;
}

// WAVE — a sine wave scrolling horizontally across all nine rows
static void matrixWaveStep() {
  fbClear();
  float t = (float)millis() / 300.0f;
  for (int x = 0; x < W; x++) {
    float s = sinf((float)x * 0.8f - t);
    int   cy = 4 + (int)(s * 3.5f);
    cy = constrain(cy, 0, H - 1);
    fbSet(x, cy,     255);
    if (cy > 0)     fbSet(x, cy - 1, 80);
    if (cy < H - 1) fbSet(x, cy + 1, 80);
  }
  fbPush();
}

// TWINKLE — random pixels flare up and fade independently
static void matrixTwinkleStep() {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      s_twinkle[y][x] = s_twinkle[y][x] > 20 ? s_twinkle[y][x] - 20 : 0;
  for (int n = 0; n < 3; n++)
    if (random(4) == 0)
      s_twinkle[random(H)][random(W)] = 255;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      fbSet(x, y, s_twinkle[y][x]);
  fbPush();
}

// RAIN — bright pixels spawn at the top and fall, fading as they descend
static void matrixRainStep() {
  unsigned long now = millis();
  if (now - s_rainLast >= 80) {
    s_rainLast = now;
    for (int y = H - 1; y > 0; y--)
      for (int x = 0; x < W; x++)
        s_rain[y][x] = s_rain[y - 1][x] > 50 ? s_rain[y - 1][x] - 50 : 0;
    for (int x = 0; x < W; x++)
      s_rain[0][x] = (random(4) == 0) ? 255 : 0;
  }
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      fbSet(x, y, s_rain[y][x]);
  fbPush();
}

// PULSE — whole screen breathes in and out together
static void matrixPulseStep() {
  float   t = (float)millis() / 800.0f * 2.0f * 3.14159265f;
  uint8_t b = (uint8_t)((sinf(t) + 1.0f) * 0.5f * 255.0f);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      fbSet(x, y, b);
  fbPush();
}

// PLASMA — interference pattern of three overlapping sine waves
static void matrixPlasmaStep() {
  float t = (float)millis() / 600.0f;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      float v = sinf(x * 1.1f + t)
              + sinf(y * 0.9f + t * 0.7f)
              + sinf((x + y) * 0.5f + t * 1.3f);
      fbSet(x, y, (uint8_t)((v / 3.0f + 1.0f) * 0.5f * 255.0f));
    }
  }
  fbPush();
}

// SWIRL — a single spiral arm rotating outward from the centre
static void matrixSwirlStep() {
  float t  = (float)millis() / 500.0f;
  float cx = (W - 1) * 0.5f;
  float cy = (H - 1) * 0.5f;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      float dx  = x - cx;
      float dy  = y - cy;
      float ang = atan2f(dy, dx);
      float d   = sqrtf(dx * dx + dy * dy);
      float v   = sinf(ang * 2.0f - d * 1.2f + t * 2.5f);
      fbSet(x, y, (uint8_t)((v + 1.0f) * 0.5f * 255.0f));
    }
  }
  fbPush();
}

// COUNTER-SWIRL — two spiral arms rotating in opposite directions
static void matrixCounterSwirlStep() {
  float t  = (float)millis() / 500.0f;
  float cx = (W - 1) * 0.5f;
  float cy = (H - 1) * 0.5f;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      float dx  = x - cx;
      float dy  = y - cy;
      float ang = atan2f(dy, dx);
      float d   = sqrtf(dx * dx + dy * dy);
      float v1  = sinf(ang * 3.0f - d * 1.5f + t * 2.5f);
      float v2  = sinf(ang * 3.0f + d * 1.5f - t * 2.0f);
      fbSet(x, y, (uint8_t)(((v1 + v2) * 0.5f + 1.0f) * 0.5f * 255.0f));
    }
  }
  fbPush();
}

// Dispatch to the correct animation based on g_cfg.matrixAnim.
static void matrixAnimStep() {
  switch (g_cfg.matrixAnim) {
    case MATRIX_ANIM_WAVE:    matrixWaveStep();    break;
    case MATRIX_ANIM_TWINKLE: matrixTwinkleStep(); break;
    case MATRIX_ANIM_RAIN:    matrixRainStep();    break;
    case MATRIX_ANIM_PULSE:   matrixPulseStep();   break;
    case MATRIX_ANIM_PLASMA:  matrixPlasmaStep();  break;
    default: break;
  }
}

// ============================================================ snake ========

struct Snake {
  int8_t bodyX[81];
  int8_t bodyY[81];
  int    length;
  int    dx, dy;     // direction
  int    foodX, foodY;
  unsigned long lastStep;
  unsigned long stepMs;
  bool   gameOver;
  bool   exitRequested;
};

static Snake snake;

static void snakePlaceFood() {
  for (int tries = 0; tries < 200; tries++) {
    int fx = random(W);
    int fy = random(H);
    bool onBody = false;
    for (int i = 0; i < snake.length; i++) {
      if (snake.bodyX[i] == fx && snake.bodyY[i] == fy) { onBody = true; break; }
    }
    if (!onBody) { snake.foodX = fx; snake.foodY = fy; return; }
  }
  // Fallback (rare)
  snake.foodX = 0; snake.foodY = 0;
}

static void snakeReset() {
  snake.length = 3;
  for (int i = 0; i < snake.length; i++) {
    snake.bodyX[i] = W / 2 - i;
    snake.bodyY[i] = H / 2;
  }
  snake.dx = 1; snake.dy = 0;
  snake.lastStep = 0;
  snake.stepMs = 220;
  snake.gameOver = false;
  snake.exitRequested = false;
  snakePlaceFood();
}

static void snakeStep() {
  // Input — direction changes
  if (input.pressed[BI_UP]    && snake.dy !=  1) { snake.dx = 0; snake.dy = -1; }
  if (input.pressed[BI_DOWN]  && snake.dy != -1) { snake.dx = 0; snake.dy =  1; }
  if (input.pressed[BI_LEFT]  && snake.dx !=  1) { snake.dx = -1; snake.dy = 0; }
  if (input.pressed[BI_RIGHT] && snake.dx != -1) { snake.dx =  1; snake.dy = 0; }
  if (input.pressed[BI_B])    { snake.exitRequested = true; return; }
  if (input.pressed[BI_A])    { snake.stepMs = (snake.stepMs > 80) ? snake.stepMs - 30 : 80; }

  unsigned long now = millis();
  if (!snake.gameOver && now - snake.lastStep >= snake.stepMs) {
    snake.lastStep = now;

    int newX = ((snake.bodyX[0] + snake.dx) + W) % W;
    int newY = ((snake.bodyY[0] + snake.dy) + H) % H;

    // self-collision
    for (int i = 0; i < snake.length; i++) {
      if (snake.bodyX[i] == newX && snake.bodyY[i] == newY) {
        snake.gameOver = true;
        break;
      }
    }

    if (!snake.gameOver) {
      // shift body
      for (int i = snake.length; i > 0; i--) {
        snake.bodyX[i] = snake.bodyX[i-1];
        snake.bodyY[i] = snake.bodyY[i-1];
      }
      snake.bodyX[0] = newX;
      snake.bodyY[0] = newY;
      // eat
      if (newX == snake.foodX && newY == snake.foodY) {
        if (snake.length < 80) snake.length++;
        snakePlaceFood();
        snake.stepMs = (snake.stepMs > 90) ? snake.stepMs - 4 : 90;
      }
    }
  }

  // Render
  fbClear();
  fbSet(snake.foodX, snake.foodY, 220);
  for (int i = 0; i < snake.length; i++) {
    uint8_t b = (i == 0) ? 200 : (uint8_t)max(40, 160 - i * 8);
    fbSet(snake.bodyX[i], snake.bodyY[i], b);
  }
  if (snake.gameOver) {
    // dim flash overlay so the game over is visible
    uint8_t pulse = (millis() / 200) & 1 ? 80 : 20;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        fbOr(x, y, pulse);
    if (input.pressed[BI_A] || input.pressed[BI_B]) {
      snake.exitRequested = true;
    }
  }
  fbPush();
}

// ============================================================ simon ========
//
// Sequence-repeat game. Each "color" is one of the four directional buttons,
// shown on the matching front LED + a coloured arrow on the matrix.

enum SimonPhase { SP_INTRO, SP_PLAYBACK, SP_INPUT, SP_FAIL, SP_DONE };

struct Simon {
  int sequence[64];          // each entry = BI_UP/DOWN/LEFT/RIGHT
  int length;
  int playbackIdx;
  int inputIdx;
  unsigned long phaseEnter;
  unsigned long lastEvent;
  SimonPhase phase;
  bool exitRequested;
};

static Simon simon;

static int simonBtnToFront(int bi) {
  switch (bi) {
    case BI_UP:    return 0;
    case BI_RIGHT: return 1;
    case BI_DOWN:  return 2;
    case BI_LEFT:  return 3;
  }
  return -1;
}

// Draw a small arrow on the matrix for a given direction button.
// Arrows designed so the arrowhead contains no diagonal pixels (x == y).
// The shaft passes through (4,4) which is a non-existent diagonal LED;
// that pixel is simply omitted — the fbSet guard silently drops it anyway.
// The resulting one-pixel gap in the shaft is a hardware reality.
static void simonDrawArrow(int bi, uint8_t b) {
  fbClear();
  if (bi == BI_UP) {
    // tip
    fbSet(2, 3, b);
    // arrowhead — all pixels clear of diagonal
    fbSet(3, 4, b);
    fbSet(1, 4, b);
    // shaft — (4,4) is diagonal, not drawn; gap is intentional
    fbSet(2, 4, b); fbSet(2, 5, b); fbSet(2, 6, b); fbSet(2, 7, b);
  } else if (bi == BI_DOWN) {
    // shaft — (4,4) is diagonal, not drawn; gap is intentional
    fbSet(2, 3, b); fbSet(2, 4, b); fbSet(2, 5, b); fbSet(2, 6, b);
    // head
    fbSet(3, 6, b); fbSet(1, 6, b);
    // tip
    fbSet(2, 7, b);
  } else if (bi == BI_LEFT) {
    // tip
    fbSet(0, 5, b);
    // head
    fbSet(1, 6, b); fbSet(1, 4, b);
    // shaft
    fbSet(1, 5, b); fbSet(2, 5, b); fbSet(3, 5, b); fbSet(4, 5, b);

  } else if (bi == BI_RIGHT) {
    // tip
    fbSet(4,5,b);
    // head
    fbSet(3,4,b); fbSet(3,6,b);
    //Shaft
    fbSet(0,5,b); fbSet(1, 5, b); fbSet(2, 5, b); fbSet(3, 5, b);
  }
}

static void simonNextRound() {
  static const int CHOICES[4] = { BI_UP, BI_DOWN, BI_LEFT, BI_RIGHT };
  if (simon.length < 64) {
    simon.sequence[simon.length++] = CHOICES[random(4)];
  }
  simon.playbackIdx = 0;
  simon.inputIdx = 0;
  simon.phase = SP_PLAYBACK;
  simon.phaseEnter = millis();
  simon.lastEvent = 0;
  frontAll(FRONT_LED_OFF);
}

static void simonReset() {
  simon.length = 0;
  simon.playbackIdx = 0;
  simon.inputIdx = 0;
  simon.exitRequested = false;
  simon.phase = SP_INTRO;
  simon.phaseEnter = millis();
  fbClear(); fbPush();
  frontAll(FRONT_LED_OFF);
}

static void simonStep() {
  if (input.pressed[BI_B]) { simon.exitRequested = true; frontAll(FRONT_LED_OFF); return; }

  unsigned long now = millis();
  unsigned long elapsed = now - simon.phaseEnter;

  switch (simon.phase) {
    case SP_INTRO: {
      // Pulse all four front LEDs once, then start round 1
      bool on = ((elapsed / 200) & 1) == 0;
      frontAll(on ? FRONT_LED_ON : FRONT_LED_OFF);
      // draw a "1" on the matrix to hint round 1
      fbClear();
      for (int y = 1; y <= 7; y++) fbSet(4, y, 160);
      fbSet(3, 2, 160);
      fbPush();
      if (elapsed > 800) simonNextRound();
      break;
    }
    case SP_PLAYBACK: {
      // Per-step: 450 ms on, 200 ms off, until done. Then move to INPUT.
      const unsigned long onMs  = 420;
      const unsigned long offMs = 180;
      const unsigned long perStep = onMs + offMs;
      unsigned long stepIdx = elapsed / perStep;
      unsigned long withinStep = elapsed % perStep;

      if ((int)stepIdx >= simon.length) {
        // playback finished
        frontAll(FRONT_LED_OFF);
        fbClear(); fbPush();
        simon.phase = SP_INPUT;
        simon.phaseEnter = now;
        simon.inputIdx = 0;
        break;
      }
      int bi = simon.sequence[stepIdx];
      int fi = simonBtnToFront(bi);
      if (withinStep < onMs) {
        frontAll(FRONT_LED_OFF);
        frontSet(fi, FRONT_LED_ON);
        simonDrawArrow(bi, 220);
        fbPush();
      } else {
        frontAll(FRONT_LED_OFF);
        fbClear(); fbPush();
      }
      break;
    }
    case SP_INPUT: {
      // Wait for player to press the sequence in order.
      // On each press, briefly flash the matching front LED.
      for (int bi : {BI_UP, BI_DOWN, BI_LEFT, BI_RIGHT}) {
        if (input.pressed[bi]) {
          int fi = simonBtnToFront(bi);
          frontAll(FRONT_LED_OFF);
          frontSet(fi, FRONT_LED_ON);
          simonDrawArrow(bi, 220);
          fbPush();
          simon.lastEvent = now;

          if (bi != simon.sequence[simon.inputIdx]) {
            simon.phase = SP_FAIL;
            simon.phaseEnter = now;
            return;
          }
          simon.inputIdx++;
          if (simon.inputIdx >= simon.length) {
            // round complete -> wait a moment, then add another
            simon.phase = SP_DONE;
            simon.phaseEnter = now;
            return;
          }
        }
      }
      // turn off feedback after 200ms
      if (simon.lastEvent && (now - simon.lastEvent) > 200) {
        frontAll(FRONT_LED_OFF);
        fbClear(); fbPush();
        simon.lastEvent = 0;
      }
      break;
    }
    case SP_DONE: {
      // Brief celebratory blink, then next round
      bool on = ((elapsed / 100) & 1) == 0;
      frontAll(on ? FRONT_LED_ON : FRONT_LED_OFF);
      if (elapsed > 600) simonNextRound();
      break;
    }
    case SP_FAIL: {
      // Pulse all front LEDs and red-out the matrix, then return to menu
      bool on = ((elapsed / 150) & 1) == 0;
      frontAll(on ? FRONT_LED_ON : FRONT_LED_OFF);
      uint8_t b = on ? 200 : 40;
      for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
          fbSet(x, y, b);
      fbPush();
      if (elapsed > 1500 || input.pressed[BI_A] || input.pressed[BI_B]) {
        simon.exitRequested = true;
        frontAll(FRONT_LED_OFF);
      }
      break;
    }
  }
}

// ============================================================ settings =======
//
// Brightness selector: UP/DOWN steps through 8 levels.
// The matrix shows a horizontal bar (rows 3-5) that fills left-to-right.
// Changes are applied and persisted immediately so the user sees the effect.
// A or B returns to the menu.

struct SettingsState {
  uint8_t brightness;    // working copy of g_brightness while in this screen
  bool    exitRequested;
};
static SettingsState settingsCtx;

static void settingsReset() {
  settingsCtx.brightness    = g_brightness;
  settingsCtx.exitRequested = false;
}

static void settingsStep() {
  if (input.pressed[BI_A] || input.pressed[BI_B]) {
    settingsCtx.exitRequested = true;
    return;
  }

  bool changed = false;
  if ((input.pressed[BI_UP] || input.pressed[BI_RIGHT]) && settingsCtx.brightness < 8) {
    settingsCtx.brightness++;
    changed = true;
  }
  if ((input.pressed[BI_DOWN] || input.pressed[BI_LEFT]) && settingsCtx.brightness > 1) {
    settingsCtx.brightness--;
    changed = true;
  }
  if (changed) { g_brightness = settingsCtx.brightness; configSaveBrightness(); }

  // — Draw brightness display ———————————————————————————
  // Rows 2-6: full-width bar at pixel value 255.  Because fbPush scales every
  // pixel by g_brightness/8, the bar itself gets brighter/dimmer as you adjust
  // — the display IS the preview of the selected brightness.
  // Rows 0 and 8: eight small tick marks (cols 0-7) showing the current level;
  // the active tick is full-bright, the rest are dim so the position is clear.
  fbClear();
  for (int x = 0; x < W; x++) {
    fbSet(x, 2, 255); fbSet(x, 3, 255);
    fbSet(x, 4, 255);
    fbSet(x, 5, 255); fbSet(x, 6, 255);
  }
  int cur = settingsCtx.brightness - 1;   // 0..7
  for (int x = 0; x < 8; x++) {
    uint8_t tick = (x == cur) ? 255 : 40;
    fbSet(x, 0, tick);
    fbSet(x, 8, tick);
  }
  fbPush();
}

// ============================================================ IR sender ======
//
// Software IR modulation on LED_FLASHLIGHT (GPIO15 → MOSFET → IR/white LEDs).
// Implements NEC (32-bit) and Samsung (which is NEC-compatible but with a
// repeated address byte).  Sony SIRC-12 is also supported.
//
// Blocking during transmission (~67 ms for NEC), but called only on user input.

#define IR_PIN       LED_FLASHLIGHT
#define IR_FREQ_NEC  38000
#define IR_FREQ_SONY 40000

static void irOn(uint16_t us) {
  // 33 % duty is standard for IR LEDs — avoids overheating
  analogWrite(IR_PIN, 333);
  delayMicroseconds(us);
}
static void irOff(uint16_t us) {
  analogWrite(IR_PIN, 0);
  delayMicroseconds(us);
}

static void irSendNEC(uint16_t address, uint8_t command, bool samsung = false) {
  analogWriteFreq(IR_FREQ_NEC);
  analogWriteRange(1000);
  // Header: 9 ms burst + 4.5 ms space
  irOn(9000);
  irOff(samsung ? 4500 : 4500);   // Samsung header is identical to NEC here
  // 16-bit address (8-bit addr + ~addr for NEC; 8-bit addr repeated for Samsung)
  uint8_t a1 = (uint8_t)(address & 0xFF);
  uint8_t a2 = samsung ? a1 : (uint8_t)(~a1 & 0xFF);
  uint8_t c1 = command;
  uint8_t c2 = (uint8_t)(~c1 & 0xFF);
  for (int i = 0; i < 8; i++) { irOn(562); irOff((a1 >> i) & 1 ? 1687 : 562); }
  for (int i = 0; i < 8; i++) { irOn(562); irOff((a2 >> i) & 1 ? 1687 : 562); }
  for (int i = 0; i < 8; i++) { irOn(562); irOff((c1 >> i) & 1 ? 1687 : 562); }
  for (int i = 0; i < 8; i++) { irOn(562); irOff((c2 >> i) & 1 ? 1687 : 562); }
  irOn(562);
  analogWrite(IR_PIN, 0);
}

static void irSendSony(uint16_t address, uint8_t command) {
  analogWriteFreq(IR_FREQ_SONY);
  analogWriteRange(1000);
  irOn(2400); irOff(600);
  for (int i = 6; i >= 0; i--) { irOn((command >> i) & 1 ? 1200 : 600); irOff(600); }
  for (int i = 4; i >= 0; i--) { irOn((address >> i) & 1 ? 1200 : 600); irOff(600); }
  analogWrite(IR_PIN, 0);
}

static void irSendCode(const IrCode& c) {
  uint16_t addr = irCodeAddr(c);
  switch (c.protocol) {
    case IR_PROTO_NEC:     irSendNEC(addr, c.command, false); break;
    case IR_PROTO_SAMSUNG: irSendNEC(addr, c.command, true);  break;
    case IR_PROTO_SONY:    irSendSony(addr, c.command);       break;
    case IR_PROTO_RC5:     irSendNEC(addr, c.command, false); break; // fallback
  }
}

// ============================================================ flashlight =====
//
// All four front LEDs continue their Knight Rider animation in the background.
// LED_FLASHLIGHT (GPIO15) is PWM'd at variable duty for the flashlight.
//   Level 8 → duty 1000 (constant on, maximum output)
//   Level 1 → duty 125 (dim — still on, no strobe at minimum)
// Up/Down → adjust brightness level.   B → exit to menu.

struct FlashState {
  bool exitRequested;
};
static FlashState flashCtx;

static void flashApply() {
  int duty = (int)((float)g_flashBright / 8.0f * 1000.0f);
  analogWriteFreq(1000);
  analogWriteRange(1000);
  analogWrite(IR_PIN, duty);
}

static void flashReset() {
  flashCtx.exitRequested = false;
  flashApply();
}

static void flashStep() {
  if (input.pressed[BI_B]) {
    flashCtx.exitRequested = true;
    analogWrite(IR_PIN, 0);   // off when leaving
    return;
  }
  bool changed = false;
  if (input.pressed[BI_UP]   && g_flashBright < 8) { g_flashBright++; changed = true; }
  if (input.pressed[BI_DOWN] && g_flashBright > 1) { g_flashBright--; changed = true; }
  if (changed) { configSaveFlash(); flashApply(); }

  // Draw brightness level bar on matrix (rows 3-5, same style as Settings)
  fbClear();
  uint8_t bar = 180;
  for (int x = 0; x < g_flashBright; x++) {
    fbSet(x, 3, bar);
    fbSet(x, 4, bar);
    fbSet(x, 5, bar);
  }
  int edge = g_flashBright - 1;
  fbSet(edge, 1, 255);
  fbSet(edge, 7, 255);
  // Torch icon — two pixels above bar
  fbSet(3, 0, 200); fbSet(4, 0, 255); fbSet(5, 0, 200);
  fbPush();
}

// ============================================================ IR remote ======
//
// Each directional/A/B button sends the configured IR code.
// A+B held simultaneously exits.  The matrix shows a remote icon.
// The front LEDs continue Knight Rider in the background.

static unsigned long irLastSend = 0;
static bool irExitRequested = false;

static void irRemoteReset() {
  irExitRequested = false;
  irLastSend = 0;
}

static void irRemoteDrawIcon() {
  fbClear();
  // Stylised remote outline on 9×9 matrix
  for (int y = 1; y <= 7; y++) { fbSet(2, y, 80); fbSet(6, y, 80); }
  for (int x = 2; x <= 6; x++) { fbSet(x, 1, 80); fbSet(x, 7, 80); }
  fbSet(4, 2, 200);  // top button
  fbSet(3, 4, 120); fbSet(5, 4, 120);  // side buttons
  fbSet(4, 5, 120);  // bottom button
  fbPush();
}

static void irRemoteStep() {
  // Exit on A+B held together
  if (input.down[BI_A] && input.down[BI_B]) {
    irExitRequested = true;
    analogWrite(IR_PIN, 0);
    return;
  }

  unsigned long now = millis();
  // 200 ms gap between sends to avoid flooding
  if (now - irLastSend < 200) { irRemoteDrawIcon(); return; }

  static const int BTN_MAP[IR_BTN_COUNT] = {
    BI_A, BI_B, BI_UP, BI_DOWN, BI_LEFT, BI_RIGHT
  };
  for (int i = 0; i < IR_BTN_COUNT; i++) {
    if (input.pressed[BTN_MAP[i]]) {
      // Flash all front LEDs briefly to indicate send
      for (int p : FRONT_LEDS) analogWrite(p, 1000);
      irSendCode(g_cfg.ir[i]);
      for (int p : FRONT_LEDS) analogWrite(p, 0);
      irLastSend = now;
      break;
    }
  }
  irRemoteDrawIcon();
}

// ============================================================ IR havoc =======
//
// Cycles through a built-in table of power/source codes for common TV brands
// and transmits them one by one, letting the RF spray across the room.
//
// The matrix shows a plasma animation in the background; each transmission
// triggers a brief white flash on the matrix and a front-LED pulse.
//
// Controls (while active):
//   B — exit back to the menu
//
// Configure via the serial CLI:
//   havoc codes power|input|all   — which code types to send
//   havoc delay <100-2000>        — ms between sends
//
// NOTE: codes are best-effort approximations of documented IR databases.
// Many TV models use non-standard addresses or commands — YMMV.

struct TvCode {
  const char* brand;
  uint8_t     proto;    // IR_PROTO_*
  uint16_t    addr;
  uint8_t     power;    // 0xFF = no code known for this type
  uint8_t     input;    // 0xFF = no code known for this type
};

static const TvCode TV_CODES[] = {
  //  Brand           Protocol           Addr    Power  Input/Source
  { "SAMSUNG",    IR_PROTO_SAMSUNG, 0x0707, 0x02,  0x01 },
  { "LG",         IR_PROTO_NEC,     0x0004, 0x08,  0x0B },
  { "SONY",       IR_PROTO_SONY,    0x0001, 0x15,  0x25 },
  { "PHILIPS",    IR_PROTO_RC5,     0x0000, 0x0C,  0xFF },
  { "PANASONIC",  IR_PROTO_NEC,     0x4004, 0x3D,  0x3B },
  { "TOSHIBA",    IR_PROTO_NEC,     0x0002, 0x12,  0x34 },
  { "SHARP",      IR_PROTO_NEC,     0x00AA, 0x02,  0xA9 },
  { "VIZIO",      IR_PROTO_NEC,     0x0F0F, 0x09,  0x0C },
  { "HISENSE",    IR_PROTO_NEC,     0x0000, 0x46,  0x38 },
  { "TCL",        IR_PROTO_NEC,     0x0000, 0x4D,  0x37 },
  { "JVC",        IR_PROTO_NEC,     0x00C5, 0x29,  0x0E },
  { "HITACHI",    IR_PROTO_NEC,     0x0001, 0x60,  0x62 },
  { "GRUNDIG",    IR_PROTO_RC5,     0x0000, 0x0C,  0xFF },
  { "FUNAI",      IR_PROTO_NEC,     0x0000, 0x28,  0x2E },
  { "BEKO",       IR_PROTO_NEC,     0x01FE, 0x40,  0x6F },
};
static const int TV_CODE_COUNT = sizeof(TV_CODES) / sizeof(TV_CODES[0]);

// Helper: build a temporary IrCode on the stack and transmit it.
static void irHavocSend(uint8_t proto, uint16_t addr, uint8_t cmd) {
  IrCode c;
  c.protocol = proto;
  c.addr_lo  = (uint8_t)(addr & 0xFF);
  c.addr_hi  = (uint8_t)(addr >> 8);
  c.command  = cmd;
  irSendCode(c);
}

struct HavocCtx {
  uint8_t       tvIdx;        // which TV brand we're currently targeting
  uint8_t       codePhase;    // 0 = power, 1 = input
  unsigned long lastSend;     // millis() of last transmission
  uint8_t       flashFrames;  // frames remaining for post-send matrix flash
  uint8_t       animIdx;      // cycles through swirl → counter-swirl → plasma
  bool          exitRequested;
};
static HavocCtx havocCtx;

// Advance tvIdx / codePhase to the next valid code, respecting havocCodes mask.
static void havocAdvance() {
  bool wantPower = (g_cfg.havocCodes & HAVOC_SEND_POWER) != 0;
  bool wantInput = (g_cfg.havocCodes & HAVOC_SEND_INPUT) != 0;
  for (int tries = 0; tries < TV_CODE_COUNT * 2 + 4; tries++) {
    // Step forward
    if (havocCtx.codePhase == 0) {
      havocCtx.codePhase = 1;
    } else {
      havocCtx.codePhase = 0;
      havocCtx.tvIdx = (havocCtx.tvIdx + 1) % TV_CODE_COUNT;
    }
    const TvCode& tv = TV_CODES[havocCtx.tvIdx];
    if (havocCtx.codePhase == 0 && wantPower && tv.power != 0xFF) return;
    if (havocCtx.codePhase == 1 && wantInput && tv.input != 0xFF) return;
  }
  // Fallback — nothing matched (e.g. havocCodes = 0): reset silently
  havocCtx.tvIdx = 0; havocCtx.codePhase = 0;
}

static void havocReset() {
  havocCtx.tvIdx        = 0;
  havocCtx.codePhase    = 0;
  havocCtx.lastSend     = 0;
  havocCtx.flashFrames  = 0;
  havocCtx.animIdx      = 0;
  havocCtx.exitRequested = false;
  matrixAnimReset();
}

static void havocStep() {
  if (input.pressed[BI_B]) {
    havocCtx.exitRequested = true;
    analogWrite(IR_PIN, 0);
    return;
  }

  unsigned long now     = millis();
  unsigned long delayMs = (unsigned long)g_cfg.havocDelay * 100UL;

  if (now - havocCtx.lastSend >= delayMs) {
    havocCtx.lastSend = now;

    const TvCode& tv = TV_CODES[havocCtx.tvIdx];
    uint8_t cmd = (havocCtx.codePhase == 0) ? tv.power : tv.input;

    if (cmd != 0xFF) {
      for (int p : FRONT_LEDS) analogWrite(p, 1000);
      irHavocSend(tv.proto, tv.addr, cmd);
      for (int p : FRONT_LEDS) analogWrite(p, 0);
      havocCtx.flashFrames = 4;
      // Step the background animation on each successful send
      havocCtx.animIdx = (havocCtx.animIdx + 1) % 3;
    }
    havocAdvance();
  }

  // Matrix: full-white flash on send, then cycle swirl → counter-swirl → plasma
  if (havocCtx.flashFrames > 0) {
    havocCtx.flashFrames--;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        fbSet(x, y, 255);
    fbPush();
  } else {
    switch (havocCtx.animIdx) {
      case 0: matrixSwirlStep();        break;
      case 1: matrixCounterSwirlStep(); break;
      default: matrixPlasmaStep();      break;
    }
  }
}

// ============================================================ welcome msgs ==
//
// One of these is chosen at random each boot for the ST_WELCOME_SCROLL state.
// Add or remove entries freely — the count is derived automatically.

// Messages containing "%s" are personalised — the badge owner's name is
// substituted at runtime.  They are skipped silently when no name is stored.
// Add new personalised lines here by including "%s" anywhere in the string.
static const char* const WELCOME_MSGS[] = {
  "WELCOME, %s TO SF 2026!"
};
static const int WELCOME_MSG_COUNT = sizeof(WELCOME_MSGS) / sizeof(WELCOME_MSGS[0]);

// Expansion buffer — sized for the longest personalised template plus a full name.
static char welcomeNameBuf[40];

// Pick a welcome message.  If the chosen entry contains "%s" and a name is
// stored, the name is substituted and the result returned from welcomeNameBuf.
// Entries that need a name are retried (up to 5 attempts) when no name is set,
// so the pool degrades gracefully for unnamed badges.
static const char* pickWelcomeMsg() {
  for (int attempt = 0; attempt < 5; attempt++) {
    const char* msg = WELCOME_MSGS[random(WELCOME_MSG_COUNT)];
    if (strstr(msg, "%s") == nullptr) return msg;        // no substitution needed
    if (g_cfg.name[0] == '\0')        continue;          // skip — no name available
    snprintf(welcomeNameBuf, sizeof(welcomeNameBuf), msg, g_cfg.name);
    return welcomeNameBuf;
  }
  return WELCOME_MSGS[0];  // safe fallback (always a plain string)
}

// ============================================================ menu =========

const char* MENU_ITEMS[] = {
  "SNAKE", "SIMON", "WELCOME", "ANIMATIONS", "SETTINGS", "FLASHLIGHT", "IR REMOTE", "IR HAVOC", "NAME BADGE"
};
const int   MENU_LEN = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
static int  menuIdx  = 0;

// Idle screensaver — timeout and speeds come from g_cfg at runtime
static const char* const IDLE_MSGS[] = {
  "HACK THE PLANET",
  "CTRL ALT DEFEAT",
  "0xDEADBEEF",
  "ROOT ACCESS",
  "PATCH YOUR STUFF",
  "COFFEE POWERED",
  "NO CLOUD PLS",
  "RTFM",
  "STAY PARANOID",
  "GLENN GLENN GLENN",
  "EJ I TRAFIK",
  "FÖRSENINGAR PGA FÖRSENINGAR"
};
static const int IDLE_MSG_COUNT = sizeof(IDLE_MSGS) / sizeof(IDLE_MSGS[0]);

static unsigned long menuLastActivity    = 0;
static bool          menuIdle            = false;
static uint8_t       menuIdleMsgIdx      = 0;
static bool          menuIdlePausing     = false;
static unsigned long menuIdlePauseUntil  = 0;

#define IDLE_PAUSE_MS 1200  // blank gap between idle messages (not configurable)

static void menuEnter() {
  menuLastActivity   = millis();
  menuIdle           = false;
  menuIdlePausing    = false;
  scrollStart(MENU_ITEMS[menuIdx], g_cfg.menuScrollMs, MATRIX_BRIGHTNESS);
}

static void menuStep() {
  if (input.pressed[BI_UP] || input.pressed[BI_LEFT]) {
    menuIdx = (menuIdx + MENU_LEN - 1) % MENU_LEN;
    menuEnter();
    return;
  }
  if (input.pressed[BI_DOWN] || input.pressed[BI_RIGHT]) {
    menuIdx = (menuIdx + 1) % MENU_LEN;
    menuEnter();
    return;
  }

  // Any other button press wakes from idle without consuming the press
  if (anyPressed() && menuIdle) {
    menuIdle         = false;
    menuIdlePausing  = false;
    menuLastActivity = millis();
    scrollStart(MENU_ITEMS[menuIdx], g_cfg.menuScrollMs, MATRIX_BRIGHTNESS);
  }

  // Enter idle if screensaver enabled and inactive long enough
  unsigned long idleMs = (unsigned long)g_cfg.idleTimeoutSec * 1000UL;
  if (g_cfg.idleEnable && !menuIdle && (millis() - menuLastActivity) >= idleMs) {
    menuIdle        = true;
    menuIdlePausing = false;
    if (g_cfg.matrixAnim == MATRIX_ANIM_SCROLL) {
      menuIdleMsgIdx = (uint8_t)random(IDLE_MSG_COUNT);
      scrollStart(IDLE_MSGS[menuIdleMsgIdx], g_cfg.idleScrollMs, MATRIX_BRIGHTNESS);
    } else {
      matrixAnimReset();
    }
  }

  // Render idle animation or menu text scroller
  if (menuIdle && g_cfg.matrixAnim != MATRIX_ANIM_SCROLL) {
    matrixAnimStep();
  } else {
    scrollRenderTick();
    if (scroller.done) {
      if (menuIdle) {
        if (!menuIdlePausing) {
          menuIdlePausing    = true;
          menuIdlePauseUntil = millis() + IDLE_PAUSE_MS;
          fbClear();
        } else if (millis() >= menuIdlePauseUntil) {
          menuIdlePausing = false;
          menuIdleMsgIdx  = (menuIdleMsgIdx + 1) % IDLE_MSG_COUNT;
          scrollStart(IDLE_MSGS[menuIdleMsgIdx], g_cfg.idleScrollMs, MATRIX_BRIGHTNESS);
        }
      } else {
        scrollStart(MENU_ITEMS[menuIdx], g_cfg.menuScrollMs, MATRIX_BRIGHTNESS);
      }
    }
    fbPush();
  }
}

// ============================================================ state mach. ==

enum AppState {
  ST_BOOT_SWEEP,
  ST_WELCOME_SCROLL,
  ST_FIREWORKS,
  ST_MENU,
  ST_SNAKE,
  ST_SIMON,
  ST_ANIMATIONS,
  ST_SETTINGS,
  ST_FLASHLIGHT,
  ST_IR_REMOTE,
  ST_IR_HAVOC,
  ST_NAME_BADGE,
  ST_GAMEPAD,
};

// ============================================================ name badge ====
//
// Scrolls the owner's name (set via serial CONFIG → name command) on the
// matrix in a continuous loop.  B exits back to the menu.

struct NameBadgeState {
  bool exitRequested;
};
static NameBadgeState nameBadgeCtx;

static void nameBadgeReset() {
  nameBadgeCtx.exitRequested = false;
  const char* txt = (g_cfg.name[0] != '\0') ? g_cfg.name : "SET YOUR NAME";
  scrollStart(txt, g_cfg.nameScrollMs, MATRIX_BRIGHTNESS);
}

static void nameBadgeStep() {
  if (input.pressed[BI_B]) { nameBadgeCtx.exitRequested = true; return; }
  scrollRenderTick();
  if (scroller.done) {
    const char* txt = (g_cfg.name[0] != '\0') ? g_cfg.name : "SET YOUR NAME";
    scrollStart(txt, g_cfg.nameScrollMs, MATRIX_BRIGHTNESS);
  }
  fbPush();
}

// ============================================================ animations ====
//
// Lets the user cycle through all MATRIX_ANIM_* modes and preview them live.
//   LEFT / RIGHT (or UP / DOWN) — cycle through animations
//   A  — save the current selection as the idle animation and exit
//   B  — exit without saving (restores the previous setting)

struct AnimCtx {
  uint8_t origAnim;      // value on entry, restored on B-cancel
  bool    exitRequested;
};
static AnimCtx animCtx;

static void animationsReset() {
  animCtx.origAnim      = g_cfg.matrixAnim;
  animCtx.exitRequested = false;
  matrixAnimReset();
  if (g_cfg.matrixAnim == MATRIX_ANIM_SCROLL) {
    menuIdleMsgIdx = (uint8_t)random(IDLE_MSG_COUNT);
    scrollStart(IDLE_MSGS[menuIdleMsgIdx], g_cfg.idleScrollMs, MATRIX_BRIGHTNESS);
  }
}

static void animationsStep() {
  if (input.pressed[BI_B]) {
    g_cfg.matrixAnim = animCtx.origAnim;   // cancel — restore original
    animCtx.exitRequested = true;
    return;
  }
  if (input.pressed[BI_A]) {
    configSaveSettings();                  // commit preview as idle animation
    animCtx.exitRequested = true;
    return;
  }

  bool changed = false;
  if (input.pressed[BI_RIGHT] || input.pressed[BI_DOWN]) {
    g_cfg.matrixAnim = (g_cfg.matrixAnim + 1) % MATRIX_ANIM_COUNT;
    changed = true;
  }
  if (input.pressed[BI_LEFT] || input.pressed[BI_UP]) {
    g_cfg.matrixAnim = (g_cfg.matrixAnim + MATRIX_ANIM_COUNT - 1) % MATRIX_ANIM_COUNT;
    changed = true;
  }
  if (changed) {
    matrixAnimReset();
    if (g_cfg.matrixAnim == MATRIX_ANIM_SCROLL) {
      menuIdleMsgIdx = (uint8_t)random(IDLE_MSG_COUNT);
      scrollStart(IDLE_MSGS[menuIdleMsgIdx], g_cfg.idleScrollMs, MATRIX_BRIGHTNESS);
    }
  }

  if (g_cfg.matrixAnim == MATRIX_ANIM_SCROLL) {
    scrollRenderTick();
    if (scroller.done) {
      menuIdleMsgIdx = (menuIdleMsgIdx + 1) % IDLE_MSG_COUNT;
      scrollStart(IDLE_MSGS[menuIdleMsgIdx], g_cfg.idleScrollMs, MATRIX_BRIGHTNESS);
    }
    fbPush();
  } else {
    matrixAnimStep();
  }
}

// ============================================================ gamepad mode ===
//
// Activated by holding A while connecting to USB / turning on.
// Presents as a composite CDC + HID gamepad; Serial still works.
//
// Left stick:  UP/DOWN/LEFT/RIGHT axes  (−32767 … 32767)
// Buttons:     A → bit 0,  B → bit 1
//
// Matrix shows a top-down controller outline with live button highlights.
// Hold B for 2 s to exit back to the main menu (HID stays present but idle).

static bool gamepadExitRequested = false;
static bool gamepadIntroDone     = false;

static void gamepadDrawIcon() {
  fbClear();
  const uint8_t OUTLINE = 40;
  const uint8_t BTN_OFF = 70;
  const uint8_t BTN_ON  = 220;

  // Shoulder bumps (row 0)
  fbSet(2, 0, OUTLINE); fbSet(3, 0, OUTLINE);
  fbSet(5, 0, OUTLINE); fbSet(6, 0, OUTLINE);
  // Top body edge (row 1)
  for (int x = 1; x <= 7; x++) fbSet(x, 1, OUTLINE);
  // Body walls (rows 2-4)
  fbSet(0, 2, OUTLINE); fbSet(8, 2, OUTLINE);
  fbSet(0, 3, OUTLINE); fbSet(8, 3, OUTLINE);
  fbSet(0, 4, OUTLINE); fbSet(8, 4, OUTLINE);
  // Lower body (row 5, centre gap for grip)
  fbSet(1,5,OUTLINE); fbSet(2,5,OUTLINE); fbSet(3,5,OUTLINE);
  fbSet(5,5,OUTLINE); fbSet(6,5,OUTLINE); fbSet(7,5,OUTLINE);
  // Grip bumps (row 6)
  fbSet(1,6,OUTLINE); fbSet(2,6,OUTLINE);
  fbSet(6,6,OUTLINE); fbSet(7,6,OUTLINE);

  // D-pad cross (left side, centre at col 2 row 3)
  fbSet(2, 2, input.down[BI_UP]    ? BTN_ON : BTN_OFF);
  fbSet(1, 3, input.down[BI_LEFT]  ? BTN_ON : BTN_OFF);
  fbSet(2, 3, OUTLINE);                                    // cross centre
  fbSet(3, 3, input.down[BI_RIGHT] ? BTN_ON : BTN_OFF);
  fbSet(2, 4, input.down[BI_DOWN]  ? BTN_ON : BTN_OFF);

  // Face buttons A / B (right side)
  fbSet(6, 3, input.down[BI_A] ? BTN_ON : BTN_OFF);
  fbSet(7, 3, input.down[BI_B] ? BTN_ON : BTN_OFF);

  fbPush();
}

static void gamepadReset() {
  gamepadExitRequested = false;
  gamepadIntroDone     = false;
  Joystick.useManualSend(true);   // batch all axis/button updates into one report
  scrollStart("CTRL", 50, MATRIX_BRIGHTNESS);
}

static void gamepadStep() {
  // Left stick axes (0-1023, centre = 511)
  Joystick.X(input.down[BI_LEFT]  ?    0 : input.down[BI_RIGHT] ? 1023 : 511);
  Joystick.Y(input.down[BI_UP]    ?    0 : input.down[BI_DOWN]  ? 1023 : 511);

  // Hat/D-pad (also maps directions for games that read hat instead of axes)
  bool u = input.down[BI_UP], d = input.down[BI_DOWN];
  bool l = input.down[BI_LEFT],  r = input.down[BI_RIGHT];
  if      (u && r) Joystick.hat(HID_Joystick::UP_RIGHT);
  else if (u && l) Joystick.hat(HID_Joystick::UP_LEFT);
  else if (d && r) Joystick.hat(HID_Joystick::DOWN_RIGHT);
  else if (d && l) Joystick.hat(HID_Joystick::DOWN_LEFT);
  else if (u)      Joystick.hat(HID_Joystick::UP);
  else if (d)      Joystick.hat(HID_Joystick::DOWN);
  else if (l)      Joystick.hat(HID_Joystick::LEFT);
  else if (r)      Joystick.hat(HID_Joystick::RIGHT);
  else             Joystick.hat(HID_Joystick::IDLE);

  // Face buttons
  Joystick.setButton(0, input.down[BI_A]);
  Joystick.setButton(1, input.down[BI_B]);
  Joystick.send_now();

  // Scroll "CTRL" intro once, then show live controller icon
  if (!gamepadIntroDone) {
    if (scrollRenderTick()) gamepadIntroDone = true;
    fbPush();
  } else {
    gamepadDrawIcon();
  }

  // Hold B >= 2 s to return to menu
  if (input.down[BI_B] && (millis() - input.heldSince[BI_B]) > 2000) {
    gamepadExitRequested = true;
  }
}

// ============================================================ state mach. ==

static AppState appState = ST_BOOT_SWEEP;
static unsigned long stateEnter = 0;

static void enterState(AppState s) {
  appState = s;
  stateEnter = millis();
  fbClear(); fbPush();
  frontAll(FRONT_LED_OFF);
  switch (s) {
    case ST_WELCOME_SCROLL: scrollStart(pickWelcomeMsg(), 50, MATRIX_BRIGHTNESS); break;
    case ST_FIREWORKS:      fwInit(); break;
    case ST_MENU:           menuEnter(); break;
    case ST_SNAKE:          snakeReset(); break;
    case ST_SIMON:          simonReset(); break;
    case ST_ANIMATIONS:     animationsReset(); break;
    case ST_SETTINGS:       settingsReset(); break;
    case ST_FLASHLIGHT:     flashReset(); break;
    case ST_IR_REMOTE:      irRemoteReset(); break;
    case ST_IR_HAVOC:       havocReset(); break;
    case ST_NAME_BADGE:     nameBadgeReset(); break;
    case ST_GAMEPAD:        gamepadReset(); break;
    default: break;
  }
}

// ============================================================ SAO PWM ========
//
// GP1 (GPIO0): hardware PWM, duty cycle follows sin(time) — full 0-100 % swing,
//              period ≈ 3 seconds.
// GP2 (GPIO1): hardware PWM at a constant 10 % duty cycle.
//
// Both channels run at 1 kHz. analogWriteRange(1000) maps 0..1000 = 0..100 %.

static void saoSetup() {
  analogWriteFreq(1000);    // 1 kHz PWM carrier
  analogWriteRange(1000);   // 0..1000 = 0..100 %
  analogWrite(SAO_GP2, 30); // constant 10 %
  analogWrite(SAO_GP1, 0);   // will be updated every loop
}

static void saoUpdate() {
  // sin() oscillates -1..+1 with a ~3-second period.
  // Mapped to 0..1000 for the PWM duty register.
  float t    = (float)millis() / 1000.0f * 2.0f * 3.14159265f;
  float s    = sinf(t);                         // -1 .. +1
  int   duty = (int)((s + 1.0f) * 500.0f);     // 0 .. 1000
  duty = (duty>500)*1000;
  analogWrite(SAO_GP1, duty);
  analogWrite(SAO_GP2, ((s+1)/2.0)*1000);
}

// ============================================================ front LED animations =====
//
// Dispatches to one of several animation modes based on g_cfg.ledAnim.
// Snake / Simon / IR Remote own the front LEDs — all other states use this.
//
// All animations respect g_brightness:
//   8 → full brightness; lower values dim the peak and raise the floor for
//       KNIGHT mode, or scale the peak duty for other animations.

static void frontLedUpdate() {
  if (appState == ST_SNAKE || appState == ST_SIMON || appState == ST_IR_REMOTE) return;

  unsigned long now = millis();
  // Scale factor 0.0–1.0 based on brightness level (used by several modes)
  float bscale = (float)g_brightness / 8.0f;
  int   bpeak  = (int)(bscale * 1000.0f);

  switch (g_cfg.ledAnim) {

    case LED_ANIM_KNIGHT: {
      if (g_brightness >= 8) {
        for (int i = 0; i < NUM_FRONT_LEDS; i++) analogWrite(FRONT_LEDS[i], 1000);
        return;
      }
      const float period = 800.0f;
      float t   = fmodf((float)now, period) / period;
      float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);
      float pos = tri * 3.0f;
      float animDepth = (float)(8 - g_brightness) / 7.0f;
      float floor_val = 1.0f - animDepth;
      for (int i = 0; i < NUM_FRONT_LEDS; i++) {
        float dist   = fabsf(pos - (float)i);
        float peak   = (dist < 1.5f) ? (1.0f - dist / 1.5f) : 0.0f;
        float bright = floor_val + (1.0f - floor_val) * peak;
        analogWrite(FRONT_LEDS[i], (int)(bright * 1000.0f));
      }
      break;
    }

    case LED_ANIM_PULSE: {
      float t = fmodf((float)now / 2000.0f, 1.0f) * 2.0f * 3.14159265f;
      int duty = (int)((sinf(t) + 1.0f) * 0.5f * (float)bpeak);
      for (int i = 0; i < NUM_FRONT_LEDS; i++) analogWrite(FRONT_LEDS[i], duty);
      break;
    }

    case LED_ANIM_STROBE: {
      bool on = ((now / 80) & 1) == 0;
      int duty = on ? bpeak : 0;
      for (int i = 0; i < NUM_FRONT_LEDS; i++) analogWrite(FRONT_LEDS[i], duty);
      break;
    }

    case LED_ANIM_ALTERNATE: {
      bool phase = ((now / 250) & 1) != 0;
      for (int i = 0; i < NUM_FRONT_LEDS; i++) {
        bool lit = ((bool)(i & 1)) == phase;
        analogWrite(FRONT_LEDS[i], lit ? bpeak : 0);
      }
      break;
    }

    case LED_ANIM_CHASE: {
      const float period = 600.0f;
      float t   = fmodf((float)now, period) / period;
      float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);
      float pos = tri * 3.0f;
      for (int i = 0; i < NUM_FRONT_LEDS; i++) {
        float dist = fabsf(pos - (float)i);
        analogWrite(FRONT_LEDS[i], (dist < 0.6f) ? bpeak : 0);
      }
      break;
    }

    case LED_ANIM_ON:
      for (int i = 0; i < NUM_FRONT_LEDS; i++) analogWrite(FRONT_LEDS[i], bpeak);
      break;

    case LED_ANIM_OFF:
      for (int i = 0; i < NUM_FRONT_LEDS; i++) analogWrite(FRONT_LEDS[i], 0);
      break;
  }
}

// ============================================================ setup ========

void setup() {
  // Detect gamepad boot mode: hold A while connecting / powering on.
  // Must happen before Serial.begin() (which starts the USB stack) so the
  // USB device class is decided before the host enumerates.
  pinMode(BTN_A, INPUT_PULLUP);
  delayMicroseconds(500);   // let the pull-up settle
  g_gamepadMode = (digitalRead(BTN_A) == LOW);

  Serial.begin(115200);
  delay(800);
  if (g_gamepadMode) {
    Serial.println("\n=== Secfest 2026 — CONTROLLER MODE ===");
    Joystick.begin();
  } else {
    Serial.println("\n=== Secfest 2026 Welcome Demo ===");
  }

  // Buttons (BTN_A was already configured above; setting it again is harmless)
  for (int i = 0; i < BI_COUNT; i++) pinMode(BUTTON_PINS[i], INPUT_PULLUP);

  // Front LEDs
  for (int p : FRONT_LEDS) { pinMode(p, OUTPUT); digitalWrite(p, FRONT_LED_OFF); }
  pinMode(LED_FLASHLIGHT, OUTPUT);
  digitalWrite(LED_FLASHLIGHT, LOW);

  // Matrix on Wire (I2C0, GP4 = SDA_LED, GP5 = SCL_LED)
  Wire.setSDA(LED_MATRIX_SDA);
  Wire.setSCL(LED_MATRIX_SCL);
  Wire.setClock(400000);
  Wire.begin();
  if (!matrix.begin(IS31_ADDR, &Wire)) {
    Serial.println("IS31FL3731 not found at 0x74 — check matrix bus wiring.");
  } else {
    Serial.println("Matrix OK.");
  }
  // Make sure no leftover PWM is on
  for (uint8_t reg = 0; reg < 144; reg++) matrix.setLEDPWM(reg, 0);

  // Seed RNG from a floating ADC pin if available; otherwise use micros().
  randomSeed(micros() ^ analogRead(A0));

  // Load persisted config (brightness, flashlight brightness, IR codes)
  configLoad();

  // Serial terminal games (Mastermind, Hangman, Minesweeper)
  serialGames.begin();

  // SAO GPIO PWM (must come after randomSeed / analogRead so ADC init is done)
  saoSetup();

  enterState(g_gamepadMode ? ST_GAMEPAD : ST_BOOT_SWEEP);
}

// ============================================================ loop =========

void loop() {
  serialGames.update();        // USB-CDC terminal games (non-blocking)
  saoUpdate();                 // SAO GP1 sin-wave PWM, GP2 constant 10 %
  frontLedUpdate();            // front LED animation
  inputUpdate();

  // Any button press during the intro animations jumps to the menu.
  if ((appState == ST_BOOT_SWEEP || appState == ST_WELCOME_SCROLL || appState == ST_FIREWORKS)
      && anyPressed()) {
    enterState(ST_MENU);
    return;
  }

  switch (appState) {
    case ST_BOOT_SWEEP: {
      // Front LEDs handled by frontKnightRiderUpdate().
      // Small matrix accent that grows across the screen over 1 second.
      unsigned long t = millis() - stateEnter;
      int slot = (int)(t / 250);
      if (slot >= NUM_FRONT_LEDS) {
        enterState(ST_WELCOME_SCROLL);
        break;
      }
      fbClear();
      for (int i = 0; i <= slot && i < W; i++) fbSet(i, H / 2, 120);
      fbPush();
      break;
    }

    case ST_WELCOME_SCROLL: {
      bool done = scrollRenderTick();
      fbPush();
      if (done) enterState(ST_FIREWORKS);
      break;
    }

    case ST_FIREWORKS: {
      // Run the firework engine on a ~60 ms tick.
      static unsigned long lastFw = 0;
      unsigned long now = millis();
      if (now - lastFw >= 60) { lastFw = now; fwStep(); }
      if (now - stateEnter > 10000) enterState(ST_MENU);
      break;
    }

    case ST_MENU: {
      menuStep();
      if (input.pressed[BI_A]) {
        switch (menuIdx) {
          case 0: enterState(ST_SNAKE);          break;
          case 1: enterState(ST_SIMON);          break;
          case 2: enterState(ST_WELCOME_SCROLL); break;
          case 3: enterState(ST_ANIMATIONS);     break;
          case 4: enterState(ST_SETTINGS);       break;
          case 5: enterState(ST_FLASHLIGHT);     break;
          case 6: enterState(ST_IR_REMOTE);      break;
          case 7: enterState(ST_IR_HAVOC);       break;
          case 8: enterState(ST_NAME_BADGE);     break;
        }
      }
      break;
    }

    case ST_SNAKE: {
      snakeStep();
      if (snake.exitRequested) enterState(ST_MENU);
      break;
    }

    case ST_SIMON: {
      simonStep();
      if (simon.exitRequested) enterState(ST_MENU);
      break;
    }

    case ST_ANIMATIONS: {
      animationsStep();
      if (animCtx.exitRequested) enterState(ST_MENU);
      break;
    }

    case ST_SETTINGS: {
      settingsStep();
      if (settingsCtx.exitRequested) enterState(ST_MENU);
      break;
    }

    case ST_FLASHLIGHT: {
      flashStep();
      if (flashCtx.exitRequested) enterState(ST_MENU);
      break;
    }

    case ST_IR_REMOTE: {
      irRemoteStep();
      if (irExitRequested) enterState(ST_MENU);
      break;
    }

    case ST_IR_HAVOC: {
      havocStep();
      if (havocCtx.exitRequested) enterState(ST_MENU);
      break;
    }

    case ST_NAME_BADGE: {
      nameBadgeStep();
      if (nameBadgeCtx.exitRequested) enterState(ST_MENU);
      break;
    }

    case ST_GAMEPAD: {
      gamepadStep();
      if (gamepadExitRequested) enterState(ST_MENU);
      break;
    }
  }
}
