/*
 * NeurOne 1064nm Smart Zone Module — Type Definitions
 * Document: NP-FW-PBM1064-001 Rev A §4
 */

#ifndef NP_PBM1064_TYPES_H
#define NP_PBM1064_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_pbm1064_config.h"

/* ── Status codes ───────────────────────────────────────────────────────────── */

typedef enum {
    NP_PBM1064_OK                   =  0,
    NP_PBM1064_ERR_INVALID_ARG      = -1,
    NP_PBM1064_ERR_NOT_SMART        = -2,  /* ADC reading is base module range     */
    NP_PBM1064_ERR_DEBOUNCE_FAIL    = -3,  /* < 2/3 ADC reads agreed              */
    NP_PBM1064_ERR_I2C_PROBE_FAIL   = -4,  /* no ACK from driver IC at 0x30       */
    NP_PBM1064_ERR_I2C_WRITE        = -5,
    NP_PBM1064_ERR_I2C_READ         = -6,
    NP_PBM1064_ERR_DRIVER_FAULT     = -7,  /* STATUS.FAULT set on driver IC       */
    NP_PBM1064_ERR_THERMAL          = -8,
    NP_PBM1064_ERR_OCP              = -9,  /* over-current protection triggered   */
    NP_PBM1064_ERR_DOSE_LIMIT       = -10, /* per-wavelength dose limit reached   */
    NP_PBM1064_ERR_IRRADIANCE_LIMIT = -11, /* aggregate irradiance ceiling        */
    NP_PBM1064_ERR_SESSION_ACTIVE   = -12,
    NP_PBM1064_ERR_NO_SESSION       = -13,
    NP_PBM1064_ERR_SIG_INVALID      = -14, /* Ed25519 session descriptor failed   */
    NP_PBM1064_ERR_ADC_TIMEOUT      = -15,
    NP_PBM1064_ERR_SAFETY_REJECTED  = -16, /* safety MCU denied enable            */
} np_pbm1064_status_t;

/* ── Slot type (result of ZONE_ID ADC classification) ───────────────────────── */

typedef enum {
    NP_SLOT_ABSENT      = 0,   /* ADC ≥ NP_PBM1064_ADC_NO_MODULE_MIN            */
    NP_SLOT_SMART       = 1,   /* ADC < NP_PBM1064_ADC_SMART_MAX; I2C driver IC  */
    NP_SLOT_BASE_MODULE = 2,   /* ADC in base ladder range; standard 2-LED zone  */
} np_slot_type_t;

/* ── Wavelength index (used for coefficient arrays) ─────────────────────────── */

typedef enum {
    NP_WL_660NM  = 0,
    NP_WL_808NM  = 1,
    NP_WL_1064NM = 2,
} np_wl_idx_t;

/* ── TIA gain setting (Vishay DG2788A, Hub PCB Rev B — OI-PBM-HW-01) ────────── */

typedef enum {
    NP_TIA_GAIN_HIGH = 0,  /* GAIN_SEL LOW  → Rf = 47 kΩ (default; base / absent) */
    NP_TIA_GAIN_LOW  = 1,  /* GAIN_SEL HIGH → Rf = 22 kΩ (smart module; InGaAs)   */
} np_tia_gain_t;

/* ── Smart module detection state machine ───────────────────────────────────── */

typedef enum {
    NP_SM_IDLE        = 0,   /* polling for insertion                              */
    NP_SM_SETTLING    = 1,   /* contact settle delay                               */
    NP_SM_DEBOUNCING  = 2,   /* 3 ADC reads in progress                           */
    NP_SM_I2C_PROBING = 3,   /* sending I2C address probe                         */
    NP_SM_ANNOUNCING  = 4,   /* bone conduction announcement via np_za            */
    NP_SM_ACTIVE      = 5,   /* module confirmed, I2C operational                 */
    NP_SM_REMOVING    = 6,   /* removal debounce in progress                      */
} np_sm_state_t;

/* ── Per-slot detection context ─────────────────────────────────────────────── */

