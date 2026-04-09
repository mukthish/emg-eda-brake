/**
 * @file supervisor.c
 * @brief Supervisor / State Manager — owns system state and validates transitions.
 *
 * Owner: Mukthish
 *
 * Implements the full UML statechart from docs/uml-statecharts.md:
 *
 *   StartupSafe ──valid_data──► Idle
 *   StartupSafe ──no_data_timeout──► SoftFault
 *
 *   Idle ──emg_above_thresh──► IntentPending
 *   Idle ──data_missing/corrupt──► SoftFault
 *   Idle ──sensor_fault──► HardFault
 *
 *   IntentPending ──sustained_emg──► IntentConfirmed
 *   IntentPending ──emg_drop──► Idle
 *   IntentPending ──invalid_data──► SoftFault
 *   IntentPending ──sensor_fault──► HardFault
 *
 *   IntentConfirmed ──low_emg──► Recovery
 *   IntentConfirmed ──sensor_fault──► HardFault
 *   IntentConfirmed ──valid_data_continues──► Idle   (via Recovery)
 *   IntentConfirmed ──missing/invalid_data──► SoftFault
 *
 *   Recovery ──recovery_complete──► Idle
 *   Recovery ──emg_rise──► IntentPending
 *   Recovery ──invalid_data──► SoftFault
 *   Recovery ──sensor_fault──► HardFault
 *
 *   SoftFault ──data_stable──► Idle
 *   SoftFault ──watchdog_timeout──► StartupSafe
 *
 *   HardFault ──manual_reset──► StartupSafe
 */

#include "supervisor.h"
#include "output_manager.h"
#include "hal.h"

#include <stdio.h>
#include <string.h>

/* ── Internal state ─────────────────────────────────────────────────── */

static SystemState  current_state;
static uint32_t     last_transition_ts;
static uint32_t     soft_fault_entry_ts;

/** Watchdog timeout for SoftFault → StartupSafe (ms) */
#define SOFT_FAULT_WATCHDOG_MS  10000

/* ── Name tables ────────────────────────────────────────────────────── */

static const char *state_names[] = {
    "StartupSafe", "Idle", "IntentPending",
    "IntentConfirmed", "Recovery", "SoftFault", "HardFault"
};

static const char *event_names[] = {
    "ValidData", "EmgAboveThresh", "EmgSustained",
    "EmgDrop", "LowEmg", "RecoveryComplete", "EmgRise",
    "DataMissing", "DataInvalid", "SensorFault",
    "DataStable", "WatchdogTimeout", "ManualReset"
};

const char *supervisor_state_name(SystemState s)
{
    if (s >= 0 && s <= STATE_HARD_FAULT) return state_names[s];
    return "Unknown";
}

const char *supervisor_event_name(SystemEvent e)
{
    if (e >= 0 && e <= EVT_MANUAL_RESET) return event_names[e];
    return "Unknown";
}

/* ── Transition helpers ─────────────────────────────────────────────── */

static void enter_state(SystemState new_state)
{
    SystemState old_state = current_state;
    current_state = new_state;
    last_transition_ts = hal_get_tick_ms();

    /* Log transition */
    char msg[128];
    snprintf(msg, sizeof(msg), "[SUP] %s -> %s",
             supervisor_state_name(old_state),
             supervisor_state_name(new_state));
    hal_log(msg);

    /* Entry actions */
    switch (new_state) {
    case STATE_INTENT_CONFIRMED:
        output_set_brake_request(1);
        output_log_event("SUP", EVT_EMG_SUSTAINED);
        hal_led_set(2, 1);  /* Red LED = brake active */
        break;

    case STATE_RECOVERY:
        output_set_brake_request(0);
        output_log_event("SUP", EVT_LOW_EMG);
        hal_led_set(2, 0);
        hal_led_set(3, 1);  /* Blue LED = recovery */
        break;

    case STATE_IDLE:
        output_set_brake_request(0);
        hal_led_set(0, 1);  /* Green LED = monitoring */
        hal_led_set(1, 0);
        hal_led_set(2, 0);
        hal_led_set(3, 0);
        break;

    case STATE_SOFT_FAULT:
        output_set_brake_request(0);
        soft_fault_entry_ts = hal_get_tick_ms();
        output_log_event("SUP", EVT_DATA_MISSING);
        hal_led_set(1, 1);  /* Orange LED = fault */
        hal_led_set(0, 0);
        hal_led_set(2, 0);
        break;

    case STATE_HARD_FAULT:
        output_set_brake_request(0);
        output_log_event("SUP", EVT_SENSOR_FAULT);
        /* All LEDs blink pattern handled elsewhere */
        hal_led_set(1, 1);
        hal_led_set(2, 1);
        break;

    case STATE_STARTUP_SAFE:
        output_set_brake_request(0);
        hal_led_set(0, 0);
        hal_led_set(1, 0);
        hal_led_set(2, 0);
        hal_led_set(3, 0);
        break;

    case STATE_INTENT_PENDING:
        output_log_event("SUP", EVT_EMG_ABOVE_THRESH);
        hal_led_set(0, 1);
        hal_led_set(3, 1);  /* Green + Blue = pending */
        break;
    }
}

