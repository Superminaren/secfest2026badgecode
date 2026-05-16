// Secfest 2026 Badge — Hardware Test Sketch
// Tests: buttons, front LEDs, flashlight LED, IS31FL3731 LED matrix, I2C bus scan, scrolling text

#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <Adafruit_IS31FL3731.h>

bool hidMode = false;

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
Adafruit_IS31FL3731* matrix = nullptr;

static const int MATRIX_W = 9;
static const int MATRIX_H = 16;

static inline bool matrixPixelIsDead(int x, int y) {
  return x == y;
}

static inline void matrixSetPixel(int x, int y, uint8_t brightness) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;
  matrix->drawPixel(x, (MATRIX_H - 1) - y, brightness);
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

// ------------------------------------------------------------------- animation state ---

enum AnimState {
  ANIM_ALL_ON,
  ANIM_CORNERS,
  ANIM_ROW_SWEEP,
  ANIM_COL_SWEEP,
  ANIM_CHEVRON,
  ANIM_DRIVEABLE,
  ANIM_TEXT_SCROLL,
  ANIM_COUNT
};

AnimState animState = ANIM_ALL_ON;
uint32_t animStartTime = 0;
const uint32_t ANIM_DURATION = 2000;
int scrollPos = 0;

struct Point { int x; int y; };
const Point corners[4] = {
  {0, 0},
  {MATRIX_W - 1, 0},
  {0, MATRIX_H - 1},
  {MATRIX_W - 1, MATRIX_H - 1}
};

