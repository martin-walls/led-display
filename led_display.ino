#include "avr/interrupt.h"
#include <EEPROM.h>

#define LAYER_FRONT 0
#define LAYER_BACK 1

#define BTN_ON 0
#define BTN_OFF 1

#define DIR_L_TO_R 0b000001
#define DIR_R_TO_L 0b000010
#define DIR_T_TO_B 0b000100
#define DIR_B_TO_T 0b001000
#define DIR_F_TO_B 0b010000
#define DIR_B_TO_F 0b100000

// animation constants
#define OFF 0
// text effects
#define TEXT_STATIC 1
#define TEXT_SCROLL 2
#define TEXT_SCROLL_IF_LONG 3
#define TEXT_SCROLL_BOTH_LAYERS 4
#define TEXT_STATIC_BOTH_LAYERS 5
// general animation effects
#define SOLID 32
#define WIPE 33
#define WIPE_DIAGONAL 34
#define SNAKE 35
#define BOX_OUTLINE 36
// special animations
#define PACMAN 64

// animation flags
#define TEXT_SCROLL_FRAME_DELAY 120
#define TEXT_SCROLL_FLAG_Z 0b0001

// animation values
#define WIPE_FRAMEDELAY_X 20
#define WIPE_FRAMEDELAY_Y 70
#define WIPE_DIAGONAL_FRAMEDELAY 20
#define WIPE_DIAGONAL_ROW_OFFSET 2
#define WIPE_DIAGONAL_WIDTH 28

#define PACMAN_FRAMEDELAY 300
#define PACDOT_FRAMEDELAY 100
#define PACMAN_FLAG_PACMAN_SHAPE 0b000001
#define PACMAN_FLAG_DIRECTION    0b000010
#define PACMAN_FLAG_DIR_L_TO_R       0b00
#define PACMAN_FLAG_DIR_R_TO_L       0b10
#define PACMAN_FLAG_NUM_GHOSTS   0b011100
#define PACMAN_FLAG_NUM_GHOSTS_LSHIFT 2
#define PACMAN_GHOST_GAP 12

#define PACMAN_BLANK_FRAME 0
#define PACMAN_SCROLL 1
#define PACMAN_SCROLL_GHOSTS 2

#define PACDOT_SPACING 4
#define PACDOT_HEIGHT 2

// communication to wifi chip
#define BAUDRATE 115200
#define SERIAL_CONFIG SERIAL_8N1

#define PIN_MASTER_SLAVE_MODE A3
#define ARDUINO_MASTER 0
#define WIFI_MASTER 1

#define PIN_WIFI_ENABLED A4
#define WIFI_ENABLED 1
#define WIFI_DISABLED 0

#define PIN_WIFI_CONNECTED A5
#define WIFI_CONNECTED 1
#define WIFI_DISCONNECTED 0


// each 32 bit row is split into two 16-bit ints. 
//   [  [ front layer bottom to top ],  [ back layer bottom to top ]  ]
//   [  [0000000000000000, (uint16_t), (uint16_t), ...x6],  [(uint16_t), (uint16_t), ...x6]  ]
volatile uint16_t matrix[2][6][2];
// current row being output
volatile uint8_t currentRow = 0;

// millis of last button press
uint32_t lastBtnIntMillis = 0;
// current mode
volatile uint8_t mode = 0;
// flag set to true whenever mode is changed by btn press
volatile bool modeChanged = false;
// stores the last state of PINB pins
volatile uint8_t PINB_lastState = 0;

// Address where mode is stored in EEPROM memory
uint16_t eeModeAddr = 0;

uint8_t masterSlaveMode = ARDUINO_MASTER;
uint8_t wifiEnabledMode = WIFI_DISABLED;
uint8_t wifiConnectedMode = WIFI_DISCONNECTED;

// character and shape tables defined at bottom of file
extern const uint8_t ascii[][6];
extern const uint8_t asciiLen[];
extern const uint8_t pacmanShapes[][6];
extern const uint8_t pacmanShapeLen;
extern const uint8_t pacmanGhostShape[6];
extern const uint8_t pacmanGhostShapeLen;


uint8_t activeAnim = OFF;
uint8_t animFlags;
char text[64];

uint16_t frameDelay;
uint32_t lastFrameUpdate;

uint16_t totalSteps;
uint16_t curStep;

uint8_t activeSubAnim;
uint16_t subAnimTotalSteps;
uint16_t subAnimCurStep;
    


void setup() {
    
    // set PORTD input/output modes
    // D0-3 unused
    // D4 clock
    // D5-6 cathodes
    // D7 anodes 25-32
    DDRD |= 0b11110000;

    // set PORTB input/output modes
    // D13 unused, first two bits map to crystal and unusable
    // D8 anodes 17-24
    // D9 anodes 9-16
    // D10 anodes 1-8
    // D11-12 btns
    DDRB = 0b00111;


    PORTB &= 0b11111000;
    PORTD &= 0b00011111;

    for (uint8_t i = 0; i < 8; i++) {
        PORTD &= (0 << PD4);
        PORTD |= (1 << PD4);
    }

    // TC2 (Timer/Counter 2)

    // TC2 Control Register A
    TCCR2A = 0;

    // TC2 Control Register B
    TCCR2B = 0;

    // set CTC mode for TC2 (Clear Timer on Compare match)
    // counter is cleared when TCNT2 == OCR2A
    // (Timer/Counter Register: actual counter value) == (Output Compare Register)
    TCCR2A |= (1 << WGM21);

    // Timer 2 Output Compare Register A
    // interrupt every 25600th CPU cyle (256 (prescalar) * (1/10)*1000)
    OCR2A = 75;

    // TC2 Counter Value Register
    // start counter at 0
    TCNT2 = 0;

    // select clock source for TC2
    // set a 256 prescaler
    TCCR2B |= (1 << CS22) | (1 << CS21);


    // TC2 Interrupt Mask Register
    TIMSK2 |= (1 << OCIE2A);


    // setup button interrupts

    // Pin Change Interrupt Control Register 
    // enable interrupts for D8-13
    PCICR |= (1 << PCIE0);

    // Pin Change Mask Register 0
    // enable interrupts for D11, D12
    // PCINT3 = pin14, D11
    // PCINT4 = pin15, D12 
    PCMSK0 |= (1 << PCINT3) | (1 << PCINT4);


    // store current state of PORTB pins for button interrupts
    PINB_lastState = PINB;

    pinMode(PIN_MASTER_SLAVE_MODE, OUTPUT);
    pinMode(PIN_WIFI_ENABLED, INPUT);
    pinMode(PIN_WIFI_CONNECTED, INPUT);

    delay(500);

    // see if wifi chip is present and enabled
    wifiEnabledMode = digitalRead(PIN_WIFI_ENABLED);
    wifiConnectedMode = digitalRead(PIN_WIFI_CONNECTED);
    // if wifi chip present and enabled, and it is requesting master
    if (wifiEnabledMode == WIFI_ENABLED/* && digitalRead(PIN_WIFI_REQUEST_MASTER) == REQUEST_MASTER_TRUE*/) {
        setMasterSlaveMode(WIFI_MASTER);
        if (wifiConnectedMode == WIFI_DISCONNECTED) {
            scrollText("CONNECTING...", LAYER_FRONT);
        }
    }


    // get mode from non-volatile memory
    mode = EEPROM.read(eeModeAddr);

    if (masterSlaveMode == ARDUINO_MASTER) {
        displayMode();
        modeChanged = true;
    }

    randomSeed(analogRead(0));
}

