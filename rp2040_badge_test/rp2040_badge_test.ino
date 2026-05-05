#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_IS31FL3731.h>

#define I2C_LED_MATRIX_SDA 2
#define I2C_LED_MATRIX_SCL 3

#define BTN_A   9
#define BTN_B   10
#define BTN_UP  11
#define BTN_DWN 12
#define BTN_LFT 13
#define BTN_RGT 14

#define LED_FLASHLIGHT 15
#define LED_FRONT_1  23
#define LED_FRONT_2  24
#define LED_FRONT_3  25
#define LED_FRONT_4  26

Adafruit_IS31FL3731 ledmatrix = Adafruit_IS31FL3731();

int ledPattern[9][9] = {0};
int frameBuffer = 0;
unsigned long lastFrame = 0;

struct Point { int x; int y; };
Point playerPos = {4, 4};

void drawPlayer() {
    for (int y = 0; y < 9; y++) {
        for (int x = 0; x < 9; x++) {
            ledmatrix.enable(x, y, false);
        }
    }
    if (playerPos.x >= 0 && playerPos.x < 9 && playerPos.y >= 0 && playerPos.y < 9) {
        ledmatrix.enable(playerPos.x, playerPos.y, true);
    }
}

void drawSplash() {
    ledmatrix.clear();
    for (int i = 0; i < 9; i++) {
        ledmatrix.enable(i, i, true);
        ledmatrix.enable(8 - i, i, true);
    }
}

void drawCircle() {
    ledmatrix.clear();
    for (int angle = 0; angle < 360; angle += 10) {
        int x = 4 + (int)(3.5 * cos(angle * PI / 180));
        int y = 4 + (int)(3.5 * sin(angle * PI / 180));
        if (x >= 0 && x < 9 && y >= 0 && y < 9) {
            ledmatrix.enable(x, y, true);
        }
    }
}

void drawHeart() {
    ledmatrix.clear();
    int heart[9][9] = {
        {0,1,1,0,0,0,1,1,0},
        {1,1,1,1,0,1,1,1,1},
        {1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1},
        {0,1,1,1,1,1,1,1,0},
        {0,0,1,1,1,1,1,0,0},
        {0,0,0,1,1,1,0,0,0},
        {0,0,0,0,1,0,0,0,0},
        {0,0,0,0,0,0,0,0,0}
    };
    for (int y = 0; y < 9; y++) {
        for (int x = 0; x < 9; x++) {
            ledmatrix.enable(x, y, heart[y][x]);
        }
    }
}

void drawFrame1() {
    ledmatrix.clear();
    for (int i = 0; i < 9; i++) {
        ledmatrix.enable(i, 0, true);
        ledmatrix.enable(i, 8, true);
        ledmatrix.enable(0, i, true);
        ledmatrix.enable(8, i, true);
    }
}

void drawFrame2() {
    ledmatrix.clear();
    for (int i = 1; i < 8; i++) {
        ledmatrix.enable(i, 1, true);
        ledmatrix.enable(i, 7, true);
        ledmatrix.enable(1, i, true);
        ledmatrix.enable(7, i, true);
    }
}

void drawX() {
    ledmatrix.clear();
    for (int i = 0; i < 9; i++) {
        ledmatrix.enable(i, i, true);
        ledmatrix.enable(i, 8 - i, true);
    }
}

void cycleGraphics() {
    unsigned long now = millis();
    if (now - lastFrame > 500) {
        lastFrame = now;
        frameBuffer = (frameBuffer + 1) % 6;
        switch (frameBuffer) {
            case 0: drawSplash(); break;
            case 1: drawHeart(); break;
            case 2: drawCircle(); break;
            case 3: drawFrame1(); break;
            case 4: drawFrame2(); break;
            case 5: drawX(); break;
        }
    }
}

bool isButtonPressed(int btn) {
    return digitalRead(btn) == LOW;
}

void handleButtons() {
    if (isButtonPressed(BTN_A)) {
        digitalWrite(LED_FRONT_1, HIGH);
        playerPos.x = min(8, playerPos.x + 1);
    } else {
        digitalWrite(LED_FRONT_1, LOW);
    }
    
    if (isButtonPressed(BTN_B)) {
        digitalWrite(LED_FRONT_2, HIGH);
        playerPos.x = max(0, playerPos.x - 1);
    } else {
        digitalWrite(LED_FRONT_2, LOW);
    }
    
    if (isButtonPressed(BTN_UP)) {
        digitalWrite(LED_FRONT_3, HIGH);
        playerPos.y = max(0, playerPos.y - 1);
    } else {
        digitalWrite(LED_FRONT_3, LOW);
    }
    
    if (isButtonPressed(BTN_DWN)) {
        digitalWrite(LED_FRONT_4, HIGH);
        playerPos.y = min(8, playerPos.y + 1);
    } else {
        digitalWrite(LED_FRONT_4, LOW);
    }
    
    if (isButtonPressed(BTN_LFT)) {
        digitalWrite(LED_FLASHLIGHT, HIGH);
    } else {
        digitalWrite(LED_FLASHLIGHT, LOW);
    }
    
    if (isButtonPressed(BTN_RGT)) {
        cycleGraphics();
    }
}

void blinkLEDs() {
    static unsigned long lastBlink = 0;
    static bool blinkState = false;
    unsigned long now = millis();
    
    if (now - lastBlink > 1000) {
        lastBlink = now;
        blinkState = !blinkState;
        
        digitalWrite(LED_FRONT_1, blinkState ? HIGH : LOW);
        digitalWrite(LED_FRONT_2, blinkState ? HIGH : LOW);
        digitalWrite(LED_FRONT_3, blinkState ? HIGH : LOW);
        digitalWrite(LED_FRONT_4, blinkState ? HIGH : LOW);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("RP2040 Badge TestStarting...");
    
    Wire.setSDA(I2C_LED_MATRIX_SDA);
    Wire.setSCL(I2C_LED_MATRIX_SCL);
    
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DWN, INPUT_PULLUP);
    pinMode(BTN_LFT, INPUT_PULLUP);
    pinMode(BTN_RGT, INPUT_PULLUP);
    
    pinMode(LED_FLASHLIGHT, OUTPUT);
    pinMode(LED_FRONT_1, OUTPUT);
    pinMode(LED_FRONT_2, OUTPUT);
    pinMode(LED_FRONT_3, OUTPUT);
    pinMode(LED_FRONT_4, OUTPUT);
    
    Serial.println("Initializing LED Matrix...");
    if (!ledmatrix.begin(0x74, &Wire)) {
        Serial.println("IS31FL3731 not found!");
        while (1) delay(1);
    }
    Serial.println("LED Matrix initialized!");
    
    ledmatrix.setRotation(0);
    ledmatrix.clear();
    
    drawSplash();
    delay(1000);
    
    Serial.println("Setup complete!");
}

void loop() {
    handleButtons();
    
    if (isButtonPressed(BTN_RGT)) {
        drawPlayer();
    }
    
    if (!isButtonPressed(BTN_A) && !isButtonPressed(BTN_B) && 
        !isButtonPressed(BTN_UP) && !isButtonPressed(BTN_DWN) &&
        !isButtonPressed(BTN_LFT) && !isButtonPressed(BTN_RGT)) {
        blinkLEDs();
    }
}