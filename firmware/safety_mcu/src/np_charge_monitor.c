/*
 * NeuroPulse Safety MCU — SW01-M03: Per-Electrode Charge Density Monitor
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M03
 *
 * Enforces the 40 µC/cm² charge density limit per electrode.
 * App cannot override this limit (hardware enforcement via safety MCU GPIO).
 *
 * Charge is accumulated each heartbeat (200ms) from the commanded current
 * carried in np_safety_rx_ext_frame_t.current_ua[].  Accumulator is reset on
 * session start.  Limits are per-channel to support mixed electrode geometries
 * within a single session.
 *
 * FMEA-M03-01 mitigation: s_charge_nc[] is uint64_t (no overflow at 40µC/cm²
 * limit even if polled at 10kHz for 24 hours).  MISRA C:2012 Rule 10.1.
 *
 * OI-CHARGE-01 CLOSED — np_charge_monitor_accumulate() is now called in
 *   np_safety_main.c every heartbeat iteration for each granted channel that
 *   carries a non-zero commanded current in the extended heartbeat frame.
 *   dt_us is always NP_SAFETY_HEARTBEAT_EXP_MS × 1000 = 200000µs (constant
 *   known to the MCU from np_safety_config.h; not transmitted over SPI).
 *
 * OI-CHARGE-02 OPEN — NP_CHARGE_LIMIT_UC = 1000µC is correct for a standard
 *   25cm² tDCS electrode (NP_ELECTRODE_AREA_CM2 = 25).  For T2 HD-tDCS with
 *   3.5mm Ag/AgCl electrodes (area ≈ 0.096cm²) the limit is 40×0.096 = 3.84µC
 *   — 260× smaller.  Use np_charge_monitor_set_channel_limit() at session start
 *   once the session runner knows the electrode geometry.  A single global
 *   constant cannot serve both geometries — OI-CHARGE-02 is BLOCKING for T2
 *   HD-tDCS clinical use.
 */

#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Per-electrode charge accumulator — NP_SAFETY_MAX_CHANNELS channels, nC resolution */
static uint64_t s_charge_nc[NP_SAFETY_MAX_CHANNELS];
static bool     s_limit_reached[NP_SAFETY_MAX_CHANNELS];
/* Per-channel limit in nC; default = NP_CHARGE_LIMIT_UC × 1000.
 * OI-CHARGE-02: call np_charge_monitor_set_channel_limit() at session start
 * to set the geometry-correct limit for each channel.                        */
static uint64_t s_limit_nc[NP_SAFETY_MAX_CHANNELS];

np_safe_status_t np_charge_monitor_init(void)
{
    uint8_t ch;
    memset(s_charge_nc, 0, sizeof(s_charge_nc));
    memset(s_limit_reached, 0, sizeof(s_limit_reached));
    for (ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        s_limit_nc[ch] = (uint64_t)NP_CHARGE_LIMIT_UC * 1000ULL;
    }
    return NP_SAFE_OK;
}

/*
 * np_charge_monitor_set_channel_limit — override the per-channel charge limit.
 * Must be called at session start (after reset_session, before first heartbeat)
 * when the electrode geometry differs from the default 25cm² tDCS pad.
 *
 * channel:  0–13, matching NP_SAFETY_EN_* bit positions.
 * limit_uc: per-electrode charge limit in µC (e.g. 4 for T2 HD-tDCS 3.5mm
 *           Ag/AgCl electrodes — OI-CHARGE-02 provides the exact value).
 *
 * The default (set by np_charge_monitor_init and reset_session) is
 * NP_CHARGE_LIMIT_UC = 1000µC (40µC/cm² × 25cm²).
 */
void np_charge_monitor_set_channel_limit(uint8_t channel, uint32_t limit_uc)
{
    if (channel < NP_SAFETY_MAX_CHANNELS) {
        s_limit_nc[channel] = (uint64_t)limit_uc * 1000ULL;
    }
}

/*
 * np_charge_monitor_reset_session — zero all accumulators and restore default
 * limits at session start.  Called by main loop when session_active 0→1.
 * After this call, use np_charge_monitor_set_channel_limit() for any channels
 * whose electrode geometry requires a non-default limit (OI-CHARGE-02).
 */
void np_charge_monitor_reset_session(void)
{
    uint8_t ch;
    memset(s_charge_nc, 0, sizeof(s_charge_nc));
    memset(s_limit_reached, 0, sizeof(s_limit_reached));
    for (ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        s_limit_nc[ch] = (uint64_t)NP_CHARGE_LIMIT_UC * 1000ULL;
    }
}

/*
 * np_charge_monitor_accumulate — add charge for one channel for one tick.
 * Called by np_safety_main.c every heartbeat for each granted channel.
 *
 * channel:    0–13 matching NP_SAFETY_EN_* bit positions.
 * current_ua: commanded current magnitude in µA (SHDR — from session descriptor).
 *             NOT ADC-measured actual current (which would be UHDR-class).
 * dt_us:      elapsed time in µs; always NP_SAFETY_HEARTBEAT_EXP_MS × 1000.
 */
void np_charge_monitor_accumulate(uint8_t channel, uint32_t current_ua, uint32_t dt_us)
{
    if (channel >= NP_SAFETY_MAX_CHANNELS) {
        return;
    }
    /* charge_nc += current_ua * dt_us / 1000  (µA·µs → nC) */
    s_charge_nc[channel] += ((uint64_t)current_ua * (uint64_t)dt_us) / 1000ULL;
}

/*
 * np_charge_monitor_tick — check all channels for per-channel limit violation.
 * Clears the granted_mask bits for channels that exceed their individual limit.
 * Uses s_limit_nc[ch] (set by init/reset/set_channel_limit) per channel.
 */
void np_charge_monitor_tick(np_safety_state_t *state)
{
    uint8_t ch;
    for (ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        if (s_charge_nc[ch] >= s_limit_nc[ch] && !s_limit_reached[ch]) {
            s_limit_reached[ch]  = true;
            state->granted_mask &= (uint16_t)~(1U << ch);
            state->status       |= NP_SAFETY_STATUS_CHARGE | NP_SAFETY_STATUS_CUTOFF;
            state->fault_slot    = ch;
        }
    }
}
