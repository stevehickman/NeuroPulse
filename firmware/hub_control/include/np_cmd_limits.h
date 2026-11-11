/*
 * NeurOne Hub Control Program — command limits: refuse, never clamp
 * Document: NP-FW-HUB-001 §8 (REQ-FWHUB-78)
 *
 * The limits the hub's T1 drivers hold on a commanded value, and the one check that
 * applies them.  A command outside a limit is REFUSED (NP_HUB_ERR_INVALID_ARG).  It
 * is never reduced, raised or rounded to fit: a clamped command is a different
 * stimulus from the one authored (CLAUDE.md §3; NP-NPPS-REF-001 REQ-NPPS-23 (c)).
 *
 * The same predicate runs in two places, so they cannot disagree:
 *   · at load, np_cmd_limits_admit() over the whole descriptor (np_runner_load), so
 *     the person learns it when the protocol is sent;
 *   · at dispatch, np_cmd_limits_check() first in each driver's control(), before
 *     any hardware, enable, published current or state is touched.
 *
 * The limits keep the values they had as clamps.  This file changes what happens
 * above them, not where they are.
 *
 * Header-only and pure (no state, no HAL), so every target that compiles a driver
 * or the runner has it with no link change.  IEC 62304 Class B — SW-02 hub control.
 * The safety MCU's own ceilings are independent of these and unchanged.
 */

#ifndef NP_CMD_LIMITS_H
#define NP_CMD_LIMITS_H

#include <string.h>
#include "np_hub_types.h"
#include "../../common/include/np_shared_constants.h"   /* NP_VNS_MAX_CURRENT_UA */
#include "../../pbm/include/np_pbm_config.h"            /* NP_PBM_DUTY_MAX_REG   */
#include "../../cervical_vns/include/np_cvns_config.h"  /* NP_CVNS_* ranges      */

/* BES / tACS peak amplitude and tDCS current (CLAUDE.md §3: ≤ 1 mA T1, ≤ 2 mA).  The
 * safety MCU holds its own, independent, charge ceilings. */
#define STIM_MAX_AMP_BES_UA   1000U  /* 1 mA */
#define STIM_MAX_AMP_TDCS_UA  2000U  /* 2 mA */
/* BES / tACS frequency, in the wire's milli-hertz (CLAUDE.md §3: 0.5–40 Hz).  These are BES_TACS_MIN_HZ and
 * BES_TACS_MAX_HZ of common/npps/constants.json x 1000, the range the shared validator already enforces before
 * signing; scripts/check-hub-wire-format.ts fails if either differs from the JSON.  The floor is also what keeps
 * the phase the hub declares to the safety MCU true: at 0.5 Hz a phase is 1 s, inside NP_CHARGE_MAX_PHASE_US (2 s),
 * so np_chan_decl_half_period_us() never clamps a phase the hub then drives (and 0 Hz, a division by zero there,
 * never reaches it). */
#define NP_BES_FREQ_MHZ_MIN   500U    /* 0.5 Hz */
#define NP_BES_FREQ_MHZ_MAX   40000U  /* 40 Hz  */
/* Clinical tACS (T2) frequency floor, milli-hertz: CLINICAL_TACS_MIN_HZ of constants.json x 1000, for the same
 * reason (it shares np_chan_decl_half_period_us).  Its maximum, 100 Hz, is above what the 16-bit milli-hertz
 * field can carry, so the field is its own upper bound.  The driver is not built; this is the load-time check
 * only (np_mod_t2_stubs.c). */
#define NP_CLIN_TACS_FREQ_MHZ_MIN 500U  /* 0.5 Hz */

/* EEG PGA gain code: np_mod_eeg_params_t defines 0..6 (1x to 24x).  7 is not a gain the ADS1299 CHnSET field
 * defines for use, and a code above 7 does not fit the 3-bit field: it used to be masked into another gain. */
#define NP_EEG_GAIN_CODE_MAX  6U

/* tDCS ramp (CLAUDE.md §3: 30 s ramp).  A shorter authored ramp is refused, not
 * lengthened: a longer ramp is also a stimulus nobody authored. */
#define STIM_MIN_RAMP_S       30U

/* Intranasal duty, 25 %: inherited from the cranial tiles, not derived (UC-016).
 * INTERIM, as NP_PBM_DUTY_MAX_REG is: the 25 % duty cap is retired as policy
 * (CLAUDE.md Rev 59) and both go when the pre-signing check of OI-HEXTILE-31 is
 * built.  Until then the hub refuses above it (it used to clamp). */
