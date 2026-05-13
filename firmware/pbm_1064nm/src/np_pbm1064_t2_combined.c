/*
 * NeuroPulse 1064nm Smart Zone Module — T2 Combined Session
 * Document: NP-FW-PBM1064-001 Rev A §8
 *
 * Coordinates 1064nm smart zone modules with the T2 1170nm laser subsystem.
 * T2 1170nm API is stubbed via np_pbm1064_hal_t2_throttle_request() pending
 * Issue #54.
 *
 * Thermal throttle priority:
 *   1. 1170nm laser (deepest, highest subcortical heat deposition)
 *   2. 1064nm CH_C
 *   3. 808nm CH_B
 *   4. 660nm CH_A
 */

#include "np_pbm1064_t2_combined.h"
#include "np_pbm1064_hal.h"
#include <string.h>

/* ── Public API ─────────────────────────────────────────────────────────────── */

void np_pbm1064_t2_init(np_pbm1064_t2_ctx_t         *ctx,
                          np_pbm1064_session_end_cb_t  end_cb,
                          np_pbm1064_fault_cb_t        fault_cb,
                          uint32_t                     device_session_count)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->session_end_cb   = end_cb;
    ctx->fault_cb         = fault_cb;
    ctx->t2_1170_throttle_pct = 100U;  /* 100 = full output */
    ctx->stage            = NP_T2_STAGE_IDLE;

    np_pbm1064_session_init(&ctx->pbm1064,
                              end_cb, fault_cb,
                              NULL,   /* display_cb for inner session is unused */
                              device_session_count);
}

np_pbm1064_status_t np_pbm1064_t2_start(
    np_pbm1064_t2_ctx_t               *ctx,
    const np_pbm1064_session_desc_t   *desc_1064)
{
    if (ctx->stage != NP_T2_STAGE_IDLE) {
        return NP_PBM1064_ERR_SESSION_ACTIVE;
    }

    ctx->stage = NP_T2_STAGE_PREFLIGHT;
    ctx->throttle_1170_applied = false;
    ctx->throttle_ch_c_applied = false;
    ctx->throttle_ch_b_applied = false;
    ctx->throttle_ch_a_applied = false;

    /* Start 1064nm session (ramp begins inside session_start). */
    np_pbm1064_status_t rc = np_pbm1064_session_start(&ctx->pbm1064, desc_1064);
    if (rc != NP_PBM1064_OK) {
        ctx->stage = NP_T2_STAGE_FAULT;
        return rc;
    }

    /* Signal T2 1170nm subsystem to begin ramp simultaneously (stub). */
    np_pbm1064_hal_t2_throttle_request(100U);
    ctx->t2_1170_active       = true;
    ctx->t2_1170_throttle_pct = 100U;

    ctx->session_start_ms = np_pbm1064_hal_now_ms();
    ctx->stage = NP_T2_STAGE_RAMP_UP;

    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_t2_tick(np_pbm1064_t2_ctx_t *ctx,
                                          uint32_t now_ms)
{
    if (ctx->stage == NP_T2_STAGE_IDLE    ||
        ctx->stage == NP_T2_STAGE_COMPLETE ||
        ctx->stage == NP_T2_STAGE_FAULT) {
        return NP_PBM1064_ERR_NO_SESSION;
    }

    /* Delegate dose and ramp to inner 1064nm session. */
    np_pbm1064_status_t rc = np_pbm1064_session_tick(&ctx->pbm1064, now_ms);

    if (rc == NP_PBM1064_ERR_THERMAL) {
        np_pbm1064_t2_abort(ctx, NP_PBM1064_FAULT_THERMAL);
        return rc;
    }

    /* Track inner session stage to update T2 combined stage. */
    np_pbm1064_stage_t inner = np_pbm1064_session_stage(&ctx->pbm1064);
    switch (inner) {
    case NP_PBM1064_STAGE_RAMP_UP:
        ctx->stage = NP_T2_STAGE_RAMP_UP;
        break;
    case NP_PBM1064_STAGE_ACTIVE:
        ctx->stage = NP_T2_STAGE_ACTIVE;
        break;
    case NP_PBM1064_STAGE_RAMP_DOWN:
        ctx->stage = NP_T2_STAGE_RAMP_DOWN;
        break;
    case NP_PBM1064_STAGE_COMPLETE:
        /* Inner session done — shut down 1170nm subsystem. */
        np_pbm1064_hal_t2_throttle_request(0U);
        ctx->t2_1170_active = false;
        ctx->stage = NP_T2_STAGE_COMPLETE;
        break;
    case NP_PBM1064_STAGE_FAULT:
        np_pbm1064_hal_t2_throttle_request(0U);
        ctx->t2_1170_active = false;
        ctx->stage = NP_T2_STAGE_FAULT;
        break;
    default:
        break;
    }

    return NP_PBM1064_OK;
}

void np_pbm1064_t2_apply_thermal_throttle(np_pbm1064_t2_ctx_t *ctx,
                                             float throttle_fraction)
{
    if (throttle_fraction <= 0.0f || throttle_fraction > 1.0f) { return; }

    /*
     * Priority cascade:
     * Step 1: throttle 1170nm laser first (deepest, highest subcortical heat).
     */
    if (!ctx->throttle_1170_applied && ctx->t2_1170_active) {
        uint8_t new_pct = (uint8_t)((float)ctx->t2_1170_throttle_pct *
                                    (1.0f - throttle_fraction));
        np_pbm1064_hal_t2_throttle_request(new_pct);
        ctx->t2_1170_throttle_pct  = new_pct;
        ctx->throttle_1170_applied = true;
        return;
    }

    /*
     * Step 2: throttle 1064nm CH_C (on-module).
     * Delegate to dose apply_throttle which handles CH_C → CH_B → CH_A.
     */
    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        if (!(ctx->pbm1064.desc.smart_module_mask & (uint8_t)(1U << s))) {
            continue;
        }
        float agg = np_pbm1064_dose_aggregate_irradiance(&ctx->pbm1064.dose[s]);
        if (agg > 0.0f) {
            np_pbm1064_dose_apply_throttle(s, agg * (1.0f + throttle_fraction),
                                            &ctx->pbm1064.drv[s]);
        }
    }

    ctx->throttle_ch_c_applied = true;
}

np_pbm1064_status_t np_pbm1064_t2_abort(np_pbm1064_t2_ctx_t *ctx,
                                           np_pbm1064_fault_t   reason)
{
    if (ctx->stage == NP_T2_STAGE_IDLE) {
        return NP_PBM1064_ERR_NO_SESSION;
    }

    /* Abort inner 1064nm session. */
    np_pbm1064_session_abort(&ctx->pbm1064, reason);

    /* Shut down 1170nm laser immediately. */
    np_pbm1064_hal_t2_throttle_request(0U);
    ctx->t2_1170_active = false;

    ctx->stage = NP_T2_STAGE_FAULT;

    if (ctx->fault_cb) {
        ctx->fault_cb(0xFF, reason);
    }

    return NP_PBM1064_OK;
}

np_t2_stage_t np_pbm1064_t2_stage(const np_pbm1064_t2_ctx_t *ctx)
{
    return ctx->stage;
}
