/*
 * NeurOne sLORETA-guided HD-tDCS — Type Definitions
 * Document: NP-FW-HD-001 Rev A §4
 */

#ifndef NP_HD_TYPES_H
#define NP_HD_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_hd_config.h"

/* ── Status codes ───────────────────────────────────────────────────────────── */
typedef enum {
    NP_HD_OK                   =  0,
    NP_HD_ERR_INVALID_ARG      = -1,
    NP_HD_ERR_NOT_READY        = -2,  /* insufficient EEG epochs accumulated     */
    NP_HD_ERR_IMPEDANCE_HIGH   = -3,  /* electrode impedance exceeds limit       */
    NP_HD_ERR_SAFETY_REJECTED  = -4,  /* safety MCU denied stimulation enable    */
    NP_HD_ERR_SESSION_ACTIVE   = -5,
    NP_HD_ERR_NO_SESSION       = -6,
    NP_HD_ERR_CHARGE_LIMIT     = -7,  /* charge density limit would be exceeded  */
    NP_HD_ERR_NO_WEIGHT_MATRIX = -8,  /* weight matrix not loaded                */
    NP_HD_ERR_MONTAGE_INVALID  = -9,  /* electrode set geometrically invalid     */
} np_hd_status_t;

/* ── T2 21-ch qEEG electrode labels ─────────────────────────────────────────── */
/* Channel order matches ADS1299 physical cap wiring.                            */
typedef enum {
    NP_HD_CH_FP1  =  0,
    NP_HD_CH_FP2  =  1,
    NP_HD_CH_F7   =  2,
    NP_HD_CH_F3   =  3,
    NP_HD_CH_FZ   =  4,
    NP_HD_CH_F4   =  5,
    NP_HD_CH_F8   =  6,
    NP_HD_CH_FC3  =  7,
    NP_HD_CH_FC4  =  8,
    NP_HD_CH_T7   =  9,
    NP_HD_CH_C3   = 10,
    NP_HD_CH_CZ   = 11,
    NP_HD_CH_C4   = 12,
    NP_HD_CH_T8   = 13,
    NP_HD_CH_P7   = 14,
    NP_HD_CH_P3   = 15,
    NP_HD_CH_PZ   = 16,
    NP_HD_CH_P4   = 17,
    NP_HD_CH_P8   = 18,
    NP_HD_CH_O1   = 19,
    NP_HD_CH_O2   = 20,
    NP_HD_CH_COUNT = 21,
    NP_HD_CH_NONE = 0xFF,
} np_hd_electrode_t;

/* ── Montage type ───────────────────────────────────────────────────────────── */
typedef enum {
    NP_HD_MONTAGE_RING_4X1      = 0,  /* 4×1 ring: 1 anode, 4 cathodes (~1.5 cm FWHM) */
    NP_HD_MONTAGE_BILATERAL_4X1 = 1,  /* dual hemisphere 4×1 simultaneous              */
    NP_HD_MONTAGE_STANDARD_2E   = 2,  /* standard 2-electrode (T1-compatible fallback)  */
} np_hd_montage_type_t;

/* ── Clinical target regions (common HD-tDCS targets) ───────────────────────── */
typedef enum {
    NP_HD_TARGET_DLPFC_L   = 0,  /* (-46,  36, 20) — depression, executive fn     */
    NP_HD_TARGET_DLPFC_R   = 1,  /* ( 46,  36, 20) — depression bilateral, WM     */
    NP_HD_TARGET_VLPFC_L   = 2,  /* (-51,  15,  0) — language, mood               */
    NP_HD_TARGET_ACC       = 3,  /* (  0,  28, 28) — anxiety, cingulate; DEEP:    */
                                 /* indirect modulation only, see §2.3           */
    NP_HD_TARGET_MPFC      = 4,  /* (  0,  52,  6) — default mode network         */
    NP_HD_TARGET_M1_L      = 5,  /* (-37, -21, 58) — motor rehab, TMS targeting   */
    NP_HD_TARGET_M1_R      = 6,  /* ( 37, -21, 58) — motor rehab, TMS targeting   */
    NP_HD_TARGET_CUSTOM    = 7,  /* app-supplied MNI coordinate                   */
} np_hd_clinical_target_t;

