/*
 * NeurOne HD-tDCS — Stimulation Delivery
 * Document: NP-FW-HD-001 Rev A §7
 *
 * Programs the 21-ch tACS driver with independent DC currents for HD-tDCS.
 * Safety MCU owns all enable GPIO.  This module manages the ramp state machine
 * and tracks charge density for display and UHDR.
 *
 * Current ramp: linear from 0 → target over NP_HD_RAMP_DURATION_S (30 s),
 * hold, then linear from target → 0 over 30 s.
 */

#include "np_hd_stim.h"
#include "np_hd_montage.h"   /* np_hd_montage_validate() used below */
#include <string.h>

/* struct np_hd_stim_ctx is defined transparently in np_hd_stim.h so the session  */
/* pool can embed it by value.  The embedder owns the storage.                     */

/* ── Driver HAL stubs (platform team implements for tACS driver IC) ──────────── */

static void platform_driver_set_current(uint8_t driver_channel, int32_t current_ua)
{
    /* Platform HAL: write current_ua to tACS driver channel register.           */
    /* Sign convention: positive = source (anode), negative = sink (cathode).   */
    (void)driver_channel;
    (void)current_ua;
    /* np_platform_tacs_set_current(driver_channel, current_ua); */
}

static void platform_driver_all_off(void)
{
    /* Platform HAL: set all 21 driver channels to 0 in one atomic transaction. */
    /* np_platform_tacs_all_off(); */
}

static void platform_safety_mcu_request_impedance(const uint8_t *driver_channels,
                                                   uint8_t        n_channels)
{
    /* Platform HAL: send SPI command to safety MCU requesting AC impedance     */
    /* measurement on the specified channels.                                   */
    (void)driver_channels;
    (void)n_channels;
}

static void platform_safety_mcu_request_enable(const uint8_t *driver_channels,
                                                const int32_t *currents_ua,
                                                uint8_t        n_channels)
{
    /* Platform HAL: send SPI command to safety MCU requesting stimulation      */
    /* enable.  Safety MCU validates charge density before granting.            */
    (void)driver_channels;
    (void)currents_ua;
    (void)n_channels;
}

static void platform_safety_mcu_disable(void)
{
    /* Platform HAL: send SPI command to safety MCU to withdraw enable.        */
}

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

np_hd_status_t np_hd_stim_init(np_hd_stim_ctx_t         *ctx,
                                 const np_hd_montage_t    *montage,
                                 np_hd_safety_response_cb_t safety_cb)
{
    if (!ctx || !montage || !safety_cb) {
        return NP_HD_ERR_INVALID_ARG;
    }

    np_hd_status_t ret = np_hd_montage_validate(montage);
    if (ret != NP_HD_OK) {
        return ret;
    }

    memset(ctx, 0, sizeof(struct np_hd_stim_ctx));
    ctx->montage    = *montage;
    ctx->safety_cb  = safety_cb;
    ctx->phase      = NP_HD_STIM_IDLE;

    /* Build per-electrode state array: anode at [0], cathodes at [1..4]. */
    ctx->n_elec = 0U;

    /* Electrode → driver channel, per the T2 cap wiring spec.  21-ch driver:   */
    /* one electrode, one channel, no sharing.                                   */
    /*                                                                           */
    /* This MUST stay identical to k_driver_channel[] in np_hd_montage.c.  The   */
    /* montage layer validates distinctness against its copy; this copy is what  */
    /* actually gets programmed, so a divergence is a wrong-channel stimulation  */
    /* path that nothing would catch.  Nothing enforces the match today — see    */
    /* the follow-up to expose a single accessor from the montage module.        */
    static const uint8_t k_ch_to_drv[NP_HD_CH_COUNT] = {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
        11, 12, 13, 14, 15, 16, 17, 18, 19, 20
    };

    ctx->elec[0].label          = montage->center;
    ctx->elec[0].driver_channel = k_ch_to_drv[montage->center];
    ctx->elec[0].target_current_ua = (int32_t)0;  /* set in np_hd_stim_start() */
    ctx->n_elec = 1U;

    for (uint8_t k = 0U; k < montage->cathode_count && k < NP_HD_RING_CATHODE_COUNT; k++) {
        ctx->elec[ctx->n_elec].label          = montage->cathodes[k];
        ctx->elec[ctx->n_elec].driver_channel = k_ch_to_drv[montage->cathodes[k]];
        ctx->elec[ctx->n_elec].target_current_ua = (int32_t)0;
        ctx->n_elec++;
    }

    return NP_HD_OK;
}

void np_hd_stim_deinit(np_hd_stim_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    platform_driver_all_off();
    platform_safety_mcu_disable();
    memset(ctx, 0, sizeof(struct np_hd_stim_ctx));
}

