#include <Arduino.h>
#include <Wire.h>

#define BUTTON_A 9
#define BUTTON_B 10
#define BUTTON_UP 11
#define BUTTON_DOWN 12
#define BUTTON_LEFT 13
#define BUTTON_RIGHT 14

#define FRONT_LED_1 23
#define FRONT_LED_2 24
#define FRONT_LED_3 25
#define FRONT_LED_4 26

#define FLASHLIGHT_LED 15

#define SAO_GPIO0 0
#define SAO_GPIO1 1

#define LED_MATRIX_SDA 2
#define LED_MATRIX_SCL 3
#define LED_MATRIX_ADDR 0x74

#define MATRIX_COLS 9
#define MATRIX_ROWS 9

#define IDLE_TIMEOUT 5000

const char* RANDOM_MESSAGES[] = {
    "Hack the planet!",
    "0x00 0x01 0x10",
    "Securityfest 2026!",
    "RP2040 rocks!",
    "Buy the dip!",
    "Nice badge!",
    "#!.!/#!.!",
    "0xDEAD 0xBEEF",
    "Pwned!",
    "1337 h4x0r",
    "Kernel panic!",
    "sudo make me",
    "CTF{D3bug}",
    "0-day here",
    "Badgelife!"
};
const int NUM_MESSAGES = sizeof(RANDOM_MESSAGES) / sizeof(RANDOM_MESSAGES[0]);

uint8_t matrixBuffer[MATRIX_ROWS][MATRIX_COLS];
unsigned long lastActivity = 0;
int currentMenuItem = 0;
enum { MODE_MENU, MODE_TETRIS, MODE_SNAKE, MODE_BRICK, MODE_PONG, MODE_TVBGONE, MODE_IDLE } currentMode = MODE_MENU;
bool inSubmenu = false;

void ledMatrixBegin() {
    Wire.setSDA(LED_MATRIX_SDA);
    Wire.setSCL(LED_MATRIX_SCL);
    Wire.begin();
    delay(10);
    Wire.beginTransmission(LED_MATRIX_ADDR);
    Wire.write(0x00);
    Wire.write(0x01);
    Wire.endTransmission();
}

void ledMatrixClear() {
    memset(matrixBuffer, 0, sizeof(matrixBuffer));
    for (int page = 0; page < 2; page++) {
        Wire.beginTransmission(LED_MATRIX_ADDR);
        Wire.write(0x00);
        Wire.write(page);
        Wire.endTransmission();
        Wire.requestFrom(LED_MATRIX_ADDR, MATRIX_COLS);
        for (int i = 0; i < MATRIX_COLS; i++) {
            Wire.read();
        }
        for (int c = 0; c < MATRIX_COLS; c++) {
            Wire.beginTransmission(LED_MATRIX_ADDR);
            Wire.write(0x01 + c);
            Wire.write(0x00);
            Wire.endTransmission();
        }
    }
}

void ledMatrixSetPixel(int x, int y, uint8_t brightness) {
    if (x >= 0 && x < MATRIX_COLS && y >= 0 && y < MATRIX_ROWS) {
        matrixBuffer[y][x] = brightness;
        Wire.beginTransmission(LED_MATRIX_ADDR);
        Wire.write(0x01 + x);
        Wire.write(brightness);
        Wire.endTransmission();
    }
}