typedef struct {
    np_sm_state_t  state;
    np_slot_type_t slot_type;        /* confirmed type after debounce              */
    np_tia_gain_t  tia_gain;         /* current DG2788A gain setting for this slot */
    uint16_t       adc_reads[NP_PBM1064_DEBOUNCE_READS];
    uint8_t        read_count;
    uint32_t       next_read_ms;
    uint32_t       settle_until_ms;
    uint32_t       removal_since_ms;
    bool           removal_debounce_active;
    bool           i2c_probed;
} np_sm_slot_ctx_t;

/* ── Per-zone, per-wavelength calibration coefficients ──────────────────────── */

typedef struct {
    float K_PD1;          /* irradiance per ADC count from PD1                    */
    float K_PD2;          /* irradiance per ADC count from PD2                    */
    float K_ratio_nom;    /* factory-nominal PD1/PD2 ratio for this zone+wl       */
    bool  valid;          /* false → use firmware defaults (SHDR: cal_source=DEF) */
} np_pbm1064_cal_t;

/* ── Calibration source flag (SHDR) ─────────────────────────────────────────── */

typedef enum {
    NP_CAL_DEFAULT     = 0,  /* built-in flash constants; no factory cal present  */
    NP_CAL_FACTORY     = 1,  /* loaded from Config partition (integrating sphere)  */
} np_cal_source_t;

/* ── Per-zone dose state ─────────────────────────────────────────────────────── */

typedef struct {
    float dose_J_cm2[NP_PBM1064_WL_COUNT];        /* cumulative dose per wl       */
    float irradiance_mW_cm2[NP_PBM1064_WL_COUNT]; /* current irradiance per wl    */
    float pd1_counts[NP_PBM1064_WL_COUNT];         /* last ADC reading, PD1        */
    float pd2_counts[NP_PBM1064_WL_COUNT];         /* last ADC reading, PD2        */
    float ratio_current;    /* PD1/PD2 for most recent dose tick                  */
    bool  dose_limit_hit[NP_PBM1064_WL_COUNT]; /* channel disabled on limit        */
} np_pbm1064_dose_state_t;

/* ── Per-zone preset (from session descriptor) ───────────────────────────────── */

typedef struct {
    uint8_t cur_a;          /* CUR_A register value (660nm)                       */
    uint8_t cur_b;          /* CUR_B register value (808nm)                       */
    uint8_t cur_c;          /* CUR_C register value (1064nm)                      */
    uint8_t freq_hz;        /* initial frequency (2/6/10/20/40/0)                 */
    uint8_t duty;           /* duty value (≤ NP_PBM1064_DUTY_MAX_REG)            */
    uint8_t channel_mask;   /* which channels to enable (CH_A/B/C bits)           */
} np_pbm1064_preset_t;

/* ── Session descriptor v4 (from signed app protocol) ───────────────────────── */

#define NP_SES1064_VERSION  0x04U

typedef struct {
    uint8_t  version;               /* NP_SES1064_VERSION                         */
    uint8_t  smart_module_mask;     /* bit[n]=1 → slot n is smart module          */
    uint8_t  eeg_adaptive_mode;     /* 0=uniform, 1=gradient (OI-SES-02)          */
    uint8_t  reserved0;
    uint16_t duration_s;
    uint16_t reserved1;
    np_pbm1064_preset_t zone[NP_PBM1064_ZONE_COUNT];
    uint8_t  signature[64];         /* Ed25519                                    */
} np_pbm1064_session_desc_t;

/* ── Session stage ───────────────────────────────────────────────────────────── */

typedef enum {
    NP_PBM1064_STAGE_IDLE        = 0,
    NP_PBM1064_STAGE_PREFLIGHT   = 1,
    NP_PBM1064_STAGE_RAMP_UP     = 2,
    NP_PBM1064_STAGE_ACTIVE      = 3,
    NP_PBM1064_STAGE_RAMP_DOWN   = 4,
    NP_PBM1064_STAGE_COMPLETE    = 5,
    NP_PBM1064_STAGE_FAULT       = 6,
} np_pbm1064_stage_t;

/* ── EEG-adaptive band (maps dominant freq to PWM code) ─────────────────────── */