/* ── Impedance check ─────────────────────────────────────────────────────────── */

np_hd_status_t np_hd_stim_request_impedance_check(np_hd_stim_ctx_t *ctx)
{
    if (!ctx) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (ctx->phase != NP_HD_STIM_IDLE) {
        return NP_HD_ERR_SESSION_ACTIVE;
    }

    uint8_t channels[NP_HD_RING_ELECTRODE_COUNT];
    for (uint8_t i = 0U; i < ctx->n_elec; i++) {
        channels[i] = ctx->elec[i].driver_channel;
    }
    platform_safety_mcu_request_impedance(channels, ctx->n_elec);
    return NP_HD_OK;
}

/* ── Stimulation control ──────────────────────────────────────────────────────── */

np_hd_status_t np_hd_stim_start(np_hd_stim_ctx_t *ctx,
                                  uint16_t          anode_current_ua,
                                  uint16_t          duration_s,
                                  uint32_t          now_ms)
{
    if (!ctx) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (anode_current_ua > NP_HD_MAX_CURRENT_UA) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (!ctx->impedance_ok) {
        return NP_HD_ERR_IMPEDANCE_HIGH;
    }
    if (ctx->phase != NP_HD_STIM_IDLE) {
        return NP_HD_ERR_SESSION_ACTIVE;
    }

    /* Set target currents: anode positive, cathodes split equally (negative).   */
    int32_t cathode_ua = -(int32_t)anode_current_ua / (int32_t)NP_HD_CATHODE_SPLIT_DENOM;

    ctx->elec[0].target_current_ua = (int32_t)anode_current_ua;
    for (uint8_t k = 1U; k < ctx->n_elec; k++) {
        ctx->elec[k].target_current_ua = cathode_ua;
    }

    ctx->target_ua   = anode_current_ua;
    ctx->duration_s  = duration_s;

    /* Compute steady phase end time.                                            */
    uint32_t ramp_ms  = (uint32_t)NP_HD_RAMP_DURATION_S * 1000U;
    uint32_t total_ms = (uint32_t)duration_s * 1000U;
    ctx->steady_end_ms = now_ms + total_ms - ramp_ms;

    /* Request safety MCU enable.  Actual current starts in tick() after grant. */
    uint8_t  channels[NP_HD_RING_ELECTRODE_COUNT];
    int32_t  currents[NP_HD_RING_ELECTRODE_COUNT];
    for (uint8_t i = 0U; i < ctx->n_elec; i++) {
        channels[i] = ctx->elec[i].driver_channel;
        currents[i] = ctx->elec[i].target_current_ua;
    }
    platform_safety_mcu_request_enable(channels, currents, ctx->n_elec);

    ctx->phase         = NP_HD_STIM_RAMP_UP;
    ctx->phase_start_ms = now_ms;

    return NP_HD_OK;
}

void np_hd_stim_stop(np_hd_stim_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    platform_driver_all_off();
    platform_safety_mcu_disable();
    ctx->phase = NP_HD_STIM_DONE;
    for (uint8_t i = 0U; i < ctx->n_elec; i++) {
        ctx->elec[i].actual_current_ua = 0;
    }
}

/* ── Tick — ramp state machine ───────────────────────────────────────────────── */

