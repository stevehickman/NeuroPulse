/*
 * NeurOne HD-tDCS — Electrode Montage Selection API
 * Document: NP-FW-HD-001 Rev A §6
 *
 * Maps MNI target coordinates from sLORETA to T2 21-ch cap electrodes.
 * Selects 4×1 ring anode + cathode set using nearest-neighbour geometry.
 */

#ifndef NP_HD_MONTAGE_H
#define NP_HD_MONTAGE_H

#include "np_hd_types.h"

/*
 * Return the MNI scalp coordinate (approximate) for a cap electrode.
 * Coordinates are from Jurcak et al. (2007) 10-20 MNI scalp atlas.
 * Returns NP_HD_ERR_INVALID_ARG if electrode == NP_HD_CH_NONE or >= NP_HD_CH_COUNT.
 */
np_hd_status_t np_hd_electrode_mni(np_hd_electrode_t electrode, np_hd_mni_t *out);

/*
 * Find the cap electrode closest (Euclidean distance) to a given MNI target.
 * Searches across all NP_HD_CH_COUNT electrodes.
 * out_dist_mm: if non-NULL, receives the distance in mm.
 */
np_hd_electrode_t np_hd_nearest_electrode(const np_hd_mni_t *target_mni,
                                           float             *out_dist_mm);

/*
 * Select a 4×1 ring montage centred on the electrode nearest to target_mni.
 *
 * Algorithm:
 *   1. E_center = electrode nearest to target_mni (becomes anode).
 *   2. Cathodes = 4 electrodes nearest to E_center, excluding E_center,
 *      selected to maximise angular spread (validate ≥ 2 quadrants covered).
 *   3. Validate: no cathode closer to target than E_center.
 *
 * Returns NP_HD_ERR_MONTAGE_INVALID if fewer than 4 surrounding electrodes
 * exist or geometric validation fails (unlikely with 21-ch cap).
 */
np_hd_status_t np_hd_montage_select_ring(const np_hd_mni_t *target_mni,
                                          np_hd_montage_t   *out);

/*
 * Select a bilateral 4×1 montage.
 * Mirrors the ring montage to the contralateral hemisphere by negating x-MNI.
 * Both hemispheres are independent — driven by separate tACS driver channels.
 */
np_hd_status_t np_hd_montage_select_bilateral(const np_hd_mni_t *target_mni,
                                               np_hd_montage_t   *out);

/*
 * Select standard 2-electrode montage (T1-compatible fallback).
 * Anode nearest to target_mni; cathode at contralateral homologue (mirror x).
 */
np_hd_status_t np_hd_montage_select_standard(const np_hd_mni_t *target_mni,
                                              np_hd_montage_t   *out);

/*
 * Unified montage selection entry point.
 * Calls one of the three functions above based on montage_type.
 */
np_hd_status_t np_hd_montage_from_mni(const np_hd_mni_t    *target_mni,
                                        np_hd_montage_type_t  montage_type,
                                        np_hd_montage_t      *out);

/*
 * Validate that all electrodes in the montage map to distinct tACS driver
 * channels and are within the 16-channel driver range.
 * Also checks spatial validity: cathodes must not be collinear or all ipsilateral.
 */
np_hd_status_t np_hd_montage_validate(const np_hd_montage_t *montage);

/*
 * Assign tACS driver channel indices to montage electrodes.
 * Driver channel assignment follows the T2 cap wiring specification.
 * Must be called after np_hd_montage_select_* before stimulation begins.
 */
np_hd_status_t np_hd_montage_assign_channels(np_hd_montage_t *montage);

/*
 * Return the predefined MNI target for a clinical target enum value.
 * For NP_HD_TARGET_CUSTOM, returns NP_HD_ERR_INVALID_ARG (use caller-supplied MNI).
 */
np_hd_status_t np_hd_clinical_target_mni(np_hd_clinical_target_t target,
                                          np_hd_mni_t            *out);

/*
 * Human-readable electrode label string for display / logging.
 */
const char *np_hd_electrode_name(np_hd_electrode_t electrode);

#endif /* NP_HD_MONTAGE_H */
