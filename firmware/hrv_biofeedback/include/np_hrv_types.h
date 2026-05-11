/*
 * NeuroPulse HRV Biofeedback — Type Definitions
 * Document: NP-FW-HRV-001 Rev A §4
 */

#ifndef NP_HRV_TYPES_H
#define NP_HRV_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_hrv_config.h"

/* ── Protocol identifiers ───────────────────────────────────────────────────── */
typedef enum {
    NP_HRV_PROTO_STANDALONE    = 0, /* breathing pacer + coherence display only  */
    NP_HRV_PROTO_TAVNS_SYNC    = 1, /* HRV + taVNS gated to inspiration phase    */
    NP_HRV_PROTO_EEG_DUAL      = 2, /* HRV coherence + EEG band power display    */
    NP_HRV_PROTO_PBM           = 3, /* HRV coherence training with PBM running   */
} np_hrv_protocol_t;

/* ── HRV module status codes ────────────────────────────────────────────────── */
typedef enum {
    NP_HRV_OK                  =  0,
    NP_HRV_ERR_INVALID_ARG     = -1,
    NP_HRV_ERR_NOT_READY       = -2, /* insufficient RR data for computation     */
    NP_HRV_ERR_BUFFER_FULL     = -3,
    NP_HRV_ERR_TAVNS_REJECTED  = -4, /* safety MCU rejected stim enable          */
    NP_HRV_ERR_SESSION_ACTIVE  = -5,
    NP_HRV_ERR_NO_SESSION      = -6,
} np_hrv_status_t;

/* ── R-R interval circular buffer ───────────────────────────────────────────── */
typedef struct {
    uint16_t intervals_ms[NP_RR_BUFFER_SIZE]; /* R-R intervals in milliseconds   */
    uint32_t timestamps_ms[NP_RR_BUFFER_SIZE]; /* system time of each R peak     */
    uint16_t head;           /* next write index                                  */
    uint16_t count;          /* valid entries (saturates at NP_RR_BUFFER_SIZE)   */
} np_rr_buffer_t;

/* ── PPG peak detector state ────────────────────────────────────────────────── */
typedef struct {
    int32_t  adaptive_threshold; /* IIR-smoothed peak threshold (ADC counts)     */
    uint32_t last_peak_time_ms;  /* system timestamp of last accepted peak        */
    uint32_t refractory_count;   /* samples remaining in refractory period        */
    bool     above_threshold;    /* true while signal is above threshold          */
    int32_t  peak_candidate;     /* running max during above-threshold excursion  */
    uint32_t peak_candidate_time_ms;
} np_ppg_detector_t;

/* ── HRV power spectral density ────────────────────────────────────────────── */
typedef struct {
    float    psd[NP_HRV_FFT_SIZE / 2U]; /* one-sided PSD (ms²/Hz), bins 0–127   */
    uint16_t num_averages;               /* Welch averages accumulated             */
    bool     valid;                      /* true when ≥ 1 average is ready        */
} np_hrv_psd_t;

/* ── HRV coherence result ───────────────────────────────────────────────────── */
typedef struct {
    float    lf_peak_power;     /* peak power within LF band (ms²/Hz)            */
    float    lf_total_power;    /* integrated LF power (ms²)                     */
    float    hf_total_power;    /* integrated HF power (ms²)                     */
    float    lf_hf_ratio;       /* lf_total / hf_total                           */
    float    coherence_score;   /* lf_peak / (lf_total + hf_total) × 10          */
    float    rmssd_ms;          /* root-mean-square of successive differences     */
    bool     valid;
} np_hrv_coherence_result_t;

/* ── Breathing pacer state ──────────────────────────────────────────────────── */
typedef enum {
    NP_PACER_PHASE_INSPIRE  = 0,
    NP_PACER_PHASE_EXPIRE   = 1,
} np_pacer_phase_t;

