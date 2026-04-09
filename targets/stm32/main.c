/**
 * @file targets/stm32/main.c
 * @brief STM32F4 entry point — configures clocks, GPIO, UART, and SysTick.
 *
 * Adapted from docs/main.c reference implementation.
 *
 * SysTick is configured for a 1 ms interrupt that calls system_run_tick().
 * The main loop is idle — all work happens in the SysTick ISR.
 */

#include "stm32f4xx.h"
#include "system_shell.h"
#include "hal.h"

/* ── Forward declarations ───────────────────────────────────────────── */

void SystemClock_Config(void);
void GPIO_Init(void);
void USART1_Init(void);
void SysTick_Init(void);

/* ── Main ───────────────────────────────────────────────────────────── */

int main(void)
{
    /* 1. Platform hardware initialisation */
    SystemClock_Config();
    GPIO_Init();
    USART1_Init();
    SysTick_Init();

    /* 2. Firmware initialisation */
    system_init();

    /* 3. Main loop — idle, all work in SysTick_Handler */
    while (1) {
        __asm volatile ("wfi");  /* Wait For Interrupt — power saving */
    }
}

/* ── SysTick Handler (1 ms) ─────────────────────────────────────────── */

void SysTick_Handler(void)
{
    system_run_tick();
}

/* ── USART1 ISR (receive interrupt) ─────────────────────────────────── */

/* RX ring buffer for ISR → hal_uart_read_frame handoff */
#define UART_RX_RING_SIZE  512

static volatile uint8_t  uart_rx_ring[UART_RX_RING_SIZE];
static volatile uint32_t uart_rx_head = 0;
static volatile uint32_t uart_rx_tail = 0;

void USART1_IRQHandler(void)
{
    if (USART1->SR & USART_SR_RXNE) {
        uint8_t ch = (uint8_t)(USART1->DR & 0xFF);
        uint32_t next = (uart_rx_head + 1) % UART_RX_RING_SIZE;
        if (next != uart_rx_tail) {
            uart_rx_ring[uart_rx_head] = ch;
            uart_rx_head = next;
        }
        /* else: ring full — drop byte (safer than blocking in ISR) */
    }
}

/* ── Accessor for hal_stm32.c ───────────────────────────────────────── */

int stm32_uart_rx_available(void)
{
    return uart_rx_head != uart_rx_tail;
}

uint8_t stm32_uart_rx_read(void)
{
    if (uart_rx_head == uart_rx_tail) return 0;
    uint8_t ch = uart_rx_ring[uart_rx_tail];
    uart_rx_tail = (uart_rx_tail + 1) % UART_RX_RING_SIZE;
    return ch;
}

/* ── Hardware initialisation ────────────────────────────────────────── */

void SystemClock_Config(void)
{
    /* HSE on, wait ready */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* Power controller */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    /* Flash: prefetch, I-cache, D-cache, 5 wait states for 168 MHz */
    FLASH->ACR = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN
               | FLASH_ACR_LATENCY_5WS;

    /* PLL: HSE/8 * 336 / 2 = 168 MHz */
    RCC->PLLCFGR = (8UL) | (336UL << 6) | (0UL << 16)
                 | RCC_PLLCFGR_PLLSRC_HSE | (7UL << 24);

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* Bus prescalers: AHB/1, APB1/4 (42 MHz), APB2/2 (84 MHz) */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4
               | RCC_CFGR_PPRE2_DIV2;

    /* Switch to PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

void GPIO_Init(void)
{
    /* Enable GPIOD clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

    /* PD12-PD15 as output (LEDs + brake on PD12) */
    GPIOD->MODER &= ~((3UL<<24) | (3UL<<26) | (3UL<<28) | (3UL<<30));
    GPIOD->MODER |=  ((1UL<<24) | (1UL<<26) | (1UL<<28) | (1UL<<30));
}

void USART1_Init(void)
{
    /* Enable GPIOA + USART1 clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9(TX) + PA10(RX) → AF7 */
    GPIOA->MODER &= ~((3UL << 18) | (3UL << 20));
    GPIOA->MODER |=  ((2UL << 18) | (2UL << 20));
    GPIOA->AFR[1] |= (7UL << 4) | (7UL << 8);

    /* Baud 9600: APB2=84 MHz → BRR = 84M / (16*9600) ≈ 546.875 → 0x222E */
    USART1->BRR = 0x222E;

    /* Enable USART, TX, RX, RXNE interrupt */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE
                | USART_CR1_RXNEIE;

    NVIC_EnableIRQ(USART1_IRQn);
}

void SysTick_Init(void)
{
    /* 168 MHz / 1000 = 168000 ticks per 1 ms */
    SysTick->LOAD = 168000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE | SysTick_CTRL_TICKINT
                  | SysTick_CTRL_CLKSOURCE;
}
