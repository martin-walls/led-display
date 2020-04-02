#include "avr/interrupt.h"
#include <EEPROM.h>

#define LAYER_FRONT 0
#define LAYER_BACK 1

#define BTN_ON 0
#define BTN_OFF 1

#define DIR_L_TO_R 0
#define DIR_R_TO_L 1
#define DIR_T_TO_B 3
#define DIR_B_TO_T 4

// each 32 bit row is split into two 16-bit ints. 
//   [  [ front layer bottom to top ],  [ back layer bottom to top ]]
//   [  [0000000000000000, (uint16_t), (uint16_t), ...x6],  [(uint16_t), (uint16_t), ...x6]  ]
volatile uint16_t matrix[2][6][2];
volatile int currentRow = 0;

unsigned long lastBtnIntMillis = 0;
volatile uint8_t mode = 0;
volatile bool modeChanged = false;
volatile uint8_t PINB_lastState = 0;

int eeModeAddr = 0;

// lowercase letters are not defined - use only uppercase
uint8_t ascii[59][6] = {
    {// SPACE
        B000,
        B000,
        B000,
        B000,
        B000,
        B000
    },
    {// !
        B1,
        B1,
        B1,
        B1,
        B0,
        B1
    },
    {// "
        B11,
        B11,
        B00,
        B00,
        B00,
        B00
    },
    {// #
        B00000,
        B01010,
        B11111,
        B01010,
        B11111,
        B01010
    },
    {// $
        B00100,
        B01111,
        B10100,
        B01110,
        B00101,
        B11110
    },
    {// %
        B010001,
        B101010,
        B010100,
        B001010,
        B010101,
        B100010
    },
    {// &
        B01110,
        B10000,
        B10001,
        B01111,
        B10001,
        B01110
    },
    {// '
        B1,
        B1,
        B0,
        B0,
        B0,
        B0
    },
    {// (
        B01,
        B10,
        B10,
        B10,
        B10,
        B01
    },
    {// )
        B10,
        B01,
        B01,
        B01,
        B01,
        B10
    },
    {// *
        B1,
        B0,
        B0,
        B0,
        B0,
        B0
    },
    {// +
        B000,
        B000,
        B010,
        B111,
        B010,
        B000
    },
    {// ,
        B0,
        B0,
        B0,
        B0,
        B1,
        B1
    },
    {// -
        B000,
        B000,
        B000,
        B111,
        B000,
        B000
    },
    {// .
        B0,
        B0,
        B0,
        B0,
        B0,
        B1
    },
    {// /
        B001,
        B001,
        B010,
        B010,
        B100,
        B100
    },
    {// 0
        B0110,
        B1001,
        B1001,
        B1001,
        B1001,
        B0110
    },
    {// 1
        B010,
        B110,
        B010,
        B010,
        B010,
        B111
    },
    {// 2
        B0110,
        B1001,
        B0001,
        B0110,
        B1000,
        B1111
    },
    {// 3
        B1110,
        B0001,
        B0110,
        B0001,
        B0001,
        B1110
    },
    {// 4
        B00010,
        B00110,
        B01010,
        B10010,
        B11111,
        B00010
    },
    {// 5
        B1111,
        B1000,
        B1110,
        B0001,
        B1001,
        B0110
    },
    {// 6
        B0110,
        B1000,
        B1000,
        B1110,
        B1001,
        B0110
    },
    {// 7
        B1111,
        B0001,
        B0010,
        B0100,
        B0100,
        B0100
    },
    {// 8
        B0110,
        B1001,
        B0110,
        B1001,
        B1001,
        B0110
    },
    {// 9
        B0110,
        B1001,
        B0111,
        B0001,
        B0001,
        B0110
    },
    {// :
        B0,
        B0,
        B1,
        B0,
        B1,
        B0
    },
    {// ;
        B0,
        B0,
        B1,
        B0,
        B1,
        B1
    },
    {// <
        B00,
        B00,
        B01,
        B10,
        B01,
        B00
    },
    {// =
        B000,
        B000,
        B111,
        B000,
        B111,
        B000
    },
    {// >
        B00,
        B00,
        B10,
        B01,
        B10,
        B00
    },
    {// ?
        B0110,
        B1001,
        B0001,
        B0110,
        B0000,
        B0100
    },
    {// @
        B011110,
        B100001,
        B101101,
        B101111,
        B100000,
        B011111
    },
    {// A
        B0110,
        B1001,
        B1001,
        B1111,
        B1001,
        B1001
    },
    {// B
        B1110,
        B1001,
        B1110,
        B1001,
        B1001,
        B1110
    },
    {// C
        B0111,
        B1000,
        B1000,
        B1000,
        B1000,
        B0111
    },
    {// D
        B1110,
        B1001,
        B1001,
        B1001,
        B1001,
        B1110
    },
    {// E
        B1111,
        B1000,
        B1000,
        B1111,
        B1000,
        B1111
    },
    {// F
        B1111,
        B1000,
        B1111,
        B1000,
        B1000,
        B1000
    },
    {// G
        B0111,
        B1000,
        B1000,
        B1011,
        B1001,
        B0111
    },
    {// H
        B1001,
        B1001,
        B1001,
        B1111,
        B1001,
        B1001
    },
    {// I
        B111,
        B010,
        B010,
        B010,
        B010,
        B111
    },
    {// J
        B111,
        B001,
        B001,
        B001,
        B101,
        B010
    },
    {// K
        B1001,
        B1001,
        B1010,
        B1100,
        B1010,
        B1001
    },
    {// L
        B1000,
        B1000,
        B1000,
        B1000,
        B1000,
        B1111
    },
    {// M
        B10001,
        B11011,
        B10101,
        B10001,
        B10001,
        B10001
    },
    {// N
        B1001,
        B1001,
        B1101,
        B1011,
        B1001,
        B1001
    },
    {// O
        B0110,
        B1001,
        B1001,
        B1001,
        B1001,
        B0110
    },
    {// P
        B1110,
        B1001,
        B1001,
        B1110,
        B1000,
        B1000
    },
    {// Q
        B0110,
        B1001,
        B1001,
        B1001,
        B0110,
        B0001
    },
    {// R
        B1110,
        B1001,
        B1001,
        B1110,
        B1010,
        B1001
    },
    {// S
        B0111,
        B1000,
        B1000,
        B0110,
        B0001,
        B1110
    },
    {// T
        B11111,
        B00100,
        B00100,
        B00100,
        B00100,
        B00100
    },
    {// U
        B1001,
        B1001,
        B1001,
        B1001,
        B1001,
        B0110
    },
    {// V
        B10001,
        B10001,
        B10001,
        B10001,
        B01010,
        B00100
    },
    {// W
        B10001,
        B10001,
        B10001,
        B10101,
        B10101,
        B01010
    },
    {// X
        B10001,
        B10001,
        B01010,
        B00100,
        B01010,
        B10001
    },
    {// Y
        B10001,
        B10001,
        B01010,
        B00100,
        B00100,
        B00100
    },
    {// Z
        B1111,
        B0001,
        B0010,
        B0100,
        B1000,
        B1111
    }
};