void loop() {
    wifiEnabledMode = digitalRead(PIN_WIFI_ENABLED);
    if (wifiEnabledMode == WIFI_ENABLED) {
        setMasterSlaveMode(WIFI_MASTER);
    } else if (wifiEnabledMode == WIFI_DISABLED) {
        setMasterSlaveMode(ARDUINO_MASTER);
    }

    // store new mode in EEPROM memory, update display
    if (modeChanged && masterSlaveMode == ARDUINO_MASTER) {
        delay(1000);
        fillMatrix(0);

        EEPROM.write(eeModeAddr, mode);
        modeChanged = false;

        switch (mode) {
            case 0:
                pacmanInit();
                break;
            case 1:
                scrollText("HELLO WORLD", LAYER_FRONT);
                break;
            case 2:
                scrollTextBothLayers("HIIII");
                break;
            case 3:
                staticTextBothLayers("ALOHA", 2);
                break;
            case 4:
                wipe(DIR_L_TO_R);
                break;
            case 5:
                wipe(DIR_R_TO_L);
                break;
            case 6:
                wipe(DIR_B_TO_T);
                break;
            case 7:
                wipe(DIR_T_TO_B);
                break;
            case 8:
                wipeDiagonal(DIR_R_TO_L | DIR_T_TO_B);
                break;
            default:
                displayOff();
                break;
        }
    } else if (masterSlaveMode == WIFI_MASTER) {
        if (digitalRead(PIN_WIFI_CONNECTED) == WIFI_DISCONNECTED && wifiConnectedMode == WIFI_CONNECTED) {
            wifiConnectedMode = WIFI_DISCONNECTED;
            scrollText("CONNECTING...", LAYER_FRONT);
        } else if (digitalRead(PIN_WIFI_CONNECTED) == WIFI_CONNECTED && wifiConnectedMode == WIFI_DISCONNECTED) {
            wifiConnectedMode = WIFI_CONNECTED;
            displayOff();
        } else {
            if (Serial.available() > 0) {
                delay(100);
                char inText[64];

                uint8_t availableBytes = Serial.available();

                for (uint8_t i = 0; i < availableBytes; i++) {
                    inText[i] = Serial.read();
                }
                inText[availableBytes] = '\0';

                textToUpperCase(inText);

                scrollTextIfLong(inText, LAYER_FRONT);
            }
        }
    }

    updateDisplay();


/*
    // set effect for mode
    switch (mode) {
        case 0:
            displayText("HIII", LAYER_FRONT, 2);
            break;
        case 1:
            scrollTextBothLayers("HELLO WORLD!", true);
            break;
        case 2:
            pacman();
            break;
        case 3:
            wipe(DIR_L_TO_R);
            if (modeChanged) break;
            delay(1000);
            wipe(DIR_R_TO_L);
            if (modeChanged) break;
            delay(1000);
            wipe(DIR_B_TO_T);
            if (modeChanged) break;
            delay(1000);
            wipe(DIR_T_TO_B);
            if (modeChanged) break;
            delay(1000);
            break;
        case 4:
            wipeDiagonal(DIR_L_TO_R, DIR_B_TO_T);
            if (modeChanged) break;
            delay(1000);
            wipeDiagonal(DIR_L_TO_R, DIR_T_TO_B);
            if (modeChanged) break;
            delay(1000);
            wipeDiagonal(DIR_R_TO_L, DIR_B_TO_T);
            if (modeChanged) break;
            delay(1000);
            wipeDiagonal(DIR_R_TO_L, DIR_T_TO_B);
            if (modeChanged) break;
            delay(1000);
            break;
        case 5:
            fillMatrix(0);
            boxOutline(0, 0, 31, 5, LAYER_FRONT);
            boxOutline(0, 0, 31, 5, LAYER_BACK);
            if (modeChanged) break;
            delay(500);

            fillMatrix(0);
            boxOutline(1, 1, 30, 4, LAYER_FRONT);
            boxOutline(1, 1, 30, 4, LAYER_BACK);
            if (modeChanged) break;
            delay(500);
            
            fillMatrix(0);
            boxOutline(2, 2, 29, 3, LAYER_FRONT);
            boxOutline(2, 2, 29, 3, LAYER_BACK);
            if (modeChanged) break;
            delay(500);

            fillMatrix(0);
            if (modeChanged) break;
            delay(500);
            break;
        case 6:
            boxOutline(0, 0, 31, 5, LAYER_FRONT);
            boxOutline(0, 0, 31, 5, LAYER_BACK);
            break;
        case 7:
            scrollText("GOD IS GOOD ", LAYER_FRONT, false);
            if (modeChanged) break;
            scrollText("ALL THE TIME ", LAYER_BACK, false);
            if (modeChanged) break;
            scrollText("AND ALL THE TIME ", LAYER_FRONT, false);
            if (modeChanged) break;
            scrollText("GOD IS GOOD!", LAYER_BACK, true);
            break;
        case 8:
            randomSnakeXYZ(24);
            break;
        case 9:
            randomSnakeXYOneLayer(LAYER_FRONT, 8);
            break;
        case 10:
            randomSnakeXYBothLayers(8);
            break;
        default:
            fillMatrix(0);
            break;
    }
    */
}

// interrupt routine for timer 2
ISR (TIMER2_COMPA_vect) {
    // draw data from the matrix for the current row

    // get current row data from matrix
    uint16_t row0, row1;

    if (currentRow < 6) {
        row0 = matrix[0][currentRow][0];
        row1 = matrix[0][currentRow][1];
    } else {
        row0 = matrix[1][currentRow - 6][0];
        row1 = matrix[1][currentRow - 6][1];
    }

    // define data for each shift register
    uint8_t sr1, sr2, sr3, sr4, sr5, sr6;

    sr1 = row1 & 0xff;
    sr2 = (row1 >> 8) & 0xff;
    sr3 = row0 & 0xff;
    sr4 = (row0 >> 8) & 0xff;

    if (currentRow < 8) {
        sr5 = (1 << currentRow);
        sr6 = 0;
    } else {
        sr5 = 0;
        sr6 = (1 << (currentRow - 8));
    }

    // output data to shift registers
    for (uint8_t j = 0; j < 8; j++) {
        // set clock low
        PORTD &= (0 << PD4);

        PORTB &= 0b11111000;

        PORTB |= (((sr1 >> j) & 1) << 2) | (((sr2 >> j) & 1) << 1) | ((sr3 >> j) & 1);

        PORTD &= 0b00011111;

        PORTD |= (((sr4 >> j) & 1) << 7) | (((sr5 >> j) & 1) << 6) | (((sr6 >> j) & 1) << 5);

        // set clock high to shift data
        PORTD |= (1 << PD4);
    }

    // increment current row 
    currentRow++;

    if (currentRow == 12) {
        currentRow = 0;
    }
}

// interrupt routine for btns
// this is triggered for both rising and falling edges
ISR (PCINT0_vect) {
    // interrupt is handled when state of btn is low (falling edge)
    // current state of D11
    uint8_t btnState1 = (PINB >> PINB3) & 1;
    // current state of D12
    uint8_t btnState2 = (PINB >> PINB4) & 1;

    // find out which button was pressed to trigger the interrupt
    uint8_t btnPressed = 0;
    // check if btn state changed and is now low
    if ((((PINB_lastState ^ PINB) >> PINB3) & 1) == 1 && btnState1 == BTN_ON) {
        // D11 = btn 1
        btnPressed = 1;
    } else if ((((PINB_lastState ^ PINB) >> PINB4) & 1) == 1 && btnState2 == BTN_ON) {
        // D12 = btn 2
        btnPressed = 2;
    }

    // store new pin state
    PINB_lastState = PINB;

    // debounce
    if (millis() - lastBtnIntMillis > 100 && btnPressed != 0) {
        if (btnPressed == 1 && btnState2 == BTN_OFF) {
            mode--;
        } else if (btnPressed == 2 && btnState1 == BTN_OFF) {
            mode++;
        } else if (btnPressed == 1 && btnState2 == BTN_ON) {
            mode -= 10;
        } else if (btnPressed == 2 && btnState1 == BTN_ON) {
            mode += 10;
        }
        modeChanged = true;
        displayMode();
        lastBtnIntMillis = millis();
    }
}

void setMasterSlaveMode(uint8_t mode) {
    masterSlaveMode = mode;
    digitalWrite(PIN_MASTER_SLAVE_MODE, mode);

    if (mode == WIFI_MASTER) {
        Serial.begin(BAUDRATE, SERIAL_CONFIG);
    } else if (mode == ARDUINO_MASTER) {
        Serial.end();
    }
}

void textToUpperCase(char *text) {
    char *p;
    p = text;
    while (*p) {
        if (*p >= 97 && *p <= 122) {
            *p = *p - 32;
        }
        p++;
    }
}


void updateDisplay() {
    if ((millis() - lastFrameUpdate) >= frameDelay) {
        lastFrameUpdate = millis();
        switch(activeAnim) {
            case TEXT_SCROLL:
                scrollTextUpdate();
                break;
            case TEXT_SCROLL_BOTH_LAYERS:
                scrollTextBothLayersUpdate();
                break;
            case WIPE:
                wipeUpdate();
                break;
            case WIPE_DIAGONAL:
                wipeDiagonalUpdate();
                break;
            case PACMAN:
                pacmanUpdate();
                break;
            default:
                break;
        }
    }
}


void incrementStep() {
    curStep++;
    if (curStep >= totalSteps) {
        curStep = 0;
    }
}

void incrementSubAnimStep() {
    subAnimCurStep++;
    if (subAnimCurStep >= subAnimTotalSteps) {
        subAnimCurStep = 0;
    }
}

void displayOff() {
    activeAnim = OFF;
    fillMatrix(0);
}

// Shows the number of the current mode.
void displayMode() {
    fillMatrix(0);
    char modeChars[3];
    itoa(mode, modeChars, 10);
    displayText(modeChars, LAYER_FRONT, 2);
}

// TEXT EFFECTS

// Displays text.
// If text is wider than matrix, scrolls text, else displays statically.
void scrollTextIfLong(const char *message, uint8_t z) {
    uint16_t len = calculateTextPixelWidth(message, false);

    if (len > 32) {
        scrollText(message, z);
    } else {
        displayText(message, z, 0);
    }
}

// Displays text statically
void staticText(const char *message, uint8_t z, uint8_t startX) {
    activeAnim = TEXT_STATIC;
    strcpy(text, message);

    displayText(message, z, startX);
}

void staticTextBothLayers(const char *message, uint8_t startX) {
    activeAnim = TEXT_STATIC_BOTH_LAYERS;
    strcpy(text, message);
    
    displayText(message, LAYER_FRONT, startX);
    displayText(message, LAYER_BACK, startX);
}

