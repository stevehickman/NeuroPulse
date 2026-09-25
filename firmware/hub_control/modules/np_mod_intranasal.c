/*
 * NeurOne Hub Control — Intranasal Y-Probe Driver
 * Document: NP-FW-HUB-001 Rev 1 §8.7
 *
 * Bilateral intranasal PBM Y-probe: 660nm + 808-830nm per probe.
 * Authentication: optical code (resistive pogo-pin sleeve ID, no NFC/EMF).
 * The hub dock stores the probe when not in use (molded dock prevents Y-junction
 * fracture — probe must be re-docked before removal to protect silicone Y-joint).
 *
 * HAL stubs:
 *   OI-INS-01: np_mod_ins_hal_auth_check() → bool (pogo-pin optical code match)
 *   OI-INS-02: np_mod_ins_hal_pd_contact() → bool (photodiode contact check)
 *   OI-INS-03: np_mod_ins_hal_pwm_set(side, cur_660, cur_808, freq, duty)
 *
 * Commands carry ABSOLUTE irradiance (mW/cm² at the probe exit face) and are
 * converted to CUR codes by np_ins_irr_to_code().  No emitter is characterised
 * (OI-NASAL-06), so every non-zero request is refused NP_HUB_ERR_UNCHARACTERISED
 * rather than driven on an assumed scale.  CW is continuous (100 % on-time).
 *   OI-INS-04: np_mod_ins_hal_pwm_stop(side)
 *   OI-INS-05: np_mod_ins_hal_pd_read(side, wl) → uint16_t (dose ADC)
 */

#include "np_emission_cal.h"
#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_safety_spi.h"
#include "np_session_log.h"
#include <string.h>

#include "np_sw02_platform_hal.h"

#define INS_DUTY_MAX 0x32U  /* 25%, same ceiling as zone modules — pulsed only */
#define INS_DUTY_FULL 0xC8U /* 100%: CW is continuous */

/* ── State ───────────────────────────────────────────────────────────────────── */

typedef struct {
    bool    active;
    bool    authenticated;
} np_mod_ins_state_t;

static np_mod_ins_state_t s_state;

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_intranasal_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    (void)slot;
    /* Probe detected by pogo-pin contact AND optical code authentication. */
    if (np_mod_ins_hal_auth_check() && np_mod_ins_hal_pd_contact()) {
        *type_out = NP_MOD_INTRANASAL;
        return NP_HUB_OK;
    }
    return NP_HUB_ERR_NOT_PRESENT;
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_intranasal_init(uint8_t slot)
{
    (void)slot;
    memset(&s_state, 0, sizeof(s_state));
    s_state.authenticated = np_mod_ins_hal_auth_check();

    /* Write SHDR auth log (SHDR: accessory authentication pass/fail). */
    np_log_shdr_zone_auth(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL,
                           s_state.authenticated);

    return s_state.authenticated ? NP_HUB_OK : NP_HUB_ERR_NOT_PRESENT;
}

/* ── Control ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_intranasal_control(uint8_t slot, const void *params, uint16_t len)
{
    (void)slot;

    /* Stop */
    if (params == NULL || len == 0U) {
        if (s_state.active) {
            np_mod_ins_hal_pwm_stop(0U); /* left */
            np_mod_ins_hal_pwm_stop(1U); /* right */
            s_state.active = false;
            np_safety_spi_request_disable(NP_SAFETY_EN_INTRANASAL);
        }
        return NP_HUB_OK;
    }

    if (!s_state.authenticated) {
        return NP_HUB_ERR_SAFETY_REJECTED;
    }

    if (len < sizeof(np_mod_intranasal_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_intranasal_params_t *p = (const np_mod_intranasal_params_t *)params;
    if (p->irr_660 == 0U && p->irr_808 == 0U) {
        return NP_HUB_ERR_INVALID_ARG;                 /* lights nothing */
    }
    const int c660 = np_ins_irr_to_code(p->irr_660);
    const int c808 = np_ins_irr_to_code(p->irr_808);
    if (c660 < 0 || c808 < 0) {
        return NP_HUB_ERR_UNCHARACTERISED;             /* OI-NASAL-06 */
    }
    const uint8_t cur_660 = (uint8_t)c660;
    const uint8_t cur_808 = (uint8_t)c808;
    /* Continuous means continuous: CW at 100 %; pulsed capped at 25 %. */
    uint8_t duty = (p->freq_code == 0U) ? INS_DUTY_FULL
                 : ((p->duty > INS_DUTY_MAX) ? INS_DUTY_MAX : p->duty);

    np_hub_status_t rc = NP_HUB_OK;

    if (p->side == 0U || p->side == 1U) { /* bilateral or left */
        rc = np_mod_ins_hal_pwm_set(0U, cur_660, cur_808, p->freq_code, duty);
    }
    if (rc == NP_HUB_OK && (p->side == 0U || p->side == 2U)) { /* bilateral or right */
        rc = np_mod_ins_hal_pwm_set(1U, cur_660, cur_808, p->freq_code, duty);
    }

    if (rc != NP_HUB_OK) { return NP_HUB_ERR_MOD_FAULT; }

    s_state.active = true;
    np_safety_spi_request_enable(NP_SAFETY_EN_INTRANASAL);
    return NP_HUB_OK;
}

/* ── Telemetry ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_intranasal_telemetry(uint8_t slot, np_telem_record_t *out)
{
    (void)slot;
    if (out == NULL) { return NP_HUB_ERR_INVALID_ARG; }
    out->mod_type = NP_MOD_INTRANASAL;
    out->slot     = NP_HUB_SLOT_INTRANASAL;
    /* Intranasal dose — maps to UHDR PBM dose record via pbm union (same struct). */
    np_telem_pbm_t *p = &out->data.pbm;
    memset(p, 0, sizeof(*p));
    /* Read PD dose ADC for each probe / wavelength — UHDR dose */
    for (uint8_t side = 0U; side < 2U; side++) {
        uint16_t pd_660 = np_mod_ins_hal_pd_read(side, 0U);
        uint16_t pd_808 = np_mod_ins_hal_pd_read(side, 1U);
        /* Accumulate dose proxy — unit conversion done by session log. */
        p->dose_J_cm2[side] = (float)(pd_660 + pd_808) * 0.001f; /* placeholder K */
    }
    return NP_HUB_OK;
}

/* ── Shutdown ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_intranasal_shutdown(uint8_t slot)
{
    return np_mod_intranasal_control(slot, NULL, 0U);
}

/* Cervical VNS (NP_MOD_CVNS) now has its own driver — see modules/np_mod_cvns.c.
 * It integrates the firmware/cervical_vns/ safety library (session + cardiac
 * interlock + stim) against the hub module framework. */
