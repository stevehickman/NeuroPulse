/*
 * NeurOne Hub Control — Stimulation Driver (BES/tACS + tDCS)
 * Document: NP-FW-HUB-001 Rev 1 §8.3
 *
 * Covers two protocol module types on a shared driver because both use the same
 * stimulation DAC hardware.  The session runner dispatches NP_MOD_BES_TACS and
 * NP_MOD_TDCS commands to the same control() function; the type field in the
 * registry entry identifies which command format to expect.
 *
 * Safety constraints (hardware-enforced by safety MCU):
 *   BES/tACS: ≤ 1mA per electrode pair (40µC/cm² charge density hardware limit)
 *   tDCS:     ≤ 2mA DC; 30s minimum ramp enforced in firmware; hardware charge
 *              density limit 40µC/cm² (safety MCU cannot be overridden by app)
 *
 * HAL stubs:
 *   OI-STIM-01: np_mod_stim_hal_dac_set(pair, waveform, freq_mhz, amp_ua)
 *   OI-STIM-02: np_mod_stim_hal_dac_stop(pair)
 *   OI-STIM-03: np_mod_stim_hal_dc_set(pair, current_ua, polarity)
 *   OI-STIM-04: np_mod_stim_hal_dc_ramp(pair, target_ua, polarity, ramp_s)
 *   OI-STIM-05: np_mod_stim_hal_read_impedance(pair) → float kohm
 *   OI-STIM-06: np_mod_stim_hal_read_current(pair)   → float µA (actual delivered)
 *               For BES/tACS this is the PEAK magnitude over the sample window,
 *               the same quantity as the commanded amplitude_ua, because the
 *               OI-FMEA-09 cross-check (np_stim_xcheck.c) compares the two.
 *   OI-STIM-07: np_mod_stim_hal_read_charge(pair)    → uint16_t µC (session total)
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_session_log.h"
#include "np_safety_spi.h"
#include "np_stim_xcheck.h"
#include "np_ramp_plan.h"
#include <string.h>

#include "np_sw02_platform_hal.h"

/* Safety caps (firmware layer; safety MCU has an independent hardware cap). */
#define STIM_MAX_AMP_BES_UA   1000U  /* 1mA */
#define STIM_MAX_AMP_TDCS_UA  2000U  /* 2mA */
#define STIM_MIN_RAMP_S       30U

/* ── State ───────────────────────────────────────────────────────────────────── */

/* tDCS ramp model (SWR-SS-05).  The hub models the ramp it commanded: a straight line from the level at the start of
 * the segment to the segment's end, at the slope target / ramp_s.  A ramp-down on pause is at that slope, and a resume
 * during it ramps back up at the same slope from where the down-ramp had got to, so the cost of a pause is only the
 * distance it covered. */
typedef enum { TD_IDLE = 0, TD_UP, TD_STEADY, TD_DOWN } td_phase_t;

typedef struct {
    np_hub_mod_type_t type;
    uint8_t           active_pair;
    bool              active;
    /* tDCS only */
    td_phase_t        phase;
    uint8_t           polarity;
    uint16_t          target_ua;
    uint16_t          ramp_s;        /* the full-scale ramp, seconds (>= STIM_MIN_RAMP_S) */
    uint16_t          seg_from_ua;   /* level at the start of the current segment */
    uint16_t          seg_to_ua;
    uint32_t          seg_start_ms;
    uint32_t          seg_ms;        /* segment length */
} np_mod_stim_state_t;

static uint32_t s_now_ms;            /* the runner's wall clock, as of the last np_mod_stim_tick() */

/* One driver, two slots: NP_HUB_SLOT_BES_TACS and NP_HUB_SLOT_TDCS. They share
 * the silicon but not the state — they carry different parameter structs and the
 * safety MCU gates them on different enable bits.
 *
 * This used to guess, returning s_bes_state whenever s_bes_state.type was
 * NP_MOD_BES_TACS — which np_mod_stim_init always makes true, so BOTH slots
 * resolved to the BES state. It went unnoticed because the driver had no
 * probe-table entry and was never reached. */
static np_mod_stim_state_t s_bes_state;
static np_mod_stim_state_t s_tdcs_state;

static np_mod_stim_state_t *state_for_slot(uint8_t slot)
{
    switch (slot) {
        case NP_HUB_SLOT_BES_TACS: return &s_bes_state;
        case NP_HUB_SLOT_TDCS:     return &s_tdcs_state;
        default:                   return NULL;
    }
}


/* ── tDCS ramp model ─────────────────────────────────────────────────────────── */