// Scrolls text from right to left.
void scrollText(const char *message, uint8_t z) {
    activeAnim = TEXT_SCROLL;
    strcpy(text, message);
    animFlags = 0;
    animFlags |= (TEXT_SCROLL_FLAG_Z & z);

    totalSteps = calculateTextPixelWidth(message, true) + 32;
    curStep = 0;

    frameDelay = TEXT_SCROLL_FRAME_DELAY;
}

void scrollTextUpdate() {
    uint8_t z = (animFlags & TEXT_SCROLL_FLAG_Z) == TEXT_SCROLL_FLAG_Z;
    int16_t x = 31 - curStep;
    displayText(text, z, x);

    incrementStep();
}

// Scrolls text from right to left on both layers.
// This gives a "3D" effect.
void scrollTextBothLayers(const char *message) {
    activeAnim = TEXT_SCROLL_BOTH_LAYERS;
    strcpy(text, message);
    animFlags = 0;

    totalSteps = calculateTextPixelWidth(message, true) + 32;
    curStep = 0;

    frameDelay = TEXT_SCROLL_FRAME_DELAY;
}

void scrollTextBothLayersUpdate() {
    int16_t x = 31 - curStep;
    displayText(text, LAYER_FRONT, x);
    displayText(text, LAYER_BACK, x);

    incrementStep();
}

uint16_t calculateTextPixelWidth(const char *message, bool scrolling) {
    uint16_t len = 0;

    const char *p;
    p = message;
    while (*p) {
        len += asciiLen[*p - ' '];
        if (scrolling) {
            len += 2;
        } else {
            len++;
        }
        p++;
    }

    return len;
}

void displayText(const char *message, uint8_t z, int16_t startX) {
    int16_t x;

    if (startX > 0) {
        // clear matrix left of text
        for (x = 0; x < startX; x++) {
            setColOff(x, z);
        }
    } else {
        x = startX;
    }

    // display each char
    const char *p;
    p = message;
    while (*p) {
        if (x >= 32) {
            break;
        }
        for (uint8_t i = 0; i < asciiLen[*p - ' ']; i++) {
            setColForChar(*p, i, x, z);
            x++;
        }
        // one col space between each char
        setColOff(x, z);
        x++;
        p++;
    }

    // clear matrix right of text
    while (x < 32) {
        setColOff(x, z);
        x++;
    }
}

// Sets a single col of a character to a col in the matrix.
void setColForChar(char c, uint8_t col, uint8_t x, uint8_t z) {
    c -= ' ';

    for (uint8_t i = 0; i < 6; i++) {
        setVoxel(x, i, z, (ascii[c][5 - i] >> (asciiLen[c] - 1 - col)) & 1);
    }
}

// Sets a single col of a character to the last col in the matrix.
// See setColForChar()
void addColOfCharToMatrixR(char c, uint8_t col, uint8_t z) {
    setColForChar(c, col, 31, z);
}



// ANIMATION EFFECTS

void wipe(uint8_t direction) {
    activeAnim = WIPE;
    animFlags = 0;
    animFlags |= direction;

    if (direction & (DIR_L_TO_R | DIR_R_TO_L)) {
        totalSteps = 65;
        frameDelay = WIPE_FRAMEDELAY_X;
    } else if (direction & (DIR_B_TO_T | DIR_T_TO_B)) {
        totalSteps = 13;
        frameDelay = WIPE_FRAMEDELAY_Y;
    }

    curStep = 0;
}

void wipeUpdate() {
    // horizontal
    if (animFlags & (DIR_L_TO_R | DIR_R_TO_L)) {
        if (curStep <= 32) {
            for (uint8_t x = 0; x < 32; x++) {
                if (x < curStep) {
                    if (animFlags & DIR_L_TO_R) {
                        setColBothLayers(x, 1);
                    } else {
                        setColBothLayers(31 - x, 1);
                    }
                } else {
                    if (animFlags & DIR_L_TO_R) {
                        setColBothLayers(x, 0);
                    } else {
                        setColBothLayers(31 - x, 0);
                    }
                }
            }
        } else {
            for (uint8_t x = 0; x < 32; x++) {
                if (x < (curStep - 32)) {
                    if (animFlags & DIR_L_TO_R) {
                        setColBothLayers(x, 0);
                    } else {
                        setColBothLayers(31 - x, 0);
                    }
                } else {
                    if (animFlags & DIR_L_TO_R) {
                        setColBothLayers(x, 1);
                    } else {
                        setColBothLayers(31 - x, 1);
                    }
                }
            }
        }
    // vertical
    } else if (animFlags & (DIR_B_TO_T | DIR_T_TO_B)) {
        if (curStep <= 6) {
            for (uint8_t y = 0; y < 6; y++) {
                if (y < curStep) {
                    if (animFlags & DIR_B_TO_T) {
                        setRowBothLayers(y, 1);
                    } else {
                        setRowBothLayers(5 - y, 1);
                    }
                } else {
                    if (animFlags & DIR_B_TO_T) {
                        setRowBothLayers(y, 0);
                    } else {
                        setRowBothLayers(5 - y, 0);
                    }
                }
            }
        } else {
            for (uint8_t y = 0; y < 32; y++) {
                if (y < (curStep - 6)) {
                    if (animFlags & DIR_B_TO_T) {
                        setRowBothLayers(y, 0);
                    } else {
                        setRowBothLayers(5 - y, 0);
                    }
                } else {
                    if (animFlags & DIR_B_TO_T) {
                        setRowBothLayers(y, 1);
                    } else {
                        setRowBothLayers(5 - y, 1);
                    }
                }
            }
        }
    }

    incrementStep();
}

void wipeDiagonal(uint8_t direction) {
    activeAnim = WIPE_DIAGONAL;
    animFlags = 0;
    animFlags |= direction;

    totalSteps = 32 + (5 * WIPE_DIAGONAL_ROW_OFFSET) + WIPE_DIAGONAL_WIDTH;
    frameDelay = WIPE_DIAGONAL_FRAMEDELAY;
    curStep = 0;
}

void wipeDiagonalUpdate() {
    if ((animFlags & (DIR_L_TO_R | DIR_B_TO_T)) == (DIR_L_TO_R | DIR_B_TO_T)) {
        for (uint8_t y = 0; y < 6; y++) {
            setVoxelOn(curStep - (2 * y), y, LAYER_FRONT);
            setVoxelOff(curStep - WIPE_DIAGONAL_WIDTH - (2 * y), y, LAYER_FRONT);
        }
    } else if ((animFlags & (DIR_L_TO_R | DIR_T_TO_B)) == (DIR_L_TO_R | DIR_T_TO_B)) {
        for (uint8_t y = 0; y < 6; y++) {
            setVoxelOn(curStep - (2 * (5 - y)), y, LAYER_FRONT);
            setVoxelOff(curStep - WIPE_DIAGONAL_WIDTH - (2 * (5 - y)), y, LAYER_FRONT);
        }
    } else if ((animFlags & (DIR_R_TO_L | DIR_B_TO_T)) == (DIR_R_TO_L | DIR_B_TO_T)) {
        for (uint8_t y = 0; y < 6; y++) {
            setVoxelOn((totalSteps - curStep - 1) - WIPE_DIAGONAL_WIDTH - (2 * (5 - y)), y, LAYER_FRONT);
            setVoxelOff((totalSteps - curStep - 1) - (2 * (5 - y)), y, LAYER_FRONT);
        }
    } else if ((animFlags & (DIR_R_TO_L | DIR_T_TO_B)) == (DIR_R_TO_L | DIR_T_TO_B)) {
        for (uint8_t y = 0; y < 6; y++) {
            setVoxelOn((totalSteps - curStep - 1) - WIPE_DIAGONAL_WIDTH - (2 * y), y, LAYER_FRONT);
            setVoxelOff((totalSteps - curStep - 1) - (2 * y), y, LAYER_FRONT);
        }
    }

    incrementStep();
}

// PACMAN ANIMATION

void pacmanInit() {
    activeAnim = PACMAN;
    animFlags = 0;
    totalSteps = 8;
    frameDelay = PACMAN_FRAMEDELAY;
    curStep = 4;
    pacmanSubAnimInit();
}

void pacmanUpdate() {
    pacmanSubAnimUpdate();
    if (subAnimCurStep == 0) {
        incrementStep();
        pacmanSubAnimInit();
    }
}

void pacmanSubAnimInit() {
    switch (curStep) {
        case 0:
            pacmanScrollInit(DIR_L_TO_R);
            break;
        case 1:
            blankFrameInit(2);
            break;
        case 2:
            pacmanScrollInit(DIR_R_TO_L);
            break;
        case 3:
            blankFrameInit(2);
            break;
        case 4:
            pacmanScrollWGhostsInit(2, DIR_L_TO_R);
            break;
        case 5:
            blankFrameInit(2);
            break;
        case 6:
            pacmanScrollWGhostsInit(3, DIR_R_TO_L);
            break;
        case 7:
            blankFrameInit(2);
            break;
    }
}

