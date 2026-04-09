/**
 * @file system_shell.c
 * @brief System Shell — bootstrap lifecycle and deterministic scheduler tick.
 *
 * Execution order per tick:
 *   1. input_poll()
 *   2. signal_proc (if samples available)
 *   3. safety_evaluate (if features ready)
 *   4. supervisor (post intent event)
 *   5. output (flush logs)
 */

#include "system_shell.h"
#include "hal.h"
#include "supervisor.h"
#include "input_acq.h"
#include "signal_proc.h"
#include "safety_manager.h"
#include "output_manager.h"

/* ── Internal state ─────────────────────────────────────────────────── */

static int boot_complete = 0;

/* ── Step timer for 60 ms evaluation cadence ────────────────────────── */

static uint32_t last_eval_tick = 0;

/* ── Public functions ───────────────────────────────────────────────── */

void system_init(void)
{
    /* Platform hardware first */
    hal_init();

    /* Initialise modules in dependency order (leaves → root) */
    output_init();
    input_init();
    signal_proc_init();
    safety_init();
    supervisor_init();

    boot_complete = 1;
    last_eval_tick = hal_get_tick_ms();

    hal_log("[SYS] system_init complete — entering StartupSafe");
}

void system_run_tick(void)
{
    if (!boot_complete) return;

    /* ── 1. Acquire new samples ─────────────────────────────────────── */
    input_poll();

    /* ── 2. Feed samples into signal processing ─────────────────────── */
    SampleFrame frame;
    if (input_fetch_samples(&frame)) {
        signal_proc_process_batch(&frame, 1);

        /* If we are in StartupSafe and got valid data, transition to Idle */
        if (frame.valid && supervisor_get_state() == STATE_STARTUP_SAFE) {
            supervisor_post_event(EVT_VALID_DATA);
        }
    }

    /* ── 3. Evaluate intent at 60 ms cadence ────────────────────────── */
    uint32_t now = hal_get_tick_ms();
    if ((now - last_eval_tick) >= SP_STEP_MS) {
        last_eval_tick = now;

        FeatureVector feat;
        int link_health = input_get_link_health();

        if (signal_proc_get_features(&feat)) {
            /* Run safety evaluation */
            safety_evaluate(&feat, link_health);

            /* ── 4. Post the resulting event to the supervisor ───────── */
            SystemEvent evt = safety_get_intent_event();
            if (evt != EVT_VALID_DATA) {   /* EVT_VALID_DATA = no-op sentinel */
                supervisor_post_event(evt);
            }
        }

        /* Check link health independently */
        if (link_health == 0) {
            supervisor_post_event(EVT_DATA_MISSING);
        }
    }

    /* ── 5. Flush logs ──────────────────────────────────────────────── */
    output_flush_logs();
}

void system_reset(void)
{
    hal_log("[SYS] system_reset — reinitialising all modules");

    boot_complete = 0;

    output_init();
    input_init();
    signal_proc_init();
    safety_init();
    supervisor_init();

    boot_complete = 1;
    last_eval_tick = hal_get_tick_ms();
}
