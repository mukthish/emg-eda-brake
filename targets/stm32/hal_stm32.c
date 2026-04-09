/**
 * @file targets/stm32/hal_stm32.c
 * @brief Real STM32F4 HAL implementation.
 *
 * UART: reads from ISR-driven ring buffer (in main.c)
 * GPIO: PD12 = brake output, PD13-PD15 = LED indicators
 * Timing: SysTick-based millisecond counter
 */

#include "hal.h"
#include "stm32f4xx.h"

#include <stdio.h>
#include <string.h>

/* ── External: UART RX ring from main.c ─────────────────────────────── */

extern int     stm32_uart_rx_available(void);
extern uint8_t stm32_uart_rx_read(void);

/* ── Tick counter (incremented by SysTick_Handler) ──────────────────── */

static volatile uint32_t tick_ms = 0;

/* Called from SysTick_Handler indirectly via system_run_tick timing */
void hal_systick_increment(void)
{
    tick_ms++;
}

/* ── GPIO pin mapping ───────────────────────────────────────────────── */

/*
 * PD12 = Green  LED / Brake output
 * PD13 = Orange LED
 * PD14 = Red    LED
 * PD15 = Blue   LED
 */
static const uint32_t led_pins[] = {
    (1UL << 12),  /* LED 0 = PD12 Green  */
    (1UL << 13),  /* LED 1 = PD13 Orange */
    (1UL << 14),  /* LED 2 = PD14 Red    */
    (1UL << 15),  /* LED 3 = PD15 Blue   */
};

/* ── Frame accumulator ──────────────────────────────────────────────── */

#define ACCUM_SIZE  256
static char accum_buf[ACCUM_SIZE];
static int  accum_len = 0;

/* ── Public interface ───────────────────────────────────────────────── */

void hal_init(void)
{
    /*
     * Hardware init (clocks, GPIO, UART, SysTick) is done in main.c
     * before system_init() is called.  Nothing extra needed here.
     */
    tick_ms = 0;
    accum_len = 0;
}

int hal_uart_read_frame(char *buf, int max_len)
{
    /* Drain UART RX ring into accumulator */
    while (stm32_uart_rx_available() && accum_len < ACCUM_SIZE - 1) {
        accum_buf[accum_len++] = (char)stm32_uart_rx_read();
    }

    /* Look for complete frame: $...* */
    char *start = NULL;
    for (int i = 0; i < accum_len; i++) {
        if (accum_buf[i] == '$') { start = &accum_buf[i]; break; }
    }
    if (!start) {
        accum_len = 0;
        return 0;
    }

    /* Move start to front */
    if (start != accum_buf) {
        int discard = (int)(start - accum_buf);
        memmove(accum_buf, start, (size_t)(accum_len - discard));
        accum_len -= discard;
    }

    /* Find end delimiter */
    char *end = NULL;
    for (int i = 0; i < accum_len; i++) {
        if (accum_buf[i] == '*' || accum_buf[i] == '\n') {
            end = &accum_buf[i];
            break;
        }
    }
    if (!end) return 0;

    int frame_len = (int)(end - accum_buf) + 1;
    if (frame_len >= max_len) frame_len = max_len - 1;

    memcpy(buf, accum_buf, (size_t)frame_len);
    buf[frame_len] = '\0';

    int consumed = (int)(end - accum_buf) + 1;
    memmove(accum_buf, accum_buf + consumed, (size_t)(accum_len - consumed));
    accum_len -= consumed;

    return frame_len;
}

void hal_gpio_assert_brake(void)
{
    /* Assert PD12 (Green LED / brake line) */
    GPIOD->BSRR = (1UL << 12);
}

void hal_gpio_clear_brake(void)
{
    /* De-assert PD12 */
    GPIOD->BSRR = (1UL << 12) << 16;
}

void hal_led_set(int led_id, int on)
{
    if (led_id < 0 || led_id > 3) return;

    if (on) {
        GPIOD->BSRR = led_pins[led_id];
    } else {
        GPIOD->BSRR = led_pins[led_id] << 16;
    }
}

void hal_delay_ms(uint32_t ms)
{
    uint32_t start = tick_ms;
    while ((tick_ms - start) < ms);
}

uint32_t hal_get_tick_ms(void)
{
    return tick_ms;
}

void hal_log(const char *msg)
{
    /*
     * On STM32, log output goes to USART1 TX (or SWO if configured).
     * Simple polled TX for diagnostics — not performance-critical.
     */
    while (*msg) {
        while (!(USART1->SR & USART_SR_TXE));
        USART1->DR = (uint32_t)(*msg++ & 0xFF);
    }
    /* Newline */
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = '\n';
}
