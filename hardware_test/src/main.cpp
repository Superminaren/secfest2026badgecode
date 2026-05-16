// Secfest 2026 Badge — Hardware Test Sketch
// Tests: buttons, front LEDs, flashlight LED, IS31FL3731 9x9 matrix, I2C bus scan

#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <Adafruit_IS31FL3731.h>

// --- Buttons (active LOW, internal pull-ups) ---
// Per schematic: BTN_A=GP8, B=GP9, UP=GP10, DWN=GP11, LFT=GP12, RGT=GP13
#define BTN_A     8
#define BTN_B     9
#define BTN_UP    10
#define BTN_DOWN  11
#define BTN_LEFT  12
#define BTN_RIGHT 13

const int    BUTTONS[]   = { BTN_A, BTN_B, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT };
const char*  BTN_NAMES[] = { "A", "B", "Up", "Down", "Left", "Right" };
const int    NUM_BUTTONS = 6;

// --- LEDs ---
#define LED_FLASHLIGHT 15   // MOSFET gate, HIGH = on
#define LED_FRONT_1    23
#define LED_FRONT_2    24
#define LED_FRONT_3    25
#define LED_FRONT_4    26

const int FRONT_LEDS[] = { LED_FRONT_1, LED_FRONT_2, LED_FRONT_3, LED_FRONT_4 };
const int NUM_LEDS     = 4;

// Front LEDs are wired active-low (anode to 3V3, cathode to GPIO via series R).
// Drive LOW to light them; HIGH (or open-drain Hi-Z) to turn them off.
const int FRONT_LED_ON  = LOW;
const int FRONT_LED_OFF = HIGH;

// --- IS31FL3731 LED matrix ---
// Dedicated I2C bus per schematic: SDA_LED=GPIO4, SCL_LED=GPIO5, address 0x74.
// GP4/GP5 are I2C0 pins on the RP2040, so the matrix uses `Wire`.
#define LED_MATRIX_SDA 4
#define LED_MATRIX_SCL 5
#define IS31_ADDR      0x74

// --- Main I2C bus (user / SAO) ---
// Per schematic: SDA=GPIO2, SCL=GPIO3.
// GP2/GP3 are I2C1 pins on the RP2040, so the SAO bus uses `Wire1`.
#define MAIN_SDA 2
#define MAIN_SCL 3

// Wire  = LED matrix bus (GPIO4/5, I2C0)
// Wire1 = main / SAO bus  (GPIO2/3, I2C1)
Adafruit_IS31FL3731 matrix(9, 16);

// --- Matrix orientation / placement ---
// Logical coordinate system for the rest of the code:
//   x = 0..8 left-to-right as you look at the badge front
//   y = 0..8 top-to-bottom as you look at the badge front
//
// Empirically: only the vertical axis is flipped between schematic and
// physical (top<->bottom). Horizontal axis already matches. So the helper
// just mirrors Y.
//
// NOTE: the badge appears to be charlieplex-wired (9 pins → 72 LEDs).
// The 9 LEDs along the main diagonal — logical (0,0), (1,1), … (8,8) —
// are not physically driveable and will always stay dark, regardless of
// what we write to the chip.
static const int MATRIX_W = 9;
static const int MATRIX_H = 16;

// Observed-dead pixels: the schematic shows 18 dedicated chip pins
// (CA1..9 + CB1..9) wired to 81 independent LEDs, so all positions should
// be addressable in principle. Empirically, the 9 LEDs on the main
// physical diagonal stay dark — root cause not yet identified (could be
// PCB defect, soldering issue on those 9 specific parts, or something
// subtle in the IS31FL3731's matrix scan). For now the helper just marks
// them so test patterns can skip drawing there until we know more.
static inline bool matrixPixelIsDead(int x, int y) {
  return x == y;
}

static inline void matrixSetPixel(int x, int y, uint8_t brightness) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;
  matrix.drawPixel(x, (MATRIX_H - 1) - y, brightness);
}

static void matrixFill(uint8_t brightness) {
  for (int y = 0; y < MATRIX_H; y++)
    for (int x = 0; x < MATRIX_W; x++)
      matrixSetPixel(x, y, brightness);
}

// ------------------------------------------------------------------ helpers --

void blinkFrontLeds(int times, int ms) {
  for (int i = 0; i < times; i++) {
    for (int p : FRONT_LEDS) digitalWrite(p, FRONT_LED_ON);
    delay(ms);
    for (int p : FRONT_LEDS) digitalWrite(p, FRONT_LED_OFF);
    delay(ms);
  }
}

void scanI2C(TwoWire& bus, const char* label) {
  Serial.print("\nI2C scan — ");
  Serial.print(label);
  Serial.println(":");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) {
      Serial.print("  Device at 0x");
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (!found) Serial.println("  (none found)");
}

// ------------------------------------------------------------------- setup ---

