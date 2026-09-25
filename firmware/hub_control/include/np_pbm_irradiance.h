/*
 * NeurOne Hub Control — Transcranial PBM irradiance → drive command
 * Document: NP-HW-HEXTILE-001 Rev 14 §4.3.3 (OI-HEXTILE-25); CLAUDE.md §3 (R-4)
 *
 * A protocol states the irradiance it wants in mW/cm² (NPPS `irradiance_mw_cm2`).
 * The hub is the first place that knows which tile it is driving, so it is the
 * place that turns that number into a CUR register code — and the place that
 * refuses a command it cannot deliver as written.
 *
 * np_pbm_irr_resolve() is pure: no I/O, no state.  It refuses (never clamps):
 *
 *   - irradiance over R-4's ceiling for the mode:
 *       CW (freq_code 0)  > NP_PBM_IRR_CW_MAX_MW_CM2   (200)
 *       pulsed            > NP_PBM_IRR_PEAK_MAX_MW_CM2 (400)
 *   - on a smart (three-channel) tile, a channel sum over R-5's aggregate
 *     ceiling, NP_PBM_IRR_AGG_MAX_MW_CM2 (600);
 *   - irradiance the tile cannot reach at full scale, or so small it rounds to
 *     a zero code (it would silently not be delivered);
 *   - a smart-tile irradiance on a channel its ch_mask does not enable.
 *
 * A protocol that asks for more than the device may or can deliver does not
 * run at something less and report success.  The duty ceiling is the one value
 * still clamped, as it always has been: CW is forced to 100 % ("continuous means
 * continuous"), pulsed is capped at 25 %.
 *
 * The full-scale irradiances below are DESIGN TARGETS (NP-HW-HEXTILE-001 §4.3,
 * scaled from 150 mA to the CUR register's 180 mA full scale), not measurements:
 * no emitter is selected (OI-HEXTILE-02).  They must be replaced from the
 * selected emitter's flux curve — and, per tile, from its dual-PD calibration
 * (OI-HUB-C06) — before a tile ships.  Conversion is floor(), so a code never
 * asks for more than the request at the design-target flux.
 *
 * IEC 62304 Class B (SW-02).  R-4 is a firmware-enforced ceiling; the Class C
 * backstop remains the safety MCU's thermal cut, and the hardware bound is
 * NP-HW-HEXTILE-001 D-9 (REQ-TDRV-01/-02) once built.
 */

#ifndef NP_PBM_IRRADIANCE_H
#define NP_PBM_IRRADIANCE_H

#include <stdint.h>
#include "np_hub_types.h"

/* R-4 (CLAUDE.md §3; NP-HW-HEXTILE-001 R-4) and R-5 (R-5). */
#define NP_PBM_IRR_PEAK_MAX_MW_CM2   400U   /* pulsed, on-state             */
#define NP_PBM_IRR_CW_MAX_MW_CM2     200U   /* continuous wave              */
#define NP_PBM_IRR_AGG_MAX_MW_CM2    600U   /* three-channel aggregate (T1-C) */

/*
 * Irradiance at CUR code 255 (180 mA), in 0.1 mW/cm², per module type and
 * channel.  NP-HW-HEXTILE-001 §4.3: T1-A (base) 45 emitters/channel, 403 mW/cm²
 * at 150 mA; T1-C (smart) 30/channel, 269 mW/cm² (660/808) and 28 mW/cm² (1064)
 * at 150 mA.  × 180/150.  DESIGN TARGETS — OI-HEXTILE-02.
 */
#define NP_PBM_IRR_FS_BASE_AB_DMW    4836U
#define NP_PBM_IRR_FS_SMART_AB_DMW   3228U
#define NP_PBM_IRR_FS_SMART_C_DMW     336U

#define NP_PBM_CUR_REG_FULL          255U

typedef struct {
    uint8_t freq_code;  /* as commanded                                        */
    uint8_t duty;       /* CW → NP_PBM_DUTY_FULL_REG; pulsed → ≤ NP_PBM_DUTY_MAX_REG */
    uint8_t cur[3];     /* CUR codes: [0]=660nm, [1]=808nm, [2]=1064nm (0 = off) */
    uint8_t ch_mask;    /* channels with a non-zero code                        */
} np_pbm_drive_cmd_t;

/*
 * Resolve one socket's command.  `irr` holds the three on-state irradiances in
 * mW/cm² ([2] must be 0 for a base tile); `ch_mask` is the smart tile's enable
 * mask (ignored for base, whose channels are lit by a non-zero irradiance).
 *
 * Returns NP_HUB_OK and fills *out, or NP_HUB_ERR_INVALID_ARG with *out zeroed.
 */
np_hub_status_t np_pbm_irr_resolve(np_hub_mod_type_t   type,
                                   uint8_t             freq_code,
                                   uint8_t             duty,
                                   const uint16_t      irr[3],
                                   uint8_t             ch_mask,
                                   np_pbm_drive_cmd_t *out);

#endif /* NP_PBM_IRRADIANCE_H */
