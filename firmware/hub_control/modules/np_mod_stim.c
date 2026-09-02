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
 *   OI-STIM-07: np_mod_stim_hal_read_charge(pair)    → uint16_t µC (session total)
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_session_log.h"
#include "np_safety_spi.h"
#include <string.h>

#include "np_sw02_platform_hal.h"

/* Safety caps (firmware layer; safety MCU has an independent hardware cap). */
#define STIM_MAX_AMP_BES_UA   1000U  /* 1mA */
#define STIM_MAX_AMP_TDCS_UA  2000U  /* 2mA */
#define STIM_MIN_RAMP_S       30U

/* ── State ───────────────────────────────────────────────────────────────────── */

typedef struct {
    np_hub_mod_type_t type;
    uint8_t           active_pair;
    bool              active;
} np_mod_stim_state_t;

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
            } else {
                /* tDCS ramp to zero over 30s before cutting */
                (void)np_mod_stim_hal_dc_ramp(st->active_pair, 0U, 0U, STIM_MIN_RAMP_S);
                np_safety_spi_request_disable(NP_SAFETY_EN_TDCS);
            }
            st->active = false;
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

    } else { /* tDCS */
        if (len != sizeof(np_mod_tdcs_params_t)) {
            return NP_HUB_ERR_INVALID_ARG;
        }
        const np_mod_tdcs_params_t *p = (const np_mod_tdcs_params_t *)params;

        uint16_t current = (p->current_ua > STIM_MAX_AMP_TDCS_UA)
                            ? STIM_MAX_AMP_TDCS_UA : p->current_ua;
        uint16_t ramp_s  = (p->ramp_s < STIM_MIN_RAMP_S) ? STIM_MIN_RAMP_S : p->ramp_s;

        np_hub_status_t rc = np_mod_stim_hal_dc_ramp(p->electrode_pair,
                                                       current,
                                                       p->polarity,
                                                       ramp_s);
        if (rc != NP_HUB_OK) { return NP_HUB_ERR_MOD_FAULT; }
        st->active_pair = p->electrode_pair;
        st->active = true;
        np_safety_spi_request_enable(NP_SAFETY_EN_TDCS);
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
