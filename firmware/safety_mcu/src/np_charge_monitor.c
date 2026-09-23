/*
 * NeurOne Safety MCU — SW01-M03: Per-Electrode Charge Monitor
 * Document: NP-SW-001 Rev 2 §SW01-M03, NP-FMEA-001 Rev 7 §3.3
 *
 * Enforces a per-electrode charge ceiling that the app cannot override
 * (hardware enforcement via safety MCU GPIO).  Since OI-CHARGE-05 there are
 * TWO ceilings, because there are two waveform classes and only one of them
 * has a session-cumulative dose at all:
 *
 *   DC channels (tDCS, HD-tDCS)
 *       Charge ACCUMULATES.  |I| x dt is integrated across the session and
 *       compared against NP_CHARGE_DC_LIMIT_MC_CM2 x area.
 *
 *   PULSED / AC channels (BES/tACS, VNS, cervical VNS, clinical tACS)
 *       Charge does NOT accumulate: these are charge-balanced biphasic, so net
 *       delivered charge is ~zero by construction and a session integral of
 *       |I| is not a physical dose.  The quantity with a damage threshold
 *       behind it is charge PER PHASE — amplitude x phase width — compared
 *       against NP_CHARGE_PHASE_LIMIT_UC_CM2 x area.  This is a per-tick
 *       predicate on the commanded amplitude, not an integral, so it costs no
 *       state and re-evaluates as an amplitude ramps.
 *
 * A channel may be BOTH (NP_SAFETY_CH_CLIN_STIM carries HD-tDCS and clinical
 * tACS on one enable bit); both checks then run.
 *
 * Charge is accumulated each heartbeat (200ms) from the commanded current
 * carried in np_safety_rx_ext_frame_t.current_ua[].  Accumulator is reset on
 * session start.  Limits are per-channel to support mixed electrode geometries
 * within a single session.
 *
 * COMMANDED, NOT DELIVERED.  current_ua[] is what the signed session
 * descriptor asked for, never an ADC measurement.  That is a deliberate
 * SIGNATURE-INDEPENDENCE choice, not a privacy one (NP-FMEA-001 §3.3): a
 * measured value cannot be signed in advance, so a Class C cutoff keyed to one
 * would inherit the Class B hub's integrity.  The consequence — this module
 * verifies what was ASKED FOR, never what reached the scalp — is why the hub
 * runs the commanded-versus-delivered cross-check and raises the SHDR
 * divergence flag (OI-FMEA-07).  Every document calling this a charge-DENSITY
 * monitor over-claims it; it is a commanded-dose monitor.
 *
 * FMEA-M03-01 mitigation: s_charge_nc[] is uint64_t (no overflow at any
 * ceiling in this file even if polled at 10kHz for 24 hours).  MISRA C:2012
 * Rule 10.1.
 *
 * OI-CHARGE-01 CLOSED (both halves, 2026-09-15) — np_charge_monitor_accumulate()
 *   is called in np_safety_main.c every heartbeat iteration for each granted DC
 *   channel carrying a non-zero commanded current, AND np_hub_control_main.c now
 *   actually populates current_ua[].  Until 2026-09-15 only the MCU half was
 *   wired: the hub passed current_ua = NULL, channel_count = 0, so the loop
 *   below never ran and this interlock enforced nothing (OI-CHARGE-05 (c)).
 *   dt_us is always NP_SAFETY_HEARTBEAT_EXP_MS x 1000 = 200000us (constant
 *   known to the MCU from np_safety_config.h; not transmitted over SPI).
 *
 * OI-CHARGE-02 CLOSED — per-channel electrode geometry arrives as AREA and the
 *   density constants are derived here, on this Class C MCU; the hub never
 *   transmits a pre-computed charge limit.  For T2 HD-tDCS with 3.5mm Ag/AgCl
 *   electrodes (area ~0.096cm²) the per-phase limit is 40 x 0.096 = 3.84uC —
 *   260x smaller than a 25cm² pad's.
 *
 * OI-CHARGE-03 CLOSED — fail-safe geometry gate.  If a session's area command
 *   is lost/corrupt, CLIN_STIM would otherwise keep the permissive pad default
 *   (fail-OPEN).  np_charge_monitor_geom_gate() blocks CLIN_STIM from
 *   granted_mask whenever the hub has declared the session needs a geometry
 *   override (heartbeat NP_SESSION_STATUS_GEOM_REQUIRED) but no valid CLIN_STIM
 *   area has been applied — fail-closed.  Clinical tACS (same enable bit, no
 *   override declared) is unaffected.
 *
 * OI-CHARGE-04 CLOSED — T1 tDCS declares its electrode geometry too, on its own
 *   heartbeat bit (NP_SESSION_STATUS_GEOM_REQ_TDCS).  A descriptor that declares
 *   no area leaves TDCS gated off rather than falling back to 25 cm².
 *   NEITHER 25 NOR 35 CHANGED: 25 remains this MCU's fallback for channels with
 *   no declared geometry (unreachable for tDCS and HD-tDCS), and 35 is the app's
 *   authoring default for a standard sponge pad, transmitted and enforced.
 *
 * OI-MMSOCK-02 (2026-09-23, NP-FW-MMSOCK-001 §5.3 C-1) — BES/tACS declares its
 *   electrode geometry too, on NP_SESSION_STATUS_GEOM_REQ_BES, and is gated
 *   fail-closed exactly as TDCS is.  It was the last electrical channel still
 *   checked against the 25 cm² fallback; a ≤1.02 cm² T1-B lattice electrode
 *   would have been allowed ~24x its per-phase ceiling.  PENDING SW-01 REVIEW.
 *
 * OI-CHARGE-05 CLOSED (2026-09-15) — the waveform split above, the two sourced
 *   ceilings in np_safety_config.h, and the fail-closed DECLARATION gate:
 *   np_charge_monitor_decl_gate() clears any ELECTRICAL channel whose waveform
 *   class was never declared, so a lost np_safety_chan_wave_cmd_t leaves the
 *   modality disabled rather than unmonitored.  It needs no new heartbeat bit:
 *   the electrical channel set is fixed at compile time, so "should this
 *   channel have a declaration?" is answerable from NP_SAFETY_CH_ELECTRICAL_MASK
 *   alone, not from anything the hub says per session.
 */

