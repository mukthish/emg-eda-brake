/**
 * @file input_acq.h
 * @brief Input Acquisition Manager — UART/BLE polling and normalisation.
 *
 * Owner: Devansh
 *
 * Responsibilities:
 * - Acquire UART (or BLE) samples
 * - Parse and normalise frames
 * - Track link health
 *
 * Frame format (UART simulation):
 * $EMG,EMG=0.72,EDA=0.31,TS=123456*
 */
#ifndef INPUT_ACQ_H
#define INPUT_ACQ_H

#include <stdint.h>
#include <stdbool.h>

// Ring Buffer Configuration
#define RX_BUFFER_SIZE 256

// Expose the initialization and update functions
void InputAcq_Init(void);
void InputAcq_Update(void);

// Expose the function that the UART ISR will call to push data
void InputAcq_PushChar(uint8_t data);

#endif /* INPUT_ACQ_H */
