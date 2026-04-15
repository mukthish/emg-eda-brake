/**
 * Owner: Devansh
 *
 * Frame format (UART simulation):
 * $EMG,EMG=0.72,EDA=0.31,TS=123456*
 */

#include "../include/input_acq.h"
#include "../include/hal.h"
#include "../include/output_manager.h"
#include "../include/supervisor.h"
#include "../include/dispatcher.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ── Configuration ──────────────────────────────────────────────────── */

#define FRAME_BUF_SIZE       256
#define LINK_HEALTH_WINDOW   20   /**< Evaluate health over last N polls   */
#define FRAME_TIMEOUT_MS     500  /**< 500 ms = link-down threshold        */
#define HARD_FAULT_ERRORS    20   /**< Consecutive errors → sensor fault   */

/* ── Internal state ─────────────────────────────────────────────────── */

static InputSource active_source;

/* Link health tracking */
static int      recent_valid[LINK_HEALTH_WINDOW];
static int      health_idx;
static int      consecutive_errors;
static uint32_t last_valid_ts;

/* ── BLE stub ───────────────────────────────────────────────────────── */

/**
 * Poll a BLE packet from the OpenBCI Cyton.
 * TODO: implement real BLE protocol parsing.
 * @return 0 always (no data available)
 */
static int ble_poll_packet(char *buf, int max_len)
{
    (void)buf;
    (void)max_len;
    return 0;   /* No BLE data — stub */
}

/* ── Frame parser ───────────────────────────────────────────────────── */

/**
 * Parse a UART frame string into a SampleFrame.
 *
 * Expected format:  $EMG,EMG=0.72,EDA=0.31,TS=123456*
 *
 * @param raw   null-terminated frame string
 * @param out   destination SampleFrame
 * @return      1 if parsed successfully, 0 on error
 */
static int parse_frame(const char *raw, SampleFrame *out)
{
    out->valid = 0;
    out->emg   = 0.0f;
    out->eda   = 0.0f;
    out->timestamp = 0;

    /* Check start/end delimiters */
    if (raw[0] != '$') return 0;

    const char *end = strchr(raw, '*');
    if (!end) return 0;

    /* Find EMG= */
    const char *emg_ptr = strstr(raw, "EMG=");
    if (!emg_ptr) return 0;
    out->emg = (float)atof(emg_ptr + 4);

    /* Find EDA= (optional) */
    const char *eda_ptr = strstr(raw, "EDA=");
    if (eda_ptr) {
        out->eda = (float)atof(eda_ptr + 4);
    }

    /* Find TS= */
    const char *ts_ptr = strstr(raw, "TS=");
    if (ts_ptr) {
        out->timestamp = (uint32_t)atol(ts_ptr + 3);
    }

    /* Basic range validation */
    if (out->emg < -10.0f || out->emg > 10.0f) return 0;
    if (out->eda < -10.0f || out->eda > 10.0f) return 0;

    out->valid = 1;
    return 1;
}

/* ── Public interface ───────────────────────────────────────────────── */

void input_init(void)
{
    active_source     = INPUT_SOURCE_UART;
    health_idx        = 0;
    consecutive_errors = 0;
    last_valid_ts     = 0;
    memset(recent_valid, 0, sizeof(recent_valid));
}

void input_poll(void)
{
    char buf[FRAME_BUF_SIZE];
    int  bytes = 0;

    switch (active_source) {
    case INPUT_SOURCE_UART:
        bytes = hal_uart_read_frame(buf, FRAME_BUF_SIZE);
        break;
    case INPUT_SOURCE_BLE:
        bytes = ble_poll_packet(buf, FRAME_BUF_SIZE);
        break;
    }

    if (bytes > 0) {
        buf[bytes < FRAME_BUF_SIZE ? bytes : FRAME_BUF_SIZE - 1] = '\0';

        SampleFrame frame;
        if (parse_frame(buf, &frame)) {
            
            /* EDA MODIFICATION: Post parsed frame to dispatcher */
            DispatchEvent ev;
            ev.type = SYS_EVT_SAMPLES_PARSED;
            ev.payload.frame = frame;
            dispatcher_post(&ev);
            
            recent_valid[health_idx % LINK_HEALTH_WINDOW] = 1;
            consecutive_errors = 0;
            last_valid_ts = hal_get_tick_ms();
        } else {
            /* Malformed frame */
            recent_valid[health_idx % LINK_HEALTH_WINDOW] = 0;
            consecutive_errors++;
            output_log_event("INP", EVT_DATA_INVALID);
        }
        health_idx++;
    } else {
        /* No data this tick — check timeout */
        uint32_t now = hal_get_tick_ms();
        if (last_valid_ts > 0 && (now - last_valid_ts) >= FRAME_TIMEOUT_MS) {
            recent_valid[health_idx % LINK_HEALTH_WINDOW] = 0;
            health_idx++;
        }
    }

    /* Check for hard fault condition */
    if (consecutive_errors >= HARD_FAULT_ERRORS) {
        output_log_event("INP", EVT_SENSOR_FAULT);
        
        /* EDA MODIFICATION: Send fault intent through the dispatcher */
        DispatchEvent fault_ev;
        fault_ev.type = SYS_EVT_INTENT_STATE;
        fault_ev.payload.intent = EVT_SENSOR_FAULT;
        dispatcher_post(&fault_ev);
        
        consecutive_errors = 0;  /* Reset after posting */
    }
}

int input_get_link_health(void)
{
    int valid_count = 0;
    for (int i = 0; i < LINK_HEALTH_WINDOW; i++) {
        valid_count += recent_valid[i];
    }
    return (valid_count * 100) / LINK_HEALTH_WINDOW;
}

void input_set_source(InputSource src)
{
    active_source = src;
    consecutive_errors = 0;
    char msg[64];
    snprintf(msg, sizeof(msg), "[INP] source changed to %s",
             src == INPUT_SOURCE_BLE ? "BLE" : "UART");
    hal_log(msg);
}