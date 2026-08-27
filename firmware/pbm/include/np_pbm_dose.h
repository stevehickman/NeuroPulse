/*
 * NeurOne 1064nm Smart Zone Module — InGaAs PD Dose Metering API
 * Document: NP-FW-PBM1064-001 Rev 1 §6
 */

#ifndef NP_PBM_DOSE_H
#define NP_PBM_DOSE_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm_types.h"
#include "np_pbm_drive.h"   /* np_pbm_drv_slot_t used in the throttle API */

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * UID-keyed calibration (OI-HUB-C06)
 *
 * Calibration coefficients characterise a physical MODULE — its LEDs and its
 * InGaAs photodiodes, measured on that module at manufacture
 * (NP-FW-PBM1064-001 §6.6). They are therefore keyed to the module's UID and
 * never to the socket it occupies. Under hex tiling tiles are universal and
 * swappable, so a location-keyed store applies the previous occupant's
 * coefficients after any swap, and does so invisibly, because cal_source would
 * still read NP_CAL_FACTORY (NP-HW-HUB-001 Rev 3 §9.5).
 *
 * The records themselves live on the hub, in np_module_map's UID-keyed NVRAM
 * record, which already invalidates a socket's entry when its UID changes.
 * This library cannot include np_module_map.h (hub_control links this library,
 * not the reverse), so the lookup arrives as an injected provider.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Provider: look up factory calibration for `uid`.
 *
 * Returns true ONLY if a factory record exists for that exact UID and every
 * wavelength row was written to cal_out. Returning false must leave cal_out
 * untouched — np_pbm_dose_load_cal() overwrites it with defaults, and a
 * provider that half-populated the array before failing would otherwise blend
 * one module's coefficients with another's.
 */
typedef bool (*np_pbm_cal_provider_fn)(
    const np_pbm_module_uid_t *uid,
    np_pbm_cal_t               cal_out[NP_PBM_WL_COUNT],
    void                          *ctx);

/*
 * Install (or, with fn == NULL, remove) the calibration provider. With no
 * provider installed every lookup yields firmware defaults, which is exactly
 * the behaviour before OI-HUB-C06 — so this change is inert on a build that
 * has not wired the hub side up.
 */
void np_pbm_dose_set_cal_provider(np_pbm_cal_provider_fn fn, void *ctx);

/*
 * Load calibration coefficients (one per wavelength) for one active socket's
 * module, keyed by that module's UID. Called once per active socket at
 * session start.
 *
 * Fails closed to firmware defaults, returning NP_CAL_DEFAULT, when: uid is
 * NULL; uid is all-zero (empty socket / unknown module); no provider is
 * installed; or the provider reports no record. Returns NP_CAL_FACTORY only
 * when the provider affirmatively supplied coefficients for that UID.
 *
 * The return value is what belongs in np_pbm_socket_shdr_t.cal_source for
 * this socket — it reports what actually happened, per socket, rather than an
 * optimistic session-wide constant.
 */
np_cal_source_t np_pbm_dose_load_cal(
    const np_pbm_module_uid_t *uid,
    np_pbm_cal_t               cal_out[NP_PBM_WL_COUNT]);

/*
 * True iff every byte of `uid` is zero (empty socket / unknown module).
 * Mirrors np_module_uid_is_zero() on the hub side.
 */
bool np_pbm_module_uid_is_zero(const np_pbm_module_uid_t *uid);

/*
 * Execute one 10 Hz dose tick for a single active socket.
 * Reads PD1 and PD2 ADC, computes irradiance and dose increment.
 * Updates dose_state in place.
 * Returns NP_PBM_OK normally; NP_PBM_ERR_DOSE_LIMIT if any per-wl
 * dose limit is newly reached (caller must disable that channel).
 */
np_pbm_status_t np_pbm_dose_tick(
    uint8_t                    socket_id,
    const np_pbm_cal_t     cal[NP_PBM_WL_COUNT],
    np_pbm_dose_state_t   *dose);

/*
 * Compute aggregate irradiance across all wavelengths for a socket.
 * Returns sum of dose->irradiance_mW_cm2[0..2].
 */
float np_pbm_dose_aggregate_irradiance(const np_pbm_dose_state_t *dose);

/*
 * Evaluate PD1/PD2 ratio for fouling vs aging disambiguation.
 * Called after np_pbm_dose_tick() updates dose->pd1_counts / pd2_counts.
 *
 * fouling_out:  true if PD1 attenuated ≥20% below nominal, PD2 stable.
 * aging_out:    true if both PD1 and PD2 attenuated proportionally.
 *
 * Writes alert flags to SHDR (ratio slope only; no raw ADC counts).
 * This function only evaluates wavelength index NP_WL_1064NM (most diagnostic).
 */
void np_pbm_dose_evaluate_ratio(
    uint8_t                        socket_id,
    const np_pbm_cal_t        *cal_1064,
    const np_pbm_dose_state_t *dose,
    uint32_t                       device_session_count,
    bool                          *fouling_out,
    bool                          *aging_out);

/*
 * Apply aggregate irradiance throttle cascade across channels of one socket.
 * Proportionally reduces duty on CH_C → CH_B → CH_A until aggregate
 * irradiance falls below NP_PBM_AGGREGATE_IRRADIANCE_MW_CM2.
 * Writes adjusted duty registers via np_pbm_drive_set_duty().
 */
np_pbm_status_t np_pbm_dose_apply_throttle(
    uint8_t socket_id,
    float   aggregate_irradiance,
    np_pbm_drv_slot_t *drv);

/*
 * Reset dose state to zero (called at session start).
 */
void np_pbm_dose_reset(np_pbm_dose_state_t *dose);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM_DOSE_H */
