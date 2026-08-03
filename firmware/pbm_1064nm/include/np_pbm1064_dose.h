/*
 * NeurOne 1064nm Smart Zone Module — InGaAs PD Dose Metering API
 * Document: NP-FW-PBM1064-001 Rev A §6
 */

#ifndef NP_PBM1064_DOSE_H
#define NP_PBM1064_DOSE_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm1064_types.h"
#include "np_pbm1064_drive.h"   /* np_pbm1064_drv_slot_t used in the throttle API */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Load calibration coefficients (one per wavelength) for a single active
 * socket. Stub: always installs firmware defaults with valid = false (SHDR:
 * cal_source = NP_CAL_DEFAULT) — real Config-partition sourcing is pending
 * OI-HUB-C06 and, per NP-HW-HUB-001 Rev C §9.5, MUST be keyed to module UID
 * (via np_module_map), never to socket_id: modules are swappable, and a
 * socket-indexed store would silently apply the previous occupant's
 * calibration after a swap while cal_source still read FACTORY. Called once
 * per active socket at session start.
 */
void np_pbm1064_dose_load_cal_stub(np_pbm1064_cal_t cal_out[NP_PBM1064_WL_COUNT]);

/*
 * Execute one 10 Hz dose tick for a single active socket.
 * Reads PD1 and PD2 ADC, computes irradiance and dose increment.
 * Updates dose_state in place.
 * Returns NP_PBM1064_OK normally; NP_PBM1064_ERR_DOSE_LIMIT if any per-wl
 * dose limit is newly reached (caller must disable that channel).
 */
np_pbm1064_status_t np_pbm1064_dose_tick(
    uint8_t                    socket_id,
    const np_pbm1064_cal_t     cal[NP_PBM1064_WL_COUNT],
    np_pbm1064_dose_state_t   *dose);

/*
 * Compute aggregate irradiance across all wavelengths for a socket.
 * Returns sum of dose->irradiance_mW_cm2[0..2].
 */
float np_pbm1064_dose_aggregate_irradiance(const np_pbm1064_dose_state_t *dose);

/*
 * Evaluate PD1/PD2 ratio for fouling vs aging disambiguation.
 * Called after np_pbm1064_dose_tick() updates dose->pd1_counts / pd2_counts.
 *
 * fouling_out:  true if PD1 attenuated ≥20% below nominal, PD2 stable.
 * aging_out:    true if both PD1 and PD2 attenuated proportionally.
 *
 * Writes alert flags to SHDR (ratio slope only; no raw ADC counts).
 * This function only evaluates wavelength index NP_WL_1064NM (most diagnostic).
 */
void np_pbm1064_dose_evaluate_ratio(
    uint8_t                        socket_id,
    const np_pbm1064_cal_t        *cal_1064,
    const np_pbm1064_dose_state_t *dose,
    uint32_t                       device_session_count,
    bool                          *fouling_out,
    bool                          *aging_out);

/*
 * Apply aggregate irradiance throttle cascade across channels of one socket.
 * Proportionally reduces duty on CH_C → CH_B → CH_A until aggregate
 * irradiance falls below NP_PBM1064_AGGREGATE_IRRADIANCE_MW_CM2.
 * Writes adjusted duty registers via np_pbm1064_drive_set_duty().
 */
np_pbm1064_status_t np_pbm1064_dose_apply_throttle(
    uint8_t socket_id,
    float   aggregate_irradiance,
    np_pbm1064_drv_slot_t *drv);

/*
 * Reset dose state to zero (called at session start).
 */
void np_pbm1064_dose_reset(np_pbm1064_dose_state_t *dose);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_DOSE_H */
