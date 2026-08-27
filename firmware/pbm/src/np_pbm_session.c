/*
 * NeurOne 1064nm Smart Zone Module — Session Orchestration
 * Document: NP-FW-PBM1064-001 Rev 1 §7 / NP-SES-1064-001 §1 (v5)
 *
 * Session stages: PREFLIGHT → RAMP_UP (30 s) → ACTIVE → RAMP_DOWN (30 s) → COMPLETE
 * Tick rate: 100 ms (10 Hz dose tick).  NTC poll: 1 Hz.  I2C status poll: 5 s.
 * EEG-adaptive: 1 Hz input.
 *
 * v5 addresses sockets via a small list of (128-bit socket mask, preset)
 * groups (NP-HW-HUB-001 Rev 3 §10) rather than v4's 8-bit smart_module_mask +
 * fixed zone[5] array. np_pbm_session_desc_expand() flattens the groups
 * into an ascending, deduplicated active-socket list once at session start;
 * every tick loop below walks that flat list instead of a fixed zone range.
 */

#include "np_pbm_session.h"
#include "np_pbm_hal.h"
#include <string.h>

/* ── Socket → module UID resolver (OI-HUB-C06) ──────────────────────────────── */

static np_pbm_socket_uid_fn s_uid_resolver     = NULL;
static void                    *s_uid_resolver_ctx = NULL;

void np_pbm_session_set_uid_resolver(np_pbm_socket_uid_fn fn, void *ctx)
{
    s_uid_resolver     = fn;
    s_uid_resolver_ctx = ctx;
}

/* ── Socket mask bit helpers (128-bit, LSB-first, 0-based) ──────────────────── */

static bool mask_test(const uint8_t mask[NP_PBM_SOCKET_MASK_BYTES], uint8_t socket_id)
{
    return (mask[socket_id / 8U] & (uint8_t)(1U << (socket_id % 8U))) != 0U;
}

/* ── Wire encode/decode ───────────────────────────────────────────────────── */

size_t np_pbm_session_desc_wire_len(uint8_t group_count)
{
    if (group_count > NP_PBM_SESSION_MAX_PRESET_GROUPS) { return 0U; }
    return sizeof(np_pbm_session_desc_hdr_t) +
           (size_t)group_count * sizeof(np_pbm_preset_group_t) +
           64U;
}

size_t np_pbm_session_desc_signed_len(uint8_t group_count)
{
    size_t wire_len = np_pbm_session_desc_wire_len(group_count);
    return (wire_len == 0U) ? 0U : (wire_len - 64U);
}

size_t np_pbm_session_desc_serialize(const np_pbm_session_desc_t *desc,
                                          uint8_t *out, size_t out_cap)
{
    if (!desc || !out) { return 0U; }

    uint8_t group_count = desc->hdr.group_count;
    size_t  wire_len     = np_pbm_session_desc_wire_len(group_count);
    if (wire_len == 0U || out_cap < wire_len) { return 0U; }

    size_t off = 0U;
    memcpy(out + off, &desc->hdr, sizeof(desc->hdr));
    off += sizeof(desc->hdr);

    memcpy(out + off, desc->groups, (size_t)group_count * sizeof(np_pbm_preset_group_t));
    off += (size_t)group_count * sizeof(np_pbm_preset_group_t);

    memcpy(out + off, desc->signature, sizeof(desc->signature));
    off += sizeof(desc->signature);

    return off;
}

