/*
 * NeurOne 1064nm Smart Zone Module — Session Orchestration
 * Document: NP-FW-PBM1064-001 Rev A §7
 *
 * Session stages: PREFLIGHT → RAMP_UP (30 s) → ACTIVE → RAMP_DOWN (30 s) → COMPLETE
 * Tick rate: 100 ms (10 Hz dose tick).
 * NTC poll: 1 Hz.  I2C status poll: 5 s.  EEG-adaptive: 1 Hz input.
 */

#include "np_pbm1064_session.h"
#include "np_pbm1064_hal.h"
#include <string.h>

/* ── Internal helpers ───────────────────────────────────────────────────────── */

static bool slot_in_mask(const np_pbm1064_session_ctx_t *ctx, uint8_t slot)
{
    return (ctx->desc.smart_module_mask & (uint8_t)(1U << slot)) != 0U;
}

/*
 * Ramp duty for all smart slots.
 * tick_count: number of 100 ms ticks elapsed since ramp start.
 * target_duty: from zone preset (already clamped).
 */
static void ramp_tick(np_pbm1064_session_ctx_t *ctx, uint32_t ramp_elapsed_ms)
{
    float progress = (float)ramp_elapsed_ms /
                     (float)(NP_PBM1064_RAMP_DURATION_S * 1000U);
    if (progress > 1.0f) { progress = 1.0f; }

    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        if (!slot_in_mask(ctx, s)) { continue; }
        uint8_t target = ctx->desc.zone[s].duty;
        uint8_t duty   = (uint8_t)((float)target * progress);
        np_pbm1064_drive_set_duty(s, &ctx->drv[s],
                                   ctx->desc.zone[s].channel_mask, duty);
    }
}

static void ramp_down_tick(np_pbm1064_session_ctx_t *ctx, uint32_t ramp_elapsed_ms)
{
    float progress = (float)ramp_elapsed_ms /
                     (float)(NP_PBM1064_RAMP_DURATION_S * 1000U);
    if (progress > 1.0f) { progress = 1.0f; }
    float remain = 1.0f - progress;

    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        if (!slot_in_mask(ctx, s)) { continue; }
        uint8_t target = ctx->desc.zone[s].duty;
        uint8_t duty   = (uint8_t)((float)target * remain);
        np_pbm1064_drive_set_duty(s, &ctx->drv[s],
                                   ctx->desc.zone[s].channel_mask, duty);
    }
}

static void disable_all_slots(np_pbm1064_session_ctx_t *ctx)
{
    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        if (!slot_in_mask(ctx, s)) { continue; }
        np_pbm1064_drive_disable_all(s, &ctx->drv[s]);
        np_pbm1064_hal_safety_mcu_enable(s, false);
    }
}

static void write_shdr_summary(const np_pbm1064_session_ctx_t *ctx,
                                np_pbm1064_fault_t fault_reason)
{
    np_pbm1064_shdr_summary_t shdr = {
        .smart_module_mask  = ctx->desc.smart_module_mask,
        .duration_s         = ctx->record.duration_s,
        .abort_reason       = (uint8_t)ctx->record.abort_reason,
        .fault_reason       = (uint8_t)fault_reason,
        .cal_source         = 0, /* all DEFAULT in this revision */
        .ocp_event_count    = 0,
        .thermal_event_count = ctx->ch_c_throttled || ctx->ch_b_throttled ? 1U : 0U,
        .i2c_probe_pass_mask = ctx->desc.smart_module_mask,
    };
    /* Mean PD1/PD2 ratio per zone (device metric — no user biology). */
    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        shdr.pd_ratio_zone[s] = ctx->dose[s].ratio_current;
    }
    /* SHDR write — stub writes to log; real HAL writes to SHDR LittleFS file. */
    (void)shdr; /* suppress unused warning in stub; HAL call omitted: OI-PBM pending */
}

/* ── Public API ─────────────────────────────────────────────────────────────── */

