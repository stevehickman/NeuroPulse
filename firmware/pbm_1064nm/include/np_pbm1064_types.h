#ifndef NP_PBM1064_TYPES_H
#define NP_PBM1064_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm1064_config.h"

/* Wavelength channel indices */
typedef enum {
    NP_PBM_CH_A = 0,   /* 660 nm */
    NP_PBM_CH_B = 1,   /* 808 nm */
    NP_PBM_CH_C = 2,   /* 1064 nm */
} np_pbm_channel_t;

/* Module slot detection result */
typedef enum {
    NP_SLOT_EMPTY       = 0,
    NP_SLOT_BASE_MODULE = 1,    /* ZM-01..ZM-05: direct-drive base module (unchanged) */
    NP_SLOT_SMART_MODULE = 2,   /* 3.3 kΩ ZONE_ID: I2C smart module */
    NP_SLOT_FAULT       = 3,    /* ZONE_ID debounce no-majority or I2C NACK */
} np_slot_type_t;

/* Calibration coefficients per zone per wavelength (from Config partition) */
typedef struct {
    float K_PD1[NP_PBM1064_ZONE_COUNT][NP_PBM1064_WAVELENGTH_COUNT];   /* mW/cm² per ADC count */
    float K_PD2[NP_PBM1064_ZONE_COUNT][NP_PBM1064_WAVELENGTH_COUNT];
    float K_ratio_nom[NP_PBM1064_ZONE_COUNT][NP_PBM1064_WAVELENGTH_COUNT]; /* nominal PD1/PD2 */
    bool  cal_from_factory[NP_PBM1064_ZONE_COUNT];  /* true=factory cal, false=default */
} np_pbm_cal_t;

/* Per-zone per-wavelength preset parameters */
typedef struct {
    uint8_t  freq_hz_A;     /* 660 nm frequency (0=CW, 1..100 Hz) */
    uint8_t  duty_pct_A;    /* 660 nm duty cycle (0–25%, ceiling enforced) */
    uint8_t  freq_hz_B;     /* 808 nm */
    uint8_t  duty_pct_B;
    uint8_t  freq_hz_C;     /* 1064 nm */
    uint8_t  duty_pct_C;
    uint8_t  cur_pct;       /* common current % of rated (80–100) */
    bool     cortical_gradient_adapt; /* EEG-adaptive gradient mode (§3.4 NP-SES-1064-001) */
    char     name[32];
} np_pbm1064_preset_t;

/* Per-zone runtime state */
typedef struct {
    np_slot_type_t slot_type;
    bool           driver_ready;
    uint8_t        driver_status;
    int8_t         ntc_c;
    float          dose_j_cm2[NP_PBM1064_WAVELENGTH_COUNT];
    float          irr_pd1[NP_PBM1064_WAVELENGTH_COUNT];       /* latest irradiance reading */
    float          irr_pd2[NP_PBM1064_WAVELENGTH_COUNT];
    float          pd1_pd2_ratio[NP_PBM1064_WAVELENGTH_COUNT];
    float          baseline_pd1[NP_PBM1064_WAVELENGTH_COUNT];  /* baseline at session start */
    bool           fouling_flag;
    bool           aging_flag;
    bool           dose_warn[NP_PBM1064_WAVELENGTH_COUNT];     /* near dose limit */
    bool           dose_limit_hit[NP_PBM1064_WAVELENGTH_COUNT];
    uint8_t        active_ch_mask;     /* which channels are currently enabled */
    uint8_t        driver_fault_count;
} np_pbm1064_zone_state_t;

/* Session-level state */
typedef struct {
    uint8_t                 smart_module_mask;  /* bitmask of slots with smart modules */
    np_pbm1064_preset_t     preset[NP_PBM1064_ZONE_COUNT];
    np_pbm_cal_t            cal;
    np_pbm1064_zone_state_t zone[NP_PBM1064_ZONE_COUNT];
    bool                    session_active;
    uint32_t                session_tick_ms;
    bool                    eeg_adaptive;
    float                   eeg_target_freq_hz; /* from closed-loop EEG (updated externally) */
} np_pbm1064_session_t;

/* T2 combined session state (§8, NP-FW-PBM1064-001) */
typedef struct {
    np_pbm1064_session_t   zone_session;
    void                  *t2_laser_session;   /* opaque handle to np_t2_pbm1170_session_t */
    float                  dose_1170_j_cm2;
    bool                   laser_throttled;
    bool                   sloreta_coordinated;
} np_pbm1064_t2_session_t;

/* SHDR record (device health only, no user biology) */
typedef struct {
    uint32_t session_count_1064;
    float    pd1_pd2_ratio_final[NP_PBM1064_ZONE_COUNT][NP_PBM1064_WAVELENGTH_COUNT];
    bool     fouling_flag[NP_PBM1064_ZONE_COUNT];
    bool     aging_flag[NP_PBM1064_ZONE_COUNT];
    uint8_t  driver_fault_count[NP_PBM1064_ZONE_COUNT];
    int8_t   ntc_peak[NP_PBM1064_ZONE_COUNT];    /* peak NTC, no timestamp */
    bool     cal_from_factory[NP_PBM1064_ZONE_COUNT];
    uint32_t combined_session_count;              /* T2 only */
} np_pbm1064_shdr_t;

#endif /* NP_PBM1064_TYPES_H */
