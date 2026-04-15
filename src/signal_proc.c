/**
 * @file signal_proc.c
 * @brief Signal Processing + Feature Engine — filter, window, compute features.
 *
 * Owner: Mahesh
 */

#include "../include/signal_proc.h"
#include "../include/hal.h"
#include "../include/dispatcher.h"

#include <math.h>
#include <string.h>

/* ── Internal state ─────────────────────────────────────────────────── */

/** Sliding window of EMG envelope values */
static float window_buf[SP_WINDOW_SIZE];
static int   window_idx;
static int   window_count;  /**< How many samples have been added total */

/** EDA smoothing (simple EWMA) */
static float eda_smooth;

/** Baseline model */
static float baseline_rms;
static int   baseline_primed;  /**< 1 once we have at least one full window */

/** Previous RMS for rate-of-change */
static float prev_rms;

/** Latest computed features */
static FeatureVector latest_feat;

/** DC removal (simple first-order high-pass) */
static float hp_prev_raw;
static float hp_prev_out;

/** Envelope low-pass (2nd-order Butterworth at 2 Hz) */
#define LPF_B0  0.0675f
#define LPF_B1  0.1349f
#define LPF_B2  0.0675f
#define LPF_A1 (-1.1430f)
#define LPF_A2  0.4128f

static float lpf_x1, lpf_x2;   /* past inputs  */
static float lpf_y1, lpf_y2;   /* past outputs */

/* ── Filter functions ───────────────────────────────────────────────── */

/** First-order DC-removal high-pass (alpha ≈ 0.95) */
static float highpass(float x)
{
    float y = 0.95f * hp_prev_out + x - hp_prev_raw;
    hp_prev_raw = x;
    hp_prev_out = y;
    return y;
}

/** Rectify (absolute value) */
static float rectify(float x)
{
    return (x < 0.0f) ? -x : x;
}

/** 2nd-order Butterworth low-pass envelope filter */
static float envelope_lpf(float x)
{
    float y = LPF_B0 * x + LPF_B1 * lpf_x1 + LPF_B2 * lpf_x2
            - LPF_A1 * lpf_y1 - LPF_A2 * lpf_y2;

    lpf_x2 = lpf_x1;  lpf_x1 = x;
    lpf_y2 = lpf_y1;  lpf_y1 = y;
    return y;
}

/** Compute RMS over the current window buffer */
static float compute_rms(void)
{
    int n = (window_count < SP_WINDOW_SIZE) ? window_count : SP_WINDOW_SIZE;
    if (n == 0) return 0.0f;

    float sum_sq = 0.0f;
    for (int i = 0; i < n; i++) {
        sum_sq += window_buf[i] * window_buf[i];
    }
    return sqrtf(sum_sq / (float)n);
}

/* ── Quality estimator ──────────────────────────────────────────────── */

static int  valid_streak;
static int  total_fed;

static int estimate_quality(void)
{
    if (total_fed < SP_WINDOW_SIZE) return 50;  /* Not enough data yet */
    /* Simple metric: ratio of valid samples in last window */
    int q = (valid_streak * 100) / SP_WINDOW_SIZE;
    if (q > 100) q = 100;
    return q;
}

/* ── Public interface ───────────────────────────────────────────────── */

void signal_proc_init(void)
{
    memset(window_buf, 0, sizeof(window_buf));
    window_idx     = 0;
    window_count   = 0;
    eda_smooth     = 0.0f;
    baseline_rms   = 0.0f;
    baseline_primed = 0;
    prev_rms       = 0.0f;
    valid_streak   = 0;
    total_fed      = 0;

    hp_prev_raw = 0.0f;
    hp_prev_out = 0.0f;
    lpf_x1 = lpf_x2 = 0.0f;
    lpf_y1 = lpf_y2 = 0.0f;

    memset(&latest_feat, 0, sizeof(latest_feat));
}

void signal_proc_handle_event(const SampleFrame *frame)
{
    if (!frame->valid) {
        valid_streak = 0;
        return;
    }

    valid_streak++;
    total_fed++;

    /* Pipeline: HP → rectify → LPF envelope */
    float filtered   = highpass(frame->emg);
    float rectified  = rectify(filtered);
    float envelope   = envelope_lpf(rectified);

    /* Push into sliding window */
    window_buf[window_idx] = envelope;
    window_idx = (window_idx + 1) % SP_WINDOW_SIZE;
    if (window_count < SP_WINDOW_SIZE) window_count++;

    /* Smooth EDA (contextual signal) */
    eda_smooth = 0.9f * eda_smooth + 0.1f * frame->eda;

    /* Update features once we have a full window */
    if (window_count >= SP_WINDOW_SIZE) {
        float rms = compute_rms();

        latest_feat.emg_rms            = rms;
        latest_feat.emg_rate_of_change = rms - prev_rms;
        latest_feat.eda_level          = eda_smooth;
        latest_feat.quality            = estimate_quality();

        prev_rms = rms;

        /* Update baseline (EWMA, only while signal is low/idle) */
        if (!baseline_primed) {
            baseline_rms   = rms;
            baseline_primed = 1;
        } else {
            /* Only update baseline when RMS is below current baseline × 1.5
             * (i.e. during quiet driving, not during braking) */
            if (rms < baseline_rms * 1.5f) {
                baseline_rms = (1.0f - SP_BASELINE_ALPHA) * baseline_rms
                             + SP_BASELINE_ALPHA * rms;
            }
        }

        /* EDA MODIFICATION: Post extracted features to dispatcher */
        DispatchEvent ev;
        ev.type = SYS_EVT_FEATURES_READY;
        ev.payload.features = latest_feat;
        dispatcher_post(&ev);
    }
}

void signal_proc_reset_baseline(void)
{
    baseline_rms    = 0.0f;
    baseline_primed = 0;
    prev_rms        = 0.0f;
    hal_log("[SIG] baseline reset");
}

/* ── Accessor for safety_manager to read baseline ───────────────────── */

float signal_proc_get_baseline_rms(void)
{
    return baseline_rms;
}