uint8_t asciiLen[59] = {
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

uint8_t pacmanShapes[2][6] = {
    {
        B00000,
        B01110,
        B10001,
        B10010,
        B10001,
        B01110
    },
    {
        B00000,
        B01110,
        B10001,
        B10011,
        B10001,
        B01110
    }
};

uint8_t pacmanShapeLen = 5;

uint8_t pacmanGhostShapes[1][6] = {
    {
        B01110,
        B10001,
        B11011,
        B10001,
        B11011,
        B10101
    }
};

uint8_t pacmanGhostShapeLen = 5;

void setup() {
    
    // set PORTD input/output modes
    // D0-3 unused
    // D4 clock
    // D5-6 cathodes
    // D7 anodes 25-32
    DDRD |= B11110000;

    // set PORTB input/output modes
    // D13 unused, first two bits map to crystal and unusable
    // D8 anodes 17-24
    // D9 anodes 9-16
    // D10 anodes 1-8
    // D11-12 btns
    DDRB = B00111;


    PORTB &= B11111000;
    PORTD &= B00011111;

    for (int i = 0; i < 8; i++) {
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

    // startupAnim();

    // get mode from non-volatile memory
    mode = EEPROM.read(eeModeAddr);

    displayMode();
    modeChanged = true;
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
    for (int j = 0; j < 8; j++) {
        // set clock low
        PORTD &= (0 << PD4);

        PORTB &= B11111000;

        PORTB |= (((sr1 >> j) & 1) << 2);
        PORTB |= (((sr2 >> j) & 1) << 1);
        PORTB |= ((sr3 >> j) & 1);

        PORTD &= B00011111;

        PORTD |= (((sr4 >> j) & 1) << 7);
        PORTD |= (((sr5 >> j) & 1) << 6);
        PORTD |= (((sr6 >> j) & 1) << 5);

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

void loop() {
    // store new mode in EEPROM memory
    if (modeChanged) {
        delay(1000);
        fillMatrix(0);

        EEPROM.write(eeModeAddr, mode);
        modeChanged = false;
    }

    // set effect for mode
    switch (mode) {
        case 255:
            displayText("BYEEE", LAYER_FRONT, 2);
            break;
        case 0:
            displayText("SORRY", LAYER_FRONT, 2);
            break;
        case 1:
            scrollText("HELLO WORLD!", LAYER_FRONT, true);
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
            scrollText("ALL THE TIME ", LAYER_BACK, false);
            scrollText("AND ALL THE TIME ", LAYER_FRONT, false);
            scrollText("GOD IS GOOD!", LAYER_BACK, true);
            break;
        default:
            fillMatrix(0);
            break;
    }
}

// ==============================================
//  Effect functions
// ==============================================

// ================
//  Text effects
// ================

// shows the number of the current mode
void displayMode() {
    fillMatrix(0);
    displayText(String(mode), LAYER_FRONT, 2);
}

// shows static text
void displayText(String message, uint8_t z, uint8_t startX) {
    uint8_t x;

    for (x = 0; x < startX; x++) {
        setColOff(x, z);
    }

    for (char c : message) {
        for (uint8_t i = 0; i < asciiLen[c - ' ']; i++) {
            setColForChar(c, i, x, z);
            x++;
        }
        setColOff(x, z);
        x++;
    }

    for (; x < 32; x++) {
        setColOff(x, z);
    }
}

// scrolls text from right to left
void scrollText(String message, uint8_t z, bool scrollOut) {
    uint16_t frameDelay = 150;

    for (char c : message) {

        // add each column of the char to the matrix
        for (int i = 0; i < asciiLen[c - ' ']; i++) {
            addNextColToMatrix(c, i, z);

            if (modeChanged) return;
            delay(frameDelay);
            shiftMatrixL();
        }

        if (modeChanged) return;
        delay(frameDelay);
        shiftMatrixL();
        if (modeChanged) return;
        delay(frameDelay);
        shiftMatrixL();
    }

    if (scrollOut) {
        scrollMatrixOutL(frameDelay);
    }
}

void scrollTextBothLayers(String message, bool scrollOut) {
    uint16_t frameDelay = 150;

    for (char c : message) {

        // add each column of the char to the matrix
        for (int i = 0; i < asciiLen[c - ' ']; i++) {
            addNextColToMatrix(c, i, LAYER_FRONT);
            addNextColToMatrix(c, i, LAYER_BACK);
            
            if (modeChanged) return;
            delay(frameDelay);
            shiftMatrixL();
        }

        if (modeChanged) return;
        delay(frameDelay);
        shiftMatrixL();
        if (modeChanged) return;
        delay(frameDelay);
        shiftMatrixL();
    }

    if (scrollOut) {
        scrollMatrixOutL(frameDelay);
    }
}

void setColForChar(char c, uint8_t col, uint8_t x, uint8_t z) {
    c -= ' ';

    for (uint8_t i = 0; i < 6; i++) {
        setVoxel(x, i, z, (ascii[c][5 - i] >> (asciiLen[c] - 1 - col)) & 1);
    }
}

void addNextColToMatrix(char c, uint8_t col, uint8_t z) {
    matrix[z][0][1] |= (ascii[c - ' '][5] >> (asciiLen[c - ' '] - 1 - col));
    matrix[z][1][1] |= (ascii[c - ' '][4] >> (asciiLen[c - ' '] - 1 - col));
    matrix[z][2][1] |= (ascii[c - ' '][3] >> (asciiLen[c - ' '] - 1 - col));
    matrix[z][3][1] |= (ascii[c - ' '][2] >> (asciiLen[c - ' '] - 1 - col));
    matrix[z][4][1] |= (ascii[c - ' '][1] >> (asciiLen[c - ' '] - 1 - col));
    matrix[z][5][1] |= (ascii[c - ' '][0] >> (asciiLen[c - ' '] - 1 - col));
}

// =======================
//  Animation Effects
// =======================

void startupAnim() {
    fillMatrix(0);
    boxOutline(0, 0, 31, 5, LAYER_FRONT);
    boxOutline(0, 0, 31, 5, LAYER_BACK);
    delay(500);

    fillMatrix(0);
    boxOutline(1, 1, 30, 4, LAYER_FRONT);
    boxOutline(1, 1, 30, 4, LAYER_BACK);
    delay(500);
    
    fillMatrix(0);
    boxOutline(2, 2, 29, 3, LAYER_FRONT);
    boxOutline(2, 2, 29, 3, LAYER_BACK);
    delay(500);

    fillMatrix(0);
    delay(500);
}

void wipe(uint8_t direction) {
    // horizontal
    if (direction == DIR_L_TO_R) {
        for (uint8_t x = 0; x < 32; x++) {
            setColBothLayers(x, 1);
            if (modeChanged) return;
            delay(20);
        }
        for (uint8_t x = 0; x < 32; x++) {
            setColBothLayers(x, 0);
            if (modeChanged) return;
            delay(20);
        }
    } else if (direction == DIR_R_TO_L) {
        for (int8_t x = 31; x >= 0; x--) {
            setColBothLayers(x, 1);
            if (modeChanged) return;
            delay(20);
        }
        for (int8_t x = 31; x >= 0; x--) {
            setColBothLayers(x, 0);
            if (modeChanged) return;
            delay(20);
        }
    // vertical
    } else if (direction == DIR_B_TO_T) {
        for (uint8_t y = 0; y < 6; y++) {
            setRowBothLayers(y, 1);
            if (modeChanged) return;
            delay(70);
        }
        for (uint8_t y = 0; y < 6; y++) {
            setRowBothLayers(y, 0);
            if (modeChanged) return;
            delay(70);
        }
    } else if (direction == DIR_T_TO_B) {
        for (int8_t y = 5; y >= 0; y--) {
            setRowBothLayers(y, 1);
            if (modeChanged) return;
            delay(70);
        }
        for (int8_t y = 5; y >= 0; y--) {
            setRowBothLayers(y, 0);
            if (modeChanged) return;
            delay(70);
        }
    }
}

void wipeDiagonal(uint8_t xDirection, uint8_t yDirection) {
    uint8_t width = 28;
    if (xDirection == DIR_L_TO_R && yDirection == DIR_B_TO_T) {
        for (int8_t x = 0; x < 42 + width; x++) {
            for (uint8_t y = 0; y < 6; y++) {
                setVoxelOn(x - (2 * y), y, LAYER_FRONT);
                setVoxelOff(x - width - (2 * y), y, LAYER_FRONT);
            }
            if (modeChanged) return;
            delay(20);
        }
    } else if (xDirection == DIR_L_TO_R && yDirection == DIR_T_TO_B) {
        for (int8_t x = 0; x < 42 + width; x++) {
            for (uint8_t y = 0; y < 6; y++) {
                setVoxelOn(x - (2 * (5 - y)), y, LAYER_FRONT);
                setVoxelOff(x - width - (2 * (5 - y)), y, LAYER_FRONT);
            }
            if (modeChanged) return;
            delay(20);
        }
    } else if (xDirection == DIR_R_TO_L && yDirection == DIR_B_TO_T) {
        for (int8_t x = 41 + width; x >= 0; x--) {
            for (uint8_t y = 0; y < 6; y++) {
                setVoxelOn(x - width - (2 * y), y, LAYER_FRONT);
                setVoxelOff(x - (2 * y), y, LAYER_FRONT);
            }
            if (modeChanged) return;
            delay(20);
        }
    } else if (xDirection == DIR_R_TO_L && yDirection == DIR_T_TO_B) {
        for (int8_t x = 41 + width; x >= 0; x--) {
            for (uint8_t y = 0; y < 6; y++) {
                setVoxelOn(x - width - (2 * (5 - y)), y, LAYER_FRONT);
                setVoxelOff(x - (2 * (5 - y)), y, LAYER_FRONT);
            }
            if (modeChanged) return;
            delay(20);
        }
    }
}

void pacman() {
    int frameDelay = 400;

    fillLayer(LAYER_FRONT, 0);

    // drawPacDots(DIR_L_TO_R);
    // pacmanScrollLtoR(frameDelay);
    // if (modeChanged) return;

    // drawPacDots(DIR_R_TO_L);
    // pacmanScrollRtoL(frameDelay);
    // if (modeChanged) return;

    drawPacDots(DIR_L_TO_R);
    pacmanScrollLtoRwGhosts(frameDelay, 2, 12);
    
    drawPacDots(DIR_R_TO_L);
    pacmanScrollRtoLwGhosts(frameDelay, 2, 12);
}

void drawPacDots(uint8_t direction) {
    if (direction == DIR_L_TO_R) {
        fillRow(2, LAYER_BACK, B00100010);
    } else {
        fillRow(2, LAYER_BACK, B01000100);
    }
}

void pacmanScrollLtoR(int frameDelay) {
    uint8_t shape = 1;
    for (int8_t x = -5; x < 33; x++) {
        drawPacmanShape(x, LAYER_FRONT, shape, DIR_L_TO_R);
        setColOff(x - 1, LAYER_FRONT);
        setColOff(x + 3, LAYER_BACK);

        if (modeChanged) return;
        delay(frameDelay);
        shape++;
        if (shape == 2) {
            shape = 0;
        }
    }
}

void pacmanScrollRtoL(int frameDelay) {
    uint8_t shape = 1;
    for (int8_t x = 32; x >= -5; x--) {
        drawPacmanShape(x, LAYER_FRONT, shape, DIR_R_TO_L);
        setColOff(x + pacmanShapeLen, LAYER_FRONT);
        setColOff(x + 1, LAYER_BACK);

        if (modeChanged) return;
        delay(frameDelay);
        shape++;
        if (shape == 2) shape = 0;
    }
}

void pacmanScrollLtoRwGhosts(int frameDelay, uint8_t numGhosts, uint8_t gap) {
    // max 4 ghosts
    if (numGhosts > 4) numGhosts == 4;

    uint8_t pacShape = 1;
    for (int8_t x = -5; x < 31 + gap + numGhosts * (pacmanGhostShapeLen + 2); x++) {
        // draw pacman
        drawPacmanShape(x, LAYER_FRONT, pacShape, DIR_L_TO_R);
        setColOff(x - 1, LAYER_FRONT);
        setColOff(x + 3, LAYER_BACK);

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
        pacShape++;
        if (pacShape == 2) pacShape = 0;
    }
}

void pacmanScrollRtoLwGhosts(int frameDelay, uint8_t numGhosts, uint8_t gap) {
    // max 4 ghosts
    if (numGhosts > 4) numGhosts == 4;

    uint8_t pacShape = 1;
    for (int8_t x = 32; x >= -3 - gap - numGhosts * (pacmanGhostShapeLen + 2); x--) {
        // draw pacman
        drawPacmanShape(x, LAYER_FRONT, pacShape, DIR_R_TO_L);
        setColOff(x + pacmanShapeLen, LAYER_FRONT);
        setColOff(x + 1, LAYER_BACK);

        // draw ghosts
        int8_t ghostPos;
        uint8_t ghostLayer;
        for (uint8_t i = 0; i < numGhosts; i++) {
            ghostPos = x + gap + pacmanShapeLen + i * (pacmanGhostShapeLen + 2);
            ghostLayer = i % 2 == 0 ? LAYER_FRONT : LAYER_BACK;
            drawGhostShape(ghostPos, ghostLayer);
            setColOff(ghostPos + pacmanGhostShapeLen, ghostLayer);
        }

        if (modeChanged) return;
        delay(frameDelay);
        pacShape++;
        if (pacShape == 2) pacShape = 0;
    }
}

void drawPacmanShape(int x, uint8_t z, uint8_t shape, uint8_t direction) {
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

void drawGhostShape(int x, uint8_t z) {
    if (x < 1 - pacmanGhostShapeLen || x >= 32) {
        return;
    }

    for (uint8_t col = 0; col < pacmanGhostShapeLen; col++) {
        setColForShape(pacmanGhostShapes[0], pacmanGhostShapeLen, col, x + col, z);
    }
}

void setColForShape(uint8_t shape[6], uint8_t len, uint8_t col, uint8_t x, uint8_t z) {
    for (uint8_t i = 0; i < 6; i++) {
        setVoxel(x, i, z, (shape[5 - i] >> len - 1 - col) & 1);
    }
}

// ==============================================
//  General drawing functions
// ==============================================

void shiftMatrixL() {
    for (uint8_t i = 0; i < 2; i++) {
        for (uint8_t j = 0; j < 6; j++) {
            uint8_t overflow = (matrix[i][j][1] >> 15) & 1;

            matrix[i][j][0] = (matrix[i][j][0] << 1) | overflow;
            matrix[i][j][1] = (matrix[i][j][1] << 1);
        }
    }
}

void scrollMatrixOutL(int delayTime) {
    for (int i = 0; i < 32; i++) {
        if (modeChanged) return;
        delay(delayTime);
        shiftMatrixL();
    }
}

// range validation
bool isInRangeX(uint8_t x) {
    return (x >= 0 && x < 32);
}

bool isInRangeY(uint8_t y) {
    return (y >= 0 && y < 6);
}

bool isInRangeZ(uint8_t z) {
    return (z >= 0 && z < 2);
}

bool isInRange(uint8_t x, uint8_t y, uint8_t z) {
    return (isInRangeX(x) && isInRangeY(y) && isInRangeZ(z));
}

void setVoxelOn(uint8_t x, uint8_t y, uint8_t z) {
    if (isInRange(x, y, z)) {
        if (x > 15) {
            matrix[z][y][1] |= (1 << (31 - x));
        } else {
            matrix[z][y][0] |= (1 << (15 - x));
        }
    }
}

void setVoxelOff(uint8_t x, uint8_t y, uint8_t z) {
    if (isInRange(x, y, z)) {
        if (x > 15) {
            matrix[z][y][1] &= ~(1 << (31 - x));
        } else {
            matrix[z][y][0] &= ~(1 << (15 - x));
        }
    }
}

void setVoxel(uint8_t x, uint8_t y, uint8_t z, uint8_t state) {
    if (state) {
        setVoxelOn(x, y, z);
    } else {
        setVoxelOff(x, y, z);
    }
}

void flipVoxel(uint8_t x, uint8_t y, uint8_t z) {
    if (isInRange(x, y, z)) {
        if (x > 15) {
            matrix[z][y][1] ^= (1 << (31 - x));
        } else {
            matrix[z][y][0] ^= (1 << (15 - x));
        }
    }
}

void setRowOn(uint8_t y, uint8_t z) {
    if (isInRangeY(y) && isInRangeY(y)) {
        matrix[z][y][0] = 0xffff;
        matrix[z][y][1] = 0xffff;
    }
}

void setRowOff(uint8_t y, uint8_t z) {
    if (isInRangeY(y) && isInRangeY(y)) {
        matrix[z][y][0] = 0x0000;
        matrix[z][y][1] = 0x0000;
    }
}

void setRow(uint8_t y, uint8_t z, uint8_t state) {
    if (state) {
        setRowOn(y, z);
    } else {
        setRowOff(y, z);
    }
}

void setRowBothLayers(uint8_t y, uint8_t state) {
    if (state) {
        setRowOn(y, LAYER_FRONT);
        setRowOn(y, LAYER_BACK);
    } else {
        setRowOff(y, LAYER_FRONT);
        setRowOff(y, LAYER_BACK);
    }
}

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

void setCol(uint8_t x, uint8_t z, uint8_t state) {
    if (state) {
        setColOn(x, z);
    } else {
        setColOff(x, z);
    }
}

void setColBothLayers(uint8_t x, uint8_t state) {
    if (state) {
        setColOn(x, LAYER_FRONT);
        setColOn(x, LAYER_BACK);
    } else {
        setColOff(x, LAYER_FRONT);
        setColOff(x, LAYER_BACK);
    }
}

// fills the matrix with the given 8-bit pattern
void fillMatrix(uint8_t pattern) {
    for (uint8_t z = 0; z < 2; z++) {
        for (uint8_t y = 0; y < 6; y++) {
            matrix[z][y][0] = (pattern << 8) | pattern;
            matrix[z][y][1] = (pattern << 8) | pattern;
        }
    }
}

// fills the given layer with the given 8-bit pattern
void fillLayer(uint8_t z, uint8_t pattern) {
    for (uint8_t y = 0; y < 6; y++) {
        matrix[z][y][0] = (pattern << 8) | pattern;
        matrix[z][y][1] = (pattern << 8) | pattern;
    }
}

void fillRow(uint8_t y, uint8_t z, uint8_t pattern) {
    matrix[z][y][0] = (pattern << 8) | pattern;
    matrix[z][y][1] = (pattern << 8) | pattern;
}

// makes sure x1 > x2
void setuint8_tsAscOrder(uint8_t x1, uint8_t x2, uint8_t *p1, uint8_t *p2) {
    if (x1 > x2) {
        uint8_t temp = x1;
        x1 = x2;
        x2 = temp;
    }
    *p1 = x1;
    *p2 = x2;
}

void boxOutline(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t z) {

    setuint8_tsAscOrder(x1, x2, &x1, &x2);
    setuint8_tsAscOrder(y1, y2, &y1, &y2);

    matrix[z][y1][0] = line0(x1, x2);
    matrix[z][y1][1] = line1(x1, x2);

    matrix[z][y2][0] = line0(x1, x2);
    matrix[z][y2][1] = line1(x1, x2);

    for (uint8_t i = y1; i < y2; i++) {
        setVoxelOn(x1, i, z);
        setVoxelOn(x2, i, z);
    }
}

uint16_t line(uint8_t start, uint8_t end) {
    return ((0xffff >> start) & ~(0xffff >> (end + 1)));
}

uint16_t line0(uint8_t start, uint8_t end) {
    if (start > 15) {
        return 0;
    }
    if (end > 15) {
        end = 15;
    }
    return line(start, end);
}

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