void np_pbm1064_session_init(np_pbm1064_session_ctx_t    *ctx,
                               np_pbm1064_session_end_cb_t  end_cb,
                               np_pbm1064_fault_cb_t        fault_cb,
                               np_pbm1064_display_cb_t      display_cb,
                               uint32_t                     device_session_count)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->session_end_cb       = end_cb;
    ctx->fault_cb             = fault_cb;
    ctx->display_cb           = display_cb;
    ctx->device_session_count = device_session_count;
    ctx->stage                = NP_PBM1064_STAGE_IDLE;
}

np_pbm1064_status_t np_pbm1064_session_start(
    np_pbm1064_session_ctx_t          *ctx,
    const np_pbm1064_session_desc_t   *desc)
{
    if (ctx->stage != NP_PBM1064_STAGE_IDLE) {
        return NP_PBM1064_ERR_SESSION_ACTIVE;
    }

    /* Signature verification (real implementation in np_signature.c). */
    /* Stub: always accepts — signature check is OI-PBM-SIG pending bootloader HAL. */

    if (desc->version != NP_SES1064_VERSION) {
        return NP_PBM1064_ERR_SIG_INVALID;
    }

    ctx->stage = NP_PBM1064_STAGE_PREFLIGHT;
    ctx->desc  = *desc;
    memset(&ctx->record, 0, sizeof(ctx->record));
    ctx->record.session_start_unix = 0; /* populated by HAL (RTC not yet stubbed) */
    ctx->record.smart_module_mask  = desc->smart_module_mask;
    ctx->record.eeg_adaptive_mode  = desc->eeg_adaptive_mode;

    /* Load calibration coefficients. */
    np_pbm1064_dose_load_cal(ctx->cal);

    /* Reset dose state for all zones. */
    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        np_pbm1064_dose_reset(&ctx->dose[s]);
    }

    /* Preflight: startup each smart module slot. */
    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        if (!slot_in_mask(ctx, s)) { continue; }

        np_pbm1064_status_t rc = np_pbm1064_drive_startup(s, &ctx->drv[s],
                                                            &ctx->desc.zone[s]);
        if (rc != NP_PBM1064_OK) {
            disable_all_slots(ctx);
            ctx->stage = NP_PBM1064_STAGE_FAULT;
            return rc;
        }

        /* Request safety MCU enable for this slot. */
        rc = np_pbm1064_hal_safety_mcu_enable(s, true);
        if (rc != NP_PBM1064_OK) {
            disable_all_slots(ctx);
            ctx->stage = NP_PBM1064_STAGE_FAULT;
            return NP_PBM1064_ERR_SAFETY_REJECTED;
        }
    }

    /* Preflight complete — begin ramp. */
    uint32_t now_ms = np_pbm1064_hal_now_ms();
    ctx->ramp_start_ms   = now_ms;
    ctx->last_dose_tick_ms = now_ms;
    ctx->last_i2c_poll_ms  = now_ms;
    ctx->last_ntc_poll_ms  = now_ms;
    ctx->stage = NP_PBM1064_STAGE_RAMP_UP;

    return NP_PBM1064_OK;
}

