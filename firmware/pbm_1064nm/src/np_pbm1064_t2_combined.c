/*
 * NeurOne 1064nm Smart Zone Module — T2 Combined Session
 * Document: NP-FW-PBM1064-001 Rev A §8 / NP-SES-1064-001 §6
 *
 * Three-tier penetration stack:
 *   660nm (surface, CH_A) → 1064nm (cortical, CH_C) → 1170nm (subcortical, TEC laser)
 *
 * Thermal throttle priority:
 *   1. 1170nm laser  2. 1064nm CH_C  3. 808nm CH_B  4. 660nm CH_A
 *
 * Combined ramp: both subsystems ramp in parallel from a single safety MCU enable.
 * UHDR: per-zone 1064nm dose + 1170nm dose + sLORETA MNI target.
 * SHDR: device health metrics only (no user biology).
 */

#include "np_pbm1064_t2_combined.h"
#include "np_pbm1064_hal.h"
#include "np_pbm1064_drive.h"
#include "np_pbm1064_dose.h"
#include <string.h>

/* ── Internal helpers ───────────────────────────────────────────────────────── */

static void laser_1170_shutdown(np_pbm1064_t2_ctx_t *ctx)
{
    np_pbm1064_hal_t2_1170_set_duty(0U);
    np_pbm1064_hal_t2_1170_enable(false);
    np_pbm1064_hal_t2_throttle_request(0U);
    ctx->t2_1170_active     = false;
    ctx->t2_1170_duty_pct   = 0U;
}

static void read_final_1170_dose(np_pbm1064_t2_ctx_t *ctx)
{
    float dose = 0.0f;
    if (np_pbm1064_hal_t2_1170_get_dose(&dose) == NP_PBM1064_OK) {
        ctx->combined_record.dose_1170_J_cm2 = dose;
    }
}

static void write_combined_shdr(const np_pbm1064_t2_ctx_t *ctx,
                                 np_pbm1064_fault_t fault_reason,
                                 uint32_t duration_s)
{
    np_t2_combined_shdr_summary_t shdr;
    memset(&shdr, 0, sizeof(shdr));

    /* 1064nm device health (inner session summary). */
    shdr.pbm1064_shdr.smart_module_mask  = ctx->pbm1064.desc.smart_module_mask;
    shdr.pbm1064_shdr.duration_s         = duration_s;
    shdr.pbm1064_shdr.abort_reason       = (uint8_t)fault_reason;
    shdr.pbm1064_shdr.fault_reason       = (uint8_t)fault_reason;
    shdr.pbm1064_shdr.i2c_probe_pass_mask = ctx->pbm1064.desc.smart_module_mask;
    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        shdr.pbm1064_shdr.pd_ratio_zone[s] = ctx->pbm1064.dose[s].ratio_current;
    }

    /* 1170nm device health (no user biology). */
    shdr.tec_throttle_events = (uint8_t)(ctx->combined_record.throttle_events_1170
                                          > 255U ? 255U :
                                          ctx->combined_record.throttle_events_1170);
    shdr.laser_fault_flag    = (fault_reason != NP_PBM1064_FAULT_NONE) ? 1U : 0U;
    shdr.sloreta_session_flag = ctx->sloreta_valid ? 1U : 0U;
    shdr.abort_reason         = (uint8_t)fault_reason;
    shdr.duration_s           = duration_s;

    /* SHDR write — stub: real HAL writes to SHDR LittleFS file. */
    (void)shdr;
}

/* ── Public API ─────────────────────────────────────────────────────────────── */

void np_pbm1064_t2_init(np_pbm1064_t2_ctx_t         *ctx,
                          np_pbm1064_session_end_cb_t  end_cb,
                          np_pbm1064_fault_cb_t        fault_cb,
                          uint32_t                     device_session_count)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->session_end_cb      = end_cb;
    ctx->fault_cb            = fault_cb;
    ctx->t2_1170_duty_pct    = 0U;
    ctx->t2_1170_tec_temp_c  = 0.0f;
    ctx->stage               = NP_T2_STAGE_IDLE;

    np_pbm1064_session_init(&ctx->pbm1064,
                              end_cb, fault_cb,
                              NULL,
                              device_session_count);
}

