/**
 * @file targets/sim/hal_sim.c
 * @brief PC Simulation HAL — mock drivers using stdin/stdout/stderr.
 *
 * UART: reads from stdin (non-blocking via select/poll)
 * GPIO: printf messages for brake assert/clear
 * LEDs: printf messages
 * Timing: clock_gettime(CLOCK_MONOTONIC)
 */

#include "hal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

/* ── Timing base ────────────────────────────────────────────────────── */

static struct timespec boot_time;

/* ── LED state (for display) ────────────────────────────────────────── */

static int led_state[4] = {0, 0, 0, 0};
static const char *led_names[] = {"GREEN", "ORANGE", "RED", "BLUE"};

/* ── Frame accumulator ──────────────────────────────────────────────── */

#define ACCUM_SIZE  512
static char accum_buf[ACCUM_SIZE];
static int  accum_len = 0;

/* ── Public interface ───────────────────────────────────────────────── */

void hal_init(void)
{
    clock_gettime(CLOCK_MONOTONIC, &boot_time);

    /* Set stdin to non-blocking */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    fprintf(stderr, "[HAL-SIM] initialised (stdin=non-blocking)\n");
}

int hal_uart_read_frame(char *buf, int max_len)
{
    /* Read available bytes from stdin into accumulator */
    if (accum_len < ACCUM_SIZE - 1) {
        int space = ACCUM_SIZE - 1 - accum_len;

        /* Check if data is available via select (timeout = 0) */
        fd_set rfds;
        struct timeval tv = {0, 0};
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);

        int ready = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
        if (ready > 0) {
            int n = (int)read(STDIN_FILENO, accum_buf + accum_len, (size_t)space);
            if (n > 0) accum_len += n;
        }
    }

    /* Look for a complete frame: $...*  or  $...\n */
    char *start = memchr(accum_buf, '$', (size_t)accum_len);
    if (!start) {
        /* No frame start — discard everything */
        accum_len = 0;
        return 0;
    }

    /* Move start to beginning if needed */
    if (start != accum_buf) {
        int discard = (int)(start - accum_buf);
        memmove(accum_buf, start, (size_t)(accum_len - discard));
        accum_len -= discard;
    }

    /* Find frame end: '*' or '\n' */
    char *end_star = memchr(accum_buf, '*', (size_t)accum_len);
    char *end_nl   = memchr(accum_buf, '\n', (size_t)accum_len);

    char *end = NULL;
    if (end_star && end_nl) {
        end = (end_star < end_nl) ? end_star : end_nl;
    } else if (end_star) {
        end = end_star;
    } else if (end_nl) {
        end = end_nl;
    }

    if (!end) return 0;  /* Incomplete frame */

    /* Extract frame */
    int frame_len = (int)(end - accum_buf) + 1;
    if (frame_len >= max_len) frame_len = max_len - 1;

    memcpy(buf, accum_buf, (size_t)frame_len);
    buf[frame_len] = '\0';

    /* Remove frame from accumulator */
    int consumed = (int)(end - accum_buf) + 1;
    memmove(accum_buf, accum_buf + consumed, (size_t)(accum_len - consumed));
    accum_len -= consumed;

    return frame_len;
}

void hal_gpio_assert_brake(void)
{
    fprintf(stderr, "[HAL-SIM] >>> BRAKE GPIO ASSERTED <<<\n");
}

void hal_gpio_clear_brake(void)
{
    fprintf(stderr, "[HAL-SIM] brake GPIO cleared\n");
}

void hal_led_set(int led_id, int on)
{
    if (led_id < 0 || led_id > 3) return;
    if (led_state[led_id] == on) return;  /* No change */

    led_state[led_id] = on;
    fprintf(stderr, "[HAL-SIM] LED %s (%d) = %s\n",
            led_names[led_id], led_id, on ? "ON" : "OFF");
}

void hal_delay_ms(uint32_t ms)
{
    usleep(ms * 1000);
}

uint32_t hal_get_tick_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint32_t sec_diff  = (uint32_t)(now.tv_sec  - boot_time.tv_sec);
    int32_t  nsec_diff = (int32_t)(now.tv_nsec - boot_time.tv_nsec);

    return sec_diff * 1000 + (uint32_t)(nsec_diff / 1000000);
}

void hal_log(const char *msg)
{
    uint32_t t = hal_get_tick_ms();
    fprintf(stderr, "[%8u] %s\n", (unsigned)t, msg);
}
