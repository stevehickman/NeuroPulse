/*
 * NeuroPulse Cervical VNS — Session Management API
 * Document: NP-FW-CVNS-001 Rev A §8
 *
 * Orchestrates the full cervical VNS session workflow:
 * impedance check → cardiac baseline → ramp-up → active → ramp-down → complete.
 * Composes np_cvns_interlock and np_cvns_stim, owns UHDR/SHDR record writing.
 */

#ifndef NP_CVNS_SESSION_H
#define NP_CVNS_SESSION_H

#include "np_cvns_types.h"
#include "np_cvns_interlock.h"
#include "np_cvns_stim.h"

/* ── Opaque context ──────────────────────────────────────────────────────────── */
struct np_cvns_session_ctx {
    np_cvns_interlock_ctx_t *interlock;
    np_cvns_stim_ctx_t      *stim;

    np_cvns_session_config_t  config;
    np_cvns_stage_t           stage;

    uint32_t  session_start_ms;
    uint32_t  session_start_unix;
    uint32_t  stim_start_ms;        /* ms when RAMP_UP began                   */
    uint32_t  ramp_end_ms;          /* ms when RAMP_UP → ACTIVE transition done */

    np_cvns_session_record_t uhdr_record;
    np_cvns_shdr_summary_t   shdr_summary;

    np_cvns_session_end_cb_t end_cb;
    np_cvns_display_cb_t     display_cb;
};

_Static_assert(sizeof(struct np_cvns_session_ctx) <= NP_CVNS_SESSION_CTX_SIZE_BYTES,
               "np_cvns_session_ctx exceeds NP_CVNS_SESSION_CTX_SIZE_BYTES");

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */
np_cvns_status_t np_cvns_session_init(np_cvns_session_ctx_t   *ctx,
                                       np_cvns_interlock_ctx_t *interlock,
                                       np_cvns_stim_ctx_t      *stim,
                                       np_cvns_session_end_cb_t end_cb,
                                       np_cvns_display_cb_t     display_cb);

/* ── Session control ─────────────────────────────────────────────────────────── */
/*
 * Starts a new session.  The session must be delivered via a cryptographically
 * signed protocol (same CSPRNG mechanism as all NeuroPulse modalities).
 * now_ms: free-running millisecond counter.
 * now_unix: UTC epoch seconds (for UHDR timestamp).
 */
np_cvns_status_t np_cvns_session_start(np_cvns_session_ctx_t          *ctx,
                                        const np_cvns_session_config_t *config,
                                        uint32_t                        now_ms,
                                        uint32_t                        now_unix);

void np_cvns_session_stop(np_cvns_session_ctx_t *ctx, uint32_t now_ms);

/*
 * Main scheduler tick.  Must be called every NP_CVNS_STIM_TICK_MS (100 ms).
 */
void np_cvns_session_tick(np_cvns_session_ctx_t *ctx,
                            uint32_t               now_ms,
                            uint32_t               now_s);

/* ── State accessors ─────────────────────────────────────────────────────────── */
np_cvns_stage_t np_cvns_session_stage(const np_cvns_session_ctx_t *ctx);

/* ── Re-enable after cardiac cutoff ─────────────────────────────────────────── */
/*
 * Called by the app layer after the user explicitly confirms re-enable.
 * Only succeeds if lockout has elapsed and baseline has been re-established.
 */
np_cvns_status_t np_cvns_session_request_reenable(np_cvns_session_ctx_t *ctx,
                                                    uint32_t               now_s);

#endif /* NP_CVNS_SESSION_H */
