/**
 * @file system_shell.h
 * @brief System Shell — bootstrap lifecycle and deterministic scheduler tick.
 *
 * The System Shell is the outermost software layer. It owns:
 * - boot_complete flag
 * - the 1 ms deterministic tick
 *
 * In the Event-Driven Architecture, it no longer micromanages the pipeline.
 * Instead, it generates a 1ms heartbeat event and commands the dispatcher
 * to process the event queue.
 */

#ifndef SYSTEM_SHELL_H
#define SYSTEM_SHELL_H

/**
 * One-time system initialisation.
 * Calls hal_init(), dispatcher_init(), and then initialises every software module.
 */
void system_init(void);

/**
 * Deterministic scheduler tick — called every 1 ms from the platform main loop
 * or SysTick ISR.
 *
 * Pushes a SYS_EVT_TICK_1MS event to the dispatcher and triggers queue evaluation.
 */
void system_run_tick(void);

/**
 * Full system reset — reinitialises all module state.
 */
void system_reset(void);

#endif /* SYSTEM_SHELL_H */