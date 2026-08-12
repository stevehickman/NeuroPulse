/*
 * NeurOne HRV Biofeedback — Breathing Pacer
 * Document: NP-FW-HRV-001 Rev 1 §7
 *
 * Manages the breathing cadence cue delivered as:
 *   - Visual ring expand/contract events (relayed to app via callback).
 *   - Optional bone conduction audio tone (relayed to audio subsystem).
 *
 * Resonance frequency (RF) personalization:
 *   On first use, a sweep from NP_PACER_RATE_MIN_BPM to NP_PACER_RATE_MAX_BPM
 *   in NP_PACER_SWEEP_STEP_BPM increments is performed.  Each rate is held for
 *   NP_PACER_SWEEP_DWELL_S seconds while coherence is monitored.  The rate
 *   producing the highest mean coherence is stored as the user's resonance
 *   frequency in the Config partition (SHDR — no user biology in the frequency
 *   value itself; see NP-FW-EMMC-001 §12 for classification rationale).
 *
 * The pacer is tick-driven: np_hrv_pacer_tick() must be called at ≥ 10 Hz.
 */

#ifndef NP_HRV_PACER_H
#define NP_HRV_PACER_H

#include "np_hrv_types.h"

/*
 * Callback invoked on every phase transition.
 * `phase` is the NEW phase that has just started.
 * `duration_ms` is the expected duration of the new phase.
 */
typedef void (*np_pacer_phase_cb_t)(np_pacer_phase_t phase, uint32_t duration_ms);

/*
 * Initialize pacer state.
 * `rate_bpm` sets the initial target rate (0.0 → use NP_PACER_RATE_DEFAULT_BPM).
 * `personalised_rate_bpm` is the user's stored RF (0.0 → not yet personalized).
 * `phase_cb` is called on every inspire/expire transition (may be NULL).
 */
void np_hrv_pacer_init(np_pacer_state_t  *pacer,
                        float              rate_bpm,
                        float              personalised_rate_bpm,
                        np_pacer_phase_cb_t phase_cb);

/*
 * Advance the pacer by `now_ms` (monotonic system time in milliseconds).
 * Fires `phase_cb` when a phase boundary is reached.
 * During RF sweep, calls np_hrv_pacer_sweep_tick() internally.
 */
void np_hrv_pacer_tick(np_pacer_state_t *pacer, uint32_t now_ms);

/*
 * Feed the latest coherence score into the RF sweep evaluator.
 * Must be called whenever a new coherence result is available during a sweep.
 * Has no effect when sweep_active is false.
 */
void np_hrv_pacer_sweep_coherence(np_pacer_state_t *pacer, float coherence_score);

/*
 * Finalise the RF sweep: compute best rate and mark pacer as personalized.
 * Writes the result to `best_rate_bpm_out`.
 * Returns NP_HRV_ERR_NOT_READY if the sweep has not completed all steps.
 */
np_hrv_status_t np_hrv_pacer_sweep_finalise(np_pacer_state_t *pacer,
                                              float            *best_rate_bpm_out);

/*
 * Override the current target rate mid-session (used by closed-loop EEG mode).
 * Rate is clamped to [NP_PACER_RATE_MIN_BPM, NP_PACER_RATE_MAX_BPM].
 */
void np_hrv_pacer_set_rate(np_pacer_state_t *pacer, float rate_bpm);

#endif /* NP_HRV_PACER_H */
