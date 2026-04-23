#include "logger.h"
#include "stm32f4xx.h"
#include <stdio.h>

// Bring in the global time function we defined in main.c
extern uint32_t GetSystemTime(void);

// ---------------------------------------------------------
// Helper: Blast a string character-by-character out of USART1
// ---------------------------------------------------------
static void USART1_SendString(const char* str) {
    while (*str) {
        // Wait until the Transmit Data Register is empty (TXE flag)
        while (!(USART1->SR & USART_SR_TXE)); 
        // Load the next character
        USART1->DR = *str++;
    }
}

// ---------------------------------------------------------
void Logger_Init(void) {
    // Print the CSV Header to the PC when the board boots up
    USART1_SendString("Timestamp_ms,EventType,Float_Val,Int_Val\r\n");
}

// ---------------------------------------------------------
// Packages the event data and transmits it
// ---------------------------------------------------------
void Logger_LogEvent(SystemEvent evt) {
    char csv_buffer[64];
    uint32_t timestamp = GetSystemTime();
    
    // Format: Timestamp, EventType (enum int), Float Payload, Int Payload
    sprintf(csv_buffer, "%u,%d,%f,%d\r\n", 
            timestamp, 
            evt.type, 
            evt.float_payload, 
            evt.int_payload);
    
		//sprintf(csv_buffer, "HELLO\n");
	
    USART1_SendString(csv_buffer);
}
