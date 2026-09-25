/*
 * NeurOne Hub Control — Absolute emission → drive, for the non-cranial emitters
 * Document: NP-NPPS-REF-001 Rev 18
 *
 * See np_emission_cal.h.  Every conversion refuses while its full-scale
 * constant is zero, which it is for all three until the part is characterised.
 */

#include "np_emission_cal.h"
#include <math.h>

int np_ins_irr_to_code(uint16_t irr_mw_cm2)
{
    if (irr_mw_cm2 == 0U) {
        return 0;
    }
#if NP_INS_IRR_FS_DMW == 0
    return -1;                                   /* OI-NASAL-06 */
#else
    uint32_t code = ((uint32_t)irr_mw_cm2 * 10U * 255U) / NP_INS_IRR_FS_DMW;
    return (code == 0U || code > 255U) ? -1 : (int)code;
#endif
}

int np_vis_irr_to_level(uint16_t irr_uw_cm2)
{
    if (irr_uw_cm2 == 0U) {
        return 0;
    }
#if NP_VIS_IRR_FS_UW == 0
    return -1;                                   /* OI-VIS-ABS-01 */
#else
    uint32_t level = ((uint32_t)irr_uw_cm2 * 100U) / NP_VIS_IRR_FS_UW;
    return (level == 0U || level > 100U) ? -1 : (int)level;
#endif
}

int np_audio_dba_to_pct(uint8_t level_dba)
{
    if (level_dba == 0U) {
        return 0;
    }
#if NP_AUDIO_DBA_FS == 0
    return -1;                                   /* OI-AUDIOHW-01 */
#else
    if (level_dba > NP_AUDIO_DBA_FS) {
        return -1;
    }
    float pct = 100.0f * powf(10.0f, ((float)level_dba - (float)NP_AUDIO_DBA_FS) / 20.0f);
    int p = (int)pct;                            /* floor: never louder than asked */
    return (p < 1) ? -1 : p;
#endif
}