#define INS_DUTY_MAX          0x32U

/*
 * np_cmd_limits_check — is this one command inside the limits the hub holds?
 *
 *   NP_HUB_OK               a stop (no parameter block), a modality this file holds
 *                           no limit for, a block too short to be that modality's
 *                           struct (the driver's own length check refuses it), or a
 *                           command inside every limit
 *   NP_HUB_ERR_INVALID_ARG  a value outside a limit; exactly at the limit is inside
 *
 * The block is copied out: it is packed and the buffer is byte-aligned.
 */
static inline np_hub_status_t np_cmd_limits_check(np_hub_mod_type_t mod_type,
                                                  const void       *params,
                                                  uint16_t          len)
{
    if (params == NULL || len == 0U) {
        return NP_HUB_OK;
    }
    switch (mod_type) {
    case NP_MOD_BES_TACS: {
        np_mod_bes_tacs_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.amplitude_ua > STIM_MAX_AMP_BES_UA ||
                p.freq_mhz < NP_BES_FREQ_MHZ_MIN || p.freq_mhz > NP_BES_FREQ_MHZ_MAX)
                 ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_CLIN_TACS: {
        np_mod_clin_tacs_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.freq_mhz < NP_CLIN_TACS_FREQ_MHZ_MIN) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_EEG: {
        np_mod_eeg_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.gain > NP_EEG_GAIN_CODE_MAX) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_AUDIO: {
        np_mod_audio_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.level_half_db > NP_AUDIO_MAX_LEVEL_HALF_DB) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_CVNS: {
        /* The ranges np_mod_cvns_build_config() holds; it calls this, so there is one definition.  A pulse
         * width of 0 is the documented default (250 us), an absent field and not a value out of range. */
        np_mod_cvns_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        const uint32_t freq_mhz = (uint32_t)p.freq_mhz;
        const uint16_t pw = (p.pulse_width_us == 0U) ? 250U : p.pulse_width_us;
        return (freq_mhz < (NP_CVNS_FREQ_HZ_MIN * 1000U) || freq_mhz > (NP_CVNS_FREQ_HZ_MAX * 1000U) ||
                p.amplitude_ua > NP_CVNS_CURRENT_UA_MAX ||
                pw < NP_CVNS_PULSE_WIDTH_US_MIN || pw > NP_CVNS_PULSE_WIDTH_US_MAX ||
                p.side > 2U)
                 ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_TDCS: {
        np_mod_tdcs_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.current_ua > STIM_MAX_AMP_TDCS_UA || p.ramp_s < STIM_MIN_RAMP_S)
                 ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_VNS_HRV: {
        np_mod_vns_hrv_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.amplitude_ua > NP_VNS_MAX_CURRENT_UA) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_INTRANASAL: {
        np_mod_intranasal_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.duty > INS_DUTY_MAX) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_PBM_BASE: {
        np_mod_pbm_base_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.duty > NP_PBM_DUTY_MAX_REG) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_PBM_SMART: {
        np_mod_pbm_smart_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return (p.duty > NP_PBM_DUTY_MAX_REG) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    default:
        return NP_HUB_OK;
    }
}

/*
 * np_cmd_limits_admit — the load-time half (REQ-FWHUB-78 (g)).  Refuses a descriptor
 * that holds ANY command np_cmd_limits_check() refuses, whole, with
 * NP_HUB_ERR_INVALID_ARG: reason 2 in the upload-check frame, the cause the apps
 * already show for a descriptor built wrongly.  The shared core refuses every one of
 * these values before signing, so a descriptor that reaches this was not made by it.
 */
static inline np_hub_status_t np_cmd_limits_admit(const np_session_desc_t *desc)
{
    if (desc == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    for (uint8_t i = 0U; i < desc->cmd_count && i < NP_HUB_PROTO_CMD_MAX; i++) {
        const np_session_cmd_t *c = &desc->cmds[i];
        if (np_cmd_limits_check(c->mod_type, c->params, c->params_len) != NP_HUB_OK) {
            return NP_HUB_ERR_INVALID_ARG;
        }
    }
    return NP_HUB_OK;
}

#endif /* NP_CMD_LIMITS_H */