static uint16_t td_level(const np_mod_stim_state_t *st, uint32_t now_ms)
{
    switch (st->phase) {
        case TD_UP:
        case TD_DOWN: {
            const uint32_t dt = now_ms - st->seg_start_ms;
            if (st->seg_ms == 0U || dt >= st->seg_ms) { return st->seg_to_ua; }
            const int32_t span = (int32_t)st->seg_to_ua - (int32_t)st->seg_from_ua;
            return (uint16_t)((int32_t)st->seg_from_ua + (int32_t)(((int64_t)span * (int64_t)dt) / (int64_t)st->seg_ms));
        }
        case TD_STEADY: return st->target_ua;
        default:        return 0U;
    }
}

/* Whole seconds, rounded up, at least 1: the hardware ramp takes seconds, and rounding up is never faster than the slope. */
static uint16_t ceil_s(uint32_t ms)
{
    uint32_t s = (ms + 999U) / 1000U;
    if (s < 1U) { s = 1U; }
    if (s > 0xFFFFU) { s = 0xFFFFU; }
    return (uint16_t)s;
}

/* Time to cover `delta` µA at the full-scale slope target / ramp_s. */
static uint32_t td_time_ms(const np_mod_stim_state_t *st, uint32_t delta_ua)
{
    if (st->target_ua == 0U) { return 0U; }
    return (uint32_t)(((uint64_t)delta_ua * (uint64_t)st->ramp_s * 1000ULL + st->target_ua - 1U) / st->target_ua);
}

static void td_segment(np_mod_stim_state_t *st, td_phase_t ph, uint16_t from, uint16_t to, uint32_t ms, uint32_t now_ms)
{
    st->phase = ph; st->seg_from_ua = from; st->seg_to_ua = to; st->seg_start_ms = now_ms; st->seg_ms = ms;
}

static void td_finish_off(np_mod_stim_state_t *st)
{
    np_safety_spi_request_disable(NP_SAFETY_EN_TDCS);
    np_safety_spi_set_channel_current(NP_SAFETY_CH_TDCS, 0U);
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 0U, 0U);
    st->phase = TD_IDLE;
    st->active = false;
}

void np_mod_stim_tick(uint32_t now_ms)
{
    s_now_ms = now_ms;
    np_mod_stim_state_t *st = &s_tdcs_state;
    if (!st->active) { return; }
    if (st->phase == TD_UP && now_ms - st->seg_start_ms >= st->seg_ms) { st->phase = TD_STEADY; }
    if (st->phase == TD_DOWN && now_ms - st->seg_start_ms >= st->seg_ms) { td_finish_off(st); }
}

/* Pause: ramp down at the slope, with the enable held until the current reaches zero.  Stimulation is delivered for as
 * long as the ramp-down takes; a stop or abort still cuts at once (np_mod_stim_control with no parameters). */
bool np_mod_stim_tdcs_pause(void)
{
    np_mod_stim_state_t *st = &s_tdcs_state;
    if (!st->active || st->phase == TD_DOWN) { return st->active; }
    const uint16_t level = td_level(st, s_now_ms);
    if (level == 0U) { td_finish_off(st); return false; }
    const uint32_t ms = td_time_ms(st, level);
    const uint16_t s = ceil_s(ms);
    if (np_mod_stim_hal_dc_ramp(st->active_pair, 0U, st->polarity, s) != NP_HUB_OK) {
        /* cannot ramp it down: fall back to what a stop does */
        (void)np_mod_stim_hal_dc_ramp(st->active_pair, 0U, 0U, STIM_MIN_RAMP_S);
        td_finish_off(st);
        return false;
    }
    td_segment(st, TD_DOWN, level, 0U, (uint32_t)s * 1000U, s_now_ms);
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 0U, (uint32_t)s * 1000U);   /* hold the previous level as the bound */
    return true;
}

void np_mod_stim_tdcs_ramp(np_ramp_report_t *out)
{
    const np_mod_stim_state_t *st = &s_tdcs_state;
    out->phase = NP_RAMP_NONE; out->pct = 0U; out->remaining_ms = 0U;
    if (!st->active || (st->phase != TD_UP && st->phase != TD_DOWN)) { return; }
    uint32_t dt = s_now_ms - st->seg_start_ms;
    if (dt > st->seg_ms) { dt = st->seg_ms; }
    out->phase = (st->phase == TD_UP) ? NP_RAMP_UP : NP_RAMP_DOWN;
    out->remaining_ms = st->seg_ms - dt;
    out->pct = (st->seg_ms == 0U) ? 100U : (uint8_t)(((uint64_t)dt * 100ULL) / st->seg_ms);
}

/* How long dispatching this tDCS command now would take to reach its level: the whole ramp, or the rest of it when it
 * is a resume that reverses a ramp-down in progress. */
