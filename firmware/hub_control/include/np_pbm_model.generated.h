/*
 * NeurOne Hub Control Program — PBM power and thermal model constants
 * GENERATED FILE — DO NOT EDIT. Source: hardware/np_pbm_model.json.
 * Regenerate: bun scripts/check-pbm-model.ts --write
 * Checked by: scripts/check-pbm-model.ts (tooling-ci.yml `pbm-model`)
 *
 * NP-PWRSRC-001 OI-PWRSRC-13: one source of truth for TILE_W, the non-PBM
 * overhead and the thermal constants, read by the TypeScript audits and by the
 * runtime governor (OI-FWHUB-09). Every value is PROVISIONAL or a PLACEHOLDER;
 * the JSON records each one's source. None is measured.
 *
 * Values are scaled integers; the macro suffix names the unit:
 *   _MW        milliwatts
 *   _DC        0.1 °C
 *   _S         seconds
 *   _MC_PER_W  0.001 °C per W
 *   _X10       CEM43 × 10
 */

#ifndef NP_PBM_MODEL_GENERATED_H
#define NP_PBM_MODEL_GENERATED_H

#define NP_PBM_MODEL_VERSION 1U

/* 25 W — provisional */
#define NP_PBM_TILE_W_660_808NM_MW 25000U
/* 6.3 W — provisional */
#define NP_PBM_TILE_W_1064NM_MW 6300U
/* 22.95 W — provisional */
#define NP_PBM_TILE_W_660_808_1064NM_MW 22950U
/* 40 W — provisional */
#define NP_PBM_AVAILABLE_W_MW 40000U
/* 6 W — provisional */
#define NP_PBM_OVERHEAD_W_MIN_MW 6000U
/* 8 W — provisional */
#define NP_PBM_OVERHEAD_W_MAX_MW 8000U
/* 28 W — provisional */
#define NP_PBM_THERMAL_BUDGET_W_MW 28000U
/* 42 degC — external-limit */
#define NP_PBM_FACE_LIMIT_C_DC 420U
/* 25 degC — provisional */
#define NP_PBM_T_AMBIENT_NOMINAL_C_DC 250U
/* 35 min — provisional */
#define NP_PBM_TAU_FACE_LOW_S 2100U
/* 45 min — provisional */
#define NP_PBM_TAU_FACE_HIGH_S 2700U
/* 0.41 degC/W — provisional */
#define NP_PBM_R_CAVITY_CONSERVATIVE_MC_PER_W 410U
/* 0.23 degC/W — provisional */
#define NP_PBM_R_CAVITY_OPTIMISTIC_MC_PER_W 230U
/* 5.7 degC — provisional */
#define NP_PBM_CALIB_FACE_RISE_C_DC 57U
/* 6.25 W — provisional */
#define NP_PBM_CALIB_TILE_W_MW 6250U
/* 2 CEM43 — placeholder */
#define NP_PBM_CEM43_REVIEW_LINE_X10 20U
/* 40 CEM43 — placeholder */
#define NP_PBM_CEM43_CONCERN_LINE_X10 400U

#endif /* NP_PBM_MODEL_GENERATED_H */
