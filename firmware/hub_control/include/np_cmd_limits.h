/*
 * NeurOne Hub Control Program — command limits: refuse, never clamp
 * Document: NP-FW-HUB-001 §8 (REQ-FWHUB-76)
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

/* BES / tACS peak amplitude and tDCS current (CLAUDE.md §3: ≤ 1 mA T1, ≤ 2 mA).  The
 * safety MCU holds its own, independent, charge ceilings. */
#define STIM_MAX_AMP_BES_UA   1000U  /* 1 mA */
#define STIM_MAX_AMP_TDCS_UA  2000U  /* 2 mA */
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
        return (p.amplitude_ua > STIM_MAX_AMP_BES_UA) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
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
 * np_cmd_limits_admit — the load-time half (REQ-FWHUB-76 (g)).  Refuses a descriptor
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
