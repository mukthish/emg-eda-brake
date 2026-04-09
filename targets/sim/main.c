/**
 * @file targets/sim/main.c
 * @brief PC Simulation entry point.
 *
 * Runs the deterministic 1 ms tick using usleep().
 * Reads UART data from stdin (pipe from stream_simulator.py).
 *
 * Usage:
 *   python3 tools/stream_simulator.py | ./build/sim_emg_brake
 *   Or:
 *   ./build/sim_emg_brake < test_data.txt
 */

#include "system_shell.h"
#include "hal.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static volatile int running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    running = 0;
}

int main(void)
{
    /* Install SIGINT handler for clean shutdown */
    signal(SIGINT, sigint_handler);

    printf("=== EMG-EDA Brake Intent Detection — PC Simulator ===\n");
    printf("Feed UART frames via stdin (pipe or redirect).\n");
    printf("Press Ctrl+C to stop.\n\n");

    system_init();

    while (running) {
        system_run_tick();
        usleep(1000);  /* 1 ms tick */
    }

    printf("\n=== Simulator stopped ===\n");
    system_reset();

    return 0;
}
