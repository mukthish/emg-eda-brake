/**
 * @file signal_proc.h
 * @brief Signal Processing + Feature Engine — filter, window, and compute features.
 *
 * Owner: Mahesh
 *
 * Signal processing pipeline (from Ju et al. 2021):
 * 1. Bandpass filter    : 4th-order Butterworth, 15–90 Hz
 * 2. Linear envelope    : rectification + 2nd-order Butterworth LPF @ 2 Hz
 * 3. Windowed RMS       : 1-second sliding window
 * 4. Rate of change     : derivative of successive RMS values
 * 5. Baseline tracking  : EWMA of quiet-state EMG
 */

#ifndef SIGNAL_PROC_H
#define SIGNAL_PROC_H

#include "input_acq.h"

/* ── Configuration constants (from research paper) ──────────────────── */

#define SP_WINDOW_SIZE      20    /**< 1 s window at 20 Hz sim rate       */
#define SP_STEP_MS          60    /**< Evaluation step (paper: 60 ms)     */
#define SP_BASELINE_ALPHA   0.01f /**< EWMA smoothing for baseline model  */

/* ── Feature vector output ──────────────────────────────────────────── */

typedef struct {
    float emg_rms;              /**< RMS of EMG envelope over window          */
    float emg_rate_of_change;   /**< Δ RMS between successive evaluations     */
    float eda_level;            /**< Smoothed EDA (contextual, not primary)   */
    int   quality;              /**< Signal quality estimate  0–100           */
} FeatureVector;

/* ── Interface ──────────────────────────────────────────────────────── */

/** Initialise filter state, window buffer, and baseline model. */
void signal_proc_init(void);

/**
 * Handle a single parsed sample frame.
 * Applies DSP filters, updates the sliding window, and if the window is full,
 * calculates features and pushes a SYS_EVT_FEATURES_READY event to the dispatcher.
 * @param frame  pointer to the newly parsed SampleFrame
 */
void signal_proc_handle_event(const SampleFrame *frame);

/** Reset the baseline model (e.g. on state transition to Idle). */
void signal_proc_reset_baseline(void);

/** Expose baseline for dynamic threshold calculation in safety_manager. */
float signal_proc_get_baseline_rms(void);

#endif /* SIGNAL_PROC_H */