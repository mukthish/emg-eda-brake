/**
 * @file system_shell.c
 * @brief System Shell — bootstrap lifecycle and deterministic scheduler tick.
 */

#include "../include/system_shell.h"
#include "../include/dispatcher.h"
#include "../include/hal.h"

/* We still include these strictly so we can call their _init() functions.
 * Once initialized, system_shell never interacts with them directly again. */
#include "../include/supervisor.h"
#include "../include/input_acq.h"
#include "../include/signal_proc.h"
#include "../include/safety_manager.h"
#include "../include/output_manager.h"

/* ── Internal state ─────────────────────────────────────────────────── */

static int boot_complete = 0;


void system_init(void)
{
    /* Platform hardware first */
    hal_init();

    /* Initialize the central event dispatcher */
    dispatcher_init();

    /* Initialise modules in dependency order (leaves → root) */
    output_init();
    input_init();
    signal_proc_init();
    safety_init();
    supervisor_init();

    boot_complete = 1;

    hal_log("[SYS] system_init complete — entering StartupSafe");
}

void system_run_tick(void)
{
    if (!boot_complete) return;

    /* ── 1. Generate the hardware heartbeat event ───────────────────── */
    DispatchEvent tick_ev;
    tick_ev.type = SYS_EVT_TICK_1MS;
    dispatcher_post(&tick_ev);

    /* ── 2. Process all events cascaded by this tick ────────────────── */
    dispatcher_run_once();
}

void system_reset(void)
{
    hal_log("[SYS] system_reset — reinitialising all modules");

    boot_complete = 0;

    dispatcher_init();
    output_init();
    input_init();
    signal_proc_init();
    safety_init();
    supervisor_init();

    boot_complete = 1;
}