uint32_t np_mod_stim_tdcs_ramp_up_ms(const void *params, uint16_t len)
{
    if (params == NULL || len != sizeof(np_mod_tdcs_params_t)) { return 0U; }
    const np_mod_tdcs_params_t *p = (const np_mod_tdcs_params_t *)params;
    const uint16_t cur = (p->current_ua > STIM_MAX_AMP_TDCS_UA) ? STIM_MAX_AMP_TDCS_UA : p->current_ua;
    const uint16_t ramp_s = (p->ramp_s < STIM_MIN_RAMP_S) ? STIM_MIN_RAMP_S : p->ramp_s;
    const np_mod_stim_state_t *st = &s_tdcs_state;
    if (st->active && st->phase == TD_DOWN && st->active_pair == p->electrode_pair && st->polarity == p->polarity &&
        st->target_ua == cur && st->ramp_s == ramp_s) {
        const uint16_t level = td_level(st, s_now_ms);
        if (level >= cur) { return 0U; }
        return (uint32_t)ceil_s(td_time_ms(st, (uint32_t)cur - level)) * 1000U;
    }
    return (uint32_t)ramp_s * 1000U;
}

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_stim_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    /* Stimulation hardware is always present (fixed silicon on hub PCB).
     * The registry calls detect once per stim slot; the slot IS the type.
     * Any other slot is not this driver's — say so rather than defaulting to
     * tDCS, which would register the wrong modality against it. */
    switch (slot) {
        case NP_HUB_SLOT_BES_TACS: *type_out = NP_MOD_BES_TACS; return NP_HUB_OK;
        case NP_HUB_SLOT_TDCS:     *type_out = NP_MOD_TDCS;     return NP_HUB_OK;
        default:                   return NP_HUB_ERR_NOT_PRESENT;
    }
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_stim_init(uint8_t slot)
{
    (void)slot;
    memset(&s_bes_state,  0, sizeof(s_bes_state));
    memset(&s_tdcs_state, 0, sizeof(s_tdcs_state));
    s_bes_state.type  = NP_MOD_BES_TACS;
    s_tdcs_state.type = NP_MOD_TDCS;
    return NP_HUB_OK;
}