/* ── Transition table ───────────────────────────────────────────────── */

static void handle_startup_safe(SystemEvent evt)
{
    switch (evt) {
    case EVT_VALID_DATA:      enter_state(STATE_IDLE);       break;
    case EVT_DATA_MISSING:    enter_state(STATE_SOFT_FAULT); break;
    default: break;
    }
}

static void handle_idle(SystemEvent evt)
{
    switch (evt) {
    case EVT_EMG_ABOVE_THRESH: enter_state(STATE_INTENT_PENDING); break;
    case EVT_DATA_MISSING:
    case EVT_DATA_INVALID:     enter_state(STATE_SOFT_FAULT);     break;
    case EVT_SENSOR_FAULT:     enter_state(STATE_HARD_FAULT);     break;
    default: break;
    }
}

static void handle_intent_pending(SystemEvent evt)
{
    switch (evt) {
    case EVT_EMG_SUSTAINED:  enter_state(STATE_INTENT_CONFIRMED); break;
    case EVT_EMG_DROP:       enter_state(STATE_IDLE);             break;
    case EVT_DATA_MISSING:
    case EVT_DATA_INVALID:   enter_state(STATE_SOFT_FAULT);       break;
    case EVT_SENSOR_FAULT:   enter_state(STATE_HARD_FAULT);       break;
    default: break;
    }
}

static void handle_intent_confirmed(SystemEvent evt)
{
    switch (evt) {
    case EVT_LOW_EMG:        enter_state(STATE_RECOVERY);    break;
    case EVT_SENSOR_FAULT:   enter_state(STATE_HARD_FAULT);  break;
    case EVT_DATA_MISSING:
    case EVT_DATA_INVALID:   enter_state(STATE_SOFT_FAULT);  break;
    default: break;
    }
}

static void handle_recovery(SystemEvent evt)
{
    switch (evt) {
    case EVT_RECOVERY_COMPLETE: enter_state(STATE_IDLE);            break;
    case EVT_EMG_RISE:          enter_state(STATE_INTENT_PENDING);  break;
    case EVT_DATA_MISSING:
    case EVT_DATA_INVALID:      enter_state(STATE_SOFT_FAULT);      break;
    case EVT_SENSOR_FAULT:      enter_state(STATE_HARD_FAULT);      break;
    default: break;
    }
}

static void handle_soft_fault(SystemEvent evt)
{
    switch (evt) {
    case EVT_DATA_STABLE:      enter_state(STATE_IDLE);         break;
    case EVT_WATCHDOG_TIMEOUT: enter_state(STATE_STARTUP_SAFE); break;
    case EVT_SENSOR_FAULT:     enter_state(STATE_HARD_FAULT);   break;
    default: break;
    }
}

static void handle_hard_fault(SystemEvent evt)
{
    switch (evt) {
    case EVT_MANUAL_RESET: enter_state(STATE_STARTUP_SAFE); break;
    default: break;
    }
}

/* ── Public interface ───────────────────────────────────────────────── */

void supervisor_init(void)
{
    current_state      = STATE_STARTUP_SAFE;
    last_transition_ts = 0;
    soft_fault_entry_ts = 0;
    hal_log("[SUP] supervisor_init — state = StartupSafe");
}

void supervisor_post_event(SystemEvent evt)
{
    /* Watchdog check for SoftFault */
    if (current_state == STATE_SOFT_FAULT) {
        uint32_t elapsed = hal_get_tick_ms() - soft_fault_entry_ts;
        if (elapsed >= SOFT_FAULT_WATCHDOG_MS) {
            enter_state(STATE_STARTUP_SAFE);
            return;
        }
    }

    switch (current_state) {
    case STATE_STARTUP_SAFE:    handle_startup_safe(evt);     break;
    case STATE_IDLE:            handle_idle(evt);             break;
    case STATE_INTENT_PENDING:  handle_intent_pending(evt);   break;
    case STATE_INTENT_CONFIRMED:handle_intent_confirmed(evt); break;
    case STATE_RECOVERY:        handle_recovery(evt);         break;
    case STATE_SOFT_FAULT:      handle_soft_fault(evt);       break;
    case STATE_HARD_FAULT:      handle_hard_fault(evt);       break;
    }
}

SystemState supervisor_get_state(void)
{
    return current_state;
}

void supervisor_force_fault_hold(int reason)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "[SUP] forced HardFault (reason=%d)", reason);
    hal_log(msg);
    enter_state(STATE_HARD_FAULT);
}