void runAnimation() {
  if (!matrix) return;
  
  uint32_t elapsed = millis() - animStartTime;
  
  switch (animState) {
    case ANIM_ALL_ON:
      matrix->clear();
      matrixFill(50);
      if (elapsed > ANIM_DURATION) {
        animState = ANIM_CORNERS;
        animStartTime = millis();
      }
      break;
      
    case ANIM_CORNERS: {
      int corner = (elapsed / 500) % 4;
      matrix->clear();
      matrixSetPixel(corners[corner].x, corners[corner].y, 220);
      if (elapsed > ANIM_DURATION) {
        animState = ANIM_ROW_SWEEP;
        animStartTime = millis();
      }
      break;
    }
    
    case ANIM_ROW_SWEEP: {
      int y = (elapsed / 100) % MATRIX_H;
      matrix->clear();
      for (int x = 0; x < MATRIX_W; x++) matrixSetPixel(x, y, 180);
      if (elapsed > ANIM_DURATION) {
        animState = ANIM_COL_SWEEP;
        animStartTime = millis();
      }
      break;
    }
    
    case ANIM_COL_SWEEP: {
      int x = (elapsed / 100) % MATRIX_W;
      matrix->clear();
      for (int y = 0; y < MATRIX_H; y++) matrixSetPixel(x, y, 180);
      if (elapsed > ANIM_DURATION) {
        animState = ANIM_CHEVRON;
        animStartTime = millis();
      }
      break;
    }
    
    case ANIM_CHEVRON: {
      static const uint8_t chevron[9][9] = {
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
      matrix->clear();
      for (int y = 0; y < 9; y++)
        for (int x = 0; x < MATRIX_W; x++)
          matrixSetPixel(x, y, chevron[y][x] ? 200 : 0);
      if (elapsed > ANIM_DURATION) {
        animState = ANIM_DRIVEABLE;
        animStartTime = millis();
      }
      break;
    }
    
    case ANIM_DRIVEABLE: {
      matrix->clear();
      for (int y = 0; y < MATRIX_H; y++)
        for (int x = 0; x < MATRIX_W; x++)
          if (!matrixPixelIsDead(x, y)) matrixSetPixel(x, y, 60);
      if (elapsed > ANIM_DURATION) {
        animState = ANIM_TEXT_SCROLL;
        animStartTime = millis();
        scrollPos = 16;
      }
      break;
    }
    
    case ANIM_TEXT_SCROLL: {
      matrix->clear();
      matrix->setTextSize(1);
      matrix->setTextColor(255);
      const char* scrollText = "Welcome to Securityfest 2026   ";
      matrix->setCursor(16 - scrollPos, 1);
      matrix->print(scrollText);
      scrollPos++;
      if (scrollPos > strlen(scrollText) * 6 + 16) {
        animState = ANIM_ALL_ON;
      }
      delay(60);
      break;
    }
    
    default:
      animState = ANIM_ALL_ON;
      break;
  }
}

// ------------------------------------------------------------------- setup ---

void setup() {
  pinMode(BTN_A, INPUT_PULLUP);
  
  if (digitalRead(BTN_A) == LOW) {
    hidMode = true;
    Serial.end();
  } else {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== Secfest 2026 Badge Hardware Test ===\n");
  }
  
  for (int pin : BUTTONS) pinMode(pin, INPUT_PULLUP);
  Serial.println("[Buttons] GP8-GP13 configured with pull-ups");

  for (int pin : FRONT_LEDS) { pinMode(pin, OUTPUT); digitalWrite(pin, FRONT_LED_OFF); }
  pinMode(LED_FLASHLIGHT, OUTPUT);
  digitalWrite(LED_FLASHLIGHT, LOW);
  Serial.println("[LEDs]    GPIO 15, 23-26 configured");

  Serial.println("\n[Test] Front LEDs — sweeping both polarities");
  for (int i = 0; i < NUM_LEDS; i++) {
    int pin = FRONT_LEDS[i];
    Serial.print("  FRONT_LED_"); Serial.print(i + 1);
    Serial.print(" on GP"); Serial.println(pin);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delay(50);
    digitalWrite(pin, LOW);
    delay(50);
    pinMode(pin, INPUT);
    delay(200);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, FRONT_LED_OFF);
  }

  Serial.println("[Test] Flashlight LED — 3 pulses");
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_FLASHLIGHT, HIGH);
    delay(150);
    digitalWrite(LED_FLASHLIGHT, LOW);
    delay(150);
  }

  Wire.setSDA(LED_MATRIX_SDA);
  Wire.setSCL(LED_MATRIX_SCL);
  Wire.begin();

  Serial.println("\n[Test] IS31FL3731 LED matrix (9x9) on GP4/GP5");
  matrix = new Adafruit_IS31FL3731(9, 16);
  if (!matrix->begin(IS31_ADDR, &Wire)) {
    Serial.println("  ERROR: IS31FL3731 not found at 0x74");
    matrix = nullptr;
  } else {
    Serial.println("  Found! Starting animation loop...");
    animStartTime = millis();
  }

  Wire1.setSDA(MAIN_SDA);
  Wire1.setSCL(MAIN_SCL);
  Wire1.begin();

  scanI2C(Wire,  "matrix bus (SDA=GP4, SCL=GP5)");
  scanI2C(Wire1, "main bus   (SDA=GP2, SCL=GP3)");

  Serial.println("\n[Ready] Animation loop running. Hold Button A to reset.\n");
  blinkFrontLeds(3, 100);
}

// -------------------------------------------------------------------- TinyUSB HID ---

extern "C" {
bool tud_hid_n_report_complete_cb(uint8_t itf, uint8_t const* report, uint16_t len) {
  (void)itf; (void)report; (void)len;
  return true;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, uint8_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void)itf; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, uint8_t report_type, uint8_t* buffer, uint16_t bufsize) {
  (void)itf; (void)report_id; (void)report_type; 
  if (bufsize < 1) return 0;
  uint8_t btn = 0;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (digitalRead(BUTTONS[i]) == LOW) btn |= (1 << i);
  }
  buffer[0] = btn;
  return 1;
}
}

// -------------------------------------------------------------------- loop ---

bool prevState[NUM_BUTTONS] = {};

void loop() {
  if (digitalRead(BTN_A) == LOW && !prevState[0]) {
    Serial.println("\n[Reset] Rebooting...");
    delay(100);
    rp2040.reboot();
  }
  
  if (matrix) {
    runAnimation();
  }
  
  for (int i = 0; i < NUM_BUTTONS; i++) {
    bool pressed = (digitalRead(BUTTONS[i]) == LOW);
    if (pressed && !prevState[i]) {
      Serial.print("[Button] ");
      Serial.println(BTN_NAMES[i]);
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