void ledMatrixDrawChar(int x, int y, char c, uint8_t brightness) {
    const uint8_t font[16][5] = {
        {0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x5F,0x00,0x00},
        {0x00,0x07,0x00,0x07,0x00},
        {0x14,0x7F,0x14,0x7F,0x14},
        {0x24,0x2A,0x7F,0x2A,0x12},
        {0x46,0x26,0x10,0x26,0x46},
        {0x08,0x3E,0x28,0x3E,0x08},
        {0x00,0x00,0x07,0x00,0x00},
        {0x3E,0x3E,0x3E,0x3E,0x3E},
        {0x00,0x07,0x07,0x07,0x00},
        {0x1C,0x1C,0x1C,0x1C,0x1C},
        {0x1C,0x1C,0x1C,0x10,0x00},
        {0x00,0x04,0x3E,0x04,0x00},
        {0x08,0x1C,0x3E,0x1C,0x08},
        {0x1C,0x3E,0x3E,0x3E,0x08},
        {0x1C,0x2A,0x2A,0x2A,0x08}
    };
    int idx = 0;
    if (c >= 'A' && c <= 'Z') idx = c - 'A' + 10;
    else if (c >= '0' && c <= '9') idx = c - '0';
    else if (c == '!') idx = 1;
    else if (c == '.') idx = 15;
    else if (c == '(') idx = 13;
    else if (c == ')') idx = 13;
    else if (c == '{') idx = 0;
    else if (c == '}') idx = 0;
    else return;
    for (int col = 0; col < 5; col++) {
        uint8_t colData = font[idx][col];
        for (int row = 0; row < 8; row++) {
            if (colData & (1 << row)) {
                ledMatrixSetPixel(x + col, y + row, brightness);
            }
        }
    }
}

void drawText(const char* text, int x, int y, uint8_t brightness) {
    int cursorX = x;
    while (*text) {
        ledMatrixDrawChar(cursorX, y, *text, brightness);
        cursorX += 6;
        if (cursorX > MATRIX_COLS) break;
        text++;
    }
}

void drawTextCentered(const char* text, int y, uint8_t brightness) {
    int len = 0;
    const char* p = text;
    while (*p) { len++; p++; }
    int x = (MATRIX_COLS - len * 6) / 2;
    if (x < 0) x = 0;
    drawText(text, x, y, brightness);
}

void setupButtons() {
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_LEFT, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT, INPUT_PULLUP);
}

void setupFrontLEDs() {
    pinMode(FRONT_LED_1, OUTPUT);
    pinMode(FRONT_LED_2, OUTPUT);
    pinMode(FRONT_LED_3, OUTPUT);
    pinMode(FRONT_LED_4, OUTPUT);
}

void setupFlashlight() {
    pinMode(FLASHLIGHT_LED, OUTPUT);
    digitalWrite(FLASHLIGHT_LED, LOW);
}

void setupSAO() {
    pinMode(SAO_GPIO0, OUTPUT);
    pinMode(SAO_GPIO1, OUTPUT);
    digitalWrite(SAO_GPIO0, LOW);
    digitalWrite(SAO_GPIO1, LOW);
}

bool buttonPressed(int btn) {
    return digitalRead(btn) == LOW;
}

bool anyButtonPressed() {
    return buttonPressed(BUTTON_A) || buttonPressed(BUTTON_B) ||
           buttonPressed(BUTTON_UP) || buttonPressed(BUTTON_DOWN) ||
           buttonPressed(BUTTON_LEFT) || buttonPressed(BUTTON_RIGHT);
}

void waitForButtonRelease() {
    while (anyButtonPressed()) delay(10);
    delay(50);
}

void recordActivity() {
    lastActivity = millis();
}

void knightRiderSweep(unsigned long interval) {
    static int pos = 0;
    static bool dir = true;
    static unsigned long lastUpdate = 0;
    static bool leds[4] = {0, 0, 0, 0};

    if (millis() - lastUpdate > interval) {
        lastUpdate = millis();
        memset(leds, 0, sizeof(leds));
        if (dir) {
            pos++;
            if (pos >= 4) { pos = 3; dir = false; }
        } else {
            pos--;
            if (pos < 0) { pos = 1; dir = true; }
        }
        if (pos >= 0 && pos < 4) leds[pos] = true;
    }

    digitalWrite(FRONT_LED_1, leds[0]);
    digitalWrite(FRONT_LED_2, leds[1]);
    digitalWrite(FRONT_LED_3, leds[2]);
    digitalWrite(FRONT_LED_4, leds[3]);
}

unsigned long saoToggleTime = 0;
bool saoGPIO0State = false;
unsigned long saoPWMTime = 0;
bool saoPWMDirection = true;
float saoPWMValue = 0;