/* ── Target depth class (NP-FW-HD-001 §2.3) ─────────────────────────────────── */
/*
 * Whether a 4×1 ring can focally reach the target, which is a fact about head
 * geometry rather than about the montage algorithm.
 *
 * §2.3 claims ~1.5 cm FWHM at 10 mm depth.  That holds for targets on the
 * cortical surface, which sit 11–29 mm from their nearest cap electrode.  It does
 * not hold for a structure on the medial wall: ACC (0, 28, 28) is 47.1 mm from Fz,
 * its nearest electrode on the 21-ch cap, and ~37.9 mm from Fpz — the closest any
 * 10-10 scalp position gets.  No electrode placement reaches it focally.
 *
 * DEEP targets remain valid sLORETA SOURCE-LOCALIZATION targets — sLORETA resolves
 * deep sources — and remain selectable for stimulation, but what a 4×1 delivers
 * there is indirect network modulation, not focal stimulation of the structure.
 * Callers presenting a deep target to a clinician must say so.
 */
typedef enum {
    NP_HD_TARGET_DEPTH_SURFACE = 0,  /* cortical surface — 4×1 focality applies  */
    NP_HD_TARGET_DEPTH_DEEP    = 1,  /* medial/deep — indirect modulation only   */
} np_hd_target_depth_t;

/* ── MNI coordinate (mm, integer, MNI305 space) ─────────────────────────────── */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} np_hd_mni_t;

/* ── Band power for one electrode or voxel ─────────────────────────────────── */
typedef struct {
    float delta;   /* µV²/Hz or source power units (voxel) */
    float theta;
    float alpha;
    float beta;
} np_hd_band_power_t;

/* ── sLORETA source map result ──────────────────────────────────────────────── */
typedef struct {
    uint16_t    peak_voxel;         /* index into source power array              */
    np_hd_mni_t peak_mni;           /* MNI coordinates of peak voxel              */
    float       peak_source_power;  /* source power at peak (arbitrary units)     */
    np_hd_band_power_t peak_bands;  /* band decomposition at peak voxel           */
    bool        valid;
} np_hd_sloreta_result_t;

/* ── 4×1 ring montage electrode configuration ────────────────────────────────── */
typedef struct {
    np_hd_montage_type_t type;
    np_hd_electrode_t    center;              /* anode (4×1) or elec 1 (2-elec)   */
    np_hd_electrode_t    cathodes[NP_HD_RING_CATHODE_COUNT]; /* return cathodes   */
    uint8_t              cathode_count;        /* 4 for ring, 1 for standard 2-elec*/
    np_hd_electrode_t    center_r;            /* bilateral right hemisphere anode  */
    np_hd_electrode_t    cathodes_r[NP_HD_RING_CATHODE_COUNT]; /* bilateral right */
    uint8_t              bilateral;            /* 1 if bilateral                    */
    np_hd_mni_t          target_mni;          /* target coordinate from sLORETA   */
    uint16_t             anode_current_ua;     /* stimulation current at anode     */
} np_hd_montage_t;

/* ── Per-electrode stimulation state ────────────────────────────────────────── */
typedef struct {
    np_hd_electrode_t label;
    uint8_t           driver_channel;   /* tACS driver channel index 0–20        */
    int32_t           target_current_ua;/* signed: + = anode, - = cathode        */
    int32_t           actual_current_ua;/* current after ramp computation         */
    float             impedance_kohm;   /* last measured impedance                */
    float             charge_ua_s;      /* accumulated charge (µA·s) this session */
} np_hd_electrode_state_t;

/* ── Stimulation phase ──────────────────────────────────────────────────────── */
typedef enum {
    NP_HD_STIM_IDLE      = 0,
    NP_HD_STIM_RAMP_UP   = 1,
    NP_HD_STIM_STEADY    = 2,
    NP_HD_STIM_RAMP_DOWN = 3,
    NP_HD_STIM_DONE      = 4,
    NP_HD_STIM_FAULT     = 5,
} np_hd_stim_phase_t;

