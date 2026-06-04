/*
 * NeuroPulse Hub Control Program — Closed-Loop Adaptation Event Log
 * Document: NP-FW-HUB-001 Rev B (STEP-33 of NP-PRIV-REM-001)
 *
 * Declares np_adapt_trigger_t and np_adaptation_event_t for logging every
 * closed-loop parameter adjustment to the UHDR partition.
 *
 * Data-classification rule (NP-FW-EMMC-001 §12):
 *   - np_adaptation_event_t → UHDR only.  No SHDR routing.
 *   - Values are scaled integers (x100) or typed enums — no raw EEG/HRV floats.
 *
 * Enum stability: values are frozen once written to UHDR.  New triggers are
 * additive only; existing values must never be renumbered (UHDR forward-compat).
 */

#ifndef NP_ADAPTATION_LOG_H
#define NP_ADAPTATION_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include "np_hub_types.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adaptive trigger classification
 *
 * Each value identifies the physiological signal or system event that caused
 * the session runner to modify a stimulation parameter.
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    /* EEG band-power triggers — classified by band, not by raw µV value */
    NP_ADAPT_TRIGGER_EEG_ALPHA_LOW      = 0x00, /* frontal alpha power fell below threshold */
    NP_ADAPT_TRIGGER_EEG_ALPHA_HIGH     = 0x01, /* frontal alpha power exceeded upper bound  */
    NP_ADAPT_TRIGGER_EEG_THETA_LOW      = 0x02, /* frontal theta power fell below threshold  */
    NP_ADAPT_TRIGGER_EEG_THETA_HIGH     = 0x03, /* frontal theta power exceeded upper bound  */
    NP_ADAPT_TRIGGER_EEG_GAMMA_LOW      = 0x04, /* frontal gamma (40Hz) power fell below threshold */
    NP_ADAPT_TRIGGER_EEG_GAMMA_HIGH     = 0x05, /* frontal gamma power exceeded upper bound  */
    NP_ADAPT_TRIGGER_EEG_BETA_LOW       = 0x06, /* frontal beta power fell below threshold   */
    NP_ADAPT_TRIGGER_EEG_BETA_HIGH      = 0x07, /* frontal beta power exceeded upper bound   */
    /* HRV triggers */
    NP_ADAPT_TRIGGER_HRV_COHERENCE_LOW  = 0x08, /* HRV coherence score dropped below 4.0     */
    NP_ADAPT_TRIGGER_HRV_COHERENCE_HIGH = 0x09, /* HRV coherence score exceeded 8.0          */
    NP_ADAPT_TRIGGER_HRV_RMSSD_LOW      = 0x0A, /* HRV RMSSD fell below personalised baseline */
    /* Safety / device-condition triggers (SHDR-safe classification) */
    NP_ADAPT_TRIGGER_IMPEDANCE_CHANGE   = 0x0B, /* electrode impedance crossed ±20% of session start */
    NP_ADAPT_TRIGGER_DOSE_LIMIT_APPROACH= 0x0C, /* cumulative J/cm² reached 80% of session dose cap */
    NP_ADAPT_TRIGGER_THERMAL_THROTTLE   = 0x0D, /* zone NTC > thermal throttle threshold     */
    /* Session-lifecycle triggers */
    NP_ADAPT_TRIGGER_PHASE_TRANSITION   = 0x0E, /* session moved from ramp → steady → ramp-down */
    NP_ADAPT_TRIGGER_USER_PAUSE         = 0x0F, /* user paused; parameters reduced to idle   */
    NP_ADAPT_TRIGGER_SESSION_END_RAMP   = 0x10, /* graceful end-of-session ramp-down started */

    NP_ADAPT_TRIGGER_COUNT              = 0x11, /* sentinel — total trigger count; never written to UHDR */
} np_adapt_trigger_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adaptation event record
 *
 * One record per parameter change event.  Logged to UHDR by np_log_adapt_event().
 * No raw EEG waveform, band-power float, or HRV float is stored here.
 * Values are scaled × 100 to preserve two decimal places as integers.
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t            session_ms;       /* session-relative timestamp of the event (ms)      */
    np_adapt_trigger_t  trigger;          /* classified trigger that caused this adaptation     */
    np_hub_mod_type_t   mod_type;         /* which modality's parameter was changed             */
    uint8_t             param_id;         /* modality-specific parameter identifier (see below) */
    int32_t             value_before_x100;/* parameter value before change, scaled × 100       */
    int32_t             value_after_x100; /* parameter value after change, scaled × 100        */
    uint8_t             confidence_pct;   /* algorithm confidence 0–100%; 0xFF = not applicable */
} np_adaptation_event_t;

/*
 * param_id values per mod_type (for display in the app trigger mapping table):
 *
 *   NP_MOD_AUDIO        0x00 = beat_mhz (audio beat / binaural frequency mHz)
 *   NP_MOD_BES_TACS     0x00 = freq_mhz (BES/tACS stimulus frequency mHz)
 *                        0x01 = amplitude_ua (BES/tACS amplitude µA)
 *   NP_MOD_VNS_HRV      0x00 = hrv_proto (HRV protocol variant: 0=VNS, 1=+HRV, 2=RSA, 3=dual)
 *                        0x01 = freq_mhz (VNS stimulus frequency)
 *   NP_MOD_PBM_BASE /
 *   NP_MOD_PBM_SMART    0x00 = freq_code (PBM pulse frequency preset)
 *                        0x01 = duty (PBM duty cycle)
 *   NP_MOD_VISUAL       0x00 = freq_hz (photic drive frequency)
 *   NP_MOD_CLIN_TACS    0x00 = amplitude_ua (clinical tACS amplitude)
 *                        0x01 = freq_mhz (clinical tACS frequency)
 */

/* ═══════════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/*
 * np_adapt_log_event — queue one adaptation event for UHDR write.
 * Thread-safe: call from the session runner task only.
 * Returns false if the per-session ring buffer is full (SHDR fault count increments).
 */
bool np_adapt_log_event(const np_adaptation_event_t *event);

/*
 * np_adapt_log_flush — flush all queued adaptation events to UHDR via
 * np_log_adapt_event().  Called by np_session_log at session end and
 * periodically by the session runner.
 */
void np_adapt_log_flush(void);

/*
 * np_adapt_log_reset — clear the ring buffer at session start.
 * Called by np_runner_run() before first command dispatch.
 */
void np_adapt_log_reset(void);

#endif /* NP_ADAPTATION_LOG_H */
