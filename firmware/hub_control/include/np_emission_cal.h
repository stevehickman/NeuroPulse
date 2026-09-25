/*
 * NeurOne Hub Control — Absolute emission → drive, for the non-cranial emitters
 * Document: NP-NPPS-REF-001 Rev 18 §4.2/§4.7/§4.8; principal direction 2026-09-25
 *
 * "All emissions must be specified in absolute terms to be safe from hardware
 * part changes."  A protocol states what reaches the person — mW/cm² at the
 * intranasal probe's exit face, mW/cm² at the cornea, dBA at the ear — and the
 * hub converts it to a drive setting for the part fitted.  A percentage of a
 * part's capability silently changes meaning when the part changes; an absolute
 * value does not.
 *
 * The conversion needs one number per emitter: its output at full drive.  None
 * of the three parts is characterised yet —
 *
 *   intranasal   no emitter selected, no optical power      OI-NASAL-06
 *   visual       no lens emitter output specified          OI-VIS-ABS-01
 *   audio        driver sensitivity uncalibrated            OI-AUDIOHW-01
 *
 * — so each full-scale constant below is ZERO, and every conversion REFUSES
 * (returns -1).  The modules report NP_HUB_ERR_UNCHARACTERISED: nothing is
 * emitted on an assumed scale.  Setting a constant is the whole enabling change,
 * and each constant must come from a measurement of the fitted part.
 *
 * IEC 62304 Class B (SW-02).  Host-tested by tests/np_pbm_irradiance_tests.c.
 */

#ifndef NP_EMISSION_CAL_H
#define NP_EMISSION_CAL_H

#include <stdint.h>

/* Irradiance at the probe exit face at CUR code 255, 0.1 mW/cm². 0 = unknown. */
#define NP_INS_IRR_FS_DMW        0U
/* Corneal irradiance at full LED drive, µW/cm². 0 = unknown.                   */
#define NP_VIS_IRR_FS_UW         0U
/* A-weighted level at the ear at volume_pct 100, dBA. 0 = unknown.            */
#define NP_AUDIO_DBA_FS          0U

/* CUR code (0–255) for an intranasal irradiance; -1 if uncharacterised or
 * unreachable.  0 mW/cm² → code 0 (channel off).                              */
int np_ins_irr_to_code(uint16_t irr_mw_cm2);

/* Visual drive level (0–100) for a corneal irradiance in µW/cm²; -1 if
 * uncharacterised or unreachable.                                             */
int np_vis_irr_to_level(uint16_t irr_uw_cm2);

/* volume_pct (0–100) for an A-weighted level; -1 if uncharacterised or above
 * the driver's full-scale level.  0 dBA is the silent sentinel → 0.
 * Amplitude scaling: pct = 100 × 10^((L − L_fs) / 20).                        */
int np_audio_dba_to_pct(uint8_t level_dba);

#endif /* NP_EMISSION_CAL_H */
