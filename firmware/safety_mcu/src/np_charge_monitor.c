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
 * OI-CHARGE-02 CLOSED — NP_CHARGE_LIMIT_UC = 1000µC is correct for a standard
 *   25cm² tDCS electrode (NP_ELECTRODE_AREA_CM2 = 25).  For T2 HD-tDCS with
 *   3.5mm Ag/AgCl electrodes (area ≈ 0.096cm²) the limit is 40×0.096 = 3.84µC
 *   — 260× smaller.  The hub session runner reads the HD-tDCS montage from the
 *   signed session descriptor, derives the fixed electrode area, and delivers
 *   it over SPI (np_safety_chan_limit_cmd_t).  np_safety_main.c calls
 *   np_charge_monitor_set_channel_area_mcm2() for each delivered channel, which
 *   converts area→limit using the resident NP_CHARGE_LIMIT_UC_CM2 density
 *   constant.  The density constant never leaves this Class C MCU.
 *
 * OI-CHARGE-03 CLOSED — fail-safe geometry gate.  If a session's area command
 *   is lost/corrupt, CLIN_STIM would otherwise keep the permissive 1000µC pad
 *   default (fail-OPEN).  np_charge_monitor_geom_gate() blocks CLIN_STIM from
 *   granted_mask whenever the hub has declared the session needs a geometry
 *   override (heartbeat NP_SESSION_STATUS_GEOM_REQUIRED) but no valid CLIN_STIM
 *   area has been applied — fail-closed.  Clinical tACS (same enable bit, no
 *   override declared) is unaffected.
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

/* OI-CHARGE-03: true once a valid electrode-area command has set a
 * non-default limit on the CLIN_STIM channel this session.  Until then, if the
 * hub declares the session requires a geometry override, the geometry gate
 * keeps CLIN_STIM out of granted_mask (fail-safe, not fail-open).  Reset each
 * session by np_charge_monitor_reset_session().                              */
static bool     s_geom_applied_clin_stim;

np_safe_status_t np_charge_monitor_init(void)
{
    uint8_t ch;
    memset(s_charge_nc, 0, sizeof(s_charge_nc));
    memset(s_limit_reached, 0, sizeof(s_limit_reached));
    for (ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        s_limit_nc[ch] = (uint64_t)NP_CHARGE_LIMIT_UC * 1000ULL;
    }
    s_geom_applied_clin_stim = false;
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
 * np_charge_monitor_set_channel_area_mcm2 — set the per-channel charge limit
 * from the channel's electrode AREA (OI-CHARGE-02 primary entry point).
 *
 * Called by np_safety_main.c for each channel carried in a
 * np_safety_chan_limit_cmd_t frame the hub delivers during session setup.
 *
 * channel:   0–13, matching NP_SAFETY_EN_* bit positions.
 * area_mcm2: electrode area in milli-cm² (1 unit = 0.001 cm²).  0 = keep
 *            the current limit (no override).
 *
 * The limit is derived here so the 40µC/cm² density constant
 * (NP_CHARGE_LIMIT_UC_CM2) stays on the Class C safety MCU — the hub never
 * transmits a pre-computed charge limit.  The conversion is exact in nC:
 *   limit_nc = 40µC/cm² × (area_mcm2 / 1000) cm² × 1000 nC/µC
 *            = NP_CHARGE_LIMIT_UC_CM2 × area_mcm2
 * e.g. HD-tDCS 3.5mm electrode: 40 × 96 = 3840 nC = 3.84µC.
 * Default 25cm² pad: 40 × 25000 = 1,000,000 nC = 1000µC (unchanged).
 */
void np_charge_monitor_set_channel_area_mcm2(uint8_t channel, uint16_t area_mcm2)
{
    if (channel < NP_SAFETY_MAX_CHANNELS && area_mcm2 > 0U) {
        s_limit_nc[channel] =
            (uint64_t)NP_CHARGE_LIMIT_UC_CM2 * (uint64_t)area_mcm2;

        /* OI-CHARGE-03: applying a non-default geometry to CLIN_STIM opens the
         * fail-safe geometry gate for that channel this session.             */
        if (channel == NP_SAFETY_CH_CLIN_STIM) {
            s_geom_applied_clin_stim = true;
        }
    }
}

/*
 * np_charge_monitor_geom_gate — OI-CHARGE-03 fail-safe electrode-geometry gate.
 *
 * When the hub has declared this session requires an electrode-geometry
 * override (state->geom_required, from the heartbeat NP_SESSION_STATUS_GEOM_
 * REQUIRED bit) but no valid area command has yet set a non-default limit on
 * CLIN_STIM, this clears CLIN_STIM from granted_mask.  A lost or delayed area
 * command therefore keeps HD-tDCS DISABLED rather than running it at the
 * permissive 1000µC pad default — fail-closed, not fail-open.
 *
 * Scoped to CLIN_STIM only: clinical tACS shares the same enable bit but does
 * not set geom_required, so it is never blocked by this gate.
 *
 * Call among the interlocks, BEFORE the charge-accumulate loop, so a gated
 * channel accrues no charge while blocked.
 */
void np_charge_monitor_geom_gate(np_safety_state_t *state)
{
    if (state->geom_required && !s_geom_applied_clin_stim) {
        state->granted_mask &= (uint16_t)~NP_SAFETY_EN_CLIN_STIM;
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
    /* OI-CHARGE-03: re-arm the geometry gate for the new session. */
    s_geom_applied_clin_stim = false;
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