/* ── Control ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_stim_control(uint8_t slot, const void *params, uint16_t len)
{
    np_mod_stim_state_t *st = state_for_slot(slot);
    if (st == NULL) { return NP_HUB_ERR_INVALID_ARG; }

    /* Stop */
    if (params == NULL || len == 0U) {
        if (st->active) {
            if (st->type == NP_MOD_BES_TACS) {
                (void)np_mod_stim_hal_dac_stop(st->active_pair);
                np_safety_spi_request_disable(NP_SAFETY_EN_BES_TACS);
                /* OI-CHARGE-05 (c): the published commanded current is cleared
                 * beside the enable it belongs to, so the heartbeat never
                 * reports a channel still drawing after it was disabled. */
                np_safety_spi_set_channel_current(NP_SAFETY_CH_BES_TACS, 0U);
                np_stim_xcheck_commanded(NP_SAFETY_CH_BES_TACS, 0U, 0U);
            } else {
                /* tDCS ramp to zero over 30s before cutting */
                (void)np_mod_stim_hal_dc_ramp(st->active_pair, 0U, 0U, STIM_MIN_RAMP_S);
                np_safety_spi_request_disable(NP_SAFETY_EN_TDCS);
                np_safety_spi_set_channel_current(NP_SAFETY_CH_TDCS, 0U);
                /* OI-FMEA-09: delivery ramps down over those 30 s, so the
                 * cross-check holds the previous level as its bound. */
                np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 0U,
                                         STIM_MIN_RAMP_S * 1000U);
            }
            st->active = false;
            st->phase  = TD_IDLE;
        }
        return NP_HUB_OK;
    }

    if (st->type == NP_MOD_BES_TACS) {
        if (len != sizeof(np_mod_bes_tacs_params_t)) {
            return NP_HUB_ERR_INVALID_ARG;
        }
        const np_mod_bes_tacs_params_t *p = (const np_mod_bes_tacs_params_t *)params;

        /* Firmware amplitude cap */
        uint16_t amp = (p->amplitude_ua > STIM_MAX_AMP_BES_UA)
                        ? STIM_MAX_AMP_BES_UA : p->amplitude_ua;

        np_hub_status_t rc = np_mod_stim_hal_dac_set(p->electrode_pair,
                                                       p->waveform,
                                                       p->freq_mhz,
                                                       amp);
        if (rc != NP_HUB_OK) { return NP_HUB_ERR_MOD_FAULT; }
        st->active_pair = p->electrode_pair;
        st->active = true;
        np_safety_spi_request_enable(NP_SAFETY_EN_BES_TACS);
        /* OI-CHARGE-05 (c): publish the capped commanded amplitude — the same
         * number handed to the DAC, so the safety MCU's per-phase check runs
         * against what was actually commanded, not what was authored. */
        np_safety_spi_set_channel_current(NP_SAFETY_CH_BES_TACS, amp);
        /* OI-FMEA-09: the same number, to the commanded-vs-delivered check.
         * The DAC steps, so there is no ramp to hold through. */
        np_stim_xcheck_commanded(NP_SAFETY_CH_BES_TACS, amp, 0U);

    } else { /* tDCS */
        if (len != sizeof(np_mod_tdcs_params_t)) {
            return NP_HUB_ERR_INVALID_ARG;
        }
        const np_mod_tdcs_params_t *p = (const np_mod_tdcs_params_t *)params;

        /* OI-CHARGE-04: an undeclared pad area is a rejected command, not a
         * fall back to the safety MCU's 25cm² default.  The MCU's geometry
         * gate would keep TDCS out of granted_mask anyway, but that presents
         * as a session where tDCS silently never starts; failing the command
         * here says why.  The app validators reject the same protocol before
         * it is ever signed, so reaching this is a compiler defect.          */
        if (p->electrode_area_mcm2 == 0U) {
            return NP_HUB_ERR_INVALID_ARG;
        }

        uint16_t current = (p->current_ua > STIM_MAX_AMP_TDCS_UA)
                            ? STIM_MAX_AMP_TDCS_UA : p->current_ua;
        uint16_t ramp_s  = (p->ramp_s < STIM_MIN_RAMP_S) ? STIM_MIN_RAMP_S : p->ramp_s;

        /* SWR-SS-05: a resume during a ramp-down reverses it, at the same slope, from where it had got to. */
        uint16_t from = 0U;
        uint32_t seg_ms = (uint32_t)ramp_s * 1000U;
        uint16_t hal_s = ramp_s;
        if (st->active && st->phase == TD_DOWN && st->active_pair == p->electrode_pair &&
            st->polarity == p->polarity && st->target_ua == current && st->ramp_s == ramp_s) {
            from = td_level(st, s_now_ms);
            if (from < current) {
                hal_s  = ceil_s(td_time_ms(st, (uint32_t)current - from));
                seg_ms = (uint32_t)hal_s * 1000U;
            } else {
                seg_ms = 0U;
            }
        }

        np_hub_status_t rc = np_mod_stim_hal_dc_ramp(p->electrode_pair,
                                                       current,
                                                       p->polarity,
                                                       hal_s);
        if (rc != NP_HUB_OK) { return NP_HUB_ERR_MOD_FAULT; }
        st->active_pair = p->electrode_pair;
        st->active = true;
        st->polarity = p->polarity;
        st->target_ua = current;
        st->ramp_s = ramp_s;
        td_segment(st, (seg_ms == 0U) ? TD_STEADY : TD_UP, from, current, seg_ms, s_now_ms);
        np_safety_spi_request_enable(NP_SAFETY_EN_TDCS);
        /* OI-CHARGE-05 (c): the capped DC target.  Published at the ramp TARGET
         * rather than tracking the ramp, which over-states delivered charge
         * during the 30 s ramp-up — the strict direction, and the one that
         * keeps this a property of the signed descriptor rather than of
         * hardware timing the MCU cannot verify. */
        np_safety_spi_set_channel_current(NP_SAFETY_CH_TDCS, current);
        /* OI-FMEA-09: a lower target is reached over ramp_s, not at once. */
        np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, current, seg_ms);
    }

    return NP_HUB_OK;
}

/* ── Telemetry ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_stim_telemetry(uint8_t slot, np_telem_record_t *out)
{
    np_mod_stim_state_t *st = state_for_slot(slot);
    if (st == NULL || out == NULL) { return NP_HUB_ERR_INVALID_ARG; }

    out->mod_type  = st->type;
    out->slot      = slot;

    np_telem_stim_t *s = &out->data.stim;
    s->current_ua    = np_mod_stim_hal_read_current(st->active_pair);
    s->impedance_kohm= np_mod_stim_hal_read_impedance(st->active_pair);
    s->charge_uC     = np_mod_stim_hal_read_charge(st->active_pair);
    s->ramp_active   = st->active;

    return NP_HUB_OK;
}

/* ── Shutdown ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_stim_shutdown(uint8_t slot)
{
    return np_mod_stim_control(slot, NULL, 0U);
}