void pacmanSubAnimUpdate() {
    switch (activeSubAnim) {
        case PACMAN_SCROLL:
            pacmanScrollUpdate();
            break;
        case PACMAN_BLANK_FRAME:
            blankFrameUpdate();
            break;
        case PACMAN_SCROLL_GHOSTS:
            pacmanScrollWGhostsUpdate();
            break;
    }
}

void blankFrameInit(uint8_t numFrames) {
    activeSubAnim = PACMAN_BLANK_FRAME;
    subAnimTotalSteps = numFrames;
    subAnimCurStep = 0;
}

void blankFrameUpdate() {
    fillMatrix(0);
    incrementSubAnimStep();
}

void pacmanScrollInit(uint8_t dir) {
    activeSubAnim = PACMAN_SCROLL;
    subAnimTotalSteps = 38;
    subAnimCurStep = 0;
    animFlags &= ~PACMAN_FLAG_DIRECTION;
    if (dir == DIR_L_TO_R) {
        animFlags |= PACMAN_FLAG_DIR_L_TO_R;
    } else {
        animFlags |= PACMAN_FLAG_DIR_R_TO_L;
    }
}

void pacmanScrollUpdate() {
    uint8_t dir = animFlags & PACMAN_FLAG_DIRECTION;
    if (dir == PACMAN_FLAG_DIR_L_TO_R) dir = DIR_L_TO_R;
    else dir = DIR_R_TO_L;

    if (subAnimCurStep == 0) {
        drawPacDots(dir);
        // start with shape 1 - so it lines up with the pacdots
        animFlags |= PACMAN_FLAG_PACMAN_SHAPE;
    }

    int8_t pacmanPos;
    if (dir == DIR_L_TO_R) {
        pacmanPos = subAnimCurStep - 5;
    } else {
        pacmanPos = 32 - subAnimCurStep;
    }

    uint8_t shapeIndex = animFlags & PACMAN_FLAG_PACMAN_SHAPE;
    drawPacmanShape(pacmanPos, LAYER_FRONT, shapeIndex, dir);
    if (dir == DIR_L_TO_R) {
        setColOff(pacmanPos - 1, LAYER_FRONT);
        setVoxelOff(pacmanPos + 3, PACDOT_HEIGHT, LAYER_BACK);
    } else {
        setColOff(pacmanPos + pacmanShapeLen, LAYER_FRONT);
        setVoxelOff(pacmanPos + 1, PACDOT_HEIGHT, LAYER_BACK);
    }

    animFlags ^= PACMAN_FLAG_PACMAN_SHAPE;

    incrementSubAnimStep();
}

void pacmanScrollWGhostsInit(uint8_t numGhosts, uint8_t dir) {
    activeSubAnim = PACMAN_SCROLL_GHOSTS;
    subAnimTotalSteps = 36 + PACMAN_GHOST_GAP + numGhosts * (pacmanGhostShapeLen + 2);
    subAnimCurStep = 0;
    // max 4 ghosts
    if (numGhosts > 4) numGhosts = 4;
    animFlags &= ~PACMAN_FLAG_NUM_GHOSTS;
    animFlags |= numGhosts << PACMAN_FLAG_NUM_GHOSTS_LSHIFT;

    animFlags &= ~PACMAN_FLAG_DIRECTION;
    if (dir == DIR_L_TO_R) {
        animFlags |= PACMAN_FLAG_DIR_L_TO_R;
    } else {
        animFlags |= PACMAN_FLAG_DIR_R_TO_L;
    }
}

void pacmanScrollWGhostsUpdate() {
    uint8_t dir = animFlags & PACMAN_FLAG_DIRECTION;
    if (dir == PACMAN_FLAG_DIR_L_TO_R) dir = DIR_L_TO_R;
    else dir = DIR_R_TO_L;

    if (subAnimCurStep == 0) {
        drawPacDots(dir);
        // start at shape 1
        animFlags |= PACMAN_FLAG_PACMAN_SHAPE;
    }

    int8_t pacmanPos;
    if (dir == DIR_L_TO_R) {
        pacmanPos = subAnimCurStep - 5;
    } else {
        pacmanPos = 32 - subAnimCurStep;
    }

    uint8_t shapeIndex = animFlags & PACMAN_FLAG_PACMAN_SHAPE;
    drawPacmanShape(pacmanPos, LAYER_FRONT, shapeIndex, dir);
    if (dir == DIR_L_TO_R) {
        setColOff(pacmanPos - 1, LAYER_FRONT);
        setVoxelOff(pacmanPos + 3, PACDOT_HEIGHT, LAYER_BACK);
    } else {
        setColOff(pacmanPos + pacmanShapeLen, LAYER_FRONT);
        setVoxelOff(pacmanPos + 1, PACDOT_HEIGHT, LAYER_BACK);
    }

    uint8_t numGhosts = (animFlags & PACMAN_FLAG_NUM_GHOSTS) >> PACMAN_FLAG_NUM_GHOSTS_LSHIFT;

    int8_t ghostPos;
    uint8_t ghostLayer = LAYER_FRONT;

    for (uint8_t i = 0; i < numGhosts; i++) {
        if (dir == DIR_L_TO_R) {
            ghostPos = pacmanPos - PACMAN_GHOST_GAP - pacmanGhostShapeLen - i * (pacmanGhostShapeLen + 2);
        } else {
            ghostPos = pacmanPos + PACMAN_GHOST_GAP + pacmanShapeLen + i * (pacmanGhostShapeLen + 2);
        }
        drawGhostShape(ghostPos, ghostLayer);
        if (dir == DIR_L_TO_R) {
            setColOff(ghostPos - 1, ghostLayer);
        } else {
            setColOff(ghostPos + pacmanGhostShapeLen, ghostLayer);
        }
        // alternate layer between ghosts
        ghostLayer ^= 1;
    }

    animFlags ^= PACMAN_FLAG_PACMAN_SHAPE;

    incrementSubAnimStep();
}

////////////////////////// TODO do these //////////////////////////////////////////////////////////////
void pacmanEatGhostInit() {

}

void pacmanEatGhostUpdate() {

}

// Draws pacdots in the given direction on the back layer.
void drawPacDots(uint8_t direction) {
    // int16_t frameDelay = PACDOT_FRAMEDELAY;
    if (direction == DIR_L_TO_R) {
        for (int8_t x = 2; x < 32; x += PACDOT_SPACING) {
            setVoxelOn(x, PACDOT_HEIGHT, LAYER_BACK);
            // delay(frameDelay);
        }
    } else if (direction == DIR_R_TO_L) {
        for (int8_t x = 29; x >= 0; x -= PACDOT_SPACING) {
            setVoxelOn(x, PACDOT_HEIGHT, LAYER_BACK);
            // delay(frameDelay);
        }
    }
}

// Draws pacdots in the given direction on the back layer, 
// with the specified dot being a power pellet.
void drawPacDots(uint8_t direction, uint8_t bigDotPos) {
    uint8_t x;
    // int16_t frameDelay = PACDOT_FRAMEDELAY;
    for (int8_t dotI = 0; dotI < 8; dotI++) {
        if (direction == DIR_L_TO_R) {
            x = 2 + (dotI * PACDOT_SPACING);
        } else if (direction == DIR_R_TO_L) {
            x = 29 - (dotI * PACDOT_SPACING);
        }
        // draw power pellet
        if (dotI == bigDotPos) {
            drawPacDotPowerPellet(x);
        // draw normal pacdot
        } else {
            setVoxelOn(x, PACDOT_HEIGHT, LAYER_BACK);
        }
        // delay(frameDelay);
    }
}

// Draws a power pellet at the given x pos.
void drawPacDotPowerPellet(uint8_t x) {
    setVoxelOn(x, PACDOT_HEIGHT - 1, LAYER_BACK);
    setVoxelOn(x, PACDOT_HEIGHT, LAYER_BACK);
    setVoxelOn(x, PACDOT_HEIGHT + 1, LAYER_BACK);
    setVoxelOn(x - 1, PACDOT_HEIGHT, LAYER_BACK);
    setVoxelOn(x + 1, PACDOT_HEIGHT, LAYER_BACK);
}

// Draws pacman at the given x pos. 
// The x pos corresponds to the left-most col of the shape.
void drawPacmanShape(int8_t x, uint8_t z, uint8_t shape, uint8_t direction) {
    // check x range
    if (x < 1 - pacmanShapeLen || x >= 32) {
        return;
    }

    for (uint8_t col = 0; col < pacmanShapeLen; col++) {
        if (direction == DIR_L_TO_R) {
            setColForShape(pacmanShapes[shape], pacmanShapeLen, col, x + col, z);
        } else {
            setColForShape(pacmanShapes[shape], pacmanShapeLen, (pacmanShapeLen - col - 1), x + col, z);
        }
    }
}

// Draws a ghost at the given x pos.
// The x pos corresponds to the left-most col of the shape.
void drawGhostShape(int8_t x, uint8_t z) {
    if (x < 1 - pacmanGhostShapeLen || x >= 32) {
        return;
    }

    for (uint8_t col = 0; col < pacmanGhostShapeLen; col++) {
        setColForShape(pacmanGhostShape, pacmanGhostShapeLen, col, x + col, z);
    }
}

