/*
 * NeuroPulse Safety MCU — SW01-M03: Per-Electrode Charge Density Monitor
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev A §SW01-M03
 *
 * Enforces the 40 µC/cm² charge density limit per electrode.
 * App cannot override this limit (NP_CHARGE_LIMIT_UC hardware enforcement).
 *
 * Charge is accumulated from the current waveform telemetry sent via SPI.
 * The safety MCU reads stimulation current magnitude from the main processor
 * SPI frame extension.  Accumulator is reset on session start.
 *
 * FMEA-M03-01 mitigation: accumulator is uint64_t (no overflow at 40µC/cm²
 * limit even if polled at 10kHz for 24 hours).  MISRA C:2012 Rule 10.1.
 */

#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Per-electrode charge accumulator — 14 channels, nC resolution */
static uint64_t s_charge_nc[14];
static bool     s_limit_reached[14];

np_safe_status_t np_charge_monitor_init(void)
{
    memset(s_charge_nc, 0, sizeof(s_charge_nc));
    memset(s_limit_reached, 0, sizeof(s_limit_reached));
    return NP_SAFE_OK;
}

/*
 * np_charge_monitor_reset_session — zero all accumulators at session start.
 * Called by main loop when session_active transitions 0→1.
 */
void np_charge_monitor_reset_session(void)
{
    memset(s_charge_nc, 0, sizeof(s_charge_nc));
    memset(s_limit_reached, 0, sizeof(s_limit_reached));
}

/*
 * np_charge_monitor_accumulate — add charge for one channel for one tick.
 * current_ua: instantaneous current in µA (unsigned; caller ensures
 * charge-balanced waveform so both phases are counted separately).
 * dt_us: time step in µs.
 * channel: 0-13 matching NP_SAFETY_EN_* bit positions.
 */
void np_charge_monitor_accumulate(uint8_t channel, uint32_t current_ua, uint32_t dt_us)
{
    if (channel >= 14U) {
        return;
    }
    /* charge_nc += current_ua * dt_us / 1000 (convert µA·µs → nC) */
    uint64_t delta_nc = ((uint64_t)current_ua * (uint64_t)dt_us) / 1000ULL;
    s_charge_nc[channel] += delta_nc;
}

/*
 * np_charge_monitor_tick — check all enabled channels for limit violation.
 * Clears the granted_mask bits for channels that exceed the limit.
 */
void np_charge_monitor_tick(np_safety_state_t *state)
{
    /* Limit in nC = NP_CHARGE_LIMIT_UC * 1000 */
    const uint64_t limit_nc = (uint64_t)NP_CHARGE_LIMIT_UC * 1000ULL;

    for (uint8_t ch = 0U; ch < 14U; ch++) {
        if (s_charge_nc[ch] >= limit_nc && !s_limit_reached[ch]) {
            s_limit_reached[ch]  = true;
            state->granted_mask &= (uint16_t)~(1U << ch);
            state->status       |= NP_SAFETY_STATUS_CHARGE | NP_SAFETY_STATUS_CUTOFF;
            state->fault_slot    = ch;
        }
    }
}
