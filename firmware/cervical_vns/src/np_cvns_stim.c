/*
 * NeurOne Cervical VNS — Stimulation Delivery
 * Document: NP-FW-CVNS-001 Rev 1 §7
 *
 * Manages the biphasic charge-balanced waveform parameters and ramp state
 * machine for the cervical VNS accessory.  The safety MCU physically generates
 * the pulse waveform; this module computes target current levels and exchanges
 * them with the safety MCU via SPI.
 *
 * Waveform: cathodic-first biphasic, 100 µs inter-phase gap, charge-balanced.
 * Ramp: 10 s ramp-up, 5 s ramp-down, 100 ms tick.
 */

#include "np_cvns_stim.h"
#include <string.h>

/* ── Platform HAL stubs ──────────────────────────────────────────────────────── */

static void platform_set_current(uint16_t current_ua, uint8_t electrode_config)
{
    /* Platform HAL: update safety MCU target current via SPI.                  */
    /* The safety MCU scales the biphasic waveform amplitude accordingly.       */
    /* np_platform_cvns_set_current(current_ua, electrode_config);              */
    (void)current_ua;
    (void)electrode_config;
}

static void platform_all_off(void)
{
    /* Platform HAL: assert CVNS_ENABLE_L and CVNS_ENABLE_R low via SPI.       */
    /* np_platform_cvns_all_off();                                              */
}

static void platform_safety_mcu_request_impedance(uint8_t electrode_config)
{
    (void)electrode_config;
    /* np_platform_cvns_spi_transfer(NP_CVNS_SPI_CMD_IMPEDANCE_CHECK, ...);    */
}

static void platform_safety_mcu_request_enable(uint16_t current_ua,
                                                 uint8_t  electrode_config)
{
    (void)current_ua;
    (void)electrode_config;
    /* np_platform_cvns_spi_transfer(NP_CVNS_SPI_CMD_ENABLE, ...);             */
}

static void platform_safety_mcu_disable(void)
{
    /* np_platform_cvns_spi_transfer(NP_CVNS_SPI_CMD_DISABLE, NULL, 0);        */
}

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