np_pbm_status_t np_pbm_session_desc_parse(const uint8_t *in, size_t in_len,
                                                    np_pbm_session_desc_t *desc_out)
{
    if (!in || !desc_out || in_len < sizeof(np_pbm_session_desc_hdr_t)) {
        return NP_PBM_ERR_INVALID_ARG;
    }

    memset(desc_out, 0, sizeof(*desc_out));

    size_t off = 0U;
    memcpy(&desc_out->hdr, in + off, sizeof(desc_out->hdr));
    off += sizeof(desc_out->hdr);

    uint8_t group_count = desc_out->hdr.group_count;
    if (group_count > NP_PBM_SESSION_MAX_PRESET_GROUPS) {
        return NP_PBM_ERR_INVALID_ARG;
    }

    size_t expected_len = np_pbm_session_desc_wire_len(group_count);
    if (in_len != expected_len) {
        /* Reject a short or padded blob rather than truncate/zero-extend it —
         * the signed span is a function of group_count, and accepting a
         * mismatched length would let the caller's idea of what was signed
         * diverge from what actually gets parsed and acted on. */
        return NP_PBM_ERR_INVALID_ARG;
    }

    memcpy(desc_out->groups, in + off,
           (size_t)group_count * sizeof(np_pbm_preset_group_t));
    off += (size_t)group_count * sizeof(np_pbm_preset_group_t);

    memcpy(desc_out->signature, in + off, sizeof(desc_out->signature));

    return NP_PBM_OK;
}

np_pbm_status_t np_pbm_session_desc_expand(
    const np_pbm_session_desc_t *desc,
    uint8_t              *active_socket_id_out,
    np_pbm_preset_t  *active_preset_out,
    uint8_t              *active_socket_count_out,
    uint8_t               capacity)
{
    if (!desc || !active_socket_id_out || !active_preset_out || !active_socket_count_out) {
        return NP_PBM_ERR_INVALID_ARG;
    }

    uint8_t count = 0U;
    uint32_t socket_domain = (uint32_t)NP_PBM_SOCKET_MASK_BYTES * 8U;

    for (uint32_t socket_id = 0U; socket_id < socket_domain; socket_id++) {
        /* First group whose mask includes this socket wins (signed order). */
        for (uint8_t g = 0U; g < desc->hdr.group_count; g++) {
            if (!mask_test(desc->groups[g].mask, (uint8_t)socket_id)) { continue; }

            if (count >= capacity) {
                /* Fail closed: do not silently drive a subset of what the
                 * signed descriptor requested. */
                *active_socket_count_out = 0U;
                return NP_PBM_ERR_INVALID_ARG;
            }
            active_socket_id_out[count] = (uint8_t)socket_id;
            active_preset_out[count]    = desc->groups[g].preset;
            count++;
            break;
        }
    }

    *active_socket_count_out = count;
    return NP_PBM_OK;
}

/* ── Internal helpers ───────────────────────────────────────────────────────── */

/*
 * Ramp duty for all active sockets.
 * ramp_elapsed_ms: time elapsed since ramp start.
 */
static void ramp_tick(np_pbm_session_ctx_t *ctx, uint32_t ramp_elapsed_ms)
{
    float progress = (float)ramp_elapsed_ms /
                     (float)(NP_PBM_RAMP_DURATION_S * 1000U);
    if (progress > 1.0f) { progress = 1.0f; }

    for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
        uint8_t target = ctx->active_preset[i].duty;
        uint8_t duty   = (uint8_t)((float)target * progress);
        np_pbm_drive_set_duty(ctx->active_socket_id[i], &ctx->drv[i],
                                   ctx->active_preset[i].channel_mask, duty);
    }
}

static void ramp_down_tick(np_pbm_session_ctx_t *ctx, uint32_t ramp_elapsed_ms)
{
    float progress = (float)ramp_elapsed_ms /
                     (float)(NP_PBM_RAMP_DURATION_S * 1000U);
    if (progress > 1.0f) { progress = 1.0f; }
    float remain = 1.0f - progress;

    for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
        uint8_t target = ctx->active_preset[i].duty;
        uint8_t duty   = (uint8_t)((float)target * remain);
        np_pbm_drive_set_duty(ctx->active_socket_id[i], &ctx->drv[i],
                                   ctx->active_preset[i].channel_mask, duty);
    }
}

static void disable_all_slots(np_pbm_session_ctx_t *ctx)
{
    for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
        np_pbm_drive_disable_all(ctx->active_socket_id[i], &ctx->drv[i]);
        np_pbm_hal_safety_mcu_enable(ctx->active_socket_id[i], false);
    }
}