#include "np_safety_config.h"
#include "np_safety_protocol.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Per-electrode charge accumulator — NP_SAFETY_MAX_CHANNELS channels, nC resolution.
 * Meaningful only for channels declared NP_CHARGE_WAVE_DC; pulsed channels never
 * accumulate (see the header comment).                                        */
static uint64_t s_charge_nc[NP_SAFETY_MAX_CHANNELS];
static bool     s_limit_reached[NP_SAFETY_MAX_CHANNELS];

/* Per-channel electrode area, milli-cm².  Held rather than pre-multiplied so
 * both ceilings derive from one declared number and neither can drift from the
 * other.  0 = none declared; the defaults below then apply.                   */
static uint16_t s_area_mcm2[NP_SAFETY_MAX_CHANNELS];

/* Per-channel waveform class (NP_CHARGE_WAVE_* bits).  0 = NOT DECLARED, which
 * the declaration gate treats as fail-closed for electrical channels.          */
static uint8_t  s_wave_class[NP_SAFETY_MAX_CHANNELS];

/* Per-channel phase duration, µs.  Half-period for sinusoidal, pulse width for
 * rectangular biphasic, unused for pure DC.                                    */
static uint32_t s_phase_us[NP_SAFETY_MAX_CHANNELS];

/* OI-CHARGE-03 / OI-CHARGE-04: true once a valid electrode-area command has
 * set a non-default limit on that channel this session.  Until then, if the hub
 * declares the session requires geometry for the channel, the geometry gate
 * keeps it out of granted_mask (fail-safe, not fail-open).  Reset each session
 * by np_charge_monitor_reset_session().  Tracked per channel, not as one
 * session-wide flag, so a declaration on one gated channel cannot open — or
 * close — the gate on the other.                                             */
static bool     s_geom_applied_clin_stim;
static bool     s_geom_applied_tdcs;
static bool     s_geom_applied_bes;