np_cvns_status_t np_cvns_stim_init(np_cvns_stim_ctx_t           *ctx,
                                    const np_cvns_session_config_t *config,
                                    np_cvns_safety_response_cb_t   safety_cb)
{
    if (!ctx || !config || !safety_cb) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (config->freq_hz < NP_CVNS_FREQ_HZ_MIN ||
        config->freq_hz > NP_CVNS_FREQ_HZ_MAX) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (config->current_ua > NP_CVNS_CURRENT_UA_MAX) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (config->pulse_width_us < NP_CVNS_PULSE_WIDTH_US_MIN ||
        config->pulse_width_us > NP_CVNS_PULSE_WIDTH_US_MAX) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (config->duration_s < NP_CVNS_SESSION_MIN_S ||
        config->duration_s > NP_CVNS_SESSION_MAX_S) {
        return NP_CVNS_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->config    = *config;
    ctx->safety_cb = safety_cb;
    ctx->phase     = NP_CVNS_STIM_IDLE;

    return NP_CVNS_OK;
}

void np_cvns_stim_deinit(np_cvns_stim_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    platform_all_off();
    platform_safety_mcu_disable();
    memset(ctx, 0, sizeof(*ctx));
}

/* ── Impedance check ─────────────────────────────────────────────────────────── */

np_cvns_status_t np_cvns_stim_request_impedance_check(np_cvns_stim_ctx_t *ctx)
{
    if (!ctx) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (ctx->phase != NP_CVNS_STIM_IDLE) {
        return NP_CVNS_ERR_SESSION_ACTIVE;
    }
    platform_safety_mcu_request_impedance(ctx->config.electrode_config);
    return NP_CVNS_OK;
}

/* ── Stimulation control ─────────────────────────────────────────────────────── */

np_cvns_status_t np_cvns_stim_start(np_cvns_stim_ctx_t *ctx, uint32_t now_ms)
{
    if (!ctx) {
        return NP_CVNS_ERR_INVALID_ARG;
    }
    if (!ctx->impedance_ok) {
        return NP_CVNS_ERR_IMPEDANCE_HIGH;
    }
    if (ctx->phase != NP_CVNS_STIM_IDLE) {
        return NP_CVNS_ERR_SESSION_ACTIVE;
    }

    ctx->target_ua     = ctx->config.current_ua;
    ctx->actual_ua     = 0U;
    ctx->phase         = NP_CVNS_STIM_RAMP_UP;
    ctx->phase_start_ms = now_ms;

    /* Total session: ramp-up + active + ramp-down. */
    uint32_t ramp_up_ms   = (uint32_t)NP_CVNS_RAMP_UP_S * 1000U;
    uint32_t ramp_down_ms = (uint32_t)NP_CVNS_RAMP_DOWN_S * 1000U;
    uint32_t active_ms    = (uint32_t)ctx->config.duration_s * 1000U
                            - ramp_up_ms - ramp_down_ms;
    ctx->active_end_ms    = now_ms + ramp_up_ms + active_ms;

    platform_safety_mcu_request_enable(ctx->target_ua, ctx->config.electrode_config);
    return NP_CVNS_OK;
}

void np_cvns_stim_stop(np_cvns_stim_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    platform_all_off();
    platform_safety_mcu_disable();
    ctx->phase     = NP_CVNS_STIM_DONE;
    ctx->actual_ua = 0U;
}

/* ── Ramp state machine tick ─────────────────────────────────────────────────── */

void np_cvns_stim_tick(np_cvns_stim_ctx_t *ctx, uint32_t now_ms)
{
    if (!ctx) {
        return;
    }
    if (ctx->phase == NP_CVNS_STIM_IDLE || ctx->phase == NP_CVNS_STIM_DONE) {
        return;
    }
    if (ctx->phase == NP_CVNS_STIM_FAULT) {
        np_cvns_stim_stop(ctx);
        return;
    }

    /* Hold at 0 until safety MCU grants enable. */
    if (!ctx->safety_mcu_granted && ctx->phase == NP_CVNS_STIM_RAMP_UP) {
        return;
    }

    uint32_t ramp_up_ms   = (uint32_t)NP_CVNS_RAMP_UP_S * 1000U;
    uint32_t ramp_down_ms = (uint32_t)NP_CVNS_RAMP_DOWN_S * 1000U;
    uint32_t elapsed      = now_ms - ctx->phase_start_ms;

    switch (ctx->phase) {

    case NP_CVNS_STIM_RAMP_UP: {
        float frac = (elapsed >= ramp_up_ms)
                     ? 1.0f : (float)elapsed / (float)ramp_up_ms;
        ctx->actual_ua = (uint16_t)((float)ctx->target_ua * frac);
        platform_set_current(ctx->actual_ua, ctx->config.electrode_config);

        if (frac >= 1.0f) {
            ctx->phase          = NP_CVNS_STIM_ACTIVE;
            ctx->phase_start_ms = now_ms;
        }
        break;
    }

    case NP_CVNS_STIM_ACTIVE:
        ctx->actual_ua = ctx->target_ua;
        platform_set_current(ctx->actual_ua, ctx->config.electrode_config);

        if (now_ms >= ctx->active_end_ms) {
            ctx->phase          = NP_CVNS_STIM_RAMP_DOWN;
            ctx->phase_start_ms = now_ms;
        }
        break;

    case NP_CVNS_STIM_RAMP_DOWN: {
        float frac = (elapsed >= ramp_down_ms)
                     ? 0.0f : 1.0f - (float)elapsed / (float)ramp_down_ms;
        ctx->actual_ua = (uint16_t)((float)ctx->target_ua * frac);
        platform_set_current(ctx->actual_ua, ctx->config.electrode_config);

        if (frac <= 0.0f) {
            np_cvns_stim_stop(ctx);
        }
        break;
    }

    default:
        break;
    }
}

/* ── Safety MCU response ─────────────────────────────────────────────────────── */

void np_cvns_stim_safety_mcu_response(np_cvns_stim_ctx_t *ctx,
                                       bool                granted,
                                       const float         impedance_kohm[])
{
    if (!ctx) {
        return;
    }

    if (impedance_kohm) {
        /* Impedance measurement reply. */
        bool all_ok = true;
        for (uint8_t i = 0U; i < NP_CVNS_ELECTRODE_COUNT; i++) {
            ctx->impedance_kohm[i] = impedance_kohm[i];
            if (impedance_kohm[i] > NP_CVNS_IMPEDANCE_MAX_KOHM) {
                all_ok = false;
            }
        }
        ctx->impedance_ok = all_ok;
    } else {
        /* Enable/disable reply. */
        ctx->safety_mcu_granted = granted;
        if (!granted) {
            ctx->phase = NP_CVNS_STIM_FAULT;
        }
    }

    if (ctx->safety_cb) {
        ctx->safety_cb(ctx, granted, impedance_kohm);
    }
}

/* ── Diagnostics ─────────────────────────────────────────────────────────────── */

np_cvns_stim_phase_t np_cvns_stim_phase(const np_cvns_stim_ctx_t *ctx)
{
    return ctx ? ctx->phase : NP_CVNS_STIM_IDLE;
}

uint16_t np_cvns_stim_current_ua(const np_cvns_stim_ctx_t *ctx)
{
    return ctx ? ctx->actual_ua : 0U;
}

float np_cvns_stim_mean_impedance_kohm(const np_cvns_stim_ctx_t *ctx)
{
    if (!ctx) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (uint8_t i = 0U; i < NP_CVNS_ELECTRODE_COUNT; i++) {
        sum += ctx->impedance_kohm[i];
    }
    return sum / (float)NP_CVNS_ELECTRODE_COUNT;
}