void setup() {
  Serial.begin(115200);
  delay(2000);  // wait for USB CDC to enumerate
  Serial.println("\n=== Secfest 2026 Badge Hardware Test ===\n");

  // Buttons
  for (int pin : BUTTONS) pinMode(pin, INPUT_PULLUP);
  Serial.println("[Buttons] GP8-GP13 configured with pull-ups");

  // LEDs
  for (int pin : FRONT_LEDS) { pinMode(pin, OUTPUT); digitalWrite(pin, FRONT_LED_OFF); }
  pinMode(LED_FLASHLIGHT, OUTPUT);
  digitalWrite(LED_FLASHLIGHT, LOW);
  Serial.println("[LEDs]    GPIO 15, 23-26 configured");

  // --- Front LED sweep (both polarities, diagnostic mode) ---
  // We don't yet know whether the front LEDs are wired active-high
  // (anode-to-GPIO) or active-low (anode-to-3V3). Drive each pin HIGH
  // for ~600 ms then LOW for ~600 ms so whichever polarity is correct
  // will produce visible light. The pin is set to INPUT (Hi-Z) between
  // tests so a wrongly polarized previous LED can't keep current flowing.
  Serial.println("\n[Test] Front LEDs — sweeping both polarities");
  for (int i = 0; i < NUM_LEDS; i++) {
    int pin = FRONT_LEDS[i];
    Serial.print("  FRONT_LED_"); Serial.print(i + 1);
    Serial.print(" on GP"); Serial.println(pin);

    pinMode(pin, OUTPUT);
    Serial.println("    -> HIGH (lights active-high wiring)");
    digitalWrite(pin, HIGH);
    delay(50);
    Serial.println("    -> LOW  (lights active-low  wiring)");
    digitalWrite(pin, LOW);
    delay(50);

    // Release the pin between LEDs so a wrong-polarity LED can't stay lit
    pinMode(pin, INPUT);
    delay(200);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, FRONT_LED_OFF);
  }

  // --- Flashlight LED ---
  Serial.println("[Test] Flashlight LED — 3 pulses");
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_FLASHLIGHT, HIGH);
    delay(150);
    digitalWrite(LED_FLASHLIGHT, LOW);
    delay(150);
  }

  // --- IS31FL3731 matrix (on Wire / I2C0, GP4 = SDA_LED, GP5 = SCL_LED) ---
  Wire.setSDA(LED_MATRIX_SDA);
  Wire.setSCL(LED_MATRIX_SCL);
  Wire.begin();

  Serial.println("\n[Test] IS31FL3731 LED matrix (9x9) on GP4/GP5");
  if (!matrix.begin(IS31_ADDR, &Wire)) {
    Serial.println("  ERROR: IS31FL3731 not found at 0x74 — check wiring on GP4/GP5");
  } else {
    Serial.println("  Found! Running orientation test...");

    // 1. All-on, confirms every LED in the 9x9 lights at all.
    Serial.println("    [1/5] All 81 LEDs on (dim)");
    matrix.clear();
    matrixFill(50);
    delay(800);
    matrix.clear();

    // 2. Corners one at a time — visually confirms the orientation map.
    //    If the helper is correct, each label below should light the LED
    //    you'd expect to see in that corner of the badge front.
    struct Corner { int x, y; const char* name; };
    const Corner corners[] = {
      { 0,             0,             "top-left     (x=0, y=0)" },
      { MATRIX_W - 1,  0,             "top-right    (x=8, y=0)" },
      { 0,             MATRIX_H - 1,  "bottom-left  (x=0, y=8)" },
      { MATRIX_W - 1,  MATRIX_H - 1,  "bottom-right (x=8, y=8)" },
    };
    Serial.println("    [2/5] Corner check");
    for (const Corner& c : corners) {
      Serial.print("        ");
      Serial.println(c.name);
      matrix.clear();
      matrixSetPixel(c.x, c.y, 220);
      delay(900);
    }
    matrix.clear();

    // 3. Row sweep, top -> bottom. Should look like a horizontal bar
    //    moving downward.
    Serial.println("    [3/5] Row sweep, top -> bottom");
    for (int y = 0; y < MATRIX_H; y++) {
      matrix.clear();
      for (int x = 0; x < MATRIX_W; x++) matrixSetPixel(x, y, 180);
      delay(200);
    }
    matrix.clear();

    // 4. Column sweep, left -> right. Vertical bar moving rightward.
    Serial.println("    [4/5] Column sweep, left -> right");
    for (int x = 0; x < MATRIX_W; x++) {
      matrix.clear();
      for (int y = 0; y < MATRIX_H; y++) matrixSetPixel(x, y, 180);
      delay(200);
    }
    matrix.clear();

    // 5. Anti-diagonal chevron — every lit pixel is OFF the dead diagonal,
    //    so this pattern displays without any holes. It also points
    //    visually from top-right toward bottom-left, which is a strong
    //    asymmetric cue for orientation (mirror or rotation would be
    //    obvious immediately).
    Serial.println("    [5/5] Chevron along anti-diagonal (no diagonal slash)");
    const uint8_t chevron[MATRIX_H][MATRIX_W] = {
      {0,0,0,0,0,0,0,1,0},
      {0,0,0,0,0,0,1,0,1},
      {0,0,0,0,0,1,0,1,0},
      {0,0,0,0,1,0,1,0,0},
      {0,0,0,1,0,1,0,0,0},
      {0,0,1,0,1,0,0,0,0},
      {0,1,0,1,0,0,0,0,0},
      {1,0,1,0,0,0,0,0,0},
      {0,1,0,0,0,0,0,0,0},
    };
    for (int y = 0; y < 9; y++)
      for (int x = 0; x < MATRIX_W; x++)
        matrixSetPixel(x, y, chevron[y][x] ? 200 : 0);
    delay(1500);
    matrix.clear();

    // 6. Final pass: solid fill, but only on driveable pixels. Demonstrates
    //    matrixPixelIsDead being used to skip the diagonal cleanly.
    Serial.println("    [bonus] All driveable LEDs on (72/81)");
    for (int y = 0; y < MATRIX_H; y++)
      for (int x = 0; x < MATRIX_W; x++)
        if (!matrixPixelIsDead(x, y)) matrixSetPixel(x, y, 60);
    delay(1500);
    matrix.clear();

    // 7. Chip register sweep — lights PWM registers 0..143 one at a time
    //    so you can physically map chip register -> badge LED position.
    //    Watch the badge AND the serial monitor; for each "reg = N" line
    //    note which physical LED lit (or "none" if it stayed dark).
    //    The 9 registers that never light tell us exactly what's wrong
    //    with the diagonal LEDs. ~400 ms per register, total ~60 s.
    Serial.println("    [diag] Chip register sweep — write your observations");
    for (uint8_t reg = 0; reg < 144; reg++) {
      // Only sweep the 81 registers our 9x9 layout actually uses; the
      // other 63 (x=9..15 for each row) drive CB10..CB16, which aren't
      // wired on this badge.
      uint8_t cb = reg % 16;          // x in chip space (CB index)
      uint8_t ca = reg / 16;          // y in chip space (CA index)
      if (cb > 8 || ca > 8) continue;

      matrix.clear();
      matrix.setLEDPWM(reg, 220);
      Serial.print("      reg ");
      if (reg < 100) Serial.print(' ');
      if (reg < 10)  Serial.print(' ');
      Serial.print(reg);
      Serial.print("  (CA"); Serial.print(ca + 1);
      Serial.print(", CB"); Serial.print(cb + 1);
      Serial.println(")");
      delay(400);
    }
    matrix.clear();
    Serial.println("    [diag] Sweep done.");

    Serial.println("    [test] Individual LED test - all pixels");
    for (int y = 0; y < MATRIX_H; y++) {
      for (int x = 0; x < MATRIX_W; x++) {
        if (matrixPixelIsDead(x, y)) continue;
        matrix.clear();
        matrixSetPixel(x, y, 255);
        delay(30);
      }
    }
    matrix.clear();

    Serial.println("  Matrix test done.");

    Serial.println("\n[Test] Scrolling text: 'Welcome to Securityfest 2026'");
    matrix.setTextSize(1);
    matrix.setTextColor(255);
    
    const char* scrollText = "Welcome to Securityfest 2026   ";
    int textLen = strlen(scrollText);
    int charWidth = 6;
    int totalWidth = textLen * charWidth;
    
    for (int scroll = 0; scroll < totalWidth + 16; scroll++) {
      matrix.clear();
      matrix.setCursor(16 - scroll, 1);
      matrix.print(scrollText);
      delay(80);
    }
    matrix.clear();
    
    Serial.println("  Text scroll done.");
  }

  // --- Main / SAO I2C bus (on Wire1 / I2C1, GP2 = SDA, GP3 = SCL) ---
  Wire1.setSDA(MAIN_SDA);
  Wire1.setSCL(MAIN_SCL);
  Wire1.begin();

  // --- I2C bus scan ---
  scanI2C(Wire,  "matrix bus (SDA=GP4, SCL=GP5)");
  scanI2C(Wire1, "main bus   (SDA=GP2, SCL=GP3)");

  // Signal setup complete
  Serial.println("\n[Ready] Entering button-test loop. Press any button.\n");
  blinkFrontLeds(3, 100);
}

// -------------------------------------------------------------------- loop ---

bool prevState[NUM_BUTTONS] = {};

void loop() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    bool pressed = (digitalRead(BUTTONS[i]) == LOW);
    if (pressed && !prevState[i]) {
      Serial.print("[Button] ");
      Serial.println(BTN_NAMES[i]);
      // brief flash on button press — fires both polarities back-to-back
      // so it's visible regardless of how the front LEDs are wired
      for (int p : FRONT_LEDS) digitalWrite(p, HIGH);
      delay(60);
      for (int p : FRONT_LEDS) digitalWrite(p, LOW);
      delay(60);
      for (int p : FRONT_LEDS) digitalWrite(p, FRONT_LED_OFF);
    }
    prevState[i] = pressed;
  }
  delay(10);
}
