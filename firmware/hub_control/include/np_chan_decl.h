/*
 * NeurOne Hub Control Program — Electrical-Channel Declarations to the Safety MCU
 * Document: NP-FW-HUB-001 Rev 2 §5.4; NP-FW-MMSOCK-001 §5.3 C-1 (OI-MMSOCK-02)
 *
 * ── What this module is ──────────────────────────────────────────────────────
 *
 * The pure half of the runner's session-start geometry hand-off: scan a
 * verified descriptor and decide, for every electrical channel, the electrode
 * area, the waveform class and the phase duration the safety MCU must be told,
 * and which fail-closed geometry gates to arm. The runner then transmits it.
 *
 * It was inline in np_runner_run(), which is ARM-cross-only and reached by no
 * host test. It arms Class C gates, so it is extracted to be tested
 * (np_chan_decl_tests). No FreeRTOS dependency; no I/O.
 *
 * ── Two rules this module carries ────────────────────────────────────────────
 *
 *  1. Every channel given an area is SENT. Before extraction the area frame
 *     went out only when an HD-tDCS or tDCS command was present, so the fixed
 *     VNS (0.5 cm²), cervical-VNS (2 cm²) and BES/tACS areas were computed and
 *     then dropped in any other session — the MCU enforced its 25 cm² fallback
 *     instead: 50x looser than designed for the auricular clip. send_limits is
 *     now true whenever ANY channel carries an area.
 *
 *  2. Every channel whose area the MCU must not guess has its gate armed.
 *     HD-tDCS (CLIN_STIM, OI-CHARGE-03) and tDCS (OI-CHARGE-04) already did;
 *     BES/tACS now does too (OI-MMSOCK-02). Its area is the fixed pad constant
 *     NP_BES_ELECTRODE_AREA_MCM2 — the pad is a fixed part of the product
 *     (OI-CHARGE-07's line, np_hub_config.h), and T1 tES stays on pads
 *     (NP-FW-MMSOCK-001 P-1). A tACS target on a T1-B lattice electrode would
 *     need that electrode's area instead, which arrives with a socket target
 *     (P-2) and is not expressible today.
 *
 * IEC 62304 Class B — SW-02 hub control.
 */

#ifndef NP_CHAN_DECL_H
#define NP_CHAN_DECL_H

#include <stdbool.h>
#include <stdint.h>

#include "np_hub_types.h"

typedef struct {
    uint16_t area_mcm2[NP_SAFETY_MAX_CHANNELS];   /* 0 = keep MCU default          */
    uint8_t  wave_class[NP_SAFETY_MAX_CHANNELS];  /* NP_CHARGE_WAVE_* bits          */
    uint32_t phase_us[NP_SAFETY_MAX_CHANNELS];    /* longest declared phase         */

    bool geom_clin_stim;   /* arm NP_SESSION_STATUS_GEOM_REQUIRED  (OI-CHARGE-03)  */
    bool geom_tdcs;        /* arm NP_SESSION_STATUS_GEOM_REQ_TDCS  (OI-CHARGE-04)  */
    bool geom_bes;         /* arm NP_SESSION_STATUS_GEOM_REQ_BES   (OI-MMSOCK-02)  */

    bool send_limits;      /* some channel carries an area: send the area frame    */
    bool send_waveforms;   /* some electrical channel is commanded                 */
} np_chan_decl_t;

/*
 * np_chan_decl_build — fill *out from a verified descriptor. Always writes
 * every field of *out. desc == NULL yields an empty declaration.
 */
void np_chan_decl_build(const np_session_desc_t *desc, np_chan_decl_t *out);

/*
 * np_chan_decl_half_period_us — one phase of a periodic waveform, µs, from its
 * frequency in milli-Hz; clamped to NP_CHARGE_MAX_PHASE_US, and 0 Hz returns
 * the clamp (the strictest verdict). Exposed for the test.
 */
uint32_t np_chan_decl_half_period_us(uint16_t freq_mhz);

#endif /* NP_CHAN_DECL_H */
