/*
 * NeurOne Hub Control Program — Session Runner
 * Document: NP-FW-HUB-001 Rev A §5
 *
 * Executes a parsed np_session_desc_t in real time.  Dispatches commands to
 * registered module control functions at their session-relative start times,
 * issues implicit stop calls at (start_ms + duration_ms), and coordinates
 * graceful shutdown on completion or abort.
 */

#ifndef NP_SESSION_RUNNER_H
#define NP_SESSION_RUNNER_H

#include "np_hub_types.h"
#include "FreeRTOS.h"
#include "event_groups.h"

/* ── Event bits (posted to hub_events EventGroup) ────────────────────────────── */

#define NP_EV_SESSION_START     (1U << 0)  /* protocol loaded and verified */
#define NP_EV_SESSION_ABORT     (1U << 1)  /* user abort or safety fault */
#define NP_EV_SAFETY_FAULT      (1U << 2)  /* safety MCU reported fault */
#define NP_EV_THERMAL_FAULT     (1U << 3)  /* any module over-temperature */
#define NP_EV_EEG_PPX           (1U << 4)  /* photoparoxysmal detection */
#define NP_EV_POWER_FAULT       (1U << 5)  /* USB-C PD negotiation failure */

/* ── Runner context (static allocation, single global instance) ──────────────── */

typedef struct {
    np_session_desc_t  desc;               /* parsed session (in LPSDR4 RAM) */
    np_session_state_t state;
    TickType_t         start_tick;         /* xTaskGetTickCount() at session start */
    uint32_t           elapsed_ms;         /* running session time */
    np_abort_reason_t  abort_reason;

    /* Per-slot stop tracking: stop_at_ms[slot] = when to call control(NULL,0). */
    uint32_t  stop_at_ms[NP_HUB_SLOT_MAX];

    /* Next command to dispatch (index into desc.cmds[]). */
    uint8_t   next_cmd_idx;

    /* Session UHDR/SHDR records built during run, flushed at session end. */
    np_session_uhdr_record_t uhdr;
    np_session_shdr_record_t shdr;
} np_runner_ctx_t;

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * np_runner_init — initialize context; call once at hub startup.
 * hub_events must be created before this call.
 */
void np_runner_init(EventGroupHandle_t hub_events);

/*
 * np_runner_load — verify protocol and install it into the runner context.
 * Posts NP_EV_SESSION_START on success.
 * Returns NP_HUB_ERR_SESSION_ACTIVE if a session is already running.
 */
np_hub_status_t np_runner_load(const uint8_t *proto_buf, size_t proto_len);

/*
 * np_runner_run — blocking session execution loop.
 * Called from task_hub_control after NP_EV_SESSION_START is received.
 * Returns when the session is complete, aborted, or faulted.
 * Handles all command dispatch, stop events, and telemetry triggers.
 */
np_hub_status_t np_runner_run(void);

/*
 * np_runner_abort — signal a session abort from any task context.
 * reason is recorded in the SHDR summary.
 * Posts NP_EV_SESSION_ABORT to unblock np_runner_run().
 */
void np_runner_abort(np_abort_reason_t reason);

/*
 * np_runner_get_state — thread-safe state read (reads with a critical section).
 */
np_session_state_t np_runner_get_state(void);

/*
 * np_runner_elapsed_ms — ms since session start; 0 if not running.
 */
uint32_t np_runner_elapsed_ms(void);

#endif /* NP_SESSION_RUNNER_H */
