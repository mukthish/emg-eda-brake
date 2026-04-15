/**
 * @file safety_manager.h
 * @brief Intent + Safety Manager — evaluate confidence and apply safety gating.
 *
 * Owner: Bhagavath
 *
 * Responsibilities:
 * - Compare EMG RMS against dynamic threshold (baseline × 2.5)
 * - Track confidence counter (3 consecutive above-threshold windows)
 * - Push intent events to the central Dispatcher
 * - Enforce cooldown (2.0 s) to prevent oscillatory triggers
 * - Latch fault on sustained low-quality data
 */

#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include "signal_proc.h"
#include "supervisor.h"

/* ── Configuration constants ────────────────────────────────────────── */

#define SAFETY_THRESH_MULT       2.5f   /**< RMS threshold = baseline × this  */
#define SAFETY_CONFIRM_COUNT     3      /**< Consecutive above-threshold wins  */
#define SAFETY_COOLDOWN_MS       2000   /**< Cooldown after brake de-assert    */
#define SAFETY_FAULT_QUALITY_MIN 30     /**< Link quality below this → fault   */

/* ── Interface ──────────────────────────────────────────────────────── */

/** Initialise confidence counter, cooldown timer, fault latch. */
void safety_init(void);

/**
 * Handle a newly extracted FeatureVector.
 * Evaluates thresholds and link quality. If an intent transition or
 * fault condition occurs, a SYS_EVT_INTENT_STATE is posted to the dispatcher.
 * @param features pointer to current FeatureVector (includes quality metric)
 */
void safety_handle_event(const FeatureVector *features);

/**
 * Report a fault from an external module.
 * @param code  fault reason code (logged via output_manager)
 */
void safety_report_fault(int code);

#endif /* SAFETY_MANAGER_H */