// Sets a single col of a shape to a col in the matrix.
void setColForShape(uint8_t shape[6], uint8_t len, uint8_t col, uint8_t x, uint8_t z) {
    for (uint8_t i = 0; i < 6; i++) {
        setVoxel(x, i, z, (shape[5 - i] >> len - 1 - col) & 1);
    }
}

// GENERAL DRAWING FUNCTIONS

// Shifts the entire contents of the matrix one space to the left. 
// void shiftMatrixL() {
//     for (uint8_t i = 0; i < 2; i++) {
//         for (uint8_t j = 0; j < 6; j++) {
//             uint8_t overflow = (matrix[i][j][1] >> 15) & 1;

//             matrix[i][j][0] = (matrix[i][j][0] << 1) | overflow;
//             matrix[i][j][1] = (matrix[i][j][1] << 1);
//         }
//     }
// }

// X range validation
bool isInRangeX(uint8_t x) {
    return (x >= 0 && x < 32);
}

// Y range validation
bool isInRangeY(uint8_t y) {
    return (y >= 0 && y < 6);
}

// Z range validation
bool isInRangeZ(uint8_t z) {
    return (z >= 0 && z < 2);
}

// Validates range of (x, y, z) coordinate
bool isInRange(uint8_t x, uint8_t y, uint8_t z) {
    return (isInRangeX(x) && isInRangeY(y) && isInRangeZ(z));
}

// Sets the voxel at the given position on
void setVoxelOn(uint8_t x, uint8_t y, uint8_t z) {
    if (isInRange(x, y, z)) {
        if (x > 15) {
            matrix[z][y][1] |= (1 << (31 - x));
        } else {
            matrix[z][y][0] |= (1 << (15 - x));
        }
    }
}

// Sets the voxel at the given position off
void setVoxelOff(uint8_t x, uint8_t y, uint8_t z) {
    if (isInRange(x, y, z)) {
        if (x > 15) {
            matrix[z][y][1] &= ~(1 << (31 - x));
        } else {
            matrix[z][y][0] &= ~(1 << (15 - x));
        }
    }
}

// Sets the voxel at the given position to the given state.
void setVoxel(uint8_t x, uint8_t y, uint8_t z, uint8_t state) {
    if (state) {
        setVoxelOn(x, y, z);
    } else {
        setVoxelOff(x, y, z);
    }
}

// Flips the state of a voxel
void flipVoxel(uint8_t x, uint8_t y, uint8_t z) {
    if (isInRange(x, y, z)) {
        if (x > 15) {
            matrix[z][y][1] ^= (1 << (31 - x));
        } else {
            matrix[z][y][0] ^= (1 << (15 - x));
        }
    }
}

// Sets the given row on
void setRowOn(uint8_t y, uint8_t z) {
    if (isInRangeY(y) && isInRangeY(y)) {
        matrix[z][y][0] = 0xffff;
        matrix[z][y][1] = 0xffff;
    }
}

// Sets the given row off
void setRowOff(uint8_t y, uint8_t z) {
    if (isInRangeY(y) && isInRangeY(y)) {
        matrix[z][y][0] = 0x0000;
        matrix[z][y][1] = 0x0000;
    }
}

// Sets the given row to the given state
void setRow(uint8_t y, uint8_t z, uint8_t state) {
    if (state) {
        setRowOn(y, z);
    } else {
        setRowOff(y, z);
    }
}

// Sets the given row on both layers to the given state
void setRowBothLayers(uint8_t y, uint8_t state) {
    if (state) {
        setRowOn(y, LAYER_FRONT);
        setRowOn(y, LAYER_BACK);
    } else {
        setRowOff(y, LAYER_FRONT);
        setRowOff(y, LAYER_BACK);
    }
}

// Sets the given col on
void setColOn(uint8_t x, uint8_t z) {
    if (isInRangeX(x) && isInRangeZ(z)) {
        
        uint8_t rowHalf = 0;
        if (x > 15) {
            rowHalf = 1;
            x -= 16;
        }

        for (uint8_t i = 0; i < 6; i++) {
            matrix[z][i][rowHalf] |= (1 << (15 - x));
        }
    }
}

// Sets the given col off
void setColOff(uint8_t x, uint8_t z) {
    if (isInRangeX(x) && isInRangeZ(z)) {
        
        uint8_t rowHalf = 0;
        if (x > 15) {
            rowHalf = 1;
            x -= 16;
        }

        for (uint8_t i = 0; i < 6; i++) {
            matrix[z][i][rowHalf] &= ~(1 << (15 - x));
        }
    }
}

// Sets the given col to the given state
void setCol(uint8_t x, uint8_t z, uint8_t state) {
    if (state) {
        setColOn(x, z);
    } else {
        setColOff(x, z);
    }
}

// Sets the given col on both layers to the given state
void setColBothLayers(uint8_t x, uint8_t state) {
    if (state) {
        setColOn(x, LAYER_FRONT);
        setColOn(x, LAYER_BACK);
    } else {
        setColOff(x, LAYER_FRONT);
        setColOff(x, LAYER_BACK);
    }
}

// Fills the matrix with the given 8-bit pattern
void fillMatrix(uint8_t pattern) {
    for (uint8_t z = 0; z < 2; z++) {
        for (uint8_t y = 0; y < 6; y++) {
            matrix[z][y][0] = (pattern << 8) | pattern;
            matrix[z][y][1] = (pattern << 8) | pattern;
        }
    }
}

// Fills the given layer with the given 8-bit pattern
void fillLayer(uint8_t z, uint8_t pattern) {
    for (uint8_t y = 0; y < 6; y++) {
        matrix[z][y][0] = (pattern << 8) | pattern;
        matrix[z][y][1] = (pattern << 8) | pattern;
    }
}

// Fills the given row with the given 8-bit pattern
void fillRow(uint8_t y, uint8_t z, uint8_t pattern) {
    matrix[z][y][0] = (pattern << 8) | pattern;
    matrix[z][y][1] = (pattern << 8) | pattern;
}

// Makes sure x1 < x2
void setBytesAscOrder(uint8_t x1, uint8_t x2, uint8_t *p1, uint8_t *p2) {
    if (x1 > x2) {
        uint8_t temp = x1;
        x1 = x2;
        x2 = temp;
    }
    *p1 = x1;
    *p2 = x2;
}

// Draws the outline of a box, between (x1, y1) and (x2, y2);
void boxOutline(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t z) {

    setBytesAscOrder(x1, x2, &x1, &x2);
    setBytesAscOrder(y1, y2, &y1, &y2);

    matrix[z][y1][0] = line0(x1, x2);
    matrix[z][y1][1] = line1(x1, x2);

    matrix[z][y2][0] = line0(x1, x2);
    matrix[z][y2][1] = line1(x1, x2);

    for (uint8_t i = y1; i < y2; i++) {
        setVoxelOn(x1, i, z);
        setVoxelOn(x2, i, z);
    }
}

// Returns the 16-bit pattern for a line between start and end
uint16_t line(uint8_t start, uint8_t end) {
    return ((0xffff >> start) & ~(0xffff >> (end + 1)));
}

// Returns the first half of the 32-bit pattern for a line between start and end
uint16_t line0(uint8_t start, uint8_t end) {
    if (start > 15) {
        return 0;
    }
    if (end > 15) {
        end = 15;
    }
    return line(start, end);
}

// Returns the second half of the 32-bit pattern for a line between start and end
uint16_t line1(uint8_t start, uint8_t end) {
    if (end < 16) {
        return 0;
    }
    if (start < 16) {
        start = 0;
    }
    end -= 16;
    return line(start, end);
}













