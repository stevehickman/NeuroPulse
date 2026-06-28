/*
 * NeuroPulse HRV Biofeedback — taVNS Inspiration-Phase Synchronization
 * Document: NP-FW-HRV-001 Rev A §8
 *
 * RSA slope detection: inspiration = negative mean dRR/dt over the sliding
 * window.  A hysteresis deadband prevents rapid re-triggering on noise.
 *
 * Safety ownership: this module manages the firmware-side gate flag only.
 * The safety MCU independently verifies clip impedance before asserting the
 * physical VNS enable GPIO.  np_hrv_tavns_force_disable() is callable from
 * any context and drives the callbacks synchronously.
 */

#include "np_hrv_tavns_sync.h"
#include <string.h>

static np_tavns_enable_cb_t  s_enable_cb;
static np_tavns_disable_cb_t s_disable_cb;
static uint16_t s_stim_freq_hz;
static uint16_t s_stim_current_ua;

np_hrv_status_t np_hrv_tavns_init(np_tavns_state_t  *tavns,
                                   uint16_t           stim_freq_hz,
                                   uint16_t           stim_current_ua,
                                   np_tavns_enable_cb_t  enable_cb,
                                   np_tavns_disable_cb_t disable_cb)
{
    if (stim_freq_hz < 1U || stim_freq_hz > NP_TAVNS_DEFAULT_FREQ_HZ) {
        return NP_HRV_ERR_INVALID_ARG;
    }
    if (stim_current_ua < 100U || stim_current_ua > NP_TAVNS_MAX_CURRENT_UA) {
        return NP_HRV_ERR_INVALID_ARG;
    }

    memset(tavns, 0, sizeof(*tavns));
    tavns->phase              = NP_TAVNS_IDLE;
    tavns->safety_mcu_granted = false;

    s_stim_freq_hz    = stim_freq_hz;
    s_stim_current_ua = stim_current_ua;
    s_enable_cb       = enable_cb;
    s_disable_cb      = disable_cb;

    return NP_HRV_OK;
}

/* ── Slope computation ───────────────────────────────────────────────────────── */

/* Returns the mean of the slope buffer in ms/beat, sign indicates direction.  */
static int32_t mean_slope(const np_tavns_state_t *tavns)
{
    return tavns->slope_sum / (int32_t)NP_TAVNS_INSP_SLOPE_WIN;
}

static void push_slope(np_tavns_state_t *tavns, int16_t drr)
{
    /* Remove oldest entry from sum. */
    tavns->slope_sum -= (int32_t)tavns->rr_slope_buf[tavns->slope_buf_idx];
    /* Store new entry. */
    tavns->rr_slope_buf[tavns->slope_buf_idx] = drr;
    tavns->slope_sum += (int32_t)drr;
    tavns->slope_buf_idx = (uint8_t)((tavns->slope_buf_idx + 1U)
                                      % NP_TAVNS_INSP_SLOPE_WIN);
}

/* ── Main RR processing ──────────────────────────────────────────────────────── */

void np_hrv_tavns_process_rr(np_tavns_state_t *tavns,
                               uint16_t          rr_ms,
                               uint32_t          now_ms)
{
    /* Compute dRR relative to the most recent entry in the slope buffer.       */
    /* The previous RR is retrieved from the slot before the current write pos. */
    uint8_t prev_idx = (uint8_t)((tavns->slope_buf_idx + NP_TAVNS_INSP_SLOPE_WIN - 1U)
                                   % NP_TAVNS_INSP_SLOPE_WIN);
    int16_t prev_rr  = (tavns->rr_slope_buf[prev_idx] != 0)
                        ? tavns->rr_slope_buf[prev_idx]
                        : (int16_t)rr_ms;
    int16_t drr = (int16_t)rr_ms - prev_rr;

    push_slope(tavns, drr);

    int32_t slope = mean_slope(tavns);

    switch (tavns->phase) {
    case NP_TAVNS_IDLE:
        /* Inspiration onset: RR shortening crosses hysteresis threshold.      */
        if (slope < -(int32_t)NP_TAVNS_SLOPE_HYSTERESIS) {
            tavns->phase          = NP_TAVNS_STIMULATING;
            tavns->stim_start_ms  = now_ms;
            tavns->safety_mcu_granted = false;
            if (s_enable_cb) {
                s_enable_cb(s_stim_freq_hz, s_stim_current_ua);
            }
            tavns->stim_count++;
        }
        break;

    case NP_TAVNS_STIMULATING:
        /* Expiration onset: RR lengthening crosses hysteresis threshold.       */
        if (slope > (int32_t)NP_TAVNS_SLOPE_HYSTERESIS) {
            tavns->stim_duration_ms = now_ms - tavns->stim_start_ms;
            tavns->phase = NP_TAVNS_COOLDOWN;
            if (s_disable_cb) {
                s_disable_cb();
            }
        }
        break;

    case NP_TAVNS_COOLDOWN:
        /* Return to IDLE at the next inspiration crossing.                     */
        if (slope < -(int32_t)NP_TAVNS_SLOPE_HYSTERESIS) {
            tavns->phase         = NP_TAVNS_STIMULATING;
            tavns->stim_start_ms = now_ms;
            tavns->safety_mcu_granted = false;
            if (s_enable_cb) {
                s_enable_cb(s_stim_freq_hz, s_stim_current_ua);
            }
            tavns->stim_count++;
        }
        break;
    }
}

/* ── Tick: enforce failsafe gate timeout ─────────────────────────────────────── */

void np_hrv_tavns_tick(np_tavns_state_t *tavns, uint32_t now_ms)
{
    if (tavns->phase == NP_TAVNS_STIMULATING) {
        uint32_t stim_elapsed = now_ms - tavns->stim_start_ms;
        if (stim_elapsed >= NP_TAVNS_MAX_INSP_DURATION_MS) {
            /* Failsafe: inspiration phase cannot plausibly last this long.     */
            np_hrv_tavns_force_disable(tavns);
        }
    }
}

void np_hrv_tavns_force_disable(np_tavns_state_t *tavns)
{
    if (tavns->phase == NP_TAVNS_STIMULATING) {
        if (s_disable_cb) {
            s_disable_cb();
        }
    }
    tavns->phase              = NP_TAVNS_IDLE;
    tavns->safety_mcu_granted = false;
}

void np_hrv_tavns_safety_mcu_response(np_tavns_state_t *tavns, bool grant)
{
    if (!grant) {
        /* Safety MCU vetoed: close gate immediately. */
        np_hrv_tavns_force_disable(tavns);
        return;
    }
    tavns->safety_mcu_granted = true;
}
