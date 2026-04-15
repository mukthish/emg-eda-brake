/**
 * @file output_manager.c
 * @brief Output + Observability Manager — brake coordination and immutable logging.
 *
 * Owner: Abhishek
 */

#include "../include/output_manager.h"
#include "../include/hal.h"
/* supervisor.h is implicitly included via output_manager.h, but we can 
 * explicitly include it here if your build system prefers explicit paths */
#include "../include/supervisor.h"

#include <stdio.h>
#include <string.h>

/* ── Internal state ─────────────────────────────────────────────────── */

static int      brake_active;
static uint32_t seq_counter;

/* Immutable log ring buffer */
static LogEntry log_ring[LOG_RING_SIZE];
static int      log_head;
static int      log_tail;

/* ── Ring buffer helpers ────────────────────────────────────────────── */

static void log_push(const LogEntry *entry)
{
    log_ring[log_head] = *entry;
    log_head = (log_head + 1) & (LOG_RING_SIZE - 1);
    if (log_head == log_tail) {
        /* Overwrite oldest entry */
        log_tail = (log_tail + 1) & (LOG_RING_SIZE - 1);
    }
}

static int log_pop(LogEntry *entry)
{
    if (log_head == log_tail) return 0;
    *entry = log_ring[log_tail];
    log_tail = (log_tail + 1) & (LOG_RING_SIZE - 1);
    return 1;
}

/* ── Public interface ───────────────────────────────────────────────── */

void output_init(void)
{
    brake_active = 0;
    seq_counter  = 0;
    log_head     = 0;
    log_tail     = 0;
    memset(log_ring, 0, sizeof(log_ring));

    hal_gpio_clear_brake();
}

void output_set_brake_request(int assert_brake)
{
    if (assert_brake && !brake_active) {
        hal_gpio_assert_brake();
        brake_active = 1;
        hal_log("[OUT] *** BRAKE ASSERTED ***");
    } else if (!assert_brake && brake_active) {
        hal_gpio_clear_brake();
        brake_active = 0;
        hal_log("[OUT] brake de-asserted");
    }
}

void output_log_event(const char *tag, SystemEvent evt)
{
    LogEntry entry;
    entry.seq       = seq_counter++;
    entry.timestamp = hal_get_tick_ms();
    
    /* Observability is allowed to query the global state getter */
    entry.state     = supervisor_get_state();
    entry.event     = evt;
    entry.emg_rms   = 0.0f;  /* Will be filled by caller if needed */
    entry.brake     = brake_active;

    log_push(&entry);

    (void)tag;  /* Tag is for debug context; stored implicitly via event */
}

void output_flush_logs(void)
{
    LogEntry entry;
    char msg[192];

    while (log_pop(&entry)) {
        snprintf(msg, sizeof(msg),
                 "[LOG] seq=%u t=%u state=%s evt=%s brake=%d",
                 (unsigned)entry.seq,
                 (unsigned)entry.timestamp,
                 supervisor_state_name(entry.state),
                 supervisor_event_name(entry.event),
                 entry.brake);
        hal_log(msg);
    }
}

int output_get_brake_state(void)
{
    return brake_active;
}