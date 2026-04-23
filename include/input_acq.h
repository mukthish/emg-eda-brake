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

#include "input_acq.h"
#include "dispatcher.h"
#include <string.h>
#include <stdlib.h>

// --- Private Ring Buffer Variables ---
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint16_t head = 0;
static uint16_t tail = 0;

// --- Private String Parsing Variables ---
#define MAX_FRAME_LEN 32  // Reduced since we just expect a small float like "3.141\n"
static char frame_buffer[MAX_FRAME_LEN];
static uint8_t frame_index = 0;

// ---------------------------------------------------------
void InputAcq_Init(void) {
    head = 0;
    tail = 0;
    frame_index = 0;
}

// ---------------------------------------------------------
void InputAcq_PushChar(uint8_t data) {
    uint16_t next_head = (head + 1) % RX_BUFFER_SIZE;
    if (next_head != tail) {
        rx_buffer[head] = data;
        head = next_head;
    }
}

// ---------------------------------------------------------
void InputAcq_Update(void) {
    // Process all available characters in the ring buffer
    while (tail != head) {
        char c = (char)rx_buffer[tail];
        tail = (tail + 1) % RX_BUFFER_SIZE;
        
        // --- NEW String Parsing State Machine ---
        // If we hit a newline (\n) or carriage return (\r), the frame is done
        if (c == '\n' || c == '\r') {
            
            // Only process if we actually buffered some digits
            if (frame_index > 0) {
                frame_buffer[frame_index] = '\0'; // Null-terminate the string
                
                // Convert the raw text directly to a float
                float emg_value = (float)atof(frame_buffer);
                
                // Package the data and post it to the queue
                SystemEvent evt;
                evt.type = SYS_EVT_SAMPLES_PARSED;
                evt.float_payload = emg_value;
                evt.int_payload = 0;
                Dispatcher_PostEvent(evt);
                
                // Reset the index for the next incoming transmission
                frame_index = 0;
            }
        } 
        else if (frame_index < (MAX_FRAME_LEN - 1)) {
            // It's a normal character (a digit or a decimal point). Save it!
            frame_buffer[frame_index++] = c;
        }
    }
}
