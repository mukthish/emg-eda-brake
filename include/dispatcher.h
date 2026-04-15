/**
 * @file dispatcher.h
 * @brief Central Event Dispatcher — Event-Driven Architecture router.
 *
 * All cross-module communication is routed through this central queue.
 * Modules post events here, and the dispatcher routes them to the correct
 * consumer, completely decoupling the pipeline.
 */

#ifndef DISPATCHER_H
#define DISPATCHER_H

/* Include the headers that define the data types we are passing around */
#include "input_acq.h"
#include "signal_proc.h"
#include "supervisor.h"

/* ── Event Types ────────────────────────────────────────────────────── */

/** Broad system-level event categories */
typedef enum {
    SYS_EVT_TICK_1MS,          /**< Hardware timer tick (Heartbeat) */
    SYS_EVT_SAMPLES_PARSED,    /**< Data parsed by Input Manager */
    SYS_EVT_FEATURES_READY,    /**< DSP features extracted by Signal Proc */
    SYS_EVT_INTENT_STATE,      /**< Intent evaluation from Safety Manager */
    SYS_EVT_CMD_BRAKE,          /**< Hardware actuation command to Output */
    SYS_EVT_MAX
} SysEventType;

/* ── Event Payload Envelope ─────────────────────────────────────────── */

/** * The Event Envelope.
 * Contains the event type and a union holding the specific data.
 * Using a union ensures the struct only takes up as much memory as its largest member.
 */
typedef struct {
    SysEventType type;
    union {
        SampleFrame   frame;       /**< Used for SYS_EVT_SAMPLES_PARSED */
        FeatureVector features;    /**< Used for SYS_EVT_FEATURES_READY */
        SystemEvent   intent;      /**< Used for SYS_EVT_INTENT_STATE */
        int           brake_cmd;   /**< Used for SYS_EVT_CMD_BRAKE (1=assert, 0=clear) */
    } payload;
} DispatchEvent;

/* ── Public Dispatcher Interface ────────────────────────────────────── */

/** * Initialise the dispatcher queue.
 * Resets the ring buffer head/tail indices.
 */
void dispatcher_init(void);

/**
 * Post an event to the dispatcher queue.
 * The event is copied by value into the internal ring buffer.
 * * @param ev Pointer to the event payload to queue.
 */
void dispatcher_post(const DispatchEvent *ev);

/**
 * Empty the queue and route all events to their respective modules.
 * Called continuously by the System Shell heartbeat.
 */
void dispatcher_run_once(void);

#endif /* DISPATCHER_H */