/* ── Limit derivation ─────────────────────────────────────────────────────── */
/*
 * Both limits come from ONE declared area, converted here so the density
 * constants never leave this Class C MCU.  Exact in nC:
 *
 *   per-phase   = NP_CHARGE_PHASE_LIMIT_UC_CM2 µC/cm² x (area_mcm2/1000) cm²
 *                 x 1000 nC/µC
 *               = NP_CHARGE_PHASE_LIMIT_UC_CM2 x area_mcm2
 *
 *   per-session = NP_CHARGE_DC_LIMIT_MC_CM2 mC/cm² x (area_mcm2/1000) cm²
 *                 x 1000000 nC/mC
 *               = NP_CHARGE_DC_LIMIT_MC_CM2 x 1000 x area_mcm2
 *
 * e.g. HD-tDCS 3.5mm electrode (96 mcm²): per-phase 40 x 96 = 3840 nC = 3.84µC.
 *      Default 25cm² pad (25000 mcm²): per-phase 1,000,000 nC = 1000µC;
 *      per-session 150 x 1000 x 25000 = 3.75e9 nC = 3.75 C.
 */
static uint16_t effective_area_mcm2(uint8_t channel)
{
    return (s_area_mcm2[channel] > 0U)
             ? s_area_mcm2[channel]
             : (uint16_t)(NP_ELECTRODE_AREA_CM2 * 1000U);
}

static uint64_t phase_limit_nc(uint8_t channel)
{
    return (uint64_t)NP_CHARGE_PHASE_LIMIT_UC_CM2 *
           (uint64_t)effective_area_mcm2(channel);
}

static uint64_t session_limit_nc(uint8_t channel)
{
    return (uint64_t)NP_CHARGE_DC_LIMIT_MC_CM2 * 1000ULL *
           (uint64_t)effective_area_mcm2(channel);
}

/* Trip one channel: drop it from granted_mask and latch the charge fault. */
static void trip_channel(np_safety_state_t *state, uint8_t channel)
{
    s_limit_reached[channel] = true;
    state->granted_mask     &= (uint16_t)~(1U << channel);
    state->status           |= NP_SAFETY_STATUS_CHARGE | NP_SAFETY_STATUS_CUTOFF;
    state->fault_slot        = channel;
}

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

static void clear_session_state(void)
{
    memset(s_charge_nc,     0, sizeof(s_charge_nc));
    memset(s_limit_reached, 0, sizeof(s_limit_reached));
    memset(s_area_mcm2,     0, sizeof(s_area_mcm2));
    memset(s_wave_class,    0, sizeof(s_wave_class));
    memset(s_phase_us,      0, sizeof(s_phase_us));
    s_geom_applied_clin_stim = false;
    s_geom_applied_tdcs      = false;
    s_geom_applied_bes       = false;
}

np_safe_status_t np_charge_monitor_init(void)
{
    clear_session_state();
    return NP_SAFE_OK;
}

/*
 * np_charge_monitor_reset_session — zero all accumulators and clear every
 * per-session declaration at session start.  Called by main loop when
 * session_active 0→1.
 *
 * Clearing s_wave_class[] here is what makes the declaration gate a
 * PER-SESSION guarantee rather than a once-per-boot one: a waveform
 * declaration never outlives the session that carried it, so a session that
 * sends no wave command runs nothing electrical, even if the previous session
 * declared the same channel.
 *
 * Also clears the latched NP_SAFETY_STATUS_CHARGE bit in state->status.  The
 * charge limit is a per-session cumulative dose, so a cutoff in one session
 * must NOT disable stimulation in the next.  Without this, a single dose-limit
 * cutoff would leave NP_SAFETY_STATUS_CHARGE set in the persistent safety state
 * forever (np_spi_watchdog_tick includes CHARGE in active_faults and forces
 * granted_mask=0 on every heartbeat), locking out all stimulation until a
 * power-cycle.  CUTOFF is not touched here: np_spi_watchdog_tick clears it on
 * the next valid heartbeat once no other interlock is asserting it.
 */
void np_charge_monitor_reset_session(np_safety_state_t *state)
{
    clear_session_state();

    /* Clear the latched charge fault so the re-armed monitor can grant again. */
    if (state != NULL) {
        state->status &= (uint8_t)~NP_SAFETY_STATUS_CHARGE;
    }
}

