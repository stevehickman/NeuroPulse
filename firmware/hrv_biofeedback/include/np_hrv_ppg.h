/*
 * NeuroPulse HRV Biofeedback — PPG Signal Processing
 * Document: NP-FW-HRV-001 Rev A §5
 *
 * Processes the 808–830 nm PPG signal from the VNS auricular clip to extract
 * R-R intervals and populate the np_rr_buffer_t used by the coherence engine.
 *
 * Peak detection uses an adaptive threshold: the threshold IIR-tracks the
 * signal envelope, so it self-calibrates to contact quality variations without
 * requiring factory calibration per clip unit.
 */

#ifndef NP_HRV_PPG_H
#define NP_HRV_PPG_H

#include "np_hrv_types.h"

/*
 * Initialise peak detector state.
 * Must be called before np_hrv_ppg_process_sample().
 * `initial_threshold` is a first estimate in ADC counts; the IIR filter
 * will adapt within a few seconds of actual signal.
 */
void np_hrv_ppg_init(np_ppg_detector_t *det, int32_t initial_threshold);

/*
 * Feed one PPG ADC sample (12-bit, 0–4095).
 * `timestamp_ms` is the system time in milliseconds when the sample was taken.
 *
 * When a valid R peak is detected, the R-R interval is computed and pushed into
 * `rr_buf`.  Returns true if a new R-R interval was added, false otherwise.
 *
 * Call at NP_PPG_SAMPLE_RATE_HZ (200 Hz).
 */
bool np_hrv_ppg_process_sample(np_ppg_detector_t *det,
                                np_rr_buffer_t    *rr_buf,
                                int32_t            sample,
                                uint32_t           timestamp_ms);

/*
 * Compute RMSSD over the last NP_RR_RMSSD_WINDOW consecutive intervals in buf.
 * Returns NP_HRV_ERR_NOT_READY if fewer than 2 valid intervals are available.
 */
np_hrv_status_t np_hrv_ppg_rmssd(const np_rr_buffer_t *buf, float *rmssd_ms_out);

/*
 * Append one R-R interval (ms) to the circular buffer.
 * Silently drops the interval if it falls outside [NP_RR_MIN_MS, NP_RR_MAX_MS].
 */
void np_hrv_rr_push(np_rr_buffer_t *buf, uint16_t rr_ms, uint32_t timestamp_ms);

/*
 * Read the system timestamp (ms) of the R peak that terminated interval `idx`.
 * Returns 0 if idx ≥ buf->count.
 */
uint32_t np_hrv_rr_get_timestamp(const np_rr_buffer_t *buf, uint16_t idx);

/*
 * Read interval at logical index `idx` (0 = oldest, count-1 = newest).
 * Returns 0 if idx ≥ buf->count.
 */
uint16_t np_hrv_rr_get(const np_rr_buffer_t *buf, uint16_t idx);

#endif /* NP_HRV_PPG_H */
