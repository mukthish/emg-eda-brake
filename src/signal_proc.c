/**
 * @file signal_proc.c
 * @brief Signal Processing + Feature Engine — filter, window, compute features.
 *
 * Owner: Mahesh
 *
 * Processing pipeline (derived from Ju et al. 2021):
 *   1. Bandpass filter  : 4th-order Butterworth, 15–90 Hz
 *      (simplified for 20 Hz sim rate — acts as high-pass above DC)
 *   2. Linear envelope  : rectification + 2nd-order Butterworth LPF @ 2 Hz
 *   3. Windowed RMS     : 1 s sliding window (SP_WINDOW_SIZE samples)
 *   4. Rate of change   : Δ RMS between successive evaluations
 *   5. Baseline model   : EWMA of quiet-state EMG
 *
 * Note: At 20 Hz simulation rate, the Nyquist limit is 10 Hz, so the
 * 15–90 Hz bandpass cannot be meaningfully applied.  For simulation we
 * apply a simple DC-removal high-pass and use the envelope directly.
 * On real hardware at 200+ Hz the full Butterworth filters should be used.
 */

#include "signal_proc.h"
#include "hal.h"

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
static int           features_ready;

/** DC removal (simple first-order high-pass) */
static float hp_prev_raw;
static float hp_prev_out;

/** Envelope low-pass (2nd-order Butterworth at 2 Hz) */
/*
 * For 20 Hz sample rate, cutoff = 2 Hz:
 *      wc = 2 * pi * 2 / 20 = 0.6283
 *      Prewarped: Ω = tan(wc/2) = tan(0.3142) ≈ 0.3249
 *
 *      2nd-order Butterworth:  a0 = Ω² + √2·Ω + 1
 *      b0 = Ω² / a0,   b1 = 2·b0,   b2 = b0
 *      a1 = 2·(Ω² - 1) / a0,   a2 = (Ω² - √2·Ω + 1) / a0
 *
 * Precomputed for 20 Hz / 2 Hz cutoff:
 */
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
    features_ready = 0;
    valid_streak   = 0;
    total_fed      = 0;

    hp_prev_raw = 0.0f;
    hp_prev_out = 0.0f;
    lpf_x1 = lpf_x2 = 0.0f;
    lpf_y1 = lpf_y2 = 0.0f;

    memset(&latest_feat, 0, sizeof(latest_feat));
}

void signal_proc_process_batch(const SampleFrame *samples, int count)
{
    for (int i = 0; i < count; i++) {
        const SampleFrame *s = &samples[i];

        if (!s->valid) {
            valid_streak = 0;
            continue;
        }

        valid_streak++;
        total_fed++;

        /* Pipeline: HP → rectify → LPF envelope */
        float filtered   = highpass(s->emg);
        float rectified  = rectify(filtered);
        float envelope   = envelope_lpf(rectified);

        /* Push into sliding window */
        window_buf[window_idx] = envelope;
        window_idx = (window_idx + 1) % SP_WINDOW_SIZE;
        if (window_count < SP_WINDOW_SIZE) window_count++;

        /* Smooth EDA (contextual signal) */
        eda_smooth = 0.9f * eda_smooth + 0.1f * s->eda;
    }

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

        features_ready = 1;
    }
}

int signal_proc_get_features(FeatureVector *out)
{
    if (!features_ready) return 0;
    *out = latest_feat;
    return 1;
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
