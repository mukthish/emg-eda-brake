#include "stm32f4xx.h"
#include "input_acq.h"
#include "safety_manager.h"
#include "dispatcher.h"
#include "supervisor.h"
#include "output_mgr.h"
#include "logger.h"

void SystemClock_Config(void);
void GPIO_Init(void);
void USART1_Init(void);
void SysTick_Init(void);
void Watchdog_Init(void);
void Watchdog_Feed(void);

// --- GLOBAL TIME COUNTER ---
// Must be volatile because it is modified inside an ISR
static volatile uint32_t system_time_ms = 0;

// Getter function so other modules can read the time safely
uint32_t GetSystemTime(void) {
    return system_time_ms;
}

// --- INTENT FLAG ---
// Set this to 1 in your algorithm module (e.g., dispatcher.c) when a brake intent is detected.
volatile uint8_t brake_intent_flag = 0; 

int main(void) {
    
    SystemClock_Config(); 
    GPIO_Init();          
    USART1_Init();        
    InputAcq_Init();
    SafetyManager_Init();
    Dispatcher_Init();
    Supervisor_Init();
    OutputMgr_Init();
    Logger_Init();
    SysTick_Init();
    
    // Initialize the hardware watchdog timer (~1 second timeout)
    Watchdog_Init();

    // Variables for non-blocking LED timer
    uint32_t green_led_start_time = 0;
    uint8_t is_intent_active = 0;
    
    while (1) {
        // 1. FEED THE WATCHDOG
        // If the system hangs and this isn't called within ~1 second, the board resets.
        Watchdog_Feed();

        // 2. RUN SYSTEM TASKS
        InputAcq_Update();
        Dispatcher_ProcessQueue();
                
        // 3. CHECK FOR BRAKE INTENT (TRIGGER)
        if (brake_intent_flag == 1) {
            brake_intent_flag = 0;             // Reset the flag immediately
            GPIOD->ODR |= (1ul << 12);         // Turn ON Green LED (PD12)
            green_led_start_time = GetSystemTime(); // Record the exact time
            is_intent_active = 1;              // Mark the intent as active
        }

        // 4. CHECK LED TIMER (500ms TIMEOUT)
        if (is_intent_active && ((GetSystemTime() - green_led_start_time) >= 500)) {
            GPIOD->ODR &= ~(1ul << 12);        // Turn OFF Green LED (PD12)
            is_intent_active = 0;              // Reset active state
        }
    }
}

// --- WATCHDOG FUNCTIONS ---

void Watchdog_Init(void) {
    // 1. Enable write access to IWDG_PR and IWDG_RLR registers
    IWDG->KR = 0x5555;  			// KR -> Key Register
    
    // 2. Set prescaler to 32. 
    // The LSI clock is ~32kHz. 32000 / 32 = 1000 Hz (1 tick per millisecond)
    IWDG->PR = 0x03; 					// PR -> Prescaler Register
    
    // 3. Set reload value. For ~1000ms (1 second), set to 1000.
    IWDG->RLR = 1000;					// RLR -> Reload Register
    
    // 4. Reload the counter to apply the values
    IWDG->KR = 0xAAAA;
    
    // 5. Start the watchdog (Starts the LSI [Low Speed Clock] automatically)
    IWDG->KR = 0xCCCC;
}

void Watchdog_Feed(void) {
    // Reloads the IWDG counter back to 1000 to prevent a system reset
    IWDG->KR = 0xAAAA; 
}

// --- ISR AND PERIPHERAL INITS ---

void USART1_IRQHandler(void) {
    // 1. Check for and clear Overrun Errors (ORE) to prevent freezing
    if (USART1->SR & USART_SR_ORE) {
        // Clearing ORE requires reading SR, then reading DR
        volatile uint32_t dummy = USART1->SR;
        dummy = USART1->DR;
        (void)dummy; 
    }

    // 2. Process normal incoming data
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t received_data = USART1->DR; 
        InputAcq_PushChar(received_data);
    }
}

void SysTick_Handler(void) {
    // 1. Increment global time
    system_time_ms++;

    // 2. Drive the Supervisor's recovery cooldown timer
    Supervisor_Tick1ms();
    
    // 3. Drive the Blue LED (PD15) Heartbeat every 500ms
    static uint16_t heartbeat_timer = 0;
    heartbeat_timer++;
    
    if (heartbeat_timer >= 500) {
        heartbeat_timer = 0;
        GPIOD->ODR ^= (1ul << 15); 
    }
}

void USART1_Init(void) {
    // 1. Enable clocks for GPIOB (not GPIOA) and USART1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    
    // 2. Configure PB6 (TX) and PB7 (RX) for Alternate Function 7
    GPIOB->MODER &= ~((3ul << 12) | (3ul << 14)); // Clear bits 12-15
    GPIOB->MODER |= ((2ul << 12) | (2ul << 14));  // Set to AF mode (10 in binary)
    
    // Use AFR[0] (AFRL) because pins 6 and 7 are in the lower half (0-7)
    GPIOB->AFR[0] |= (7ul << 24) | (7ul << 28); 
    
    // 3. Set Baud Rate to 115200
    USART1->BRR = 0x02D9;
    
    // 4. Enable USART, TX, RX, and the RXNE (Read Data Register Not Empty) Interrupt
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    
    // 5. Enable the USART1 Interrupt in the core ARM NVIC
    NVIC_EnableIRQ(USART1_IRQn);
}

// Configures the system clock to 168 MHz using the 8 MHz HSE crystal
void SystemClock_Config(void) {
    // 1. Enable HSE and wait for it to be ready
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    // 2. Set Power Enable Clock and Voltage Regulator
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    // 3. Configure Flash prefetch, Instruction cache, Data cache, and wait states (5 WS for 168 MHz)
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;

    // 4. Configure the Main PLL
    RCC->PLLCFGR = (8ul) | (336ul << 6) | (0ul << 16) | RCC_PLLCFGR_PLLSRC_HSE | (7ul << 24);

    // 5. Enable the Main PLL and wait for it to be ready
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    // 6. Configure AHB/APB prescalers
    // FIRST: Clear the old prescaler bits
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2); 
    // SECOND: Set your new prescaler bits
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;
            
    // 7. Select the main PLL as system clock source and wait for it to be used
    RCC->CFGR |= RCC_CFGR_SW_PLL; 			// SW -> System Clock Switch
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);				// SWS -> System Clock Switch Status
}

// Configures PD12, PD13, PD14, PD15 as general purpose output
void GPIO_Init(void) {
    // Enable GPIOD clock on AHB1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    
    GPIOD->MODER &= ~((3ul<<24) | (3ul<<26) | (3ul<<28) | (3ul<<30));
    GPIOD->MODER |= ((1ul<<24) | (1ul<<26) | (1ul<<28) | (1ul<<30));
    
}

// Configures the ARM SysTick timer to trigger an interrupt every 1 ms
void SysTick_Init(void) {
    // 1. Program the Reload Register with the calculated value
    SysTick->LOAD = 168000 - 1; 
    
    // 2. Clear the Current Value register
    SysTick->VAL = 0;
    
    // 3. Enable SysTick, enable the interrupt, and choose the processor clock
    // Bit 0 = ENABLE, Bit 1 = TICKINT, Bit 2 = CLKSOURCE
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | 
                    SysTick_CTRL_TICKINT_Msk | 
                    SysTick_CTRL_ENABLE_Msk;
}