void updateSAO(unsigned long now) {
    if (now - saoToggleTime >= 1000) {
        saoToggleTime = now;
        saoGPIO0State = !saoGPIO0State;
        digitalWrite(SAO_GPIO0, saoGPIO0State);
    }

    if (now - saoPWMTime >= 30) {
        saoPWMTime = now;
        if (saoPWMDirection) {
            saoPWMValue += 1.67f;
            if (saoPWMValue >= 100) { saoPWMValue = 100; saoPWMDirection = false; }
        } else {
            saoPWMValue -= 1.67f;
            if (saoPWMValue <= 0) { saoPWMValue = 0; saoPWMDirection = true; }
        }
        analogWrite(SAO_GPIO1, (int)(saoPWMValue * 255.0f / 100.0f));
    }
}

const int TVBGONE_CODES[][2] = {
    {9000, 4500},
    {4500, 4500}
};
int tvbgoneStep = 0;
unsigned long tvbgoneLastToggle = 0;
bool tvbgoneOn = false;

void updateTVBGone(unsigned long now) {
    if (currentMode != MODE_TVBGONE) return;
    if (now - tvbgoneLastToggle > 20) {
        tvbgoneLastToggle = now;
        tvbgoneOn = !tvbgoneOn;
        digitalWrite(FLASHLIGHT_LED, tvbgoneOn ? HIGH : LOW);
    }
}

void showMenu() {
    ledMatrixClear();
    const char* items[] = {"Tetris", "Snake", "Brick", "Pong", "TV-B-Gone"};
    for (int i = 0; i < 5; i++) {
        if (i == currentMenuItem) {
            ledMatrixSetPixel(0, 1 + i * 2, 255);
            ledMatrixSetPixel(1, 1 + i * 2, 150);
        }
        ledMatrixSetPixel(2, 1 + i * 2, (i == currentMenuItem) ? 255 : 80);
        ledMatrixSetPixel(3, 1 + i * 2, 80);
    }
    char buf[16];
    buf[0] = 'A' + currentMenuItem;
    buf[1] = '\0';
    drawText(buf, 3, 1 + currentMenuItem * 2, 255);
}

void handleMenuInput() {
    if (buttonPressed(BUTTON_UP)) {
        recordActivity();
        if (currentMenuItem > 0) currentMenuItem--;
        waitForButtonRelease();
        showMenu();
    } else if (buttonPressed(BUTTON_DOWN)) {
        recordActivity();
        if (currentMenuItem < 4) currentMenuItem++;
        waitForButtonRelease();
        showMenu();
    } else if (buttonPressed(BUTTON_A)) {
        recordActivity();
        waitForButtonRelease();
        switch (currentMenuItem) {
            case 0: currentMode = MODE_TETRIS; break;
            case 1: currentMode = MODE_SNAKE; break;
            case 2: currentMode = MODE_BRICK; break;
            case 3: currentMode = MODE_PONG; break;
            case 4: currentMode = MODE_TVBGONE; break;
        }
        ledMatrixClear();
    }
}

class TetrisGame {
public:
    static const int W = 7;
    static const int H = 15;
    static const int BLOCKS[7][4];
    uint8_t field[H][W];
    int pieceX, pieceY, pieceType, pieceRot;
    unsigned long lastFall;
    int score;
    bool gameOver;

    TetrisGame() { reset(); }

    void reset() {
        memset(field, 0, sizeof(field));
        score = 0;
        gameOver = false;
        newPiece();
    }

    void newPiece() {
        pieceType = random(7);
        pieceX = W / 2 - 2;
        pieceY = 0;
        pieceRot = 0;
    }

