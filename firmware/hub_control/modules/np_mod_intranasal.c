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
 *   OI-INS-04: np_mod_ins_hal_pwm_stop(side)
 *   OI-INS-05: np_mod_ins_hal_pd_read(side, wl) → uint16_t (dose ADC)
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include "np_safety_spi.h"
#include "np_session_log.h"
#include "np_cmd_limits.h"
#include <string.h>

#include "np_sw02_platform_hal.h"

/* INS_DUTY_MAX (0x32, 25 %, the same ceiling as the cranial tiles) is in np_cmd_limits.h.  A duty above it is REFUSED,
 * never clamped (REQ-FWHUB-78).  The refusal is interim: it goes with the 25 % cap when OI-HEXTILE-31 is built. */

/* ── State ───────────────────────────────────────────────────────────────────── */

typedef struct {
    bool    active;
    bool    authenticated;
} np_mod_ins_state_t;

static np_mod_ins_state_t s_state;

/* True once the authentication failure of the probe now seated has been logged.
 * Kept outside s_state because init() clears s_state and does not end the
 * insertion.  Cleared when contact is lost or the sleeve authenticates. */
static bool s_fail_logged;

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_intranasal_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    (void)slot;
    /* Probe detected by pogo-pin contact AND optical code authentication. */
    const bool auth    = np_mod_ins_hal_auth_check();
    const bool contact = np_mod_ins_hal_pd_contact();
    if (auth && contact) {
        s_fail_logged = false;
        *type_out = NP_MOD_INTRANASAL;
        return NP_HUB_OK;
    }

    /* REQ-NASAL-16 (b), OI-PRIV-38 (NP-PRIV-ANALYSIS-004) part 3: a seated probe
     * whose sleeve does not authenticate is the failure the SHDR record exists
     * for.  The registry calls init() only after detect() succeeds, so init()'s
     * "fail" was unreachable and no failure ever reached SHDR.  It is logged here,
     * once per insertion: detect() repeats on every idle rescan (NP-FW-HUB-001
     * D-30), and a record per poll would be twice a second. */
    if (contact) {
        if (!s_fail_logged) {
            np_log_shdr_zone_auth(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL, false);
            s_fail_logged = true;
        }
    } else {
        s_fail_logged = false;
    }
    return NP_HUB_ERR_NOT_PRESENT;
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_intranasal_init(uint8_t slot)
{
    (void)slot;
    memset(&s_state, 0, sizeof(s_state));
    s_state.authenticated = np_mod_ins_hal_auth_check();

    /* Write SHDR auth log (SHDR: accessory authentication pass/fail).  A sleeve
     * that authenticated in detect() and not here is the same insertion's
     * failure, so detect() does not log it a second time. */
    np_log_shdr_zone_auth(NP_HUB_SLOT_INTRANASAL, NP_MOD_INTRANASAL,
                           s_state.authenticated);
    if (!s_state.authenticated) {
        s_fail_logged = true;
    }

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
        return NP_HUB_ERR_SLEEVE_NOT_AUTH;
    }

    if (len < sizeof(np_mod_intranasal_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_intranasal_params_t *p = (const np_mod_intranasal_params_t *)params;

    /* REQ-FWHUB-78: refused before either side is written or the enable requested, so nothing is started and a command
     * already emitting is left as it was. */
    if (np_cmd_limits_check(NP_MOD_INTRANASAL, params, len) != NP_HUB_OK) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    const uint8_t duty = p->duty;

    np_hub_status_t rc = NP_HUB_OK;

    if (p->side == 0U || p->side == 1U) { /* bilateral or left */
        rc = np_mod_ins_hal_pwm_set(0U, p->cur_660, p->cur_808, p->freq_code, duty);
    }
    if (rc == NP_HUB_OK && (p->side == 0U || p->side == 2U)) { /* bilateral or right */
        rc = np_mod_ins_hal_pwm_set(1U, p->cur_660, p->cur_808, p->freq_code, duty);
    }

    if (rc != NP_HUB_OK) {
        /* REQ-NASAL-15 (OI-SIMFW-21): fail closed.  A bilateral command writes the left side before the right,
         * so a failure on the second write left the first side emitting with s_state.active never set, and the
         * stop path (which acts only on that flag) could not reach it.  A command that does not fully apply
         * leaves nothing emitting: stop both sides whatever was written, drop the flag, withdraw the enable. */
        np_mod_ins_hal_pwm_stop(0U); /* left */
        np_mod_ins_hal_pwm_stop(1U); /* right */
        s_state.active = false;
        np_safety_spi_request_disable(NP_SAFETY_EN_INTRANASAL);
        return NP_HUB_ERR_MOD_FAULT;
    }

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