typedef enum {
    NP_EEG_BAND_DELTA  = 0,   /* 0.5–4 Hz  → 2 Hz code                          */
    NP_EEG_BAND_THETA  = 1,   /* 4–8 Hz    → 6 Hz code                           */
    NP_EEG_BAND_ALPHA  = 2,   /* 8–12 Hz   → 10 Hz code                          */
    NP_EEG_BAND_BETA   = 3,   /* 12–30 Hz  → 20 Hz code                          */
    NP_EEG_BAND_GAMMA  = 4,   /* >30 Hz    → 40 Hz code                          */
} np_eeg_band_t;

/* ── Fault reason codes (SHDR safe) ─────────────────────────────────────────── */

typedef enum {
    NP_PBM1064_FAULT_NONE        = 0,
    NP_PBM1064_FAULT_THERMAL     = 1,
    NP_PBM1064_FAULT_OCP_A       = 2,
    NP_PBM1064_FAULT_OCP_B       = 3,
    NP_PBM1064_FAULT_OCP_C       = 4,
    NP_PBM1064_FAULT_I2C_LOST    = 5,
    NP_PBM1064_FAULT_SAFETY_MCU  = 6,
    NP_PBM1064_FAULT_DOSE_LIMIT  = 7,
} np_pbm1064_fault_t;

/* ── UHDR session record ─────────────────────────────────────────────────────── */

typedef struct {
    uint32_t session_start_unix;
    uint32_t duration_s;
    uint8_t  smart_module_mask;
    uint8_t  eeg_adaptive_mode;
    uint8_t  abort_reason;           /* 0=normal; else np_pbm1064_status_t        */
    uint8_t  reserved;
    /* Per-zone, per-wavelength dose (J/cm²) */
    float    dose_J_cm2[NP_PBM1064_ZONE_COUNT][NP_PBM1064_WL_COUNT];
    uint16_t eeg_adapt_event_count;  /* number of freq code changes during session */
} np_pbm1064_session_record_t;

/* ── SHDR session summary (no user biology) ──────────────────────────────────── */

typedef struct {
    uint8_t  smart_module_mask;
    uint32_t duration_s;
    uint8_t  abort_reason;
    uint8_t  fault_reason;           /* np_pbm1064_fault_t                        */
    uint8_t  cal_source;             /* np_cal_source_t for each slot (packed)    */
    /* LED health metrics (device condition) */
    float    pd_ratio_zone[NP_PBM1064_ZONE_COUNT]; /* mean PD1/PD2 ratio per zone */
    uint8_t  ocp_event_count;
    uint8_t  thermal_event_count;
    uint8_t  i2c_probe_pass_mask;    /* bit[n]=1 → slot n I2C probe passed        */
} np_pbm1064_shdr_summary_t;

/* ── SHDR fault log entry ─────────────────────────────────────────────────────── */

typedef struct {
    uint32_t device_session_count;   /* unsigned; no timestamp                    */
    uint8_t  slot;
    uint8_t  channel;                /* 0=A, 1=B, 2=C, 0xFF=all                  */
    uint8_t  fault_reason;           /* np_pbm1064_fault_t                        */
    uint8_t  status_reg_value;       /* STATUS register at time of fault          */
} np_pbm1064_shdr_fault_entry_t;

/* ── Display state (app receives live session updates) ───────────────────────── */

typedef struct {
    np_pbm1064_stage_t stage;
    float              stim_progress_pct;   /* 0–100                              */
    float              dose_J_cm2_total;    /* sum across all zones and wl        */
    float              irradiance_mW_cm2;   /* current aggregate                  */
    float              ntc_temp_c;          /* warmest zone NTC                   */
    np_pbm1064_fault_t fault_reason;
    uint8_t            ch_enable_mask;      /* current CH_ENABLE state            */
} np_pbm1064_display_state_t;

/* ── T2 combined session status ──────────────────────────────────────────────── */

typedef enum {
    NP_T2_STAGE_IDLE        = 0,
    NP_T2_STAGE_PREFLIGHT   = 1,
    NP_T2_STAGE_RAMP_UP     = 2,
    NP_T2_STAGE_ACTIVE      = 3,
    NP_T2_STAGE_RAMP_DOWN   = 4,
    NP_T2_STAGE_COMPLETE    = 5,
    NP_T2_STAGE_FAULT       = 6,
} np_t2_stage_t;

/* ── Callbacks ───────────────────────────────────────────────────────────────── */

