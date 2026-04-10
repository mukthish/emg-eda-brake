/**
 * @file safety_manager.c
 * @brief Intent + Safety Manager — threshold evaluation, confidence, fault gating.
 *
 * Owner: Bhagavath
 *
 * Detection logic (adapted from Ju et al. 2021):
 *   - EMG RMS threshold   = baseline_rms × 2.5
 *   - Confidence counter   : 3 consecutive above-threshold evaluations
 *   - Cooldown timer       : 2.0 s after brake de-assert
 *   - Fault latch          : triggered when link quality < 30
 */

/*
 * Fault detection → blocks system on bad link quality (highest priority)
 * Fault recovery  → restores system after stable signal frames
 * Cooldown        → prevents rapid re-trigger using time delay
 * Threshold       → computes adaptive EMG limit from baseline
 * EMG above       → detects intent using consecutive confirmations
 * EMG below       → detects signal drop and triggers recovery
 */

/* confidence_counter → counts consecutive EMG-above-threshold frames; used to confirm intent (>=3 = sustained) */

/* fault_latched → indicates system is in fault state (e.g., bad link); blocks all detection until stable recovery */

/* cooldown_start → stores system tick when cooldown begins; used to measure cooldown duration */

/* in_cooldown → flag indicating temporary blocking of detection after EMG drop to prevent rapid re-trigger */

/* last_event → latest system event:
 *   EVT_VALID_DATA       → normal state (no issue)
 *   EVT_DATA_MISSING     → fault due to poor link quality
 *   EVT_DATA_STABLE      → recovery from fault after stable frames
 *   EVT_SENSOR_FAULT     → externally reported fault
 *   EVT_EMG_ABOVE_THRESH → first EMG detection above threshold
 *   EVT_EMG_SUSTAINED    → confirmed intent (multiple consecutive detections)
 *   EVT_EMG_DROP         → EMG fell before confirmation (noise/false trigger)
 *   EVT_LOW_EMG          → EMG dropped after confirmed intent (enter cooldown)
 *   EVT_RECOVERY_COMPLETE→ cooldown finished, system ready again
 *   EVT_EMG_RISE         → EMG rises during cooldown (early re-trigger)
 */

/* recovery_ticks → counts duration of low EMG during recovery phase to ensure stable return to idle */
/* RECOVERY_TICKS_REQUIRED → required low-EMG ticks (~2 sec) to confirm recovery completion */
/* stable_counter → counts consecutive good frames after fault to ensure stable signal before exiting fault */
/* STABLE_FRAMES_REQUIRED → minimum stable frames needed to clear fault and resume normal operation */



// CODE
#include "safety_manager.h"
#include "output_manager.h"
#include "hal.h"

#include <stdio.h>

/* ── External: baseline from signal_proc ────────────────────────────── */

extern float signal_proc_get_baseline_rms(void);

/* ── Internal state ─────────────────────────────────────────────────── */

static int          confidence_counter;   /**< Consecutive above-threshold counts */
static int          fault_latched;        /**< 1 = fault condition active         */
static uint32_t     cooldown_start;       /**< Tick when cooldown began           */
static int          in_cooldown;          /**< 1 = cooldown period active         */
static SystemEvent  last_event;           /**< Most recent generated event        */

/* Recovery tracking */
static int          recovery_ticks;       /**< Ticks of low EMG during recovery   */
#define RECOVERY_TICKS_REQUIRED  33       /**< ~2 s at 60 ms step cadence         */

/* Data stability tracking for SoftFault recovery */
static int          stable_counter;
#define STABLE_FRAMES_REQUIRED   5

/* ── Public interface ───────────────────────────────────────────────── */

void safety_init(void)
{
    confidence_counter = 0;
    fault_latched      = 0;
    cooldown_start     = 0;
    in_cooldown        = 0;
    last_event         = EVT_VALID_DATA;
    recovery_ticks     = 0;
    stable_counter     = 0;
}

void safety_evaluate(const FeatureVector *features, int link_quality)
{
    /* Default: no event */
    last_event = EVT_VALID_DATA;

    uint32_t now = hal_get_tick_ms();

    /* ── Fault gating ───────────────────────────────────────────────── */
    if (link_quality < SAFETY_FAULT_QUALITY_MIN) {
        fault_latched = 1;
        confidence_counter = 0;
        stable_counter = 0;
        last_event = EVT_DATA_MISSING;
        return;
    }

    /* Track data stability for SoftFault → Idle recovery */
    if (fault_latched) {
        stable_counter++;
        if (stable_counter >= STABLE_FRAMES_REQUIRED) {
            fault_latched = 0;
            stable_counter = 0;
            last_event = EVT_DATA_STABLE;
            return;
        }
        return;  /* Stay in fault until stable */
    }

    /* ── Cooldown gate ──────────────────────────────────────────────── */
    if (in_cooldown) {
        if ((now - cooldown_start) >= SAFETY_COOLDOWN_MS) {
            in_cooldown = 0;
            last_event = EVT_RECOVERY_COMPLETE;
            recovery_ticks = 0;
        } else {
            /* Check if EMG is rising during recovery/cooldown */
            float baseline = signal_proc_get_baseline_rms();
            float threshold = baseline * SAFETY_THRESH_MULT;
            if (features->emg_rms > threshold) {
                in_cooldown = 0;
                confidence_counter = 1;
                last_event = EVT_EMG_RISE;
                recovery_ticks = 0;
            }
        }
        return;
    }

    /* ── Threshold evaluation ───────────────────────────────────────── */
    float baseline  = signal_proc_get_baseline_rms();
    float threshold = baseline * SAFETY_THRESH_MULT;

    /* Ensure minimum threshold to avoid false triggers with zero baseline */
    if (threshold < 0.01f) threshold = 0.01f;

    if (features->emg_rms > threshold) {
        confidence_counter++;

        if (confidence_counter == 1) {
            last_event = EVT_EMG_ABOVE_THRESH;
        } else if (confidence_counter >= SAFETY_CONFIRM_COUNT) {
            last_event = EVT_EMG_SUSTAINED;
        }
        /* else: stay in pending, no new event */

    } else {
        /* EMG below threshold */
        if (confidence_counter >= SAFETY_CONFIRM_COUNT) {
            /* Was confirmed → now dropping → enter recovery */
            last_event     = EVT_LOW_EMG;
            in_cooldown    = 1;
            cooldown_start = now;
            recovery_ticks = 0;
        } else if (confidence_counter > 0) {
            /* Was pending but dropped before confirmation */
            last_event = EVT_EMG_DROP;
        }
        confidence_counter = 0;
    }
}

SystemEvent safety_get_intent_event(void)
{
    return last_event;
}

void safety_report_fault(int code)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "[SAF] external fault reported (code=%d)", code);
    hal_log(msg);
    output_log_event("SAF", EVT_SENSOR_FAULT);
    fault_latched = 1;
}
