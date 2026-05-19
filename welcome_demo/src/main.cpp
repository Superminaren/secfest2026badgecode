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
#include "font4x7.h"

// ============================================================ pin map =======

#define MATRIX_BRIGHTNESS 20

#define BTN_A     8
#define BTN_B     9
#define BTN_UP    10
#define BTN_DOWN  11
#define BTN_LEFT  12
#define BTN_RIGHT 13

#define SAO_GP1	  0
#define SAO_GP2   1

#define LED_FLASHLIGHT 15
#define LED_FRONT_1    20
#define LED_FRONT_2    21
#define LED_FRONT_3    22
#define LED_FRONT_4    23

#define LED_MATRIX_SDA 4
#define LED_MATRIX_SCL 5
#define IS31_ADDR      0x74

// Front LEDs are active-low (anode to 3V3, cathode to GPIO via series R).
const int FRONT_LED_ON  = LOW;
const int FRONT_LED_OFF = HIGH;
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
  fb[y][x] = b;
}

static inline void fbOr(int x, int y, uint8_t b) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
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
      matrix.setLEDPWM(cx + cy * 16, fb[y][x]);
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
  scroller.text = t;
  scroller.textPxWidth = (int)strlen(t) * (FONT_W + 1);
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
static void simonDrawArrow(int bi, uint8_t b) {
  fbClear();
  if (bi == BI_UP) {
    fbSet(4, 1, b); fbSet(3, 2, b); fbSet(4, 2, b); fbSet(5, 2, b);
    fbSet(2, 3, b); fbSet(4, 3, b); fbSet(6, 3, b);
    fbSet(4, 4, b); fbSet(4, 5, b); fbSet(4, 6, b); fbSet(4, 7, b);
  } else if (bi == BI_DOWN) {
    fbSet(4, 1, b); fbSet(4, 2, b); fbSet(4, 3, b); fbSet(4, 4, b);
    fbSet(2, 5, b); fbSet(4, 5, b); fbSet(6, 5, b);
    fbSet(3, 6, b); fbSet(4, 6, b); fbSet(5, 6, b);
    fbSet(4, 7, b);
  } else if (bi == BI_LEFT) {
    fbSet(1, 4, b); fbSet(2, 3, b); fbSet(2, 4, b); fbSet(2, 5, b);
    fbSet(3, 2, b); fbSet(3, 4, b); fbSet(3, 6, b);
    fbSet(4, 4, b); fbSet(5, 4, b); fbSet(6, 4, b); fbSet(7, 4, b);
  } else if (bi == BI_RIGHT) {
    fbSet(7, 4, b); fbSet(6, 3, b); fbSet(6, 4, b); fbSet(6, 5, b);
    fbSet(5, 2, b); fbSet(5, 4, b); fbSet(5, 6, b);
    fbSet(4, 4, b); fbSet(3, 4, b); fbSet(2, 4, b); fbSet(1, 4, b);
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

// ============================================================ menu =========

const char* MENU_ITEMS[] = { "SNAKE", "SIMON", "WELCOME" };
const int   MENU_LEN     = sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0]);
static int  menuIdx      = 0;

static void menuEnter() {
  scrollStart(MENU_ITEMS[menuIdx], 50, MATRIX_BRIGHTNESS);
  // Front LEDs are driven by frontKnightRiderUpdate() — no static override here.
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
  // Re-render scroller continuously (loops via the "done" reset below)
  scrollRenderTick();
  if (scroller.done) {
    scrollStart(MENU_ITEMS[menuIdx], 90, MATRIX_BRIGHTNESS);
  }
  fbPush();
}

// ============================================================ state mach. ==

enum AppState {
  ST_BOOT_SWEEP,
  ST_WELCOME_SCROLL,
  ST_FIREWORKS,
  ST_MENU,
  ST_SNAKE,
  ST_SIMON,
};

static AppState appState = ST_BOOT_SWEEP;
static unsigned long stateEnter = 0;

static void enterState(AppState s) {
  appState = s;
  stateEnter = millis();
  fbClear(); fbPush();
  frontAll(FRONT_LED_OFF);
  switch (s) {
    case ST_WELCOME_SCROLL: scrollStart("WELCOME TO SECURITY FEST 2026", 50, MATRIX_BRIGHTNESS); break;
    case ST_FIREWORKS:      fwInit(); break;
    case ST_MENU:           menuEnter(); break;
    case ST_SNAKE:          snakeReset(); break;
    case ST_SIMON:          simonReset(); break;
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
}

// ============================================================ front LED Knight Rider =======
//
// Passive background animation: a bright spot sweeps back and forth across
// the four front LEDs using hardware PWM.
//
// Front LEDs are active-low (anode → 3V3, cathode → GPIO):
//   analogWrite duty 0    = pin always LOW  = LED fully ON
//   analogWrite duty 1000 = pin always HIGH = LED fully OFF
//
// The sweep is skipped while ST_SNAKE or ST_SIMON own the front LEDs.

static void frontKnightRiderUpdate() {
  if (appState == ST_SNAKE || appState == ST_SIMON) return;

  // Triangle wave, period 800 ms → position sweeps 0.0 → 3.0 → 0.0
  const float period = 800.0f;
  float t   = fmodf((float)millis(), period) / period; // 0 .. 1
  float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0 .. 1 .. 0
  float pos = tri * 3.0f;                              // 0.0 .. 3.0

  for (int i = 0; i < NUM_FRONT_LEDS; i++) {
    float dist       = fabsf(pos - (float)i);
    // Soft glow: full brightness within 0.5 px, fades out to 1.5 px
    float brightness = (dist < 1.5f) ? (1.0f - dist / 1.5f) : 0.0f;
    // Invert for active-low: duty 0 = fully on, 1000 = fully off
    int duty = 1000 - (int)(brightness * 1000.0f);
    analogWrite(FRONT_LEDS[i], duty);
  }
}

// ============================================================ setup ========

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== Secfest 2026 Welcome Demo ===");

  // Buttons
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

  // SAO GPIO PWM (must come after randomSeed / analogRead so ADC init is done)
  saoSetup();

  enterState(ST_BOOT_SWEEP);
}

// ============================================================ loop =========

void loop() {
  saoUpdate();                 // SAO GP1 sin-wave PWM, GP2 constant 10 %
  frontKnightRiderUpdate();    // front LED Knight Rider sweep
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
          case 0: enterState(ST_SNAKE); break;
          case 1: enterState(ST_SIMON); break;
          case 2: enterState(ST_WELCOME_SCROLL); break;
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
  }
}