void np_pbm_session_build_shdr_summary(const np_pbm_session_ctx_t *ctx,
                                            np_pbm_fault_t              fault_reason,
                                            np_pbm_shdr_summary_t      *out)
{
    if (ctx == NULL || out == NULL) { return; }

    memset(out, 0, sizeof(*out));
    out->active_socket_count = ctx->active_socket_count;
    out->duration_s          = ctx->record.duration_s;
    out->abort_reason        = (uint8_t)ctx->record.abort_reason;
    out->fault_reason        = (uint8_t)fault_reason;
    out->thermal_event_count = ctx->ch_c_throttled || ctx->ch_b_throttled ? 1U : 0U;

    /* Mean PD1/PD2 ratio, I2C probe pass and calibration provenance, per active
     * socket (device metrics — no user biology). cal_source is per socket, not
     * per session: calibration is keyed to module UID (OI-HUB-C06), so a
     * session can legitimately mix FACTORY and DEFAULT sockets, and this is the
     * shape the fleet schema's per-(socket, module_uid, wavelength) row wants. */
    for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
        out->sockets[i].socket_id      = ctx->active_socket_id[i];
        out->sockets[i].pd_ratio       = ctx->dose[i].ratio_current;
        out->sockets[i].i2c_probe_pass = ctx->drv[i].initialized ? 1U : 0U;
        out->sockets[i].cal_source     = ctx->active_cal_source[i];
    }
}

static void write_shdr_summary(const np_pbm_session_ctx_t *ctx,
                                np_pbm_fault_t fault_reason)
{
    np_pbm_shdr_summary_t shdr;
    np_pbm_session_build_shdr_summary(ctx, fault_reason, &shdr);
    /* SHDR write — stub writes to log; real HAL writes to SHDR LittleFS file. */
    (void)shdr; /* suppress unused warning in stub; HAL call omitted: OI-PBM pending */
}

/* ── Public API ─────────────────────────────────────────────────────────────── */

void np_pbm_session_init(np_pbm_session_ctx_t    *ctx,
                               np_pbm_session_end_cb_t  end_cb,
                               np_pbm_fault_cb_t        fault_cb,
                               np_pbm_display_cb_t      display_cb,
                               uint32_t                     device_session_count)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->session_end_cb       = end_cb;
    ctx->fault_cb             = fault_cb;
    ctx->display_cb           = display_cb;
    ctx->device_session_count = device_session_count;
    ctx->stage                = NP_PBM_STAGE_IDLE;
}

