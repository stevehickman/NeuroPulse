/*
 * NeuroPulse Cervical VNS — Type Definitions
 * Document: NP-FW-CVNS-001 Rev A §4
 */

#ifndef NP_CVNS_TYPES_H
#define NP_CVNS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "np_cvns_config.h"

/* ── Status codes ───────────────────────────────────────────────────────────── */
typedef enum {
    NP_CVNS_OK                    =  0,
    NP_CVNS_ERR_INVALID_ARG       = -1,
    NP_CVNS_ERR_IMPEDANCE_HIGH    = -2,  /* electrode impedance exceeds limit     */
    NP_CVNS_ERR_SAFETY_REJECTED   = -3,  /* safety MCU denied enable              */
    NP_CVNS_ERR_CARDIAC_CUTOFF    = -4,  /* cardiac interlock triggered           */
    NP_CVNS_ERR_BASELINE_INVALID  = -5,  /* baseline HR not yet established       */
    NP_CVNS_ERR_SESSION_ACTIVE    = -6,
    NP_CVNS_ERR_NO_SESSION        = -7,
    NP_CVNS_ERR_LOCKOUT           = -8,  /* re-enable lockout active              */
    NP_CVNS_ERR_CHARGE_LIMIT      = -9,
    NP_CVNS_ERR_CROSSVAL_FAILED   = -10, /* main vs safety MCU baseline disagree  */
} np_cvns_status_t;

/* ── Cardiac interlock state ─────────────────────────────────────────────────── */
typedef enum {
    NP_CVNS_INTERLOCK_IDLE        = 0,
    NP_CVNS_INTERLOCK_PRE_SESSION = 1,  /* impedance + baseline checks           */
    NP_CVNS_INTERLOCK_ENABLED     = 2,  /* stimulation active; cardiac monitoring */
    NP_CVNS_INTERLOCK_FAULT       = 3,  /* cutoff; lockout active                */
} np_cvns_interlock_state_t;

/* ── Session workflow stage ──────────────────────────────────────────────────── */
typedef enum {
    NP_CVNS_STAGE_IDLE         = 0,
    NP_CVNS_STAGE_IMPEDANCE    = 1,
    NP_CVNS_STAGE_BASELINE     = 2,
    NP_CVNS_STAGE_RAMP_UP      = 3,
    NP_CVNS_STAGE_ACTIVE       = 4,
    NP_CVNS_STAGE_RAMP_DOWN    = 5,
    NP_CVNS_STAGE_COMPLETE     = 6,
    NP_CVNS_STAGE_FAULT        = 7,
} np_cvns_stage_t;

/* ── Stimulation phase ───────────────────────────────────────────────────────── */
typedef enum {
    NP_CVNS_STIM_IDLE      = 0,
    NP_CVNS_STIM_RAMP_UP   = 1,
    NP_CVNS_STIM_ACTIVE    = 2,
    NP_CVNS_STIM_RAMP_DOWN = 3,
    NP_CVNS_STIM_DONE      = 4,
    NP_CVNS_STIM_FAULT     = 5,
} np_cvns_stim_phase_t;

/* ── Fault reason codes (SHDR safe — no user biology) ───────────────────────── */
typedef enum {
    NP_CVNS_FAULT_NONE       = 0,
    NP_CVNS_FAULT_HR_CHANGE  = 1,  /* HR changed > NP_CVNS_HR_CHANGE_LIMIT_BPM */
    NP_CVNS_FAULT_DATA_LOSS  = 2,  /* R-peak signal lost > NP_CVNS_DATA_LOSS_TIMEOUT_S */
    NP_CVNS_FAULT_WATCHDOG   = 3,  /* main processor watchdog expired           */
    NP_CVNS_FAULT_IMPEDANCE  = 4,  /* electrode impedance rose above limit      */
    NP_CVNS_FAULT_SAFETY_MCU = 5,  /* safety MCU rejected or lost               */
} np_cvns_fault_reason_t;

/* ── R-R interval rolling buffer ─────────────────────────────────────────────── */
typedef struct {
    uint16_t rr_ms[NP_CVNS_RR_WINDOW_SIZE]; /* circular buffer; ms              */
    uint8_t  head;                           /* write index                      */
    uint8_t  count;                          /* valid entries (0–NP_CVNS_RR_WINDOW_SIZE) */
    uint32_t last_peak_ms;                   /* timestamp of most recent R-peak  */
} np_cvns_rr_buf_t;

