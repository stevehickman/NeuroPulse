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
 *     the person learns it when the protocol is sent; there the status is
 *     NP_HUB_ERR_OUTSIDE_LIMIT, a status of its own;
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
 * signing; scripts/check-hub-wire-format.ts fails if either differs from the JSON.  The floor also keeps 0 Hz, a
 * division by zero in np_chan_decl_half_period_us(), from reaching it.  (The phase that function declares to the
 * safety MCU is the exact half period; the 2 s clamp both processors once applied is removed.) */
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

/* Goggles zone mask: 12 bits.  zone_mask_lo is bits 0..7 and the low four bits of zone_mask_hi are bits 8..11.
 * The upper four bits of zone_mask_hi are reserved and must be zero; a descriptor that sets one is refused
 * (the principal, 2026-11-11).  A width of the field, not a number with a source of its own. */
#define NP_VISUAL_ZONE_MASK_HI_RESERVED 0xF0U

/* tDCS ramp (CLAUDE.md §3: 30 s ramp).  A shorter authored ramp is refused, not
 * lengthened: a longer ramp is also a stimulus nobody authored. */
#define STIM_MIN_RAMP_S       30U

/* Intranasal duty, 25 %: inherited from the cranial tiles, not derived (UC-016).
 * INTERIM, as NP_PBM_DUTY_MAX_REG is: the 25 % duty cap is retired as policy
 * (CLAUDE.md Rev 59) and both go when the pre-signing check of OI-HEXTILE-31 is
 * built.  Until then the hub refuses above it (it used to clamp). */
#define INS_DUTY_MAX          0x32U

/*
 * Cervical VNS block length: two STRUCTURAL checks, not limits (the principal, 2026-11-11).  There is no hardcoded
 * session maximum (retired the same day; the length is the protocol's).  What cannot be run is a block the library
 * cannot represent:
 *   · too short: no longer than the library's ramp-up plus ramp-down, so it has no time at the authored level.  A
 *     block with no length at all (0: the hub never supplies one) is this case too.
 *   · too long: more whole seconds than the library's 16-bit seconds field carries (65,535 s, about 18 hours).
 * Whole seconds, rounded up, so the library's own end is never earlier than the runner's stop at the end of the block.
 */
static inline uint32_t np_cvns_block_s(uint32_t block_ms)
{
    return (block_ms / 1000U) + ((block_ms % 1000U) != 0U ? 1U : 0U);
}
/* NP_HUB_OK, NP_HUB_ERR_CVNS_BLOCK_TOO_SHORT or NP_HUB_ERR_CVNS_BLOCK_TOO_LONG. */
static inline np_hub_status_t np_cvns_block_status(uint32_t block_ms)
{
    const uint32_t s = np_cvns_block_s(block_ms);
    if (s <= ((uint32_t)NP_CVNS_RAMP_UP_S + (uint32_t)NP_CVNS_RAMP_DOWN_S)) { return NP_HUB_ERR_CVNS_BLOCK_TOO_SHORT; }
    if (s > 0xFFFFU) { return NP_HUB_ERR_CVNS_BLOCK_TOO_LONG; }
    return NP_HUB_OK;
}
static inline bool np_cvns_block_ms_ok(uint32_t block_ms)
{
    return np_cvns_block_status(block_ms) == NP_HUB_OK;
}
/* The length a slot command authors: its own duration, or, when that is 0, the rest of the session (the wire's
 * meaning of 0 for a drive command, which registers no auto-stop: NP-FW-HUB-001 section 5.3).  Both are in the signed descriptor. */
static inline uint32_t np_cmd_authored_ms(const np_session_cmd_t *c, uint32_t session_ms)
{
    if (c->duration_ms != 0U) { return c->duration_ms; }
    return (session_ms > c->start_ms) ? (session_ms - c->start_ms) : 0U;
}

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
    case NP_MOD_VISUAL: {
        np_mod_visual_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        return ((p.zone_mask_hi & NP_VISUAL_ZONE_MASK_HI_RESERVED) != 0U) ? NP_HUB_ERR_INVALID_ARG : NP_HUB_OK;
    }
    case NP_MOD_AUDIO: {
        np_mod_audio_params_t p;
        if (len < sizeof p) { return NP_HUB_OK; }
        memcpy(&p, params, sizeof p);
        if (p.level_half_db > NP_AUDIO_MAX_LEVEL_HALF_DB) { return NP_HUB_ERR_INVALID_ARG; }
        /* REQ-AUDIO-25: the carrier and, for binaural (0) and isochronic (1), carrier plus beat, in mHz so no
         * rounding is involved.  Noise and off produce no tone. */
        if (p.mode <= 1U &&
            ((uint32_t)p.carrier_hz * 1000U + (uint32_t)p.beat_mhz) > (NP_AUDIO_MAX_TONE_HZ * 1000U)) {
            return NP_HUB_ERR_INVALID_ARG;
        }
        return NP_HUB_OK;
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
 * NP_HUB_ERR_OUTSIDE_LIMIT: a status of its own (the principal, 2026-11-11), reason 40
 * in the upload-check frame, so an app can say that a value is outside a limit and
 * not only that the descriptor was built wrongly.  One status for every limit; it does
 * not say which.  The shared core refuses or holds every one of these values before
 * signing, so a descriptor that reaches this was not made by it.
 */
static inline np_hub_status_t np_cmd_limits_admit(const np_session_desc_t *desc)
{
    if (desc == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    for (uint8_t i = 0U; i < desc->cmd_count && i < NP_HUB_PROTO_CMD_MAX; i++) {
        const np_session_cmd_t *c = &desc->cmds[i];
        if (np_cmd_limits_check(c->mod_type, c->params, c->params_len) != NP_HUB_OK) {
            return NP_HUB_ERR_OUTSIDE_LIMIT;
        }
        /* A cervical VNS block is run for the length the protocol authored, never a hardcoded one.  A block the
         * library cannot represent is refused here, not shortened or stretched, with the status that says which
         * structural check it failed (REQ-FWHUB-78 (k)). */
        if (c->mod_type == NP_MOD_CVNS && c->params_len >= sizeof(np_mod_cvns_params_t)) {
            const np_hub_status_t block = np_cvns_block_status(np_cmd_authored_ms(c, desc->duration_ms));
            if (block != NP_HUB_OK) {
                return block;
            }
        }
    }
    return NP_HUB_OK;
}

#endif /* NP_CMD_LIMITS_H */
