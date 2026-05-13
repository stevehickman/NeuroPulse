/*
 * NeuroPulse 1064nm Smart Zone Module — Session Orchestration API
 * Document: NP-FW-PBM1064-001 Rev A §7
 */

#ifndef NP_PBM1064_SESSION_H
#define NP_PBM1064_SESSION_H

#include <stdint.h>
#include <stdbool.h>
#include "np_pbm1064_types.h"
#include "np_pbm1064_drive.h"
#include "np_pbm1064_dose.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Session context (single instance; static allocation). */
typedef struct {
    np_pbm1064_stage_t        stage;
    np_pbm1064_session_desc_t desc;
    np_pbm1064_drv_slot_t     drv[NP_PBM1064_ZONE_COUNT];
    np_pbm1064_dose_state_t   dose[NP_PBM1064_ZONE_COUNT];
    np_pbm1064_cal_t          cal[NP_PBM1064_ZONE_COUNT][NP_PBM1064_WL_COUNT];
    np_pbm1064_session_record_t record;

    /* Ramp state */
    uint32_t ramp_start_ms;
    uint32_t ramp_tick_count;

    /* Dose tick tracking */
    uint32_t last_dose_tick_ms;
    uint32_t last_i2c_poll_ms;
    uint32_t last_ntc_poll_ms;

    /* EEG-adaptive state */
    float    eeg_dominant_freq_hz;   /* updated by EEG layer at 1 Hz              */
    uint8_t  eeg_tick_in_band[NP_PBM1064_ZONE_COUNT]; /* hysteresis counter       */
    uint8_t  current_freq_code;      /* uniform-mode shared freq code             */

    /* NTC throttle state */
    float    ntc_temp_c[NP_PBM1064_ZONE_COUNT];
    bool     ch_c_throttled;
    bool     ch_b_throttled;

    /* Callbacks */
    np_pbm1064_session_end_cb_t session_end_cb;
    np_pbm1064_fault_cb_t       fault_cb;
    np_pbm1064_display_cb_t     display_cb;

    uint32_t device_session_count;
} np_pbm1064_session_ctx_t;

/*
 * Initialise session context.  Must be called once before start.
 */
void np_pbm1064_session_init(np_pbm1064_session_ctx_t    *ctx,
                               np_pbm1064_session_end_cb_t  end_cb,
                               np_pbm1064_fault_cb_t        fault_cb,
                               np_pbm1064_display_cb_t      display_cb,
                               uint32_t                     device_session_count);

/*
 * Begin a session.  Verifies Ed25519 signature, runs preflight, starts ramp.
 * desc must remain valid for the duration of the session.
 * Returns NP_PBM1064_ERR_SESSION_ACTIVE if already running.
 */
np_pbm1064_status_t np_pbm1064_session_start(
    np_pbm1064_session_ctx_t          *ctx,
    const np_pbm1064_session_desc_t   *desc);

/*
 * Session tick — call at NP_PBM1064_DOSE_TICK_MS (100 ms) intervals.
 * Drives ramp, dose metering, I2C status poll, NTC poll, EEG-adaptive.
 * Returns NP_PBM1064_OK during normal operation;
 *         NP_PBM1064_ERR_NO_SESSION if no session is active.
 */
np_pbm1064_status_t np_pbm1064_session_tick(np_pbm1064_session_ctx_t *ctx,
                                               uint32_t now_ms);

/*
 * Deliver an updated EEG dominant frequency to the running session.
 * Called by the EEG processing task at 1 Hz (uniform adaptive mode).
 * No-op if no session active or eeg_adaptive_mode == 0.
 */
void np_pbm1064_session_update_eeg_freq(np_pbm1064_session_ctx_t *ctx,
                                          float dominant_freq_hz);

/*
 * Abort a running session immediately.  Disables all channels.
 */
np_pbm1064_status_t np_pbm1064_session_abort(np_pbm1064_session_ctx_t *ctx,
                                               np_pbm1064_fault_t reason);

/*
 * Query current session stage.
 */
np_pbm1064_stage_t np_pbm1064_session_stage(
    const np_pbm1064_session_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* NP_PBM1064_SESSION_H */