typedef struct {
    float            target_rate_bpm; /* current target breathing rate           */
    float            sweep_rate_bpm;  /* rate being evaluated during RF sweep    */
    np_pacer_phase_t phase;           /* current breath phase                    */
    uint32_t         phase_start_ms;  /* system timestamp of phase start         */
    uint32_t         inspire_duration_ms;
    uint32_t         expire_duration_ms;
    bool             sweep_active;    /* true during resonance frequency sweep   */
    uint8_t          sweep_step_idx;  /* index through sweep rates               */
    float            sweep_coherence[8]; /* coherence captured at each sweep step*/
    bool             personalised;    /* true once user RF has been found        */
} np_pacer_state_t;

/* ── taVNS synchronisation state ────────────────────────────────────────────── */
typedef enum {
    NP_TAVNS_IDLE       = 0,  /* waiting for inspiration onset                   */
    NP_TAVNS_STIMULATING = 1, /* stim gated ON (inspiration phase active)        */
    NP_TAVNS_COOLDOWN   = 2,  /* inter-stimulus interval after expiration onset  */
} np_tavns_phase_t;

typedef struct {
    np_tavns_phase_t phase;
    uint32_t         stim_start_ms;    /* time stim gate opened                  */
    uint32_t         stim_duration_ms; /* actual duration of last stim window    */
    int16_t          rr_slope_buf[NP_TAVNS_INSP_SLOPE_WIN]; /* dRR/dt values     */
    uint8_t          slope_buf_idx;
    int32_t          slope_sum;        /* running sum for fast average            */
    bool             safety_mcu_granted; /* safety MCU has confirmed stim enable  */
    uint32_t         stim_count;       /* total inspiration-triggered events      */
} np_tavns_state_t;

/* ── EEG band power result (one Welch estimate per channel set) ─────────────── */
typedef struct {
    float delta[NP_EEG_CHANNELS]; /* µV²/Hz per channel                         */
    float theta[NP_EEG_CHANNELS];
    float alpha[NP_EEG_CHANNELS];
    float beta[NP_EEG_CHANNELS];
    float gamma[NP_EEG_CHANNELS];
    float alpha_theta_ratio;       /* mean across frontal channels (F3/F4)       */
    bool  valid;
} np_eeg_bands_t;

/* ── Combined dual biofeedback display state ────────────────────────────────── */
typedef struct {
    np_hrv_coherence_result_t hrv;
    np_eeg_bands_t            eeg;
    float                     adaptive_pacer_rate_bpm; /* closed-loop output     */
    uint32_t                  last_update_ms;
} np_dual_biofeedback_state_t;

/* ── Per-session UHDR record ────────────────────────────────────────────────── */
/* Written to UHDR partition at session end in EDF+ annotation format.           */
typedef struct {
    uint32_t session_start_unix;   /* UTC epoch seconds                          */
    uint32_t duration_s;
    uint8_t  protocol;             /* np_hrv_protocol_t value                    */
    float    target_rate_bpm;
    float    mean_coherence;
    float    min_coherence;
    float    max_coherence;
    float    mean_rmssd_ms;
    uint16_t rr_sample_count;      /* total R-R intervals recorded               */
    uint16_t tavns_stim_count;     /* taVNS inspiration events (protocol 1 only) */
    uint8_t  reserved[8];
} np_hrv_session_record_t;

/* ── SHDR coherence trend record (no user biology) ──────────────────────────── */
typedef struct {
    float    session_mean_coherence[NP_HRV_TREND_SESSIONS]; /* ring buffer        */
    uint8_t  head;             /* next write index                               */
    uint8_t  count;            /* valid entries                                  */
    float    slope;            /* linear regression slope over stored sessions   */
} np_hrv_coherence_trend_t;

/* ── Session configuration (from app → firmware via signed protocol) ─────────── */
typedef struct {
    uint8_t          protocol;        /* np_hrv_protocol_t                        */
    uint16_t         duration_s;      /* 300–1200                                 */
    float            pacer_rate_bpm;  /* 0.0 = use personalised/default           */
    uint16_t         tavns_freq_hz;   /* 1–25; ignored for non-taVNS protocols   */
    uint16_t         tavns_current_ua;/* 100–2000; clamped by safety MCU         */
    bool             use_bone_conduction_cue;
    bool             pbm_enabled;     /* protocol 3: PBM on during session       */
} np_hrv_session_config_t;

#endif /* NP_HRV_TYPES_H */
