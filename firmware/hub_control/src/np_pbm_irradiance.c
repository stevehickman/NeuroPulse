/*
 * NeurOne Hub Control — Transcranial PBM irradiance → drive command
 * Document: NP-HW-HEXTILE-001 Rev 14 §4.3.3 (OI-HEXTILE-25)
 *
 * See np_pbm_irradiance.h for what is refused and why nothing but duty is
 * clamped.  Host-tested by tests/np_pbm_irradiance_tests.c.
 */

#include "np_pbm_irradiance.h"
#include "np_pbm_config.h"
#include <stdbool.h>
#include <string.h>

/* floor(irr × 255 / full_scale), or -1 if the tile cannot reach irr. */
static int irr_to_code(uint16_t irr_mw_cm2, uint16_t fs_dmw)
{
    uint32_t code = ((uint32_t)irr_mw_cm2 * 10U * NP_PBM_CUR_REG_FULL) / fs_dmw;
    return (code > NP_PBM_CUR_REG_FULL) ? -1 : (int)code;
}

np_hub_status_t np_pbm_irr_resolve(np_hub_mod_type_t   type,
                                   uint8_t             freq_code,
                                   uint8_t             duty,
                                   const uint16_t      irr[3],
                                   uint8_t             ch_mask,
                                   np_pbm_drive_cmd_t *out)
{
    if (out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof *out);
    if (irr == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    uint16_t fs[3];
    uint8_t  enabled;
    if (type == NP_MOD_PBM_BASE) {
        if (irr[2] != 0U) {
            return NP_HUB_ERR_INVALID_ARG;          /* a base tile has no 1064nm */
        }
        fs[0] = NP_PBM_IRR_FS_BASE_AB_DMW;
        fs[1] = NP_PBM_IRR_FS_BASE_AB_DMW;
        fs[2] = 0U;
        enabled = 0x03U;
    } else if (type == NP_MOD_PBM_SMART) {
        if ((ch_mask & (uint8_t)~0x07U) != 0U) {
            return NP_HUB_ERR_INVALID_ARG;
        }
        fs[0] = NP_PBM_IRR_FS_SMART_AB_DMW;
        fs[1] = NP_PBM_IRR_FS_SMART_AB_DMW;
        fs[2] = NP_PBM_IRR_FS_SMART_C_DMW;
        enabled = ch_mask;
    } else {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const bool     cw      = (freq_code == NP_PBM_FREQ_CODE_CW);
    const uint16_t ceiling = cw ? NP_PBM_IRR_CW_MAX_MW_CM2 : NP_PBM_IRR_PEAK_MAX_MW_CM2;
    uint32_t       sum     = 0U;
    np_pbm_drive_cmd_t cmd;
    memset(&cmd, 0, sizeof cmd);

    for (unsigned ch = 0U; ch < 3U; ch++) {
        if (irr[ch] == 0U) {
            continue;
        }
        if ((enabled & (1U << ch)) == 0U) {
            return NP_HUB_ERR_INVALID_ARG;          /* irradiance on a disabled channel */
        }
        if (irr[ch] > ceiling) {
            return NP_HUB_ERR_INVALID_ARG;          /* over R-4 for this mode */
        }
        int code = irr_to_code(irr[ch], fs[ch]);
        if (code <= 0) {
            return NP_HUB_ERR_INVALID_ARG;          /* unreachable, or rounds to off */
        }
        cmd.cur[ch]  = (uint8_t)code;
        cmd.ch_mask |= (uint8_t)(1U << ch);
        sum += irr[ch];
    }

    if (type == NP_MOD_PBM_SMART && sum > NP_PBM_IRR_AGG_MAX_MW_CM2) {
        return NP_HUB_ERR_INVALID_ARG;              /* over R-5 */
    }

    cmd.freq_code = freq_code;
    cmd.duty      = NP_PBM_DUTY_MAX_FOR(freq_code, duty);
    *out = cmd;
    return NP_HUB_OK;
}
