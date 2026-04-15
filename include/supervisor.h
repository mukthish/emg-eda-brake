/**
 * @file supervisor.h
 * @brief Supervisor / State Manager — owns global state and validates transitions.
 *
 * Owner: Mukthish
 *
 * The Supervisor is the single authority for CurrentState and the TransitionTable.
 * In the Event-Driven Architecture, it acts as a consumer of SYS_EVT_INTENT_STATE
 * events and a producer of SYS_EVT_CMD_BRAKE events.
 */

#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stdint.h>

/* ── System states (from uml-statecharts.md) ────────────────────────── */

typedef enum {
    STATE_STARTUP_SAFE,     /**< Initial safe state after boot / watchdog reset  */
    STATE_IDLE,             /**< Normal monitoring — valid data, no intent       */
    STATE_INTENT_PENDING,   /**< EMG above threshold — awaiting confirmation     */
    STATE_INTENT_CONFIRMED, /**< Sustained EMG — brake request ASSERTED          */
    STATE_RECOVERY,         /**< Low EMG after confirmation — cooldown period    */
    STATE_SOFT_FAULT,       /**< Data missing / corrupt — recoverable            */
    STATE_HARD_FAULT        /**< Sensor fault — requires manual intervention     */
} SystemState;

/* ── System events that drive transitions ───────────────────────────── */

typedef enum {
    EVT_VALID_DATA,         /**< First valid frame received (StartupSafe→Idle)   */
    EVT_EMG_ABOVE_THRESH,   /**< EMG RMS exceeds baseline × 2.5                 */
    EVT_EMG_SUSTAINED,      /**< 3 consecutive windows above threshold           */
    EVT_EMG_DROP,           /**< EMG fell below threshold during pending          */
    EVT_LOW_EMG,            /**< EMG low after confirmation — begin recovery     */
    EVT_RECOVERY_COMPLETE,  /**< Cooldown elapsed — safe to return to Idle       */
    EVT_EMG_RISE,           /**< EMG rising again during recovery                */
    EVT_DATA_MISSING,       /**< Frame timeout (>500 ms with no valid data)      */
    EVT_DATA_INVALID,       /**< Malformed / out-of-range frame                  */
    EVT_SENSOR_FAULT,       /**< 20+ consecutive frame errors — hardware fault   */
    EVT_DATA_STABLE,        /**< 5 consecutive valid frames after soft fault     */
    EVT_WATCHDOG_TIMEOUT,   /**< Soft-fault watchdog expired                     */
    EVT_MANUAL_RESET        /**< Operator-initiated reset from HardFault         */
} SystemEvent;

/* ── Interface ──────────────────────────────────────────────────────── */

/** Initialise the supervisor (state = StartupSafe, clear counters). */
void supervisor_init(void);

/**
 * Handle intent transitions. Called by the dispatcher when a 
 * SYS_EVT_INTENT_STATE event is pulled from the queue.
 */
void supervisor_post_event(SystemEvent evt);

/** Return the current system state (read-only). */
SystemState supervisor_get_state(void);

/**
 * Force entry into HardFault with the given reason code.
 * Used for unrecoverable situations detected by any module.
 */
void supervisor_force_fault_hold(int reason);

/** Return a human-readable label for a state enum. */
const char *supervisor_state_name(SystemState s);

/** Return a human-readable label for an event enum. */
const char *supervisor_event_name(SystemEvent e);

#endif /* SUPERVISOR_H */