    bool canPlace(int px, int py, int type, int rot) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (getBlock(type, rot, i, j) && (px + j < 0 || px + j >= W || py + i >= H || field[py + i][px + j])) {
                    return false;
                }
            }
        }
        return true;
    }

    bool getBlock(int type, int rot, int x, int y) {
        static const uint8_t pieces[7][4][4] = {
            {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}},
            {{0,1,1,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
            {{0,1,1,0},{0,0,1,0},{0,1,1,0},{0,0,0,0}},
            {{0,0,1,0},{0,0,1,0},{0,1,1,0},{0,0,0,0}},
            {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},
            {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}},
            {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}}
        };
        int r = rot % 4;
        if (type == 0) {
            const uint8_t I[4][4] = {{1,1,1,1},{0,0,0,0},{0,0,0,0},{0,0,0,0}};
            if (r == 0 && y < 4 && x < 4) return I[y][x];
        }
        if (y < 4 && x < 4) return pieces[type][r][y * 4 + x];
        return 0;
    }

    void lockPiece() {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (getBlock(pieceType, pieceRot, j, i)) {
                    int fx = pieceX + j;
                    int fy = pieceY + i;
                    if (fy >= 0 && fy < H && fx >= 0 && fx < W) {
                        field[fy][fx] = (pieceType + 1) * 30;
                    }
                }
            }
        }
        clearLines();
        newPiece();
    }

    void clearLines() {
        for (int y = H - 1; y >= 0; y--) {
            bool full = true;
            for (int x = 0; x < W; x++) {
                if (!field[y][x]) { full = false; break; }
            }
            if (full) {
                score += 100;
                for (int yy = y; yy > 0; yy--) {
                    for (int x = 0; x < W; x++) {
                        field[yy][x] = field[yy - 1][x];
                    }
                }
                for (int x = 0; x < W; x++) field[0][x] = 0;
            }
        }
    }

    void render() {
        ledMatrixClear();
        for (int y = 0; y < H && y < MATRIX_ROWS; y++) {
            for (int x = 0; x < W && x < MATRIX_COLS; x++) {
                if (field[y][x]) {
                    ledMatrixSetPixel(x, y, field[y][x]);
                }
            }
        }
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (getBlock(pieceType, pieceRot, j, i)) {
                    int px = pieceX + j;
                    int py = pieceY + i;
                    if (py >= 0 && py < MATRIX_ROWS && px >= 0 && px < MATRIX_COLS) {
                        ledMatrixSetPixel(px, py, 255);
                    }
                }
            }
        }
    }

    void update() {
        if (gameOver) return;
        unsigned long now = millis();
        if (now - lastFall > 500) {
            lastFall = now;
            if (canPlace(pieceX, pieceY + 1, pieceType, pieceRot)) {
                pieceY++;
            } else {
                lockPiece();
                if (!canPlace(pieceX, pieceY, pieceType, pieceRot)) {
                    gameOver = true;
                }
            }
        }
    }

    void input() {
        if (buttonPressed(BUTTON_LEFT)) {
            recordActivity();
            if (canPlace(pieceX - 1, pieceY, pieceType, pieceRot)) pieceX--;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_RIGHT)) {
            recordActivity();
            if (canPlace(pieceX + 1, pieceY, pieceType, pieceRot)) pieceX++;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_UP)) {
            recordActivity();
            if (canPlace(pieceX, pieceY, pieceType, (pieceRot + 1) % 4)) pieceRot = (pieceRot + 1) % 4;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_DOWN)) {
            recordActivity();
            if (canPlace(pieceX, pieceY + 1, pieceType, pieceRot)) pieceY++;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_B)) {
            recordActivity();
            currentMode = MODE_MENU;
            waitForButtonRelease();
        }
    }
};

class SnakeGame {
public:
    static const int MAX_LEN = 30;
    int snakeX[MAX_LEN], snakeY[MAX_LEN];
    int snakeLen;
    int dirX, dirY;
    int foodX, foodY;
    unsigned long lastMove;
    int score;
    bool gameOver;

    SnakeGame() { reset(); }

    void reset() {
        snakeLen = 3;
        for (int i = 0; i < snakeLen; i++) {
            snakeX[i] = 4;
            snakeY[i] = 4 - i;
        }
        dirX = 0;
        dirY = 1;
        score = 0;
        gameOver = false;
        spawnFood();
    }

    void spawnFood() {
        foodX = random(MATRIX_COLS);
        foodY = random(MATRIX_ROWS);
        for (int i = 0; i < snakeLen; i++) {
            if (snakeX[i] == foodX && snakeY[i] == foodY) {
                spawnFood();
                return;
            }
        }
    }

    void render() {
        ledMatrixClear();
        ledMatrixSetPixel(foodX, foodY, 255);
        for (int i = 0; i < snakeLen; i++) {
            ledMatrixSetPixel(snakeX[i], snakeY[i], i == 0 ? 255 : 150);
        }
    }