// lowercase letters are not defined - use only uppercase
const uint8_t ascii[][6] = {
    {// SPACE
        0b000,
        0b000,
        0b000,
        0b000,
        0b000,
        0b000
    },
    {// !
        0b1,
        0b1,
        0b1,
        0b1,
        0b0,
        0b1
    },
    {// "
        0b11,
        0b11,
        0b00,
        0b00,
        0b00,
        0b00
    },
    {// #
        0b00000,
        0b01010,
        0b11111,
        0b01010,
        0b11111,
        0b01010
    },
    {// $
        0b00100,
        0b01111,
        0b10100,
        0b01110,
        0b00101,
        0b11110
    },
    {// %
        0b010001,
        0b101010,
        0b010100,
        0b001010,
        0b010101,
        0b100010
    },
    {// &
        0b01110,
        0b10000,
        0b10001,
        0b01111,
        0b10001,
        0b01110
    },
    {// '
        0b1,
        0b1,
        0b0,
        0b0,
        0b0,
        0b0
    },
    {// (
        0b01,
        0b10,
        0b10,
        0b10,
        0b10,
        0b01
    },
    {// )
        0b10,
        0b01,
        0b01,
        0b01,
        0b01,
        0b10
    },
    {// *
        0b1,
        0b0,
        0b0,
        0b0,
        0b0,
        0b0
    },
    {// +
        0b000,
        0b000,
        0b010,
        0b111,
        0b010,
        0b000
    },
    {// ,
        0b0,
        0b0,
        0b0,
        0b0,
        0b1,
        0b1
    },
    {// -
        0b000,
        0b000,
        0b000,
        0b111,
        0b000,
        0b000
    },
    {// .
        0b0,
        0b0,
        0b0,
        0b0,
        0b0,
        0b1
    },
    {// /
        0b001,
        0b001,
        0b010,
        0b010,
        0b100,
        0b100
    },
    {// 0
        0b0110,
        0b1001,
        0b1001,
        0b1001,
        0b1001,
        0b0110
    },
    {// 1
        0b010,
        0b110,
        0b010,
        0b010,
        0b010,
        0b111
    },
    {// 2
        0b0110,
        0b1001,
        0b0001,
        0b0110,
        0b1000,
        0b1111
    },
    {// 3
        0b1110,
        0b0001,
        0b0110,
        0b0001,
        0b0001,
        0b1110
    },
    {// 4
        0b00010,
        0b00110,
        0b01010,
        0b10010,
        0b11111,
        0b00010
    },
    {// 5
        0b1111,
        0b1000,
        0b1110,
        0b0001,
        0b1001,
        0b0110
    },
    {// 6
        0b0110,
        0b1000,
        0b1000,
        0b1110,
        0b1001,
        0b0110
    },
    {// 7
        0b1111,
        0b0001,
        0b0010,
        0b0100,
        0b0100,
        0b0100
    },
    {// 8
        0b0110,
        0b1001,
        0b0110,
        0b1001,
        0b1001,
        0b0110
    },
    {// 9
        0b0110,
        0b1001,
        0b0111,
        0b0001,
        0b0001,
        0b0110
    },
    {// :
        0b0,
        0b0,
        0b1,
        0b0,
        0b1,
        0b0
    },
    {// ;
        0b0,
        0b0,
        0b1,
        0b0,
        0b1,
        0b1
    },
    {// <
        0b00,
        0b00,
        0b01,
        0b10,
        0b01,
        0b00
    },
    {// =
        0b000,
        0b000,
        0b111,
        0b000,
        0b111,
        0b000
    },
    {// >
        0b00,
        0b00,
        0b10,
        0b01,
        0b10,
        0b00
    },
    {// ?
        0b0110,
        0b1001,
        0b0001,
        0b0110,
        0b0000,
        0b0100
    },
    {// @
        0b011110,
        0b100001,
        0b101101,
        0b101111,
        0b100000,
        0b011111
    },
    {// A
        0b0110,
        0b1001,
        0b1001,
        0b1111,
        0b1001,
        0b1001
    },
    {// B
        0b1110,
        0b1001,
        0b1110,
        0b1001,
        0b1001,
        0b1110
    },
    {// C
        0b0111,
        0b1000,
        0b1000,
        0b1000,
        0b1000,
        0b0111
    },
    {// D
        0b1110,
        0b1001,
        0b1001,
        0b1001,
        0b1001,
        0b1110
    },
    {// E
        0b1111,
        0b1000,
        0b1000,
        0b1111,
        0b1000,
        0b1111
    },
    {// F
        0b1111,
        0b1000,
        0b1111,
        0b1000,
        0b1000,
        0b1000
    },
    {// G
        0b0111,
        0b1000,
        0b1000,
        0b1011,
        0b1001,
        0b0111
    },
    {// H
        0b1001,
        0b1001,
        0b1001,
        0b1111,
        0b1001,
        0b1001
    },
    {// I
        0b111,
        0b010,
        0b010,
        0b010,
        0b010,
        0b111
    },
    {// J
        0b111,
        0b001,
        0b001,
        0b001,
        0b101,
        0b010
    },
    {// K
        0b1001,
        0b1001,
        0b1010,
        0b1100,
        0b1010,
        0b1001
    },
    {// L
        0b1000,
        0b1000,
        0b1000,
        0b1000,
        0b1000,
        0b1111
    },
    {// M
        0b10001,
        0b11011,
        0b10101,
        0b10001,
        0b10001,
        0b10001
    },
    {// N
        0b1001,
        0b1001,
        0b1101,
        0b1011,
        0b1001,
        0b1001
    },
    {// O
        0b0110,
        0b1001,
        0b1001,
        0b1001,
        0b1001,
        0b0110
    },
    {// P
        0b1110,
        0b1001,
        0b1001,
        0b1110,
        0b1000,
        0b1000
    },
    {// Q
        0b0110,
        0b1001,
        0b1001,
        0b1001,
        0b0110,
        0b0001
    },
    {// R
        0b1110,
        0b1001,
        0b1001,
        0b1110,
        0b1010,
        0b1001
    },
    {// S
        0b0111,
        0b1000,
        0b1000,
        0b0110,
        0b0001,
        0b1110
    },
    {// T
        0b11111,
        0b00100,
        0b00100,
        0b00100,
        0b00100,
        0b00100
    },
    {// U
        0b1001,
        0b1001,
        0b1001,
        0b1001,
        0b1001,
        0b0110
    },
    {// V
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b01010,
        0b00100
    },
    {// W
        0b10001,
        0b10001,
        0b10001,
        0b10101,
        0b10101,
        0b01010
    },
    {// X
        0b10001,
        0b01010,
        0b00100,
        0b00100,
        0b01010,
        0b10001
    },
    {// Y
        0b10001,
        0b10001,
        0b01010,
        0b00100,
        0b00100,
        0b00100
    },
    {// Z
        0b1111,
        0b0001,
        0b0010,
        0b0100,
        0b1000,
        0b1111
    }
};

// Length of each character symbol
const uint8_t asciiLen[] = {
    3, // SPACE
    1, // !
    2, // "
    5, // #
    5, // $
    6, // %
    5, // &
    1, // '
    2, // (
    2, // )
    1, // *
    3, // +
    1, // ,
    3, // -
    1, // .
    3, // /
    4, // 0
    3, // 1
    4, // 2
    4, // 3
    5, // 4
    4, // 5
    4, // 6
    4, // 7
    4, // 8
    4, // 9
    1, // :
    1, // ;
    2, // <
    3, // =
    2, // >
    3, // ?
    6, // @
    4, // A
    4, // B
    4, // C
    4, // D
    4, // E
    4, // F
    4, // G
    4, // H
    3, // I
    3, // J
    4, // K
    4, // L
    5, // M
    4, // N
    4, // O
    4, // P
    4, // Q
    4, // R
    4, // S
    5, // T
    4, // U
    5, // V
    5, // W
    5, // X
    5, // Y
    4 // Z
};

// Shapes for pacman
const uint8_t pacmanShapes[3][6] = {
    {
        0b00000,
        0b01110,
        0b10001,
        0b10010,
        0b10001,
        0b01110
    },
    {
        0b00000,
        0b01110,
        0b10001,
        0b10011,
        0b10001,
        0b01110
    },
    {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000
    }
};

const uint8_t pacmanShapeLen = 5;

// Shape for pacman ghost
const uint8_t pacmanGhostShape[6] = {
    0b01110,
    0b10001,
    0b11011,
    0b10001,
    0b11011,
    0b10101
};

const uint8_t pacmanGhostShapeLen = 5;