/* ── Per-session declarations (hub → MCU during session setup) ────────────── */

/*
 * np_charge_monitor_set_channel_area_mcm2 — set the per-channel electrode AREA
 * (OI-CHARGE-02 primary entry point).  Both ceilings derive from it.
 *
 * Called by np_safety_main.c for each channel carried in a
 * np_safety_chan_limit_cmd_t frame the hub delivers during session setup.
 *
 * channel:   0–13, matching NP_SAFETY_EN_* bit positions.
 * area_mcm2: electrode area in milli-cm² (1 unit = 0.001 cm²).  0 = keep
 *            the current value (no override).
 */
void np_charge_monitor_set_channel_area_mcm2(uint8_t channel, uint16_t area_mcm2)
{
    if (channel < NP_SAFETY_MAX_CHANNELS && area_mcm2 > 0U) {
        s_area_mcm2[channel] = area_mcm2;

        /* OI-CHARGE-03 / OI-CHARGE-04: applying a non-default geometry to a
         * gated channel opens the fail-safe geometry gate for that channel
         * this session.                                                      */
        if (channel == NP_SAFETY_CH_CLIN_STIM) {
            s_geom_applied_clin_stim = true;
        } else if (channel == NP_SAFETY_CH_TDCS) {
            s_geom_applied_tdcs = true;
        } else if (channel == NP_SAFETY_CH_BES_TACS) {
            s_geom_applied_bes = true;          /* OI-MMSOCK-02 */
        } else {
            /* no gate on other channels */
        }
    }
}

/*
 * np_charge_monitor_set_channel_waveform — declare a channel's waveform class
 * and phase duration (OI-CHARGE-05 (b) primary entry point).
 *
 * Called by np_safety_main.c for each channel carried in a
 * np_safety_chan_wave_cmd_t frame.
 *
 * channel:    0–13, matching NP_SAFETY_EN_* bit positions.
 * wave_class: NP_CHARGE_WAVE_* bits.  0 is ignored rather than stored, so a
 *             frame that declares only some channels cannot un-declare the
 *             rest; and classes OR together, because one enable bit can carry
 *             two modalities of different waveform class (CLIN_STIM).
 * phase_us:   phase duration µs — half-period for sinusoidal, pulse width for
 *             rectangular biphasic.  Clamped to NP_CHARGE_MAX_PHASE_US.
 */
void np_charge_monitor_set_channel_waveform(uint8_t  channel,
                                            uint8_t  wave_class,
                                            uint32_t phase_us)
{
    if (channel >= NP_SAFETY_MAX_CHANNELS) {
        return;
    }
    if ((wave_class & (uint8_t)NP_CHARGE_WAVE_ALL) == 0U) {
        return;   /* nothing declared — leave any existing declaration alone */
    }
    s_wave_class[channel] |= (uint8_t)(wave_class & (uint8_t)NP_CHARGE_WAVE_ALL);

    /* Longest declared phase wins: with two modalities on one enable bit the
     * larger phase yields the larger per-phase charge, which is the strict
     * reading.                                                              */
    {
        uint32_t clamped = (phase_us > NP_CHARGE_MAX_PHASE_US)
                             ? (uint32_t)NP_CHARGE_MAX_PHASE_US : phase_us;
        if (clamped > s_phase_us[channel]) {
            s_phase_us[channel] = clamped;
        }
    }
}

/* ── Gates (run BEFORE accumulate, so a gated channel accrues nothing) ────── */

/*
 * np_charge_monitor_geom_gate — OI-CHARGE-03 / OI-CHARGE-04 fail-safe
 * electrode-geometry gate.
 *
 * When the hub has declared this session requires an electrode geometry on a
 * gated channel but no valid area command has yet set a non-default limit on
 * it, this clears that channel from granted_mask.  A lost or delayed area
 * command therefore keeps the modality DISABLED rather than running it at the
 * permissive pad default — fail-closed, not fail-open.
 *
 *   CLIN_STIM — state->geom_required      (NP_SESSION_STATUS_GEOM_REQUIRED)
 *   TDCS      — state->geom_required_tdcs (NP_SESSION_STATUS_GEOM_REQ_TDCS)
 *   BES_TACS  — state->geom_required_bes  (NP_SESSION_STATUS_GEOM_REQ_BES,
 *               OI-MMSOCK-02: without it BES/tACS is checked against the 25 cm²
 *               fallback, ~24x permissive for a T1-B lattice electrode)
 *
 * The two are independent by construction.  Clinical tACS shares the CLIN_STIM
 * enable bit but declares no geometry, so it is never blocked; and because the
 * tDCS declaration rides its own bit, a session carrying both tDCS and
 * clinical tACS gates only the channel whose geometry is actually missing.
 */
