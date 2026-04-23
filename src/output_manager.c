#include "output_mgr.h"
#include "stm32f4xx.h"

// Complete Pin Definitions
#define PIN_GREEN_LED  12
#define PIN_ORANGE_LED 13
#define PIN_RED_LED    14
#define PIN_BLUE_LED   15

// ---------------------------------------------------------
void OutputMgr_Init(void) {
    // Start up: Green ON (Safe), Orange ON (Calibrating)
    // Red OFF, Blue OFF
    GPIOD->BSRR = (1ul << PIN_GREEN_LED) | (1ul << PIN_ORANGE_LED) | 
                  (1ul << (PIN_RED_LED + 16)) | (1ul << (PIN_BLUE_LED + 16));
}

// ---------------------------------------------------------
void OutputMgr_SetBrakes(uint8_t brake_state) {
    // (Keep your existing SetBrakes logic here...)
    if (brake_state == 1) {
        GPIOD->BSRR = (1ul << PIN_RED_LED) | (1ul << (PIN_GREEN_LED + 16));
    } else {
        GPIOD->BSRR = (1ul << (PIN_RED_LED + 16)) | (1ul << PIN_GREEN_LED);
    }
}

// ---------------------------------------------------------
// Turns off the Orange LED when calibration is done
// ---------------------------------------------------------
void OutputMgr_SetCalibrationLED(uint8_t state) {
    if (state == 0) {
        GPIOD->BSRR = (1ul << (PIN_ORANGE_LED + 16)); // OFF
    }
}