/* ── Session workflow stage ──────────────────────────────────────────────────── */
typedef enum {
    NP_HD_STAGE_IDLE        = 0,  /* no active session                           */
    NP_HD_STAGE_EEG_ACQUIRE = 1,  /* resting-state EEG accumulation              */
    NP_HD_STAGE_SLORETA     = 2,  /* source map computation                      */
    NP_HD_STAGE_MONTAGE     = 3,  /* electrode selection and impedance check      */
    NP_HD_STAGE_STIMULATION = 4,  /* active HD-tDCS delivery                     */
    NP_HD_STAGE_COMPLETE    = 5,  /* session ended normally                      */
    NP_HD_STAGE_ABORTED     = 6,  /* session aborted (safety or error)           */
} np_hd_stage_t;

/* ── UHDR session record ─────────────────────────────────────────────────────── */
/* Written to UHDR partition at session end, AES-256-XTS encrypted with         */
/* biometric-derived key.  NeurOne never holds the decryption key.           */
typedef struct {
    uint32_t          session_start_unix;    /* UTC epoch seconds                 */
    uint32_t          eeg_duration_s;        /* resting-state EEG length          */
    uint32_t          stim_duration_s;       /* actual stimulation duration        */
    uint8_t           montage_type;          /* np_hd_montage_type_t              */
    uint8_t           center_electrode;      /* np_hd_electrode_t                 */
    uint8_t           cathode_electrodes[NP_HD_RING_CATHODE_COUNT];
    int16_t           target_mni_x;
    int16_t           target_mni_y;
    int16_t           target_mni_z;
    uint16_t          anode_current_ua;
    float             peak_source_power;     /* from sLORETA result               */
    float             mean_impedance_kohm;   /* mean across montage electrodes    */
    float             total_charge_ua_s;     /* total charge delivered at anode   */
    uint8_t           abort_reason;          /* 0 = normal, else np_hd_status_t   */
    uint8_t           reserved[7];
} np_hd_session_record_t;

/* ── SHDR session summary (no user biology) ──────────────────────────────────── */
/* Written to SHDR partition.  Contains device performance metrics only.        */
typedef struct {
    uint8_t  montage_type;               /* np_hd_montage_type_t               */
    uint8_t  center_electrode;           /* electrode index — position, not user */
    uint32_t stim_duration_s;
    uint16_t anode_current_ua;
    float    mean_impedance_kohm;        /* electrode contact quality metric    */
    uint8_t  impedance_check_pass;       /* 1 if all electrodes passed          */
    uint8_t  abort_reason;               /* 0 = normal                          */
} np_hd_shdr_summary_t;

/* ── Display callback (app receives live session updates) ────────────────────── */
typedef struct {
    np_hd_stage_t        stage;
    np_hd_stim_phase_t   stim_phase;
    float                stim_progress_pct;  /* 0–100, ramp + steady + ramp     */
    float                anode_current_ua;   /* live (during ramp, changes)     */
    float                mean_impedance_kohm;
    np_hd_sloreta_result_t sloreta;          /* valid after STAGE_SLORETA done  */
    np_hd_montage_t      montage;            /* valid after STAGE_MONTAGE done  */
} np_hd_display_state_t;

/* ── Callbacks ───────────────────────────────────────────────────────────────── */
typedef void (*np_hd_display_cb_t)(const np_hd_display_state_t *state);
typedef void (*np_hd_session_end_cb_t)(const np_hd_session_record_t *record,
                                        np_hd_status_t               result);

/* ── Session configuration (delivered via cryptographically signed protocol) ─── */
typedef struct {
    np_hd_montage_type_t   montage_type;
    np_hd_clinical_target_t target;          /* or NP_HD_TARGET_CUSTOM           */
    np_hd_mni_t             custom_target_mni; /* used when target == CUSTOM     */
    uint16_t                stim_current_ua;   /* anode current, ≤ 2000 µA       */
    uint16_t                stim_duration_s;   /* 120–1800 s                     */
    uint8_t                 eeg_band_target;   /* reserved for future closed-loop */
    bool                    concurrent_eeg;    /* record EEG during stimulation   */
} np_hd_session_config_t;

#endif /* NP_HD_TYPES_H */