    void update() {
        if (gameOver) return;
        unsigned long now = millis();
        if (now - lastMove > 150) {
            lastMove = now;
            int headX = snakeX[0] + dirX;
            int headY = snakeY[0] + dirY;
            if (headX < 0 || headX >= MATRIX_COLS || headY < 0 || headY >= MATRIX_ROWS) {
                gameOver = true;
                return;
            }
            for (int i = 1; i < snakeLen; i++) {
                if (snakeX[i] == headX && snakeY[i] == headY) {
                    gameOver = true;
                    return;
                }
            }
            for (int i = snakeLen - 1; i > 0; i--) {
                snakeX[i] = snakeX[i - 1];
                snakeY[i] = snakeY[i - 1];
            }
            snakeX[0] = headX;
            snakeY[0] = headY;
            if (headX == foodX && headY == foodY) {
                if (snakeLen < MAX_LEN) snakeLen++;
                score += 10;
                spawnFood();
            }
        }
    }

    void input() {
        if (buttonPressed(BUTTON_LEFT)) {
            recordActivity();
            if (dirX != 1) { dirX = -1; dirY = 0; }
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_RIGHT)) {
            recordActivity();
            if (dirX != -1) { dirX = 1; dirY = 0; }
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_UP)) {
            recordActivity();
            if (dirY != 1) { dirX = 0; dirY = -1; }
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_DOWN)) {
            recordActivity();
            if (dirY != -1) { dirX = 0; dirY = 1; }
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_B)) {
            recordActivity();
            currentMode = MODE_MENU;
            waitForButtonRelease();
        }
    }
};

class BrickGame {
public:
    static const int ROWS = 4;
    static const int COLS = 5;
    int bricks[ROWS][COLS];
    int ballX, ballY, ballVX, ballVY;
    int paddleX;
    unsigned long lastUpdate;
    int score;
    bool gameOver;

    BrickGame() { reset(); }

    void reset() {
        memset(bricks, 0, sizeof(bricks));
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                bricks[r][c] = (r + 1) * 40;
            }
        }
        ballX = 4;
        ballY = 4;
        ballVX = 1;
        ballVY = -1;
        paddleX = 4;
        score = 0;
        gameOver = false;
    }

    void render() {
        ledMatrixClear();
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (bricks[r][c]) {
                    ledMatrixSetPixel(c, r + 1, bricks[r][c]);
                }
            }
        }
        ledMatrixSetPixel(ballX, ballY, 255);
        ledMatrixSetPixel(paddleX, 8, 255);
        if (paddleX > 0) ledMatrixSetPixel(paddleX - 1, 8, 100);
        if (paddleX < 8) ledMatrixSetPixel(paddleX + 1, 8, 100);
    }

    void update() {
        if (gameOver) return;
        unsigned long now = millis();
        if (now - lastUpdate > 200) {
            lastUpdate = now;
            ballX += ballVX;
            ballY += ballVY;
            if (ballX < 0 || ballX >= MATRIX_COLS) {
                ballVX = -ballVX;
                ballX += ballVX;
            }
            if (ballY < 0) {
                ballVY = -ballVY;
                ballY += ballVY;
            }
            if (ballY == 8) {
                if (abs(ballX - paddleX) <= 1) {
                    ballVY = -ballVY;
                    score += 10;
                } else {
                    gameOver = true;
                }
            }
            int bx = ballX;
            int by = ballY;
            if (by >= 1 && by <= ROWS && bx >= 0 && bx < COLS) {
                if (bricks[by - 1][bx]) {
                    bricks[by - 1][bx] = 0;
                    ballVY = -ballVY;
                    score += 10;
                }
            }
        }
    }

    void input() {
        if (buttonPressed(BUTTON_LEFT)) {
            recordActivity();
            if (paddleX > 0) paddleX--;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_RIGHT)) {
            recordActivity();
            if (paddleX < 8) paddleX++;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_B)) {
            recordActivity();
            currentMode = MODE_MENU;
            waitForButtonRelease();
        }
    }
};

