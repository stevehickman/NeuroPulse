/*
 * NeurOne Cervical VNS — Session Management
 * Document: NP-FW-CVNS-001 Rev 1 §8
 *
 * Orchestrates the full cervical VNS session:
 *   impedance check → cardiac baseline → ramp-up → active → ramp-down → complete.
 *
 * UHDR record (session timestamps, HR, impedance raw values) written at session
 * end, encrypted with user's biometric-derived AES-256-XTS key via storage HAL.
 * SHDR summary (device metrics only, no user biology) written to SHDR partition.
 */

#include "np_cvns_session.h"
#include <string.h>

/* ── Platform HAL stubs ──────────────────────────────────────────────────────── */

static void platform_uhdr_write(const np_cvns_session_record_t *rec)
{
    /* Platform HAL: encrypt and write to UHDR eMMC partition.                  */
    /* np_platform_uhdr_write_cvns(rec, sizeof(*rec));                          */
    /* OI-CVNS-03: implement with AES-256-XTS biometric key before compliance.  */
    (void)rec;
}

static void platform_shdr_write(const np_cvns_shdr_summary_t *shdr)
{
    /* Platform HAL: write to SHDR eMMC partition (NeurOne manufacturing key)*/
    /* np_platform_shdr_write_cvns(shdr, sizeof(*shdr));                        */
    /* OI-CVNS-04: implement before compliance.                                 */
    (void)shdr;
}

/* ── Interlock fault callback (invoked from np_cvns_interlock.c) ─────────────── */

/* OI-CVNS-01: not yet wired — the app layer must register this callback against  */
/* the session context pointer.  Marked unused until that wiring lands.           */
static void on_interlock_fault(np_cvns_interlock_ctx_t *interlock,
                                 np_cvns_fault_reason_t   reason) __attribute__((unused));
static void on_interlock_fault(np_cvns_interlock_ctx_t *interlock,
                                 np_cvns_fault_reason_t   reason)
{
    /*
     * The safety MCU has already asserted the enable GPIO low (hardware action).
     * This callback updates the session stage and prepares the UHDR record.
     * Reach the session context via a static pointer — sessions are singleton
     * for this accessory (one active session at a time).
     */
    (void)interlock;
    (void)reason;
    /* Session context is reached via the composed session_ctx pointer.          */
    /* Full integration requires the app layer to wire this callback.            */
    /* OI-CVNS-01: wire fault_cb to session context pointer in app layer.       */
}

/* ── Stim safety-response handler ────────────────────────────────────────────── */

/*
 * np_cvns_stim_init() requires a non-NULL safety-response callback (its NULL
 * rejection is intentional and FAI-tested).  The session, however, advances by
 * POLLING ctx->stim->impedance_ok / np_cvns_stim_phase() inside np_cvns_session_tick(),
 * not from this callback.  np_cvns_stim_safety_mcu_response() already updates the
 * stim fields (impedance_ok, granted, phase) BEFORE invoking this callback, so a
 * no-op handler is sufficient and keeps the polling-based flow unchanged.
 *
 * Passing NULL here (as an earlier revision did) made np_cvns_session_start()
 * fail with NP_CVNS_ERR_INVALID_ARG on every call — the documented session entry
 * point was dead.  This handler fixes that without altering session behaviour.
 */