void np_hd_stim_tick(np_hd_stim_ctx_t *ctx, uint32_t now_ms)
{
    if (!ctx || ctx->phase == NP_HD_STIM_IDLE || ctx->phase == NP_HD_STIM_DONE) {
        return;
    }

    if (ctx->phase == NP_HD_STIM_FAULT) {
        np_hd_stim_stop(ctx);
        return;
    }

    /* Guard: if safety MCU grant has not arrived yet, hold at 0. */
    if (!ctx->safety_mcu_granted && ctx->phase == NP_HD_STIM_RAMP_UP) {
        return;
    }

    uint32_t ramp_ms = (uint32_t)NP_HD_RAMP_DURATION_S * 1000U;
    uint32_t elapsed = now_ms - ctx->phase_start_ms;

    switch (ctx->phase) {

    case NP_HD_STIM_RAMP_UP: {
        float frac = (elapsed >= ramp_ms)
                     ? 1.0f : (float)elapsed / (float)ramp_ms;

        /* Scale anode current; cathodes follow proportionally (opposite sign). */
        int32_t anode_ua = (int32_t)((float)ctx->target_ua * frac);
        int32_t cath_ua  = -(anode_ua / (int32_t)NP_HD_CATHODE_SPLIT_DENOM);

        ctx->elec[0].actual_current_ua = anode_ua;
        platform_driver_set_current(ctx->elec[0].driver_channel, anode_ua);

        for (uint8_t k = 1U; k < ctx->n_elec; k++) {
            ctx->elec[k].actual_current_ua = cath_ua;
            platform_driver_set_current(ctx->elec[k].driver_channel, cath_ua);
        }

        if (frac >= 1.0f) {
            ctx->phase          = NP_HD_STIM_STEADY;
            ctx->phase_start_ms = now_ms;
        }
        break;
    }

    case NP_HD_STIM_STEADY: {
        /* Monitor charge density. */
        float charge_uc = (float)ctx->target_ua * (float)elapsed / 1.0e9f;
        float density   = charge_uc / NP_HD_ELECTRODE_AREA_CM2;
        if (density >= NP_HD_MAX_CHARGE_DENSITY_UC_CM2 * 0.95f) {
            ctx->phase = NP_HD_STIM_FAULT;
            np_hd_stim_stop(ctx);
            return;
        }

        /* Accumulate charge for UHDR. */
        ctx->elec[0].charge_ua_s += (float)ctx->target_ua * 0.1f;  /* per 100 ms tick */

        if (now_ms >= ctx->steady_end_ms) {
            ctx->phase          = NP_HD_STIM_RAMP_DOWN;
            ctx->phase_start_ms = now_ms;
        }
        break;
    }

    case NP_HD_STIM_RAMP_DOWN: {
        float frac = (elapsed >= ramp_ms)
                     ? 0.0f : 1.0f - (float)elapsed / (float)ramp_ms;

        int32_t anode_ua = (int32_t)((float)ctx->target_ua * frac);
        int32_t cath_ua  = -(anode_ua / (int32_t)NP_HD_CATHODE_SPLIT_DENOM);

        ctx->elec[0].actual_current_ua = anode_ua;
        platform_driver_set_current(ctx->elec[0].driver_channel, anode_ua);
        for (uint8_t k = 1U; k < ctx->n_elec; k++) {
            ctx->elec[k].actual_current_ua = cath_ua;
            platform_driver_set_current(ctx->elec[k].driver_channel, cath_ua);
        }

        if (frac <= 0.0f) {
            np_hd_stim_stop(ctx);
        }
        break;
    }

    default:
        break;
    }
}

/* ── Safety MCU response ─────────────────────────────────────────────────────── */

void np_hd_stim_safety_mcu_response(np_hd_stim_ctx_t *ctx,
                                     bool              granted,
                                     const float       impedance_kohm[])
{
    if (!ctx) {
        return;
    }

    if (impedance_kohm) {
        /* Impedance check reply. */
        float sum = 0.0f;
        bool  all_ok = true;
        for (uint8_t i = 0U; i < ctx->n_elec; i++) {
            ctx->elec[i].impedance_kohm = impedance_kohm[i];
            sum += impedance_kohm[i];
            if (impedance_kohm[i] > (float)NP_HD_MAX_IMPEDANCE_KOHM) {
                all_ok = false;
            }
        }
        ctx->mean_impedance_kohm = sum / (float)ctx->n_elec;
        ctx->impedance_ok        = all_ok;
    } else {
        /* Stimulation enable reply. */
        ctx->safety_mcu_granted = granted;
        if (!granted) {
            ctx->phase = NP_HD_STIM_FAULT;
        }
    }

    if (ctx->safety_cb) {
        ctx->safety_cb(ctx, granted, impedance_kohm);
    }
}

/* ── Diagnostics ─────────────────────────────────────────────────────────────── */

np_hd_stim_phase_t np_hd_stim_phase(const np_hd_stim_ctx_t *ctx)
{
    return ctx ? ctx->phase : NP_HD_STIM_IDLE;
}

float np_hd_stim_anode_charge_ua_s(const np_hd_stim_ctx_t *ctx)
{
    return ctx ? ctx->elec[0].charge_ua_s : 0.0f;
}

float np_hd_stim_charge_density_uc_cm2(const np_hd_stim_ctx_t *ctx)
{
    if (!ctx) {
        return 0.0f;
    }
    return ctx->elec[0].charge_ua_s / (NP_HD_ELECTRODE_AREA_CM2 * 1000.0f);
}

np_hd_status_t np_hd_stim_get_electrode_states(const np_hd_stim_ctx_t *ctx,
                                                 np_hd_electrode_state_t *out,
                                                 uint8_t                  n)
{
    if (!ctx || !out) {
        return NP_HD_ERR_INVALID_ARG;
    }
    uint8_t copy_n = (n < ctx->n_elec) ? n : ctx->n_elec;
    for (uint8_t i = 0U; i < copy_n; i++) {
        out[i] = ctx->elec[i];
    }
    return NP_HD_OK;
}
