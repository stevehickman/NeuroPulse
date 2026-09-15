/*
 * NeurOne — hub-side enable-word mirror (test support only)
 * Document: NP-HW-HUB-001 Rev 3 §7.2
 *
 * np_safety_protocol.h says its enable-bit block "must match
 * hub_control/np_hub_config.h".  Nothing enforced that, and the two drifted
 * apart in exactly the way this change exists to fix: NP-HW-HUB-001 Rev 3 §7.2
 * decided the collapse to one NP_SAFETY_EN_PBM_CRANIAL bit and neither header
 * followed for two revisions.
 *
 * This translation unit exists so the agreement can be TESTED rather than
 * asserted in a comment.  The two headers cannot be included in one TU — the
 * macro names collide by design — so the hub values are read here, in a TU that
 * sees ONLY the hub header, and exported as plain data for
 * np_safety_spi_proto_tests.c to compare against the safety-MCU values.
 *
 * Test support only.  Not part of any firmware image; the Class C safety MCU
 * never includes a hub header.
 */

#include <stdint.h>

#include "../../hub_control/include/np_hub_config.h"

#include "np_hub_enable_mirror.h"

/* Ordered by bit position.  A reserved position carries 0 and is checked to
 * still be absent on the safety side.                                        */
const np_hub_enable_mirror_t np_hub_enable_mirror[] = {
    { "PBM_CRANIAL", NP_SAFETY_EN_PBM_CRANIAL },
    { "BES_TACS",    NP_SAFETY_EN_BES_TACS    },
    { "TDCS",        NP_SAFETY_EN_TDCS        },
    { "VNS_HRV",     NP_SAFETY_EN_VNS_HRV     },
    { "VISUAL",      NP_SAFETY_EN_VISUAL      },
    { "INTRANASAL",  NP_SAFETY_EN_INTRANASAL  },
    { "CVNS",        NP_SAFETY_EN_CVNS        },
    { "TMS",         NP_SAFETY_EN_TMS         },
    { "PBM_1170NM",  NP_SAFETY_EN_PBM_1170NM  },
    { "CLIN_STIM",   NP_SAFETY_EN_CLIN_STIM   },
};

const unsigned np_hub_enable_mirror_count =
    (unsigned)(sizeof(np_hub_enable_mirror) / sizeof(np_hub_enable_mirror[0]));

/* The bit ≡ charge-monitor-channel-index identity, as the hub understands it. */
const uint8_t np_hub_ch_clin_stim = (uint8_t)NP_SAFETY_CH_CLIN_STIM;
const uint8_t np_hub_ch_tdcs      = (uint8_t)NP_SAFETY_CH_TDCS;

/* OI-CHARGE-05 added three more electrical channel indices to the hub header,
 * for the per-channel waveform declaration and the commanded-current publish.
 * They carry the same drift risk as the two above and are mirrored the same
 * way — a mismatch would declare one modality's waveform against another's
 * accumulator.                                                               */
const uint8_t np_hub_ch_bes_tacs  = (uint8_t)NP_SAFETY_CH_BES_TACS;
const uint8_t np_hub_ch_vns_hrv   = (uint8_t)NP_SAFETY_CH_VNS_HRV;
const uint8_t np_hub_ch_cvns      = (uint8_t)NP_SAFETY_CH_CVNS;

/* The PROVISIONAL electrode areas for the channels that author no geometry of
 * their own (OI-CHARGE-07).  Exported so the proto test can pin the fail-safe
 * DIRECTION: each must be > 0 (or the MCU falls back to the permissive 25 cm²
 * default) and <= the 25 cm² default (a larger area is a larger charge budget,
 * which may only be justified by a measurement). */
const uint16_t np_hub_bes_area_mcm2  = (uint16_t)NP_BES_ELECTRODE_AREA_MCM2;
const uint16_t np_hub_vns_area_mcm2  = (uint16_t)NP_VNS_ELECTRODE_AREA_MCM2;
const uint16_t np_hub_cvns_area_mcm2 = (uint16_t)NP_CVNS_ELECTRODE_AREA_MCM2;

/* Audio is deliberately NOT safety-MCU gated; the hub encodes that as 0 and the
 * session runner skips the enable request.  Pinned so it cannot quietly become
 * a real bit without this test noticing.                                      */
const uint16_t np_hub_en_audio = (uint16_t)NP_SAFETY_EN_AUDIO;