/*

void randomSnakeXYZ(uint8_t snakeLen) {
    if (snakeLen == 0) return;

    uint16_t frameDelay = 20;
    uint8_t x[snakeLen];
    uint8_t y[snakeLen];
    uint8_t z[snakeLen];
    uint8_t dir = random(0, 6);

    uint8_t randX = random(0, 32);
    uint8_t randY = random(0, 6);
    uint8_t randZ = random(0, 2);

    for (uint8_t i = 0; i < snakeLen; i++) {
        x[i] = randX;
        y[i] = randY;
        z[i] = randZ;
    }

    while (true) {
        fillMatrix(0);
        uint8_t tailX = x[snakeLen - 1];
        uint8_t tailY = y[snakeLen - 1];
        uint8_t tailZ = z[snakeLen - 1];
        for (uint8_t i = snakeLen - 1; i > 0; i--) {
            x[i] = x[i - 1];
            y[i] = y[i - 1];
            z[i] = z[i - 1];
        }
        if (dir == DIR_L_TO_R) {
            x[0]++;
        } else if (dir == DIR_R_TO_L) {
            x[0]--;
        } else if (dir == DIR_B_TO_T) {
            y[0]++;
        } else if (dir == DIR_T_TO_B) {
            y[0]--;
        } else if (dir == DIR_F_TO_B) {
            z[0]++;
        } else if (dir == DIR_B_TO_F) {
            z[0]--;
        }

        for (uint8_t i = 0; i < snakeLen; i++) {
            setVoxelOn(x[i], y[i], z[i]);
        }

        uint8_t newDir = dir;
        uint8_t randNo = random(0, 10);
        if (randNo < 3 || isNewDirectionOutOfMatrix(newDir, x[0], y[0], z[0])) {
            bool validDir = false;
            while (!validDir) {
                newDir = random(0, 6);
                validDir = true;
                if (isNewDirectionReversed(dir, newDir)) validDir = false;
                else if (isNewDirectionOutOfMatrix(newDir, x[0], y[0], z[0])) validDir = false;
            }
        }

        dir = newDir;

        if (modeChanged) return;
        delay(frameDelay);
    }
}

void randomSnakeXYOneLayer(uint8_t z, uint8_t snakeLen) {
    if (snakeLen == 0) return;

    uint16_t frameDelay = 20;
    uint8_t x[snakeLen];
    uint8_t y[snakeLen];
    uint8_t dir = random(0, 4);

    for (uint8_t i = 0; i < snakeLen; i++) {
        x[i] = random(0, 32);
        y[i] = random(0, 6);
    }

    while (true) {
        fillMatrix(0);
        uint8_t tailX = x[snakeLen - 1];
        uint8_t tailY = y[snakeLen - 1];
        for (uint8_t i = snakeLen - 1; i > 0; i--) {
            x[i] = x[i - 1];
            y[i] = y[i - 1];
        }
        if (dir == DIR_L_TO_R) {
            x[0]++;
        } else if (dir == DIR_R_TO_L) {
            x[0]--;
        } else if (dir == DIR_B_TO_T) {
            y[0]++;
        } else if (dir == DIR_T_TO_B) {
            y[0]--;
        }

        for (uint8_t i = 0; i < snakeLen; i++) {
            setVoxelOn(x[i], y[i], z);
        }

        uint8_t newDir = dir;
        uint8_t randNo = random(0, 10);
        if (randNo < 3 || isNewDirectionOutOfMatrix(newDir, x[0], y[0])) {
            bool validDir = false;
            while (!validDir) { 
                newDir = random(0, 4);
                validDir = true;
                if (isNewDirectionReversed(dir, newDir)) validDir = false;
                else if (isNewDirectionOutOfMatrix(newDir, x[0], y[0])) validDir = false;
            }
        }

        dir = newDir;

        if (modeChanged) return;
        delay(frameDelay);
    }
}

void randomSnakeXYBothLayers(uint8_t snakeLen) {
    if (snakeLen == 0) return;

    uint16_t frameDelay = 20;
    uint8_t x[2][snakeLen];
    uint8_t y[2][snakeLen];
    uint8_t dir[2];

    for (uint8_t z = 0; z < 2; z++) {
        for (uint8_t i = 0; i < snakeLen; i++) {
            x[z][i] = random(0, 32);
            y[z][i] = random(0, 6);
        }
        dir[z] = random(0, 4);
    }

    
    while (true) {
        fillMatrix(0);

        for (uint8_t z = 0; z < 2; z++) {
            uint8_t tailX = x[z][snakeLen - 1];
            uint8_t tailY = y[z][snakeLen - 1];
            for (uint8_t i = snakeLen - 1; i > 0; i--) {
                x[z][i] = x[z][i - 1];
                y[z][i] = y[z][i - 1];
            }
            if (dir[z] == DIR_L_TO_R) {
                x[z][0]++;
            } else if (dir[z] == DIR_R_TO_L) {
                x[z][0]--;
            } else if (dir[z] == DIR_B_TO_T) {
                y[z][0]++;
            } else if (dir[z] == DIR_T_TO_B) {
                y[z][0]--;
            }

            for (uint8_t i = 0; i < snakeLen; i++) {
                setVoxelOn(x[z][i], y[z][i], z);
            }

            uint8_t newDir = dir[z];
            uint8_t randNo = random(0, 10);
            if (randNo < 3 || isNewDirectionOutOfMatrix(newDir, x[z][0], y[z][0])) {
                bool validDir = false;
                while (!validDir) { 
                    newDir = random(0, 4);
                    validDir = true;
                    if (isNewDirectionReversed(dir[z], newDir)) validDir = false;
                    else if (isNewDirectionOutOfMatrix(newDir, x[z][0], y[z][0])) validDir = false;
                }
            }

            dir[z] = newDir;
        }

        if (modeChanged) return;
        delay(frameDelay);
    }
}

bool isNewDirectionOutOfMatrix(uint8_t dir, uint8_t x, uint8_t y) {
    return (dir == DIR_B_TO_T && y == 5) 
            || (dir == DIR_T_TO_B && y == 0)
            || (dir == DIR_L_TO_R && x == 31)
            || (dir == DIR_R_TO_L && x == 0);
}

bool isNewDirectionOutOfMatrix(uint8_t dir, uint8_t x, uint8_t y, uint8_t z) {
    return (dir == DIR_B_TO_T && y == 5) 
            || (dir == DIR_T_TO_B && y == 0)
            || (dir == DIR_L_TO_R && x == 31)
            || (dir == DIR_R_TO_L && x == 0)
            || (dir == DIR_F_TO_B && z == 1)
            || (dir == DIR_B_TO_F && z == 0);
}

bool isNewDirectionReversed(uint8_t dir, uint8_t newDir) {
    return (dir == DIR_L_TO_R && newDir == DIR_R_TO_L)
            || (dir == DIR_R_TO_L && newDir == DIR_L_TO_R)
            || (dir == DIR_B_TO_T && newDir == DIR_T_TO_B)
            || (dir == DIR_T_TO_B && newDir == DIR_B_TO_T)
            || (dir == DIR_F_TO_B && newDir == DIR_B_TO_F)
            || (dir == DIR_B_TO_F && newDir == DIR_F_TO_B);
}

// ============================
//  PacMan animation
// ============================

// Displays the PacMan animation.
void pacman() {
    uint16_t frameDelay = 300;

    fillLayer(LAYER_FRONT, 0);

    pacmanScrollLtoR(frameDelay);
    if (modeChanged) return;
    delay(frameDelay * 4);

    pacmanScrollRtoL(frameDelay);
    if (modeChanged) return;
    delay(frameDelay * 4);

    pacmanScrollLtoRwGhosts(frameDelay, 2, 12);
    if (modeChanged) return;
    delay(frameDelay * 4);

    pacmanScrollRtoLwGhosts(frameDelay, 2, 12);
    if (modeChanged) return;
    delay(frameDelay * 4);

    pacmanEatGhost(frameDelay);
    if (modeChanged) return;
    delay(frameDelay * 4);

    pacmanScrollRtoL(frameDelay);
    if (modeChanged) return;
    delay(frameDelay * 4);
}

// PacMan goes from left to right, eating pacdots along the way.
void pacmanScrollLtoR(uint16_t frameDelay) {
    drawPacDots(DIR_L_TO_R);

    uint8_t shapeI = 1;
    for (int8_t x = -5; x < 33; x++) {
        drawPacmanShape(x, LAYER_FRONT, shapeI, DIR_L_TO_R);
        setColOff(x - 1, LAYER_FRONT);
        setVoxelOff(x + 3, PACDOT_HEIGHT, LAYER_BACK);

        if (modeChanged) return;
        delay(frameDelay);
        shapeI++;
        if (shapeI == 2) {
            shapeI = 0;
        }
    }
}

// PacMan goes from right to left, eating pacdots along the way.
void pacmanScrollRtoL(uint16_t frameDelay) {
    drawPacDots(DIR_R_TO_L);

    uint8_t shapeI = 1;
    for (int8_t x = 32; x >= -5; x--) {
        drawPacmanShape(x, LAYER_FRONT, shapeI, DIR_R_TO_L);
        setColOff(x + pacmanShapeLen, LAYER_FRONT);
        setVoxelOff(x + 1, PACDOT_HEIGHT, LAYER_BACK);

        if (modeChanged) return;
        delay(frameDelay);
        shapeI++;
        if (shapeI == 2) shapeI = 0;
    }
}

// PacMan goes from left to right, eating pacdots along the way, being chased by ghosts.
void pacmanScrollLtoRwGhosts(uint16_t frameDelay, uint8_t numGhosts, uint8_t gap) {
    drawPacDots(DIR_L_TO_R);

    // max 4 ghosts
    if (numGhosts > 4) numGhosts = 4;

    uint8_t pacShapeI = 1;
    for (int8_t x = -5; x < 31 + gap + numGhosts * (pacmanGhostShapeLen + 2); x++) {
        // draw pacman
        drawPacmanShape(x, LAYER_FRONT, pacShapeI, DIR_L_TO_R);
        setColOff(x - 1, LAYER_FRONT);
        setVoxelOff(x + 3, PACDOT_HEIGHT, LAYER_BACK);

        // draw ghosts
        int8_t ghostPos;
        uint8_t ghostLayer;
        for (uint8_t i = 0; i < numGhosts; i++) {
            ghostPos = x - gap - pacmanGhostShapeLen - i * (pacmanGhostShapeLen + 2);
            ghostLayer = i % 2 == 0 ? LAYER_FRONT : LAYER_BACK;
            drawGhostShape(ghostPos, ghostLayer);
            setColOff(ghostPos - 1, ghostLayer);
        }

        if (modeChanged) return;
        delay(frameDelay);
        pacShapeI++;
        if (pacShapeI == 2) pacShapeI = 0;
    }
}

// PacMan goes from right to left, eating pacdots along the way, being chased by ghosts.
void pacmanScrollRtoLwGhosts(uint16_t frameDelay, uint8_t numGhosts, uint8_t gap) {
    drawPacDots(DIR_R_TO_L);

    // max 4 ghosts
    if (numGhosts > 4) numGhosts = 4;

    uint8_t pacShapeI = 1;
    for (int8_t x = 32; x >= -3 - gap - numGhosts * (pacmanGhostShapeLen + 2); x--) {
        // draw pacman
        drawPacmanShape(x, LAYER_FRONT, pacShapeI, DIR_R_TO_L);
        setColOff(x + pacmanShapeLen, LAYER_FRONT);
        setVoxelOff(x + 1, PACDOT_HEIGHT, LAYER_BACK);

        // draw ghosts
        int8_t ghostPos;
        uint8_t ghostLayer;
        for (uint8_t i = 0; i < numGhosts; i++) {
            ghostPos = x + gap + pacmanShapeLen + i * (pacmanGhostShapeLen + 2);
            // alternate ghosts on front and back layer
            ghostLayer = i % 2 == 0 ? LAYER_FRONT : LAYER_BACK;
            drawGhostShape(ghostPos, ghostLayer);
            setColOff(ghostPos + pacmanGhostShapeLen, ghostLayer);
        }

        if (modeChanged) return;
        delay(frameDelay);
        pacShapeI++;
        if (pacShapeI == 2) pacShapeI = 0;
    }
}

// PacMan goes from left to right, eats a power pellet, then eats the ghost chasing him.
void pacmanEatGhost(uint16_t frameDelay) {
    drawPacDots(DIR_L_TO_R, 6);

    int8_t pacX = -6;
    int8_t ghostX = -21;

    uint8_t pacShapeI = 1;
    while (pacX < 23) {
        // draw pacman
        pacX++;
        drawPacmanShape(pacX, LAYER_FRONT, pacShapeI, DIR_L_TO_R);
        setColOff(pacX - 1, LAYER_FRONT);
        if (pacX < 22) {
            setVoxelOff(pacX + 3, PACDOT_HEIGHT, LAYER_BACK);
        }
        if (pacX == 23) {
            setColOff(pacX + 2, LAYER_BACK);
            setColOff(pacX + 3, LAYER_BACK);
            setColOff(pacX + 4, LAYER_BACK);
        }

        // draw ghost
        ghostX++;
        drawGhostShape(ghostX, LAYER_FRONT);
        setColOff(ghostX - 1, LAYER_FRONT);

        if (modeChanged) return;
        delay(frameDelay);
        pacShapeI++;
        if (pacShapeI == 2) pacShapeI = 0;
    }

    // move ghost while pacman stopped
    for (uint8_t i = 0; i < 2; i++) {
        ghostX++;
        drawGhostShape(ghostX, LAYER_FRONT);
        setColOff(ghostX - 1, LAYER_FRONT);

        if (modeChanged) return;
        delay(frameDelay);
    }

    // pacman flash from power pellet, ghost keeps moving
    for (uint8_t i = 0; i < 3; i++) {
        drawPacmanShape(pacX, LAYER_FRONT, 2, DIR_L_TO_R);
        ghostX++;
        drawGhostShape(ghostX, LAYER_FRONT);
        setColOff(ghostX - 1, LAYER_FRONT);

        if (modeChanged) return;
        delay(frameDelay / 2);

        drawPacmanShape(pacX, LAYER_FRONT, 0, DIR_L_TO_R);
        if (modeChanged) return;
        delay(frameDelay / 2);
    }

    if (modeChanged) return;
    delay(frameDelay * 2);

    // pacman chases ghost
    pacShapeI = 0;
    for (uint8_t i = 0; i < 5; i++) {
        ghostX--;
        drawGhostShape(ghostX, LAYER_FRONT);
        setColOff(ghostX + pacmanGhostShapeLen, LAYER_FRONT);

        pacX--;
        drawPacmanShape(pacX, LAYER_FRONT, pacShapeI, DIR_R_TO_L);
        setColOff(pacX + pacmanShapeLen, LAYER_FRONT);
        pacShapeI++;
        if (pacShapeI == 2) pacShapeI = 0;

        if (modeChanged) return;
        delay(frameDelay / 2);

        pacX--;
        drawPacmanShape(pacX, LAYER_FRONT, pacShapeI, DIR_R_TO_L);
        setColOff(pacX + pacmanShapeLen, LAYER_FRONT);
        pacShapeI++;
        if (pacShapeI == 2) pacShapeI = 0;

        if (modeChanged) return;
        delay(frameDelay / 2);
    }

    ghostX--;
    drawGhostShape(ghostX, LAYER_FRONT);
    setColOff(ghostX + pacmanGhostShapeLen, LAYER_FRONT);

    pacX--;
    pacShapeI = 0;
    drawPacmanShape(pacX, LAYER_FRONT, pacShapeI, DIR_R_TO_L);
    setColOff(pacX + pacmanShapeLen, LAYER_FRONT);

    if (modeChanged) return;
    delay(frameDelay);

    // ghost dies and disappears
    for (uint8_t i = 0; i < 10; i++) {
        if (i < 6) {
            matrix[LAYER_FRONT][5 - i][0] |= line(ghostX, ghostX + pacmanGhostShapeLen - 1);
        }
        if (i >= 4) {
            matrix[LAYER_FRONT][9 - i][0] &= ~line(ghostX, ghostX + pacmanGhostShapeLen - 1);
        }
        if (modeChanged) return;
        delay(100);
    }

    // pacman moves off matrix to right
    pacShapeI = 1;
    while (pacX < 32) {
        pacX++;
        drawPacmanShape(pacX, LAYER_FRONT, pacShapeI, DIR_L_TO_R);
        setColOff(pacX - 1, LAYER_FRONT);
        setVoxelOff(pacX + 3, PACDOT_HEIGHT, LAYER_BACK);

        if (modeChanged) return;
        delay(frameDelay);
        pacShapeI++;
        if (pacShapeI == 2) pacShapeI = 0;
    }
}

// Draws pacdots in the given direction on the back layer.
void drawPacDots(uint8_t direction) {
    int16_t frameDelay = 100;
    if (direction == DIR_L_TO_R) {
        for (int8_t x = 2; x < 32; x += PACDOT_SPACING) {
            setVoxelOn(x, PACDOT_HEIGHT, LAYER_BACK);
            if (modeChanged) return;
            delay(frameDelay);
        }
    } else if (direction == DIR_R_TO_L) {
        for (int8_t x = 29; x >= 0; x -= PACDOT_SPACING) {
            setVoxelOn(x, PACDOT_HEIGHT, LAYER_BACK);
            if (modeChanged) return;
            delay(frameDelay);
        }
    }
}

// Draws pacdots in the given direction on the back layer, 
// with the specified dot being a power pellet.
void drawPacDots(uint8_t direction, uint8_t bigDotPos) {
    uint8_t x;
    int16_t frameDelay = 100;
    for (int8_t dotI = 0; dotI < 8; dotI++) {
        if (direction == DIR_L_TO_R) {
            x = 2 + (dotI * PACDOT_SPACING);
        } else if (direction == DIR_R_TO_L) {
            x = 29 - (dotI * PACDOT_SPACING);
        }
        // draw power pellet
        if (dotI == bigDotPos) {
            drawPacDotPowerPellet(x);
        // draw normal pacdot
        } else {
            setVoxelOn(x, PACDOT_HEIGHT, LAYER_BACK);
        }
        if (modeChanged) return;
        delay(frameDelay);
    }
}

// Draws a power pellet at the given x pos.
void drawPacDotPowerPellet(uint8_t x) {
    setVoxelOn(x, PACDOT_HEIGHT - 1, LAYER_BACK);
    setVoxelOn(x, PACDOT_HEIGHT, LAYER_BACK);
    setVoxelOn(x, PACDOT_HEIGHT + 1, LAYER_BACK);
    setVoxelOn(x - 1, PACDOT_HEIGHT, LAYER_BACK);
    setVoxelOn(x + 1, PACDOT_HEIGHT, LAYER_BACK);
}

// Draws pacman at the given x pos. 
// The x pos corresponds to the left-most col of the shape.
void drawPacmanShape(int8_t x, uint8_t z, uint8_t shape, uint8_t direction) {
    // check x range
    if (x < 1 - pacmanShapeLen || x >= 32) {
        return;
    }

    for (uint8_t col = 0; col < pacmanShapeLen; col++) {
        if (direction == DIR_L_TO_R) {
            setColForShape(pacmanShapes[shape], pacmanShapeLen, col, x + col, z);
        } else {
            setColForShape(pacmanShapes[shape], pacmanShapeLen, (pacmanShapeLen - col - 1), x + col, z);
        }
    }
}

// Draws a ghost at the given x pos.
// The x pos corresponds to the left-most col of the shape.
void drawGhostShape(int8_t x, uint8_t z) {
    if (x < 1 - pacmanGhostShapeLen || x >= 32) {
        return;
    }

    for (uint8_t col = 0; col < pacmanGhostShapeLen; col++) {
        setColForShape(pacmanGhostShape, pacmanGhostShapeLen, col, x + col, z);
    }
}

// Sets a single col of a shape to a col in the matrix.
void setColForShape(uint8_t shape[6], uint8_t len, uint8_t col, uint8_t x, uint8_t z) {
    for (uint8_t i = 0; i < 6; i++) {
        setVoxel(x, i, z, (shape[5 - i] >> len - 1 - col) & 1);
    }
}
*/