np_pbm1064_status_t np_pbm1064_session_tick(np_pbm1064_session_ctx_t *ctx,
                                               uint32_t now_ms)
{
    if (ctx->stage == NP_PBM1064_STAGE_IDLE ||
        ctx->stage == NP_PBM1064_STAGE_COMPLETE ||
        ctx->stage == NP_PBM1064_STAGE_FAULT) {
        return NP_PBM1064_ERR_NO_SESSION;
    }

    /* ── Dose tick (10 Hz) ─────────────────────────────────────────────────── */
    if (now_ms - ctx->last_dose_tick_ms >= NP_PBM1064_DOSE_TICK_MS) {
        ctx->last_dose_tick_ms = now_ms;

        for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
            if (!slot_in_mask(ctx, s)) { continue; }

            np_pbm1064_status_t rc = np_pbm1064_dose_tick(s, ctx->cal[s],
                                                            &ctx->dose[s]);
            if (rc == NP_PBM1064_ERR_DOSE_LIMIT) {
                /* Disable the channel that hit its limit. */
                for (uint8_t w = 0; w < NP_PBM1064_WL_COUNT; w++) {
                    if (ctx->dose[s].dose_limit_hit[w]) {
                        uint8_t ch_bit = (w == 0) ? NP_PBM1064_CH_A_EN :
                                         (w == 1) ? NP_PBM1064_CH_B_EN :
                                                     NP_PBM1064_CH_C_EN;
                        np_pbm1064_drive_set_ch_enable(
                            s, &ctx->drv[s],
                            ctx->drv[s].ch_enable & ~ch_bit);
                    }
                }
                ctx->record.dose_J_cm2[s][0] = ctx->dose[s].dose_J_cm2[0];
                ctx->record.dose_J_cm2[s][1] = ctx->dose[s].dose_J_cm2[1];
                ctx->record.dose_J_cm2[s][2] = ctx->dose[s].dose_J_cm2[2];
            }

            /* Aggregate irradiance ceiling check. */
            float agg = np_pbm1064_dose_aggregate_irradiance(&ctx->dose[s]);
            if (agg > NP_PBM1064_AGGREGATE_IRRADIANCE_MW_CM2) {
                np_pbm1064_dose_apply_throttle(s, agg, &ctx->drv[s]);
            }
        }
    }

    /* ── NTC poll (1 Hz) ───────────────────────────────────────────────────── */
    if (now_ms - ctx->last_ntc_poll_ms >= NP_PBM1064_NTC_POLL_MS) {
        ctx->last_ntc_poll_ms = now_ms;

        for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
            if (!slot_in_mask(ctx, s)) { continue; }
            float temp_c = 0.0f;
            if (np_pbm1064_hal_ntc_read(s, &temp_c) != NP_PBM1064_OK) { continue; }
            ctx->ntc_temp_c[s] = temp_c;

            if (temp_c >= (float)NP_PBM1064_THERMAL_CUTOFF_C) {
                /* Immediate all-off → FAULT. */
                np_pbm1064_session_abort(ctx, NP_PBM1064_FAULT_THERMAL);
                return NP_PBM1064_ERR_THERMAL;
            } else if (temp_c >= (float)NP_PBM1064_THERMAL_FAULT_C) {
                /* Throttle CH_C first, then CH_B. */
                if (!ctx->ch_c_throttled) {
                    np_pbm1064_drive_set_ch_enable(
                        s, &ctx->drv[s],
                        ctx->drv[s].ch_enable & ~NP_PBM1064_CH_C_EN);
                    ctx->ch_c_throttled = true;
                } else if (!ctx->ch_b_throttled) {
                    np_pbm1064_drive_set_ch_enable(
                        s, &ctx->drv[s],
                        ctx->drv[s].ch_enable & ~NP_PBM1064_CH_B_EN);
                    ctx->ch_b_throttled = true;
                }
            }
        }
    }

    /* ── I2C status poll (5 s) ─────────────────────────────────────────────── */
    if (now_ms - ctx->last_i2c_poll_ms >= NP_PBM1064_I2C_STATUS_POLL_MS) {
        ctx->last_i2c_poll_ms = now_ms;

        for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
            if (!slot_in_mask(ctx, s)) { continue; }
            np_pbm1064_status_t rc = np_pbm1064_drive_poll_status(
                s, &ctx->drv[s], ctx->device_session_count);
            if (rc == NP_PBM1064_ERR_THERMAL) {
                np_pbm1064_session_abort(ctx, NP_PBM1064_FAULT_THERMAL);
                return rc;
            }
        }
    }

    /* ── Stage transitions ─────────────────────────────────────────────────── */
    uint32_t elapsed_ms = now_ms - ctx->ramp_start_ms;
    uint32_t ramp_ms    = NP_PBM1064_RAMP_DURATION_S * 1000U;
    uint32_t active_ms  = (uint32_t)ctx->desc.duration_s * 1000U - 2U * ramp_ms;
    (void)active_ms;

    switch (ctx->stage) {
    case NP_PBM1064_STAGE_RAMP_UP:
        ramp_tick(ctx, elapsed_ms);
        if (elapsed_ms >= ramp_ms) {
            /* Set full duty for active stage. */
            for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
                if (!slot_in_mask(ctx, s)) { continue; }
                np_pbm1064_drive_set_duty(s, &ctx->drv[s],
                                           ctx->desc.zone[s].channel_mask,
                                           ctx->desc.zone[s].duty);
            }
            ctx->stage = NP_PBM1064_STAGE_ACTIVE;
        }
        break;

    case NP_PBM1064_STAGE_ACTIVE: {
        uint32_t session_total_ms = (uint32_t)ctx->desc.duration_s * 1000U;
        if (elapsed_ms >= session_total_ms - ramp_ms) {
            ctx->ramp_start_ms = now_ms;
            ctx->stage = NP_PBM1064_STAGE_RAMP_DOWN;
        }
        break;
    }

    case NP_PBM1064_STAGE_RAMP_DOWN:
        ramp_down_tick(ctx, now_ms - ctx->ramp_start_ms);
        if ((now_ms - ctx->ramp_start_ms) >= ramp_ms) {
            /* Session complete. */
            disable_all_slots(ctx);

            ctx->record.duration_s = ctx->desc.duration_s;
            ctx->record.abort_reason = 0;
            for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
                for (uint8_t w = 0; w < NP_PBM1064_WL_COUNT; w++) {
                    ctx->record.dose_J_cm2[s][w] = ctx->dose[s].dose_J_cm2[w];
                }
            }

            write_shdr_summary(ctx, NP_PBM1064_FAULT_NONE);
            ctx->stage = NP_PBM1064_STAGE_COMPLETE;

            if (ctx->session_end_cb) {
                ctx->session_end_cb(&ctx->record, NP_PBM1064_OK);
            }
        }
        break;

    default:
        break;
    }

    return NP_PBM1064_OK;
}

