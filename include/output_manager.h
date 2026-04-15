/**
 * @file output_manager.h
 * @brief Output + Observability Manager — brake coordination and immutable logging.
 *
 * Owner: Abhishek
 *
 * Responsibilities:
 * - Drive brake request output via HAL GPIO
 * - Append immutable log entries (one-way, never influences control)
 * - Maintain sequence counter for log integrity
 *
 * In the Event-Driven Architecture, output_set_brake_request() and 
 * output_flush_logs() are called strictly by the central dispatcher.
 */

#ifndef OUTPUT_MANAGER_H
#define OUTPUT_MANAGER_H

#include <stdint.h>
#include "supervisor.h"

/* ── Log buffer configuration ───────────────────────────────────────── */

#define LOG_RING_SIZE  128   /**< Must be power-of-two */

/* ── Log entry (immutable once written) ─────────────────────────────── */

typedef struct {
    uint32_t    seq;        /**< Monotonic sequence number              */
    uint32_t    timestamp;  /**< hal_get_tick_ms() at time of event     */
    SystemState state;      /**< System state when event occurred       */
    SystemEvent event;      /**< The event being logged                 */
    float       emg_rms;    /**< EMG RMS at time of event               */
    int         brake;      /**< 1 = brake asserted, 0 = not            */
} LogEntry;

/* ── Interface ──────────────────────────────────────────────────────── */

/** Initialise brake line state, log buffer, and sequence counter. */
void output_init(void);

/**
 * Assert or de-assert the brake request.
 * Handled via the SYS_EVT_CMD_BRAKE event in the dispatcher.
 * @param assert_brake  non-zero = assert, 0 = clear
 */
void output_set_brake_request(int assert_brake);

/**
 * Append an immutable log entry.
 * @param tag   short descriptive tag (e.g. module name)
 * @param evt   the event to record
 */
void output_log_event(const char *tag, SystemEvent evt);

/**
 * Flush buffered log entries to the platform output (hal_log).
 * Triggered periodically by the SYS_EVT_TICK_1MS heartbeat.
 */
void output_flush_logs(void);

/** Return 1 if the brake line is currently asserted, 0 otherwise. */
int output_get_brake_state(void);

#endif /* OUTPUT_MANAGER_H */