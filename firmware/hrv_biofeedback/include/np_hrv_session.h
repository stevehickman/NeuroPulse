/*
 * NeurOne HRV Biofeedback — Session Management
 * Document: NP-FW-HRV-001 Rev 1 §10
 *
 * Owns the session lifecycle for all four HRV biofeedback protocols.
 * Orchestrates PPG processing, coherence computation, pacer, taVNS sync,
 * and EEG dual biofeedback modules from a single FreeRTOS task.
 *
 * UHDR / SHDR data routing:
 *   UHDR (written at session end, UHDR partition):
 *     - Full R-R interval time series (EDF+ annotation signal)
 *     - Per-update coherence score array
 *     - Session record (np_hrv_session_record_t)
 *   SHDR (written at session end, SHDR partition):
 *     - Coherence trend slope (linear regression over last 30 sessions)
 *     - Session count increment
 *
 * The session module does NOT hold the UHDR encryption key.  It writes
 * plaintext data to an in-RAM staging buffer; the storage layer (outside
 * this module) handles AES-256-XTS encryption before eMMC write.
 */

#ifndef NP_HRV_SESSION_H
#define NP_HRV_SESSION_H

#include "np_hrv_types.h"
#include "np_hrv_ppg.h"
#include "np_hrv_coherence.h"
#include "np_hrv_pacer.h"
#include "np_hrv_tavns_sync.h"
#include "np_hrv_eeg_biofeedback.h"

/*
 * Opaque session context.
 * Callers must not access fields directly.
 */
typedef struct np_hrv_session np_hrv_session_t;

/*
 * Callback invoked every NP_COHERENCE_UPDATE_INTERVAL_S seconds with the
 * latest display state (HRV coherence + optional EEG bands + pacer phase).
 * Used by the app layer to refresh the UI.
 */
typedef void (*np_hrv_display_cb_t)(const np_dual_biofeedback_state_t *state,
                                     np_pacer_phase_t                   pacer_phase);

/*
 * Callback invoked when a session ends (timeout or np_hrv_session_stop()).
 * `record` contains the finalized UHDR session record for storage commit.
 */
typedef void (*np_hrv_session_end_cb_t)(const np_hrv_session_record_t *record,
                                         np_hrv_status_t                reason);

/*
 * Allocate and initialize a session context.
 * Returns NULL if the static context pool is exhausted (only 1 session allowed
 * at a time; returns the existing context if already allocated).
 *
 * `config`      — session parameters from the signed app protocol.
 * `display_cb`  — called every NP_COHERENCE_UPDATE_INTERVAL_S (may be NULL).
 * `end_cb`      — called when the session ends (must not be NULL).
 * `now_ms`      — current system time.
 */
np_hrv_session_t *np_hrv_session_create(const np_hrv_session_config_t *config,
                                          np_hrv_display_cb_t            display_cb,
                                          np_hrv_session_end_cb_t        end_cb,
                                          uint32_t                        now_ms);

/*
 * Start a previously created session.
 * Returns NP_HRV_ERR_SESSION_ACTIVE if already running.
 */
np_hrv_status_t np_hrv_session_start(np_hrv_session_t *sess, uint32_t now_ms);

/*
 * Feed a PPG ADC sample into the active session.
 * Must be called at NP_PPG_SAMPLE_RATE_HZ (200 Hz) from the PPG DMA ISR.
 * Returns immediately; all heavy DSP runs in np_hrv_session_tick().
 */
void np_hrv_session_push_ppg(np_hrv_session_t *sess,
                               int32_t           sample,
                               uint32_t          timestamp_ms);

/*
 * Feed one EEG sample vector (µV) into the active session.
 * Must be called at NP_EEG_SAMPLE_RATE_HZ (500 Hz) for PROTO_EEG_DUAL.
 * No-op for other protocols.
 */
void np_hrv_session_push_eeg(np_hrv_session_t *sess,
                               const float       samples_uv[NP_EEG_CHANNELS],
                               uint32_t          timestamp_ms);

/*
 * Main session processing tick.
 * Must be called from the HRV task (≥ 10 Hz) to drive:
 *   - Coherence Welch update (every NP_COHERENCE_UPDATE_INTERVAL_S)
 *   - Pacer phase transitions
 *   - taVNS failsafe timeout
 *   - Adaptive EEG step (PROTO_EEG_DUAL, every NP_ADAPTIVE_UPDATE_INTERVAL_S)
 *   - Display callback
 *   - Session timeout check
 */
void np_hrv_session_tick(np_hrv_session_t *sess, uint32_t now_ms);

/*
 * Gracefully stop the session before its natural timeout.
 * Triggers the end_cb with NP_HRV_OK.
 */
void np_hrv_session_stop(np_hrv_session_t *sess, uint32_t now_ms);

/*
 * Release the session context back to the pool.
 * Must be called after end_cb returns and the UHDR record has been committed.
 */
void np_hrv_session_destroy(np_hrv_session_t *sess);

/*
 * Return the current coherence result without waiting for the next update cycle.
 * Useful for immediate UI queries.  Returns NP_HRV_ERR_NOT_READY if no result
 * is available yet.
 */
np_hrv_status_t np_hrv_session_get_coherence(const np_hrv_session_t    *sess,
                                               np_hrv_coherence_result_t *out);

#endif /* NP_HRV_SESSION_H */
