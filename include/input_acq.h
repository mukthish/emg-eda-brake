/**
 * @file input_acq.h
 * @brief Input Acquisition Manager — UART/BLE polling and normalisation.
 *
 * Owner: Devansh
 *
 * Responsibilities:
 * - Acquire UART (or BLE) samples
 * - Parse and normalise frames
 * - Track link health
 *
 * Frame format (UART simulation):
 * $EMG,EMG=0.72,EDA=0.31,TS=123456*
 */

#ifndef INPUT_ACQ_H
#define INPUT_ACQ_H

#include <stdint.h>

/* ── Data types ─────────────────────────────────────────────────────── */

/** Parsed EMG/EDA sample frame */
typedef struct {
    float    emg;        /**< Normalised EMG amplitude (0.0 – 1.0)  */
    float    eda;        /**< Normalised EDA level     (0.0 – 1.0)  */
    uint32_t timestamp;  /**< Sender timestamp (ms)                  */
    int      valid;      /**< 1 = successfully parsed, 0 = invalid  */
} SampleFrame;

/** Input source selector */
typedef enum {
    INPUT_SOURCE_UART,
    INPUT_SOURCE_BLE
} InputSource;

/* ── Interface ──────────────────────────────────────────────────────── */

/** Initialise the input subsystem (UART as default source). */
void input_init(void);

/**
 * Poll the active source for new data.
 * Call once per scheduler tick. Internally reads from HAL and
 * pushes valid frames to the central event dispatcher.
 */
void input_poll(void);

/**
 * Link health indicator.
 * @return 0–100  (100 = all recent frames valid, 0 = total link loss)
 */
int input_get_link_health(void);

/** Select the active input source (UART or BLE). */
void input_set_source(InputSource src);

#endif /* INPUT_ACQ_H */