np_pbm1064_status_t np_pbm1064_t2_start_combined(
    np_pbm1064_t2_ctx_t           *ctx,
    const np_t2_combined_desc_t   *desc)
{
    if (ctx->stage != NP_T2_STAGE_IDLE) {
        return NP_PBM1064_ERR_SESSION_ACTIVE;
    }
    if (!desc || desc->version != NP_T2_COMBINED_VERSION) {
        return NP_PBM1064_ERR_SIG_INVALID;
    }

    ctx->stage = NP_T2_STAGE_PREFLIGHT;

    /* Reset throttle cascade flags. */
    ctx->throttle_1170_applied = false;
    ctx->throttle_ch_c_applied = false;
    ctx->throttle_ch_b_applied = false;
    ctx->throttle_ch_a_applied = false;

    /* Zero combined UHDR record. */
    memset(&ctx->combined_record, 0, sizeof(ctx->combined_record));

    /* Read sLORETA MNI target before session starts (OI-SES-T2-01). */
    ctx->sloreta_valid = false;
    if (desc->sloreta_enable) {
        bool valid = false;
        np_pbm1064_hal_t2_sloreta_get_target(&ctx->sloreta_mni_x,
                                               &ctx->sloreta_mni_y,
                                               &ctx->sloreta_mni_z,
                                               &valid);
        ctx->sloreta_valid = valid;
    }

    /* Log sLORETA target into UHDR record. */
    ctx->combined_record.sloreta_mni_x = ctx->sloreta_mni_x;
    ctx->combined_record.sloreta_mni_y = ctx->sloreta_mni_y;
    ctx->combined_record.sloreta_mni_z = ctx->sloreta_mni_z;
    ctx->combined_record.sloreta_valid  = ctx->sloreta_valid ? 1U : 0U;

    /* Start 1064nm inner session (preflight + ramp). */
    np_pbm1064_status_t rc = np_pbm1064_session_start(&ctx->pbm1064,
                                                         &desc->pbm1064);
    if (rc != NP_PBM1064_OK) {
        ctx->stage = NP_T2_STAGE_FAULT;
        return rc;
    }

    /* Enable and ramp 1170nm laser in parallel if requested. */
    if (desc->t2_combined_enable) {
        rc = np_pbm1064_hal_t2_1170_enable(true);
        if (rc != NP_PBM1064_OK) {
            np_pbm1064_session_abort(&ctx->pbm1064, NP_PBM1064_FAULT_SAFETY_MCU);
            ctx->stage = NP_T2_STAGE_FAULT;
            return rc;
        }
        ctx->t2_1170_target_duty_pct = desc->laser1170.duty_pct > 100U ?
                                        100U : desc->laser1170.duty_pct;
        /* Ramp starts at 0%; session_tick drives the ramp. */
        np_pbm1064_hal_t2_1170_set_duty(0U);
        ctx->t2_1170_active   = true;
        ctx->t2_1170_duty_pct = 0U;

        /* Legacy throttle request API — signal full-power intent to T2 module. */
        np_pbm1064_hal_t2_throttle_request(ctx->t2_1170_target_duty_pct);
    }

    ctx->session_start_ms    = np_pbm1064_hal_now_ms();
    ctx->t2_1170_tec_poll_ms = ctx->session_start_ms;
    ctx->stage = NP_T2_STAGE_RAMP_UP;

    return NP_PBM1064_OK;
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

    memset(&ctx->combined_record, 0, sizeof(ctx->combined_record));
    ctx->sloreta_valid = false;

    np_pbm1064_status_t rc = np_pbm1064_session_start(&ctx->pbm1064, desc_1064);
    if (rc != NP_PBM1064_OK) {
        ctx->stage = NP_T2_STAGE_FAULT;
        return rc;
    }

    /* Legacy start: activate 1170nm at 100% target. */
    np_pbm1064_hal_t2_throttle_request(100U);
    ctx->t2_1170_active          = true;
    ctx->t2_1170_duty_pct        = 0U;
    ctx->t2_1170_target_duty_pct = 100U;

    ctx->session_start_ms    = np_pbm1064_hal_now_ms();
    ctx->t2_1170_tec_poll_ms = ctx->session_start_ms;
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

    /* ── 1170nm ramp tracking (parallel with 1064nm ramp) ─────────────────── */
    if (ctx->t2_1170_active &&
        (ctx->stage == NP_T2_STAGE_RAMP_UP ||
         ctx->stage == NP_T2_STAGE_ACTIVE)) {

        uint32_t elapsed_ms = now_ms - ctx->session_start_ms;
        uint32_t ramp_ms    = NP_T2_1170_RAMP_DURATION_S * 1000U;

        if (elapsed_ms < ramp_ms) {
            /* Linear ramp 0 → target_duty over 30 s. */
            uint8_t ramp_duty = (uint8_t)(
                (float)ctx->t2_1170_target_duty_pct *
                ((float)elapsed_ms / (float)ramp_ms));
            if (ramp_duty != ctx->t2_1170_duty_pct) {
                np_pbm1064_hal_t2_1170_set_duty(ramp_duty);
                ctx->t2_1170_duty_pct = ramp_duty;
            }
        } else if (ctx->t2_1170_duty_pct != ctx->t2_1170_target_duty_pct) {
            /* Ramp complete — set full target duty. */
            np_pbm1064_hal_t2_1170_set_duty(ctx->t2_1170_target_duty_pct);
            ctx->t2_1170_duty_pct = ctx->t2_1170_target_duty_pct;
        }
    }

    /* ── 1170nm ramp-down when inner session enters RAMP_DOWN ─────────────── */
    if (ctx->t2_1170_active &&
        ctx->stage == NP_T2_STAGE_RAMP_DOWN) {
        np_pbm1064_stage_t inner = np_pbm1064_session_stage(&ctx->pbm1064);
        if (inner == NP_PBM1064_STAGE_RAMP_DOWN) {
            /* Mirror ramp-down: reduce 1170nm duty symmetrically. */
            uint8_t rd = (uint8_t)((float)ctx->t2_1170_duty_pct * 0.9f);
            np_pbm1064_hal_t2_1170_set_duty(rd);
            ctx->t2_1170_duty_pct = rd;
        }
    }

    /* ── TEC temperature poll (1 Hz) ──────────────────────────────────────── */
    if (ctx->t2_1170_active &&
        (now_ms - ctx->t2_1170_tec_poll_ms >= NP_T2_1170_STATUS_POLL_MS)) {

        ctx->t2_1170_tec_poll_ms = now_ms;

        float tec_temp = 0.0f;
        if (np_pbm1064_hal_t2_1170_get_temp(&tec_temp) == NP_PBM1064_OK) {
            ctx->t2_1170_tec_temp_c = tec_temp;

            if (tec_temp >= (float)NP_T2_1170_TEC_CUTOFF_C) {
                /* TEC over-temperature: abort combined session. */
                np_pbm1064_t2_abort(ctx, NP_PBM1064_FAULT_THERMAL);
                return NP_PBM1064_ERR_THERMAL;
            } else if (tec_temp >= (float)NP_T2_1170_TEC_FAULT_C) {
                /* TEC approaching limit: throttle 1170nm first (priority step 1). */
                np_pbm1064_t2_apply_thermal_throttle(ctx, 0.25f);
                ctx->combined_record.throttle_events_1170++;
            }
        }
    }

    /* ── Delegate dose and ramp to inner 1064nm session ───────────────────── */
    np_pbm1064_status_t rc = np_pbm1064_session_tick(&ctx->pbm1064, now_ms);

    if (rc == NP_PBM1064_ERR_THERMAL) {
        np_pbm1064_t2_abort(ctx, NP_PBM1064_FAULT_THERMAL);
        return rc;
    }

    /* ── Track inner session stage → update T2 combined stage ─────────────── */
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

    case NP_PBM1064_STAGE_COMPLETE: {
        /* Inner 1064nm session complete: shut down 1170nm and write records. */
        read_final_1170_dose(ctx);
        laser_1170_shutdown(ctx);

        /* Populate combined UHDR record from inner session record. */
        ctx->combined_record.pbm1064_record   = ctx->pbm1064.record;
        ctx->combined_record.abort_reason     = 0U;
        ctx->combined_record.duration_s       = ctx->pbm1064.desc.duration_s;

        uint32_t duration_s = ctx->pbm1064.desc.duration_s;
        write_combined_shdr(ctx, NP_PBM1064_FAULT_NONE, duration_s);

        ctx->stage = NP_T2_STAGE_COMPLETE;
        break;
    }

    case NP_PBM1064_STAGE_FAULT:
        read_final_1170_dose(ctx);
        laser_1170_shutdown(ctx);
        ctx->combined_record.abort_reason = ctx->pbm1064.record.abort_reason;
        write_combined_shdr(ctx,
            (np_pbm1064_fault_t)ctx->pbm1064.record.abort_reason, 0U);
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
     * Priority cascade — each call applies the next unthrottled step.
     *
     * Step 1: 1170nm laser (deepest; highest subcortical heat deposition).
     */
    if (!ctx->throttle_1170_applied) {
        if (ctx->t2_1170_active) {
            float new_pct = (float)ctx->t2_1170_duty_pct * (1.0f - throttle_fraction);
            uint8_t duty = (uint8_t)(new_pct < 0.0f ? 0.0f : new_pct);
            np_pbm1064_hal_t2_1170_set_duty(duty);
            ctx->t2_1170_duty_pct = duty;
            np_pbm1064_hal_t2_throttle_request(duty);
        }
        ctx->throttle_1170_applied = true;
        return;
    }

    /*
     * Step 2: 1064nm CH_C (deepest smart zone channel).
     */
    if (!ctx->throttle_ch_c_applied) {
        for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
            if (!(ctx->pbm1064.desc.smart_module_mask & (uint8_t)(1U << s))) {
                continue;
            }
            if (!(ctx->pbm1064.drv[s].ch_enable & NP_PBM1064_CH_C_EN)) { continue; }
            uint8_t duty = ctx->pbm1064.drv[s].duty[2];
            uint8_t new_duty = (uint8_t)((float)duty * (1.0f - throttle_fraction));
            np_pbm1064_drive_set_duty(s, &ctx->pbm1064.drv[s],
                                       NP_PBM1064_CH_C_EN, new_duty);
            ctx->combined_record.throttle_events_ch_c++;
        }
        ctx->throttle_ch_c_applied = true;
        return;
    }

    /*
     * Step 3: 808nm CH_B.
     */
    if (!ctx->throttle_ch_b_applied) {
        for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
            if (!(ctx->pbm1064.desc.smart_module_mask & (uint8_t)(1U << s))) {
                continue;
            }
            if (!(ctx->pbm1064.drv[s].ch_enable & NP_PBM1064_CH_B_EN)) { continue; }
            uint8_t duty = ctx->pbm1064.drv[s].duty[1];
            uint8_t new_duty = (uint8_t)((float)duty * (1.0f - throttle_fraction));
            np_pbm1064_drive_set_duty(s, &ctx->pbm1064.drv[s],
                                       NP_PBM1064_CH_B_EN, new_duty);
        }
        ctx->throttle_ch_b_applied = true;
        return;
    }

    /*
     * Step 4: 660nm CH_A (shallowest — last resort).
     */
    if (!ctx->throttle_ch_a_applied) {
        for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
            if (!(ctx->pbm1064.desc.smart_module_mask & (uint8_t)(1U << s))) {
                continue;
            }
            if (!(ctx->pbm1064.drv[s].ch_enable & NP_PBM1064_CH_A_EN)) { continue; }
            uint8_t duty = ctx->pbm1064.drv[s].duty[0];
            uint8_t new_duty = (uint8_t)((float)duty * (1.0f - throttle_fraction));
            np_pbm1064_drive_set_duty(s, &ctx->pbm1064.drv[s],
                                       NP_PBM1064_CH_A_EN, new_duty);
        }
        ctx->throttle_ch_a_applied = true;
    }
    /* All steps exhausted — no further throttle action available. */
}

np_pbm1064_status_t np_pbm1064_t2_abort(np_pbm1064_t2_ctx_t *ctx,
                                           np_pbm1064_fault_t   reason)
{
    if (ctx->stage == NP_T2_STAGE_IDLE) {
        return NP_PBM1064_ERR_NO_SESSION;
    }

    /* Abort inner 1064nm session (disables all smart module channels). */
    np_pbm1064_session_abort(&ctx->pbm1064, reason);

    /* Disable 1170nm laser immediately. */
    read_final_1170_dose(ctx);
    laser_1170_shutdown(ctx);

    ctx->combined_record.abort_reason = (uint8_t)reason;
    write_combined_shdr(ctx, reason, 0U);

    ctx->stage = NP_T2_STAGE_FAULT;

    if (ctx->fault_cb) {
        ctx->fault_cb(0xFFU, reason);
    }

    return NP_PBM1064_OK;
}

np_t2_stage_t np_pbm1064_t2_stage(const np_pbm1064_t2_ctx_t *ctx)
{
    return ctx->stage;
}

const np_t2_combined_uhdr_record_t *np_pbm1064_t2_get_combined_record(
    const np_pbm1064_t2_ctx_t *ctx)
{
    return &ctx->combined_record;
}
