/*
 * NeurOne HRV Biofeedback — Dual EEG + HRV Biofeedback
 * Document: NP-FW-HRV-001 Rev 1 §9
 *
 * Protocol NP_HRV_PROTO_EEG_DUAL displays HRV coherence score and EEG band
 * power simultaneously, and drives a closed-loop adaptive breathing rate.
 *
 * Closed-loop logic (EEG-adaptive frequency):
 *   Goal: maximise frontal alpha/theta ratio while sustaining coherence ≥ 5.0.
 *   Every NP_ADAPTIVE_UPDATE_INTERVAL_S seconds:
 *     - Compute mean alpha/theta ratio across F3/F4 (channels 2,3 = idx 2,3).
 *     - If ratio < NP_ADAPTIVE_ALPHA_THETA_TARGET AND coherence ≥ 5.0:
 *         nudge pacer rate DOWN by NP_ADAPTIVE_PACER_STEP_BPM (slowing breath
 *         increases vagal tone → larger alpha).
 *     - If coherence < 5.0:
 *         nudge pacer rate UP by NP_ADAPTIVE_PACER_STEP_BPM (recover coherence).
 *     Rate is always clamped to [NP_PACER_RATE_MIN_BPM, NP_PACER_RATE_MAX_BPM].
 *
 * EEG band power computed by Welch's periodogram using a dedicated circular
 * sample buffer (NP_EEG_CH_BUF_SIZE samples per channel).
 */

#ifndef NP_HRV_EEG_BIOFEEDBACK_H
#define NP_HRV_EEG_BIOFEEDBACK_H

#include "np_hrv_types.h"

/*
 * Internal EEG channel buffer (one per channel, managed here to avoid
 * allocating on the calling-task stack).
 */
typedef struct {
    float    samples[NP_EEG_CHANNELS][NP_EEG_CH_BUF_SIZE]; /* raw µV per ch    */
    uint16_t head;           /* next write index (shared across all channels)   */
    uint16_t count;          /* valid samples (saturates at NP_EEG_CH_BUF_SIZE) */
} np_eeg_sample_buf_t;

/*
 * Initialize EEG sample buffer and Hann window table.
 */
void np_hrv_eeg_init(np_eeg_sample_buf_t *eeg_buf);

/*
 * Append one EEG sample vector (all 8 channels, µV).
 * `samples_uv` must point to an array of NP_EEG_CHANNELS float values in
 * channel order: Fp1, Fp2, F3, F4, C3, C4, P3, P4.
 * Call at NP_EEG_SAMPLE_RATE_HZ (500 Hz).
 */
void np_hrv_eeg_push_sample(np_eeg_sample_buf_t *eeg_buf,
                              const float          samples_uv[NP_EEG_CHANNELS]);

/*
 * Run one Welch periodogram update and return band-power results.
 * Called every NP_ADAPTIVE_UPDATE_INTERVAL_S seconds (not on every EEG sample).
 * Returns NP_HRV_ERR_NOT_READY if fewer than NP_EEG_FFT_SIZE samples available.
 */
np_hrv_status_t np_hrv_eeg_compute_bands(const np_eeg_sample_buf_t *eeg_buf,
                                           np_eeg_bands_t            *bands_out);

/*
 * Closed-loop adaptive step: given current EEG bands and HRV coherence,
 * compute the recommended pacer rate adjustment.
 *
 * Returns the nudge in breaths/min (positive = increase rate, negative = decrease).
 * Returns 0.0 if no adjustment is warranted.
 */
float np_hrv_eeg_adaptive_step(const np_eeg_bands_t            *bands,
                                 const np_hrv_coherence_result_t *hrv,
                                 float                            current_rate_bpm);

/*
 * Build the combined dual-biofeedback display state from current HRV and EEG.
 * `pacer` current rate is stored in state->adaptive_pacer_rate_bpm.
 */
void np_hrv_dual_biofeedback_update(np_dual_biofeedback_state_t     *state,
                                     const np_hrv_coherence_result_t *hrv,
                                     const np_eeg_bands_t            *eeg,
                                     float                            pacer_rate_bpm,
                                     uint32_t                         now_ms);

#endif /* NP_HRV_EEG_BIOFEEDBACK_H */