np_pbm_status_t np_pbm_session_start(
    np_pbm_session_ctx_t          *ctx,
    const np_pbm_session_desc_t   *desc)
{
    if (ctx->stage != NP_PBM_STAGE_IDLE) {
        return NP_PBM_ERR_SESSION_ACTIVE;
    }

    /* Signature verification (real implementation in np_signature.c). */
    /* Stub: always accepts — signature check is OI-PBM-SIG pending bootloader HAL. */

    if (desc->hdr.version != NP_SES1064_VERSION) {
        return NP_PBM_ERR_SIG_INVALID;
    }

    /* Expand the signed preset groups into a flat active-socket list before
     * touching any hardware — fail closed if the request exceeds capacity. */
    uint8_t active_socket_id[NP_PBM_SESSION_MAX_ACTIVE_SOCKETS];
    np_pbm_preset_t active_preset[NP_PBM_SESSION_MAX_ACTIVE_SOCKETS];
    uint8_t active_socket_count = 0U;

    np_pbm_status_t exp_rc = np_pbm_session_desc_expand(
        desc, active_socket_id, active_preset, &active_socket_count,
        NP_PBM_SESSION_MAX_ACTIVE_SOCKETS);
    if (exp_rc != NP_PBM_OK) {
        return exp_rc;
    }

    ctx->stage = NP_PBM_STAGE_PREFLIGHT;
    ctx->desc  = *desc;
    ctx->active_socket_count = active_socket_count;
    memcpy(ctx->active_socket_id, active_socket_id, sizeof(active_socket_id));
    memcpy(ctx->active_preset, active_preset, sizeof(active_preset));

    memset(&ctx->record, 0, sizeof(ctx->record));
    ctx->record.session_start_unix  = 0; /* populated by HAL (RTC not yet stubbed) */
    ctx->record.active_socket_count = active_socket_count;
    ctx->record.eeg_adaptive_mode   = desc->hdr.eeg_adaptive_mode;
    for (uint8_t i = 0; i < active_socket_count; i++) {
        ctx->record.sockets[i].socket_id = active_socket_id[i];
    }

    /*
     * Resolve each active socket's occupant, then load that MODULE's
     * calibration by UID (OI-HUB-C06). Never by socket_id: modules are
     * swappable under hex tiling, so a location-keyed load applies the
     * previous occupant's coefficients after a swap, invisibly, because
     * cal_source would still read FACTORY (NP-HW-HUB-001 Rev 3 §9.5).
     *
     * An unresolvable socket yields the zero UID, which np_pbm_dose_load_cal()
     * treats as "no coherent record" and answers with firmware defaults and
     * NP_CAL_DEFAULT. With no resolver installed that is every socket — i.e.
     * exactly the behaviour before this seam existed.
     */
    for (uint8_t i = 0; i < active_socket_count; i++) {
        memset(&ctx->active_uid[i], 0, sizeof(ctx->active_uid[i]));
        if (s_uid_resolver != NULL) {
            np_pbm_module_uid_t uid;
            memset(&uid, 0, sizeof(uid));
            if (s_uid_resolver(ctx->active_socket_id[i], &uid, s_uid_resolver_ctx)) {
                ctx->active_uid[i] = uid;
            }
        }
        ctx->active_cal_source[i] =
            (uint8_t)np_pbm_dose_load_cal(&ctx->active_uid[i], ctx->cal[i]);
    }

    /* Reset dose state for all active sockets. */
    for (uint8_t i = 0; i < active_socket_count; i++) {
        np_pbm_dose_reset(&ctx->dose[i]);
    }

    /* Preflight: startup each active socket. */
    for (uint8_t i = 0; i < active_socket_count; i++) {
        np_pbm_status_t rc = np_pbm_drive_startup(
            ctx->active_socket_id[i], &ctx->drv[i], &ctx->active_preset[i]);
        if (rc != NP_PBM_OK) {
            disable_all_slots(ctx);
            ctx->stage = NP_PBM_STAGE_FAULT;
            return rc;
        }

        /* Request safety MCU enable for this socket. */
        rc = np_pbm_hal_safety_mcu_enable(ctx->active_socket_id[i], true);
        if (rc != NP_PBM_OK) {
            disable_all_slots(ctx);
            ctx->stage = NP_PBM_STAGE_FAULT;
            return NP_PBM_ERR_SAFETY_REJECTED;
        }
    }

    /* Preflight complete — begin ramp. */
    uint32_t now_ms = np_pbm_hal_now_ms();
    ctx->ramp_start_ms   = now_ms;
    ctx->last_dose_tick_ms = now_ms;
    ctx->last_i2c_poll_ms  = now_ms;
    ctx->last_ntc_poll_ms  = now_ms;
    ctx->stage = NP_PBM_STAGE_RAMP_UP;

    return NP_PBM_OK;
}

