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
 * Select a bilateral 4×1 montage (NP-FW-HD-001 §6.3).
 * Mirrors the ring montage to the contralateral hemisphere by negating x-MNI.
 *
 * Both hemispheres are driven SIMULTANEOUSLY, so all 10 electrodes are selected
 * against one shared claim set: no electrode may appear in both rings, and all 10
 * must map to distinct tACS driver channels. The invariant therefore holds by
 * construction, and np_hd_montage_validate() re-checks it as a post-condition.
 *
 * Where the two rings contend for an electrode (typically a midline one such as
 * Cz), the ring at the caller's target keeps it and the mirrored ring takes its
 * next-nearest candidate. The two rings are consequently NOT guaranteed to be
 * geometrically symmetric — only distinct.
 *
 * Returns NP_HD_ERR_MONTAGE_INVALID if no conflict-free mirror ring exists, or if
 * the target has no contralateral homologue — a target on (or very near) x = 0
 * mirrors onto itself, for which bilateral is undefined. ACC and MPFC are both in
 * this category; use NP_HD_MONTAGE_RING_4X1 for them. On any error `out` is
 * zeroed, never left holding a half-built montage with `bilateral` set.
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
 * Return the tACS driver channel (0 – NP_HD_DRIVER_CHANNELS-1) that the T2 cap
 * wires to a given electrode.  Returns NP_HD_DRIVER_CH_NONE if electrode is
 * NP_HD_CH_NONE or >= NP_HD_CH_COUNT.
 *
 * SINGLE SOURCE OF TRUTH for the electrode → driver channel mapping.  The table
 * is file-static in np_hd_montage.c; this accessor is the only way to reach it
 * from another translation unit.  np_hd_stim_init(), which actually programs the
 * driver, resolves through it, so the layer that certifies channel distinctness
 * and the layer that delivers current cannot disagree.
 *
 * Do not hand-copy the table into another module.  np_hd_stim.c did exactly that
 * — a clone kept in sync only by a comment — and the duplicated-selection-loop
 * form of the same mistake is what shipped the C4/P4 driver-12 collision on the
 * bilateral right ring (see ring_select_cathodes() in np_hd_montage.c).
 *
 * The mapping is currently the identity: 21 electrodes, 21 driver channels, one
 * each.  It is still reached through this accessor rather than assumed, because
 * the note above k_driver_channel[] records the conditions under which channel
 * sharing could return — at which point every caller must follow the table, not
 * an assumption that electrode index equals channel index.
 */
uint8_t np_hd_electrode_driver_channel(np_hd_electrode_t electrode);

/*
 * Validate that every electrode in the montage is in range, appears exactly once,
 * and maps to a tACS driver channel no other montage electrode uses.
 *
 * For a montage with `bilateral` set this spans all 10 electrodes of BOTH rings
 * together, not each ring separately — §6.3 energises them simultaneously, so a
 * cross-hemisphere conflict is as disqualifying as one within a hemisphere.
 *
 * Angular-spread validation is NOT performed here; it is applied during selection
 * (see np_hd_montage_select_ring).
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
 * Return whether a 4×1 ring can focally reach the predefined clinical target, or
 * whether the target is deep enough that stimulation there is indirect network
 * modulation only (NP-FW-HD-001 §2.3, §4.3).
 *
 * ACC is the only DEEP target in the current list.  Selecting a montage for a deep
 * target is permitted and returns a valid ring, but any UI or report that presents
 * it must not claim focal stimulation of the structure.
 *
 * Returns NP_HD_ERR_INVALID_ARG for NP_HD_TARGET_CUSTOM — caller-supplied MNI has
 * no precomputed depth class; classify it from the nearest-electrode distance
 * returned by np_hd_nearest_electrode() instead.
 */
np_hd_status_t np_hd_clinical_target_depth(np_hd_clinical_target_t target,
                                            np_hd_target_depth_t   *out);

/*
 * Human-readable electrode label string for display / logging.
 */
const char *np_hd_electrode_name(np_hd_electrode_t electrode);

#endif /* NP_HD_MONTAGE_H */