void np_charge_monitor_geom_gate(np_safety_state_t *state)
{
    if (state->geom_required && !s_geom_applied_clin_stim) {
        state->granted_mask &= (uint16_t)~NP_SAFETY_EN_CLIN_STIM;
    }
    if (state->geom_required_tdcs && !s_geom_applied_tdcs) {
        state->granted_mask &= (uint16_t)~NP_SAFETY_EN_TDCS;
    }
    if (state->geom_required_bes && !s_geom_applied_bes) {
        state->granted_mask &= (uint16_t)~NP_SAFETY_EN_BES_TACS;
    }
}

/*
 * np_charge_monitor_decl_gate — OI-CHARGE-05 fail-safe WAVEFORM-DECLARATION
 * gate.  Clears from granted_mask every electrical channel whose waveform class
 * was never declared this session.
 *
 * Why it needs no heartbeat bit, where the geometry gates each needed one: the
 * geometry gates ask a per-SESSION question ("does this session's descriptor
 * declare a pad?"), which only the hub can answer.  This gate asks a
 * per-CHANNEL question fixed at compile time ("is this an electrode-bearing
 * channel at all?"), which NP_SAFETY_CH_ELECTRICAL_MASK answers without
 * trusting the hub for anything.  A hub that sends no wave command cannot
 * thereby exempt itself.
 *
 * The non-electrical channels (PBM cranial/intranasal/1170nm, visual, TMS)
 * inject no charge through electrodes and are never gated here.
 */
void np_charge_monitor_decl_gate(np_safety_state_t *state)
{
    uint8_t ch;
    for (ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        uint16_t bit = (uint16_t)(1U << ch);
        if ((NP_SAFETY_CH_ELECTRICAL_MASK & bit) == 0U) {
            continue;   /* not an electrode channel */
        }
        if (s_wave_class[ch] == 0U) {
            state->granted_mask &= (uint16_t)~bit;
        }
    }
}

/* ── DC: per-session accumulation ─────────────────────────────────────────── */

/*
 * np_charge_monitor_accumulate — add charge for one DC channel for one tick.
 * Called by np_safety_main.c every heartbeat for each granted DC channel.
 *
 * channel:    0–13 matching NP_SAFETY_EN_* bit positions.
 * current_ua: commanded current magnitude in µA (SHDR — from session descriptor).
 *             NOT ADC-measured actual current (which would be UHDR-class).
 * dt_us:      elapsed time in µs; always NP_SAFETY_HEARTBEAT_EXP_MS x 1000.
 *
 * Callers must not invoke this for a channel declared pulsed-only: a
 * session-cumulative integral of |I| on a charge-balanced waveform is not a
 * dose (OI-CHARGE-05 (b)).  np_charge_monitor_is_dc() is the predicate.
 */
void np_charge_monitor_accumulate(uint8_t channel, uint32_t current_ua, uint32_t dt_us)
{
    if (channel >= NP_SAFETY_MAX_CHANNELS) {
        return;
    }
    /* charge_nc += current_ua * dt_us / 1000  (µA·µs → nC) */
    s_charge_nc[channel] += ((uint64_t)current_ua * (uint64_t)dt_us) / 1000ULL;
}

/* True when this channel carries a DC modality this session. */
bool np_charge_monitor_is_dc(uint8_t channel)
{
    return (channel < NP_SAFETY_MAX_CHANNELS) &&
           ((s_wave_class[channel] & (uint8_t)NP_CHARGE_WAVE_DC) != 0U);
}