np_pbm_status_t np_pbm_session_tick(np_pbm_session_ctx_t *ctx,
                                               uint32_t now_ms)
{
    if (ctx->stage == NP_PBM_STAGE_IDLE ||
        ctx->stage == NP_PBM_STAGE_COMPLETE ||
        ctx->stage == NP_PBM_STAGE_FAULT) {
        return NP_PBM_ERR_NO_SESSION;
    }

    /* ── Dose tick (10 Hz) ─────────────────────────────────────────────────── */
    if (now_ms - ctx->last_dose_tick_ms >= NP_PBM_DOSE_TICK_MS) {
        ctx->last_dose_tick_ms = now_ms;

        for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
            np_pbm_status_t rc = np_pbm_dose_tick(
                ctx->active_socket_id[i], ctx->cal[i], &ctx->dose[i]);
            if (rc == NP_PBM_ERR_DOSE_LIMIT) {
                /* Disable the channel that hit its limit. */
                for (uint8_t w = 0; w < NP_PBM_WL_COUNT; w++) {
                    if (ctx->dose[i].dose_limit_hit[w]) {
                        uint8_t ch_bit = (w == 0) ? NP_PBM_CH_A_EN :
                                         (w == 1) ? NP_PBM_CH_B_EN :
                                                     NP_PBM_CH_C_EN;
                        np_pbm_drive_set_ch_enable(
                            ctx->active_socket_id[i], &ctx->drv[i],
                            ctx->drv[i].ch_enable & ~ch_bit);
                    }
                }
                ctx->record.sockets[i].dose_J_cm2[0] = ctx->dose[i].dose_J_cm2[0];
                ctx->record.sockets[i].dose_J_cm2[1] = ctx->dose[i].dose_J_cm2[1];
                ctx->record.sockets[i].dose_J_cm2[2] = ctx->dose[i].dose_J_cm2[2];
            }

            /* Aggregate irradiance ceiling check. */
            float agg = np_pbm_dose_aggregate_irradiance(&ctx->dose[i]);
            if (agg > NP_PBM_AGGREGATE_IRRADIANCE_MW_CM2) {
                np_pbm_dose_apply_throttle(ctx->active_socket_id[i], agg, &ctx->drv[i]);
            }
        }
    }

    /* ── NTC poll (1 Hz) ───────────────────────────────────────────────────── */
    if (now_ms - ctx->last_ntc_poll_ms >= NP_PBM_NTC_POLL_MS) {
        ctx->last_ntc_poll_ms = now_ms;

        for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
            float temp_c = 0.0f;
            if (np_pbm_hal_ntc_read(ctx->active_socket_id[i], &temp_c) != NP_PBM_OK) { continue; }
            ctx->ntc_temp_c[i] = temp_c;

            if (temp_c >= (float)NP_PBM_THERMAL_CUTOFF_C) {
                /* Immediate all-off → FAULT. */
                np_pbm_session_stop(ctx, NP_PBM_FAULT_THERMAL);
                return NP_PBM_ERR_THERMAL;
            } else if (temp_c >= (float)NP_PBM_THERMAL_FAULT_C) {
                /* Throttle CH_C first, then CH_B. */
                if (!ctx->ch_c_throttled) {
                    np_pbm_drive_set_ch_enable(
                        ctx->active_socket_id[i], &ctx->drv[i],
                        ctx->drv[i].ch_enable & ~NP_PBM_CH_C_EN);
                    ctx->ch_c_throttled = true;
                } else if (!ctx->ch_b_throttled) {
                    np_pbm_drive_set_ch_enable(
                        ctx->active_socket_id[i], &ctx->drv[i],
                        ctx->drv[i].ch_enable & ~NP_PBM_CH_B_EN);
                    ctx->ch_b_throttled = true;
                }
            }
        }
    }

    /* ── I2C status poll (5 s) ─────────────────────────────────────────────── */
    if (now_ms - ctx->last_i2c_poll_ms >= NP_PBM_I2C_STATUS_POLL_MS) {
        ctx->last_i2c_poll_ms = now_ms;

        for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
            np_pbm_status_t rc = np_pbm_drive_poll_status(
                ctx->active_socket_id[i], &ctx->drv[i], ctx->device_session_count);
            if (rc == NP_PBM_ERR_THERMAL) {
                np_pbm_session_stop(ctx, NP_PBM_FAULT_THERMAL);
                return rc;
            }
        }
    }

    /* ── Stage transitions ─────────────────────────────────────────────────── */
    uint32_t elapsed_ms = now_ms - ctx->ramp_start_ms;
    uint32_t ramp_ms    = NP_PBM_RAMP_DURATION_S * 1000U;
    uint32_t active_ms  = (uint32_t)ctx->desc.hdr.duration_s * 1000U - 2U * ramp_ms;
    (void)active_ms;

    switch (ctx->stage) {
    case NP_PBM_STAGE_RAMP_UP:
        ramp_tick(ctx, elapsed_ms);
        if (elapsed_ms >= ramp_ms) {
            /* Set full duty for active stage. */
            for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
                np_pbm_drive_set_duty(ctx->active_socket_id[i], &ctx->drv[i],
                                           ctx->active_preset[i].channel_mask,
                                           ctx->active_preset[i].duty);
            }
            ctx->stage = NP_PBM_STAGE_ACTIVE;
        }
        break;

    case NP_PBM_STAGE_ACTIVE: {
        uint32_t session_total_ms = (uint32_t)ctx->desc.hdr.duration_s * 1000U;
        if (elapsed_ms >= session_total_ms - ramp_ms) {
            ctx->ramp_start_ms = now_ms;
            ctx->stage = NP_PBM_STAGE_RAMP_DOWN;
        }
        break;
    }

    case NP_PBM_STAGE_RAMP_DOWN:
        ramp_down_tick(ctx, now_ms - ctx->ramp_start_ms);
        if ((now_ms - ctx->ramp_start_ms) >= ramp_ms) {
            /* Session complete. */
            disable_all_slots(ctx);

            ctx->record.duration_s = ctx->desc.hdr.duration_s;
            ctx->record.abort_reason = 0;
            for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
                for (uint8_t w = 0; w < NP_PBM_WL_COUNT; w++) {
                    ctx->record.sockets[i].dose_J_cm2[w] = ctx->dose[i].dose_J_cm2[w];
                }
            }

            write_shdr_summary(ctx, NP_PBM_FAULT_NONE);
            ctx->stage = NP_PBM_STAGE_COMPLETE;

            if (ctx->session_end_cb) {
                ctx->session_end_cb(&ctx->record, NP_PBM_OK);
            }
        }
        break;

    default:
        break;
    }

    return NP_PBM_OK;
}