void np_pbm1064_session_update_eeg_freq(np_pbm1064_session_ctx_t *ctx,
                                          float dominant_freq_hz)
{
    if (ctx->stage != NP_PBM1064_STAGE_ACTIVE) { return; }
    if (ctx->desc.eeg_adaptive_mode == 0U) { return; }

    ctx->eeg_dominant_freq_hz = dominant_freq_hz;

    /* Uniform mode: update all smart slots to same freq code. */
    for (uint8_t s = 0; s < NP_PBM1064_ZONE_COUNT; s++) {
        if (!slot_in_mask(ctx, s)) { continue; }

        uint8_t new_code = np_pbm1064_drive_map_eeg_freq(
            dominant_freq_hz,
            ctx->current_freq_code,
            &ctx->eeg_tick_in_band[s]);

        if (new_code != 0xFF) {
            np_pbm1064_drive_set_freq(s, &ctx->drv[s],
                                       ctx->drv[s].ch_enable, new_code);
            ctx->current_freq_code = new_code;
            ctx->record.eeg_adapt_event_count++;
        }
    }
}

np_pbm1064_status_t np_pbm1064_session_abort(np_pbm1064_session_ctx_t *ctx,
                                               np_pbm1064_fault_t reason)
{
    if (ctx->stage == NP_PBM1064_STAGE_IDLE) {
        return NP_PBM1064_ERR_NO_SESSION;
    }

    disable_all_slots(ctx);

    ctx->record.abort_reason = (uint8_t)reason;
    ctx->record.duration_s   = 0; /* actual duration not tracked in fault path */

    write_shdr_summary(ctx, reason);
    ctx->stage = NP_PBM1064_STAGE_FAULT;

    if (ctx->fault_cb) {
        ctx->fault_cb(0xFF, reason);
    }
    if (ctx->session_end_cb) {
        ctx->session_end_cb(&ctx->record, NP_PBM1064_ERR_THERMAL);
    }

    return NP_PBM1064_OK;
}

np_pbm1064_stage_t np_pbm1064_session_stage(
    const np_pbm1064_session_ctx_t *ctx)
{
    return ctx->stage;
}