/* ── Session configuration ───────────────────────────────────────────────────── */
typedef struct {
    uint16_t freq_hz;           /* 1–25 Hz                                       */
    uint16_t current_ua;        /* 0–2000 µA                                     */
    uint16_t pulse_width_us;    /* 200–1000 µs                                   */
    uint16_t duration_s;        /* 60–120 s                                      */
    uint8_t  electrode_config;  /* 0=bilateral, 1=unilateral_L, 2=unilateral_R   */
} np_cvns_session_config_t;

/* ── UHDR session record ─────────────────────────────────────────────────────── */
/* Written to UHDR partition; AES-256-XTS key held by user only.                */
typedef struct {
    uint32_t session_start_unix;        /* UTC epoch seconds                    */
    uint32_t session_duration_s;        /* actual stimulation duration           */
    uint16_t stim_freq_hz;
    uint16_t stim_current_ua;
    uint16_t stim_pulse_width_us;
    uint8_t  electrode_config;
    uint8_t  cutoff_occurred;           /* 1 if cardiac interlock fired          */
    uint16_t cutoff_hr_baseline_x10;   /* baseline HR × 10 (BPM, 1 dp)         */
    uint16_t cutoff_hr_at_event_x10;   /* HR at cutoff × 10; 0 if no cutoff     */
    uint32_t cutoff_time_offset_s;      /* s after stim onset; 0 if no cutoff    */
    float    impedance_left_kohm;
    float    impedance_right_kohm;
    uint8_t  abort_reason;              /* 0=normal, else np_cvns_status_t       */
    uint8_t  reserved[3];
} np_cvns_session_record_t;

/* ── SHDR session summary ─────────────────────────────────────────────────────── */
/* Written to SHDR partition; no user biology.                                  */
typedef struct {
    uint8_t  electrode_config;
    uint16_t stim_freq_hz;
    uint16_t stim_current_ua;
    uint32_t stim_duration_s;
    float    impedance_left_kohm;
    float    impedance_right_kohm;
    uint8_t  impedance_check_pass;  /* 1 if both electrodes passed              */
    uint8_t  cutoff_occurred;       /* flag only — no HR values                  */
    uint8_t  fault_reason;          /* np_cvns_fault_reason_t                   */
} np_cvns_shdr_summary_t;

/* ── Safety MCU fault log entry (SHDR) ───────────────────────────────────────── */
typedef struct {
    uint32_t session_id;          /* session counter; no timestamps             */
    uint32_t cutoff_offset_ms;    /* ms after stim enable when cutoff occurred  */
    uint8_t  reason;              /* np_cvns_fault_reason_t                     */
    uint8_t  reserved[3];
} np_cvns_fault_log_entry_t;

/* ── Display state (sent to app) ─────────────────────────────────────────────── */
typedef struct {
    np_cvns_stage_t          stage;
    np_cvns_stim_phase_t     stim_phase;
    np_cvns_interlock_state_t interlock;
    float                    stim_progress_pct;    /* 0–100                     */
    uint16_t                 current_ua;           /* live current (during ramp)*/
    float                    baseline_hr_bpm;      /* 0 if not yet valid        */
    float                    impedance_left_kohm;
    float                    impedance_right_kohm;
    np_cvns_fault_reason_t   fault_reason;
    uint32_t                 reenable_lockout_remaining_s;
} np_cvns_display_state_t;

/* ── Forward declarations for opaque context handles ─────────────────────────── */
typedef struct np_cvns_interlock_ctx np_cvns_interlock_ctx_t;
typedef struct np_cvns_stim_ctx      np_cvns_stim_ctx_t;
typedef struct np_cvns_session_ctx   np_cvns_session_ctx_t;

/* ── Callbacks ───────────────────────────────────────────────────────────────── */
typedef void (*np_cvns_fault_cb_t)(np_cvns_interlock_ctx_t *ctx,
                                    np_cvns_fault_reason_t   reason);
typedef void (*np_cvns_safety_response_cb_t)(np_cvns_stim_ctx_t *ctx,
                                              bool                granted,
                                              const float         impedance_kohm[]);
typedef void (*np_cvns_session_end_cb_t)(const np_cvns_session_record_t *record,
                                          np_cvns_status_t                result);
typedef void (*np_cvns_display_cb_t)(const np_cvns_display_state_t *state);

#endif /* NP_CVNS_TYPES_H */