void np_pbm_session_update_eeg_freq(np_pbm_session_ctx_t *ctx,
                                          float dominant_freq_hz)
{
    if (ctx->stage != NP_PBM_STAGE_ACTIVE) { return; }
    if (ctx->desc.hdr.eeg_adaptive_mode == 0U) { return; }

    ctx->eeg_dominant_freq_hz = dominant_freq_hz;

    /* Uniform mode: update all active sockets to same freq code. */
    for (uint8_t i = 0; i < ctx->active_socket_count; i++) {
        uint8_t new_code = np_pbm_drive_map_eeg_freq(
            dominant_freq_hz,
            ctx->current_freq_code,
            &ctx->eeg_tick_in_band[i]);

        if (new_code != 0xFF) {
            np_pbm_drive_set_freq(ctx->active_socket_id[i], &ctx->drv[i],
                                       ctx->drv[i].ch_enable, new_code);
            ctx->current_freq_code = new_code;
            ctx->record.eeg_adapt_event_count++;
        }
    }
}

np_pbm_status_t np_pbm_session_stop(np_pbm_session_ctx_t *ctx,
                                               np_pbm_fault_t reason)
{
    if (ctx->stage == NP_PBM_STAGE_IDLE) {
        return NP_PBM_ERR_NO_SESSION;
    }

    disable_all_slots(ctx);

    ctx->record.abort_reason = (uint8_t)reason;
    ctx->record.duration_s   = 0; /* actual duration not tracked in fault path */

    write_shdr_summary(ctx, reason);
    ctx->stage = NP_PBM_STAGE_FAULT;

    if (ctx->fault_cb) {
        ctx->fault_cb(0xFF, reason);
    }
    if (ctx->session_end_cb) {
        ctx->session_end_cb(&ctx->record, NP_PBM_ERR_THERMAL);
    }

    return NP_PBM_OK;
}

np_pbm_stage_t np_pbm_session_stage(
    const np_pbm_session_ctx_t *ctx)
{
    return ctx->stage;
}
