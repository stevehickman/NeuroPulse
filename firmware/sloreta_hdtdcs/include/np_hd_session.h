/*
 * NeuroPulse sLORETA-guided HD-tDCS — Session Orchestration API
 * Document: NP-FW-HD-001 Rev A §8
 *
 * Implements the 5-stage workflow from CLAUDE.md §3 T2 additions:
 *   Stage 1: 21-ch qEEG resting-state acquisition (2 min)
 *   Stage 2: sLORETA cortical source map computation
 *   Stage 3: App identifies/confirms target; firmware maps MNI → electrodes
 *   Stage 4: Impedance check; 4×1 current distribution delivery
 *   Stage 5: Session end, UHDR/SHDR record commit
 */

#ifndef NP_HD_SESSION_H
#define NP_HD_SESSION_H

#include "np_hd_types.h"
#include "np_sloreta.h"
#include "np_hd_montage.h"
#include "np_hd_stim.h"

/* ── Session handle (opaque, single static instance) ─────────────────────────── */
typedef struct np_hd_session np_hd_session_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

/*
 * Create (or return existing) session.
 * weight_matrix: precomputed sLORETA W matrix in LPSDR4.  Must remain
 *   valid for the full session.
 * voxel_mni: flash-resident MNI lookup table.
 * config: session parameters from signed app protocol.
 * display_cb: called at each stage transition and during stimulation tick.
 * end_cb: called once at session end or abort.
 *
 * Returns NULL only if end_cb is NULL or weight_matrix is NULL.
 */
np_hd_session_t *np_hd_session_create(const float               *weight_matrix,
                                        const np_hd_mni_t         *voxel_mni,
                                        const np_hd_session_config_t *config,
                                        np_hd_display_cb_t         display_cb,
                                        np_hd_session_end_cb_t     end_cb);

/* Destroy session context (zero all state). */
void np_hd_session_destroy(np_hd_session_t *sess);

/* ── Stage 1: EEG acquisition ────────────────────────────────────────────────── */

/*
 * Begin resting-state EEG collection.  Transitions to STAGE_EEG_ACQUIRE.
 * Drives ADS1299 configuration and DMA via platform HAL.
 */
np_hd_status_t np_hd_session_start_eeg(np_hd_session_t *sess, uint32_t now_ms);

/*
 * Push one epoch of 21-ch EEG data to the sLORETA accumulator.
 * Call from ADS1299 DMA completion task (or ISR-safe wrapper).
 * samples: [NP_HD_EEG_CHANNELS][n_samples], µV, float.
 */
void np_hd_session_push_eeg(np_hd_session_t *sess,
                              const float      samples[][NP_HD_SLORETA_FFT_SIZE],
                              uint16_t         n_samples,
                              uint32_t         timestamp_ms);

/* ── Stage 2: sLORETA computation ───────────────────────────────────────────── */

/*
 * Trigger source map computation.  Blocks for up to ~100 ms on Cortex-M7.
 * Transitions session to STAGE_SLORETA on entry, to STAGE_MONTAGE on success.
 * Must be called after EEG acquisition completes (NP_HD_SLORETA_EPOCHS epochs).
 */
np_hd_status_t np_hd_session_compute_sloreta(np_hd_session_t *sess, uint32_t now_ms);

/*
 * Retrieve sLORETA result after computation.
 * Returns NP_HD_ERR_NOT_READY if computation has not completed.
 */
np_hd_status_t np_hd_session_get_sloreta_result(const np_hd_session_t  *sess,
                                                  np_hd_sloreta_result_t *out);

/* ── Stage 3: Montage selection ──────────────────────────────────────────────── */

/*
 * Select electrode montage from sLORETA result.
 * If config->target != NP_HD_TARGET_CUSTOM, uses the predefined MNI coordinate.
 * If NP_HD_TARGET_CUSTOM, uses config->custom_target_mni.
 * The app may call np_hd_session_override_montage() to substitute a different
 * target before impedance check (e.g., clinician selects from presented options).
 */
np_hd_status_t np_hd_session_select_montage(np_hd_session_t *sess, uint32_t now_ms);

/*
 * Override the computed montage with a specific target.
 * Must be called while in STAGE_MONTAGE, before impedance check starts.
 */
np_hd_status_t np_hd_session_override_target(np_hd_session_t   *sess,
                                               const np_hd_mni_t *target_mni);

/*
 * Request impedance check for all montage electrodes.
 * Result arrives asynchronously; session advances to STAGE_STIMULATION
 * only if all electrodes pass (impedance ≤ NP_HD_MAX_IMPEDANCE_KOHM).
 */
np_hd_status_t np_hd_session_check_impedance(np_hd_session_t *sess, uint32_t now_ms);

/* ── Stage 4: Stimulation ────────────────────────────────────────────────────── */

/*
 * Start HD-tDCS delivery.  Session must be in STAGE_MONTAGE with a
 * successful impedance check.  Requests safety MCU enable.
 * Returns NP_HD_ERR_IMPEDANCE_HIGH if any electrode failed check.
 */
np_hd_status_t np_hd_session_start_stim(np_hd_session_t *sess, uint32_t now_ms);

/*
 * Deliver the safety MCU's SPI response to the session.
 * Call from safety MCU SPI task when a response is received.
 */
void np_hd_session_safety_mcu_response(np_hd_session_t *sess,
                                        bool             granted,
                                        const float      impedance_kohm[]);

/*
 * Main session tick — call from a 100 ms FreeRTOS task.
 * Drives stimulation ramp state machine; triggers stage transitions;
 * calls display_cb with updated state; handles timeouts.
 */
void np_hd_session_tick(np_hd_session_t *sess, uint32_t now_ms);

/* ── Abort ───────────────────────────────────────────────────────────────────── */

/*
 * Abort session at any stage.  Immediately stops stimulation.
 * end_cb is called with abort_reason.
 */
void np_hd_session_abort(np_hd_session_t *sess,
                           np_hd_status_t   reason,
                           uint32_t         now_ms);

/* ── Queries ─────────────────────────────────────────────────────────────────── */

np_hd_stage_t np_hd_session_stage(const np_hd_session_t *sess);

const np_hd_montage_t *np_hd_session_montage(const np_hd_session_t *sess);

#endif /* NP_HD_SESSION_H */