class PongGame {
public:
    int leftY, rightY;
    int ballX, ballY, ballVX, ballVY;
    int leftScore, rightScore;
    unsigned long lastUpdate;
    bool gameOver;

    PongGame() { reset(); }

    void reset() {
        leftY = 4;
        rightY = 4;
        ballX = 4;
        ballY = 4;
        ballVX = 1;
        ballVY = random(2) ? 1 : -1;
        leftScore = 0;
        rightScore = 0;
        gameOver = false;
    }

    void render() {
        ledMatrixClear();
        ledMatrixSetPixel(0, leftY, 255);
        ledMatrixSetPixel(8, rightY, 255);
        ledMatrixSetPixel(ballX, ballY, 255);
    }

    void update() {
        if (gameOver) return;
        unsigned long now = millis();
        if (now - lastUpdate > 200) {
            lastUpdate = now;
            ballX += ballVX;
            ballY += ballVY;
            if (ballY < 0 || ballY >= MATRIX_ROWS) {
                ballVY = -ballVY;
            }
            if (ballX == 1 && abs(ballY - leftY) <= 1) {
                ballVX = -ballVX;
                leftScore++;
            }
            if (ballX == 7 && abs(ballY - rightY) <= 1) {
                ballVX = -ballVX;
                rightScore++;
            }
            if (ballX < 0 || ballX > 8) {
                gameOver = true;
            }
        }
    }

    void input() {
        if (buttonPressed(BUTTON_LEFT)) {
            recordActivity();
            if (leftY > 0) leftY--;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_RIGHT)) {
            recordActivity();
            if (rightY > 0) rightY--;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_UP)) {
            recordActivity();
            rightY = leftY;
            if (rightY > 0) rightY--;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_DOWN)) {
            recordActivity();
            rightY = leftY;
            if (rightY < 8) rightY++;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_A)) {
            recordActivity();
            if (leftY < 8) leftY++;
            waitForButtonRelease();
        } else if (buttonPressed(BUTTON_B)) {
            recordActivity();
            currentMode = MODE_MENU;
            waitForButtonRelease();
        }
    }
};

TetrisGame tetrisGame;
SnakeGame snakeGame;
BrickGame brickGame;
PongGame pongGame;

void showIdleScreen() {
    ledMatrixClear();
    int msgIdx = random(NUM_MESSAGES);
    drawTextCentered(RANDOM_MESSAGES[msgIdx], 4, 150);
    currentMode = MODE_IDLE;
    lastActivity = millis();
}

void setup() {
    delay(1000);
    setupButtons();
    setupFrontLEDs();
    setupFlashlight();
    setupSAO();
    ledMatrixBegin();
    ledMatrixClear();
    showMenu();
    lastActivity = millis();
}

void loop() {
    unsigned long now = millis();

    knightRiderSweep(80);
    updateSAO(now);
    updateTVBGone(now);

    if (anyButtonPressed()) {
        if (currentMode == MODE_IDLE) {
            currentMode = MODE_MENU;
            showMenu();
        }
        lastActivity = millis();
    }

    if (now - lastActivity > IDLE_TIMEOUT) {
        if (currentMode != MODE_IDLE && currentMode != MODE_TVBGONE) {
            showIdleScreen();
        }
    }

    switch (currentMode) {
        case MODE_MENU:
            handleMenuInput();
            break;
        case MODE_TETRIS:
            tetrisGame.input();
            tetrisGame.update();
            tetrisGame.render();
            break;
        case MODE_SNAKE:
            snakeGame.input();
            snakeGame.update();
            snakeGame.render();
            break;
        case MODE_BRICK:
            brickGame.input();
            brickGame.update();
            brickGame.render();
            break;
        case MODE_PONG:
            pongGame.input();
            pongGame.update();
            pongGame.render();
            break;
        case MODE_TVBGONE:
            if (buttonPressed(BUTTON_B)) {
                recordActivity();
                currentMode = MODE_MENU;
                digitalWrite(FLASHLIGHT_LED, LOW);
                waitForButtonRelease();
                showMenu();
            }
            break;
        case MODE_IDLE:
            break;
    }

    delay(10);
}
