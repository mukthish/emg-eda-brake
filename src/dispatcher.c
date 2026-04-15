#include "../include/dispatcher.h"

/* Include all downstream consumers */
#include "../include/input_acq.h"
#include "../include/signal_proc.h"
#include "../include/safety_manager.h"
#include "../include/supervisor.h"
#include "../include/output_manager.h"
#include "../include/hal.h"

/* ── Configuration ──────────────────────────────────────────────────── */

#define DISPATCHER_QUEUE_SIZE 64  /**< Maximum number of pending events */

/* ── Internal State ─────────────────────────────────────────────────── */

static DispatchEvent evt_queue[DISPATCHER_QUEUE_SIZE];
static int head;
static int tail;

/* ── Event Handler Type ─────────────────────────────────────────────── */

typedef void (*EventHandler)(const DispatchEvent *ev);

/* ── Event Handlers ─────────────────────────────────────────────────── */

static void handle_tick_1ms(const DispatchEvent *ev)
{
    (void)ev;  // unused
    input_poll();
    output_flush_logs();
}

static void handle_samples_parsed(const DispatchEvent *ev)
{
    signal_proc_handle_event(&ev->payload.frame);
}

static void handle_features_ready(const DispatchEvent *ev)
{
    safety_handle_event(&ev->payload.features);
}

static void handle_intent_state(const DispatchEvent *ev)
{
    supervisor_post_event(ev->payload.intent);
}

static void handle_cmd_brake(const DispatchEvent *ev)
{
    output_set_brake_request(ev->payload.brake_cmd);
}

static void handle_unknown(const DispatchEvent *ev)
{
    (void)ev;
    hal_log("[WARN] Dispatcher received unknown event type");
}

/* ── Dispatch Table ─────────────────────────────────────────────────── */

static EventHandler dispatch_table[SYS_EVT_MAX] = {
    [SYS_EVT_TICK_1MS]        = handle_tick_1ms,
    [SYS_EVT_SAMPLES_PARSED]  = handle_samples_parsed,
    [SYS_EVT_FEATURES_READY]  = handle_features_ready,
    [SYS_EVT_INTENT_STATE]    = handle_intent_state,
    [SYS_EVT_CMD_BRAKE]       = handle_cmd_brake
};

/* ── Public Interface ───────────────────────────────────────────────── */

void dispatcher_init(void)
{
    head = 0;
    tail = 0;
    hal_log("[DISP] dispatcher initialized");
}

void dispatcher_post(const DispatchEvent *ev)
{
    int next_head = (head + 1) % DISPATCHER_QUEUE_SIZE;

    if (next_head == tail) {
        /* Queue overflow: drop the event */
        hal_log("[ERR] Dispatcher queue overflow! Event dropped.");
        return;
    }

    evt_queue[head] = *ev;
    head = next_head;
}

void dispatcher_run_once(void)
{
    while (head != tail) {

        /* 1. Pop oldest event */
        DispatchEvent ev = evt_queue[tail];
        tail = (tail + 1) % DISPATCHER_QUEUE_SIZE;

        /* 2. O(1) dispatch */
        if (ev.type < SYS_EVT_MAX) {
            EventHandler handler = dispatch_table[ev.type];

            if (handler) {
                handler(&ev);
            } else {
                handle_unknown(&ev);
            }
        } else {
            handle_unknown(&ev);
        }
    }
}