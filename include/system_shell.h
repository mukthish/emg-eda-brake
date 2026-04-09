/**
 * @file system_shell.h
 * @brief System Shell — bootstrap lifecycle and deterministic scheduler tick.
 *
 * Owner: Team
 *
 * The System Shell is the outermost software layer.  It owns:
 *   - boot_complete flag
 *   - the 1 ms deterministic tick
 *
 * It delegates all control to the Supervisor after initialisation.
 */

#ifndef SYSTEM_SHELL_H
#define SYSTEM_SHELL_H

/**
 * One-time system initialisation.
 * Calls hal_init() then initialises every software module in dependency order.
 */
void system_init(void);

/**
 * Deterministic scheduler tick — called every 1 ms from the platform main loop
 * or SysTick ISR.
 *
 * Execution order:
 *   1. input_poll()          — acquire new UART/BLE samples
 *   2. signal_proc           — filter / feature extraction
 *   3. safety_evaluate       — intent + fault gating
 *   4. supervisor            — state transitions
 *   5. output                — brake line + logging
 */
void system_run_tick(void);

/**
 * Full system reset — reinitialises all module state.
 * Returns the state machine to StartupSafe.
 */
void system_reset(void);

#endif /* SYSTEM_SHELL_H */