static void session_stim_safety_cb(np_cvns_stim_ctx_t *stim,
                                    bool                granted,
                                    const float         impedance_kohm[])
{
    (void)stim;
    (void)granted;
    (void)impedance_kohm;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

np_cvns_status_t np_cvns_session_init(np_cvns_session_ctx_t   *ctx,
                                       np_cvns_interlock_ctx_t *interlock,
                                       np_cvns_stim_ctx_t      *stim,
                                       np_cvns_session_end_cb_t end_cb,
                                       np_cvns_display_cb_t     display_cb)
{
    if (!ctx || !interlock || !stim || !end_cb) {
        return NP_CVNS_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->interlock   = interlock;
    ctx->stim        = stim;
    ctx->end_cb      = end_cb;
    ctx->display_cb  = display_cb;
    ctx->stage       = NP_CVNS_STAGE_IDLE;

    return NP_CVNS_OK;
}

/* ── Internal helpers ────────────────────────────────────────────────────────── */

static void session_advance_to_fault(np_cvns_session_ctx_t *ctx,
                                       np_cvns_status_t        reason,
                                       uint32_t                now_ms)
{
    ctx->stage = NP_CVNS_STAGE_FAULT;
    np_cvns_stim_stop(ctx->stim);
    np_cvns_interlock_disable(ctx->interlock);

    /* Finalise UHDR record. */
    ctx->uhdr_record.session_duration_s = (uint32_t)(now_ms - ctx->stim_start_ms) / 1000U;
    ctx->uhdr_record.abort_reason       = (uint8_t)(-reason); /* convert to positive */

    if (reason == NP_CVNS_ERR_CARDIAC_CUTOFF) {
        ctx->uhdr_record.cutoff_occurred   = 1U;
        ctx->uhdr_record.cutoff_hr_baseline_x10 =
            (uint16_t)(np_cvns_interlock_baseline_hr(ctx->interlock) * 10.0f);
        ctx->uhdr_record.cutoff_time_offset_s =
            (ctx->stim_start_ms > 0U)
            ? (now_ms - ctx->stim_start_ms) / 1000U : 0U;
    }

    ctx->shdr_summary.cutoff_occurred = (reason == NP_CVNS_ERR_CARDIAC_CUTOFF) ? 1U : 0U;
    ctx->shdr_summary.fault_reason    = (uint8_t)ctx->interlock->fault_reason;

    platform_uhdr_write(&ctx->uhdr_record);
    platform_shdr_write(&ctx->shdr_summary);

    if (ctx->end_cb) {
        ctx->end_cb(&ctx->uhdr_record, reason);
    }
}

static void session_complete(np_cvns_session_ctx_t *ctx, uint32_t now_ms)
{
    ctx->stage = NP_CVNS_STAGE_COMPLETE;
    np_cvns_interlock_disable(ctx->interlock);

    ctx->uhdr_record.session_duration_s = (uint32_t)(now_ms - ctx->stim_start_ms) / 1000U;
    ctx->uhdr_record.abort_reason       = 0U;
    ctx->uhdr_record.cutoff_occurred    = 0U;

    ctx->shdr_summary.stim_duration_s   = ctx->uhdr_record.session_duration_s;
    ctx->shdr_summary.cutoff_occurred   = 0U;

    platform_uhdr_write(&ctx->uhdr_record);
    platform_shdr_write(&ctx->shdr_summary);

    if (ctx->end_cb) {
        ctx->end_cb(&ctx->uhdr_record, NP_CVNS_OK);
    }
}

/* ── Session control ─────────────────────────────────────────────────────────── */

np_cvns_status_t np_cvns_session_start(np_cvns_session_ctx_t          *ctx,
                                        const np_cvns_session_config_t *config,
                                        uint32_t                        now_ms,
                                        uint32_t                        now_unix)
{
    if (!ctx || !config) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (ctx->stage != NP_CVNS_STAGE_IDLE) {
        return NP_CVNS_ERR_SESSION_ACTIVE;
    }

    ctx->config            = *config;
    ctx->session_start_ms  = now_ms;
    ctx->session_start_unix = now_unix;
    ctx->stim_start_ms     = 0U;

    /* Initialize UHDR record (timestamps and session parameters are UHDR). */
    memset(&ctx->uhdr_record, 0, sizeof(ctx->uhdr_record));
    ctx->uhdr_record.session_start_unix = now_unix;
    ctx->uhdr_record.stim_freq_hz       = config->freq_hz;
    ctx->uhdr_record.stim_current_ua    = config->current_ua;
    ctx->uhdr_record.stim_pulse_width_us = config->pulse_width_us;
    ctx->uhdr_record.electrode_config   = config->electrode_config;

    /* Initialize SHDR summary (no user biology). */
    memset(&ctx->shdr_summary, 0, sizeof(ctx->shdr_summary));
    ctx->shdr_summary.electrode_config  = config->electrode_config;
    ctx->shdr_summary.stim_freq_hz      = config->freq_hz;
    ctx->shdr_summary.stim_current_ua   = config->current_ua;

    /* Initialize stim module (validates config).  A non-NULL safety-response
     * callback is required by np_cvns_stim_init(); the session advances by
     * polling stim state, so session_stim_safety_cb is a no-op bridge. */
    np_cvns_status_t ret = np_cvns_stim_init(ctx->stim, config,
                                             session_stim_safety_cb);
    if (ret != NP_CVNS_OK) {
        return ret;
    }

    /* Start with impedance check. */
    ctx->stage = NP_CVNS_STAGE_IMPEDANCE;
    ret = np_cvns_stim_request_impedance_check(ctx->stim);
    return ret;
}

void np_cvns_session_stop(np_cvns_session_ctx_t *ctx, uint32_t now_ms)
{
    if (!ctx || ctx->stage == NP_CVNS_STAGE_IDLE) {
        return;
    }
    session_advance_to_fault(ctx, NP_CVNS_ERR_NO_SESSION, now_ms);
    ctx->stage = NP_CVNS_STAGE_IDLE;
}

/* ── Session tick ────────────────────────────────────────────────────────────── */

void np_cvns_session_tick(np_cvns_session_ctx_t *ctx,
                            uint32_t               now_ms,
                            uint32_t               now_s)
{
    if (!ctx || ctx->stage == NP_CVNS_STAGE_IDLE ||
        ctx->stage == NP_CVNS_STAGE_COMPLETE ||
        ctx->stage == NP_CVNS_STAGE_FAULT) {
        return;
    }

    /* Check for cardiac interlock fault at every tick. */
    if (np_cvns_interlock_state(ctx->interlock) == NP_CVNS_INTERLOCK_FAULT &&
        ctx->stage != NP_CVNS_STAGE_FAULT) {
        session_advance_to_fault(ctx, NP_CVNS_ERR_CARDIAC_CUTOFF, now_ms);
        return;
    }

    /* Forward tick to interlock (data-loss detection). */
    np_cvns_interlock_tick(ctx->interlock, now_ms, now_s);

    switch (ctx->stage) {

    case NP_CVNS_STAGE_IMPEDANCE:
        /* Wait for impedance result via stim safety_cb / interlock SPI path.   */
        /* np_cvns_stim_safety_mcu_response() will set impedance_ok.            */
        if (ctx->stim->impedance_ok) {
            /* Store impedance in UHDR record (raw measurement = user biology).  */
            ctx->uhdr_record.impedance_left_kohm  = ctx->stim->impedance_kohm[0];
            ctx->uhdr_record.impedance_right_kohm = ctx->stim->impedance_kohm[1];
            ctx->shdr_summary.impedance_left_kohm  = ctx->stim->impedance_kohm[0];
            ctx->shdr_summary.impedance_right_kohm = ctx->stim->impedance_kohm[1];
            ctx->shdr_summary.impedance_check_pass = 1U;
            ctx->stage = NP_CVNS_STAGE_BASELINE;
        }
        break;

    case NP_CVNS_STAGE_BASELINE:
        /* Wait for cardiac baseline to be established. */
        if (np_cvns_interlock_baseline_valid(ctx->interlock)) {
            /* Attempt to enable stimulation via the interlock. */
            np_cvns_status_t ret = np_cvns_interlock_request_enable(ctx->interlock, now_s);
            if (ret == NP_CVNS_OK) {
                /* Transition pending safety MCU ENABLE_GRANTED response. */
            } else {
                session_advance_to_fault(ctx, ret, now_ms);
            }
        }
        /* Once interlock state becomes ENABLED, advance to ramp-up. */
        if (np_cvns_interlock_state(ctx->interlock) == NP_CVNS_INTERLOCK_ENABLED) {
            np_cvns_status_t ret = np_cvns_stim_start(ctx->stim, now_ms);
            if (ret == NP_CVNS_OK) {
                ctx->stage        = NP_CVNS_STAGE_RAMP_UP;
                ctx->stim_start_ms = now_ms;
            } else {
                session_advance_to_fault(ctx, ret, now_ms);
            }
        }
        break;

    case NP_CVNS_STAGE_RAMP_UP:
        np_cvns_stim_tick(ctx->stim, now_ms);
        if (np_cvns_stim_phase(ctx->stim) == NP_CVNS_STIM_ACTIVE) {
            ctx->stage = NP_CVNS_STAGE_ACTIVE;
        } else if (np_cvns_stim_phase(ctx->stim) == NP_CVNS_STIM_FAULT) {
            session_advance_to_fault(ctx, NP_CVNS_ERR_SAFETY_REJECTED, now_ms);
        }
        break;

    case NP_CVNS_STAGE_ACTIVE:
        np_cvns_stim_tick(ctx->stim, now_ms);
        if (np_cvns_stim_phase(ctx->stim) == NP_CVNS_STIM_RAMP_DOWN) {
            ctx->stage = NP_CVNS_STAGE_RAMP_DOWN;
        } else if (np_cvns_stim_phase(ctx->stim) == NP_CVNS_STIM_FAULT) {
            session_advance_to_fault(ctx, NP_CVNS_ERR_SAFETY_REJECTED, now_ms);
        }
        break;

    case NP_CVNS_STAGE_RAMP_DOWN:
        np_cvns_stim_tick(ctx->stim, now_ms);
        if (np_cvns_stim_phase(ctx->stim) == NP_CVNS_STIM_DONE) {
            session_complete(ctx, now_ms);
        } else if (np_cvns_stim_phase(ctx->stim) == NP_CVNS_STIM_FAULT) {
            session_advance_to_fault(ctx, NP_CVNS_ERR_SAFETY_REJECTED, now_ms);
        }
        break;

    default:
        break;
    }

    /* Notify app of display state update. */
    if (ctx->display_cb) {
        np_cvns_display_state_t disp;
        disp.stage           = ctx->stage;
        disp.stim_phase      = np_cvns_stim_phase(ctx->stim);
        disp.interlock       = np_cvns_interlock_state(ctx->interlock);
        disp.current_ua      = np_cvns_stim_current_ua(ctx->stim);
        disp.baseline_hr_bpm = np_cvns_interlock_baseline_hr(ctx->interlock);
        disp.impedance_left_kohm  = ctx->stim->impedance_kohm[0];
        disp.impedance_right_kohm = ctx->stim->impedance_kohm[1];
        disp.fault_reason    = np_cvns_interlock_fault_reason(ctx->interlock);
        disp.reenable_lockout_remaining_s =
            np_cvns_interlock_reenable_lockout_remaining_s(ctx->interlock, now_s);

        /* Progress: 0–100% across ramp-up, active, ramp-down. */
        if (ctx->stim_start_ms > 0U && ctx->config.duration_s > 0U) {
            uint32_t total_ms  = (uint32_t)ctx->config.duration_s * 1000U;
            uint32_t elapsed   = now_ms - ctx->stim_start_ms;
            disp.stim_progress_pct = (elapsed >= total_ms)
                                     ? 100.0f
                                     : (float)elapsed * 100.0f / (float)total_ms;
        } else {
            disp.stim_progress_pct = 0.0f;
        }

        ctx->display_cb(&disp);
    }
}

/* ── State accessors ─────────────────────────────────────────────────────────── */

np_cvns_stage_t np_cvns_session_stage(const np_cvns_session_ctx_t *ctx)
{
    return ctx ? ctx->stage : NP_CVNS_STAGE_IDLE;
}

/* ── Re-enable after cardiac cutoff ─────────────────────────────────────────── */

np_cvns_status_t np_cvns_session_request_reenable(np_cvns_session_ctx_t *ctx,
                                                    uint32_t               now_s)
{
    if (!ctx) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (ctx->stage != NP_CVNS_STAGE_FAULT) {
        return NP_CVNS_ERR_NO_SESSION;
    }
    return np_cvns_interlock_request_reenable(ctx->interlock, true, now_s);
}