typedef void (*np_pbm1064_session_end_cb_t)(
    const np_pbm1064_session_record_t *record,
    np_pbm1064_status_t                result);

typedef void (*np_pbm1064_fault_cb_t)(
    uint8_t slot, np_pbm1064_fault_t reason);

typedef void (*np_pbm1064_display_cb_t)(
    const np_pbm1064_display_state_t *state);

typedef void (*np_pbm1064_remove_cb_t)(uint8_t slot);

/* ── FAI test result ─────────────────────────────────────────────────────────── */

typedef struct {
    const char *test_id;
    const char *description;
    bool        passed;
    const char *details;
} np_pbm1064_fai_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * T2 combined session types — NP-SES-1064-001 §6
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* ── T2 1170nm laser preset ──────────────────────────────────────────────────── */

typedef struct {
    uint8_t  duty_pct;     /* laser drive power 0–100%; TEC-limited in firmware    */
    uint8_t  freq_hz;      /* 0 = CW; else pulsed at freq_hz (session presets use CW) */
    uint16_t duration_s;   /* active duration (may differ from 1064nm duration)    */
} np_t2_1170_preset_t;

/* ── T2 combined session descriptor ─────────────────────────────────────────── */

/*
 * Signed by the app Ed25519 key.  Hub firmware verifies the signature before
 * any laser enable.  Contains the 1064nm sub-descriptor plus 1170nm laser params
 * and optional sLORETA coordination flags.
 */
typedef struct {
    uint8_t  version;              /* NP_T2_COMBINED_VERSION (0x01)                */
    uint8_t  t2_combined_enable;   /* 1 = activate 1170nm laser                    */
    uint8_t  sloreta_enable;       /* 1 = read sLORETA MNI target at session start  */
    uint8_t  reserved;
    np_pbm1064_session_desc_t pbm1064; /* 1064nm smart zone sub-descriptor          */
    np_t2_1170_preset_t       laser1170;
    uint8_t  signature[64];        /* Ed25519 over all preceding fields             */
} np_t2_combined_desc_t;

/* ── T2 combined UHDR session record ─────────────────────────────────────────── */

typedef struct {
    np_pbm1064_session_record_t pbm1064_record; /* 1064nm per-zone, per-wl dose     */

    /* 1170nm deep laser metrics */
    float    dose_1170_J_cm2;           /* total subcortical dose this session       */
    float    irradiance_1170_peak_mW_cm2; /* peak irradiance observed               */
    uint32_t throttle_events_1170;      /* TEC thermal throttle events (count)       */
    uint32_t throttle_events_ch_c;      /* 1064nm CH_C throttle events from T2 load */

    /* sLORETA coordination target (OI-SES-T2-01; stub pending T2 prototype) */
    int16_t  sloreta_mni_x;            /* MNI X coordinate (mm)                     */
    int16_t  sloreta_mni_y;            /* MNI Y coordinate (mm)                     */
    int16_t  sloreta_mni_z;            /* MNI Z coordinate (mm)                     */
    uint8_t  sloreta_valid;            /* 1 = coordinates were read from sLORETA     */
    uint8_t  abort_reason;             /* 0 = normal; else np_pbm1064_fault_t       */
    uint32_t duration_s;               /* actual session duration                   */
} np_t2_combined_uhdr_record_t;

/* ── T2 combined SHDR session summary (no user biology) ─────────────────────── */

typedef struct {
    np_pbm1064_shdr_summary_t pbm1064_shdr; /* 1064nm device health metrics        */

    /* 1170nm device health metrics (device condition — no user biology) */
    uint8_t  tec_throttle_events;       /* count of TEC temp threshold crossings    */
    uint8_t  laser_fault_flag;          /* 1 = 1170nm module reported a fault       */
    uint8_t  sloreta_session_flag;      /* 1 = sLORETA target was read at start      */
    uint8_t  abort_reason;              /* np_pbm1064_fault_t; 0 = normal           */
    uint32_t duration_s;
} np_t2_combined_shdr_summary_t;

/* ── T2 combined stage (mirrors np_t2_stage_t; aliased for clarity) ──────────── */
/* np_t2_stage_t defined above is reused for the combined session. */

#endif /* NP_PBM1064_TYPES_H */
