/**
 * @file hal.h
 * @brief Hardware Abstraction Layer — platform-agnostic interface.
 *
 * Every function declared here is implemented once per platform target:
 *   targets/sim/hal_sim.c   — PC simulation (stdin / printf)
 *   targets/stm32/hal_stm32.c — Real STM32F4 hardware
 *
 * NO hardware-specific code is allowed in this header.
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>

/* ── Platform bootstrap ─────────────────────────────────────────────── */

/** Initialise all platform-specific hardware (clocks, GPIO, UART, etc.) */
void hal_init(void);

/* ── UART / data link ───────────────────────────────────────────────── */

/**
 * Non-blocking read of one UART frame into @p buf.
 * @param buf      destination buffer (caller-owned)
 * @param max_len  maximum bytes to write into buf (including '\0')
 * @return         number of bytes placed in buf, 0 if nothing available
 */
int hal_uart_read_frame(char *buf, int max_len);

/* ── Brake GPIO ─────────────────────────────────────────────────────── */

/** Assert the emergency brake request line (active-high). */
void hal_gpio_assert_brake(void);

/** De-assert the emergency brake request line. */
void hal_gpio_clear_brake(void);

/* ── LED indicators ─────────────────────────────────────────────────── */

/**
 * Set an onboard LED state.
 * @param led_id  0-3 (maps to PD12-PD15 on STM32F4 Discovery)
 * @param on      non-zero = ON, 0 = OFF
 */
void hal_led_set(int led_id, int on);

/* ── Timing ─────────────────────────────────────────────────────────── */

/** Blocking delay for @p ms milliseconds. */
void hal_delay_ms(uint32_t ms);

/** Monotonic millisecond tick counter (wraps at UINT32_MAX). */
uint32_t hal_get_tick_ms(void);

/* ── Logging / observability ────────────────────────────────────────── */

/** Emit a diagnostic string (platform routes to stderr / SWO / etc.) */
void hal_log(const char *msg);

#endif /* HAL_H */