/*
 * np_charge_monitor_tick — check every DC channel against its per-session
 * budget.  Clears the granted_mask bit for any channel that exceeds it.
 *
 * Pulsed channels are skipped: their accumulator is never fed, so testing it
 * would be testing a zero.  Skipping them explicitly rather than relying on
 * that emptiness keeps the two models separable if accumulation is ever added
 * for another purpose.
 */
void np_charge_monitor_tick(np_safety_state_t *state)
{
    uint8_t ch;
    for (ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        if (!np_charge_monitor_is_dc(ch)) {
            continue;
        }
        if (s_charge_nc[ch] >= session_limit_nc(ch) && !s_limit_reached[ch]) {
            trip_channel(state, ch);
        }
    }
}

/* ── PULSED / AC: per-phase predicate ─────────────────────────────────────── */

/*
 * charge_per_phase_nc — charge delivered in ONE phase at this commanded
 * amplitude, in nC.
 *
 *   rectangular biphasic:  Q = I x T
 *   sinusoidal:            Q = integral over the half-cycle of a sine of peak I
 *                            = 2I/omega = I x T x 2/pi
 *
 * The 2/pi factor matters and is not a refinement: omitting it would over-state
 * a sine's phase charge by pi/2 (57%), and at the 0.5 Hz bottom of the BES/tACS
 * band — where the half-period is a full second and phase charge is at its
 * largest — that is the difference between passing and failing a protocol that
 * is within the limit.  Integer arithmetic, 2000/3141 in place of 2/pi, is
 * chosen to round the RESULT UP (2000/3141 = 0.6367 > 2/pi = 0.63662), so the
 * approximation errs strict.
 *
 * Overflow: current_ua <= 65535 and phase_us <= NP_CHARGE_MAX_PHASE_US (2e6),
 * so the widest intermediate is 65535 x 2e6 x 2000 = 2.6e14 — comfortably
 * inside uint64.
 */
static uint64_t charge_per_phase_nc(uint8_t channel, uint32_t current_ua)
{
    uint64_t phase_us = (uint64_t)s_phase_us[channel];
    uint64_t q_nc;

    if (phase_us == 0ULL) {
        return 0ULL;
    }

    if ((s_wave_class[channel] & (uint8_t)NP_CHARGE_WAVE_SINE) != 0U) {
        q_nc = ((uint64_t)current_ua * phase_us * 2000ULL) / 3141ULL / 1000ULL;
    } else {
        q_nc = ((uint64_t)current_ua * phase_us) / 1000ULL;
    }
    return q_nc;
}

/*
 * np_charge_monitor_phase_tick — check every granted PULSED/AC channel's
 * charge per phase at the amplitude commanded THIS beat.
 *
 * Unlike the DC path this is a predicate, not an integral: it holds no state
 * and re-evaluates every heartbeat, so an amplitude ramp is followed exactly
 * and a channel that drops back under the ceiling is not punished for a beat
 * it has left behind — except that, once tripped, s_limit_reached[] keeps it
 * off for the rest of the session, matching the DC path's latch.  A commanded
 * amplitude that exceeds the per-phase ceiling is a defective descriptor, not
 * a transient, so re-granting it would be re-granting a protocol the app
 * should never have signed.
 *
 * current_ua/channel_count come straight from the heartbeat frame; channels
 * beyond channel_count are treated as commanding nothing.
 */
void np_charge_monitor_phase_tick(np_safety_state_t *state,
                                  const uint16_t    *current_ua,
                                  uint8_t            channel_count)
{
    uint8_t ch;

    if (current_ua == NULL) {
        return;
    }
    for (ch = 0U; ch < channel_count && ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        uint16_t bit = (uint16_t)(1U << ch);

        if ((s_wave_class[ch] &
             (uint8_t)(NP_CHARGE_WAVE_PULSE | NP_CHARGE_WAVE_SINE)) == 0U) {
            continue;               /* pure DC, or nothing declared */
        }
        if ((state->granted_mask & bit) == 0U || current_ua[ch] == 0U) {
            continue;               /* not enabled, or commanding nothing */
        }
        if (s_limit_reached[ch]) {
            continue;               /* already latched off this session */
        }
        if (charge_per_phase_nc(ch, (uint32_t)current_ua[ch]) >=
                phase_limit_nc(ch)) {
            trip_channel(state, ch);
        }
    }
}
