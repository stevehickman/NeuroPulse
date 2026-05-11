/*
 * NeuroPulse HRV Biofeedback — Coherence Algorithm
 * Document: NP-FW-HRV-001 Rev A §6
 *
 * Coherence score = LF_peak_power / (LF_total_power + HF_total_power) × 10
 *
 * Computation pipeline:
 *  1. Linear interpolation of R-R interval series to NP_RR_INTERP_RATE_HZ grid.
 *  2. Mean subtraction (detrend).
 *  3. Welch's periodogram: NP_HRV_FFT_SIZE-point FFT, Hann window, 50 % overlap.
 *  4. LF peak search (bins NP_HRV_LF_BIN_LO–NP_HRV_LF_BIN_HI).
 *  5. LF and HF band power integration.
 *  6. Coherence score mapping and RMSSD computation.
 *
 * The FFT is a self-contained radix-2 DIT implementation (no external DSP lib).
 * All arithmetic uses float32 exploiting the i.MX RT1062 FPU.
 */

#ifndef NP_HRV_COHERENCE_H
#define NP_HRV_COHERENCE_H

#include "np_hrv_types.h"

/*
 * Initialise Welch accumulator and pre-compute Hann window coefficients.
 * Must be called once before np_hrv_coherence_update().
 */
void np_hrv_coherence_init(np_hrv_psd_t *psd);

/*
 * Feed the current R-R buffer into the Welch estimator.
 *
 * Internally:
 *  - Interpolates buf to a uniform NP_RR_INTERP_RATE_HZ grid.
 *  - Extracts the most recent NP_HRV_FFT_SIZE interpolated samples.
 *  - Applies Hann window, FFT, accumulates into psd->psd[].
 *
 * Returns NP_HRV_ERR_NOT_READY if buf->count < NP_HRV_MIN_RR_FOR_COHERENCE.
 * When the call returns NP_HRV_OK, psd->valid is set to true and
 * psd->num_averages is incremented.
 */
np_hrv_status_t np_hrv_coherence_update(const np_rr_buffer_t *buf,
                                         np_hrv_psd_t         *psd);

/*
 * Extract coherence result from an up-to-date PSD.
 * `rr_buf` is also passed to compute RMSSD.
 * Returns NP_HRV_ERR_NOT_READY if psd->valid is false.
 */
np_hrv_status_t np_hrv_coherence_compute(const np_hrv_psd_t   *psd,
                                          const np_rr_buffer_t *rr_buf,
                                          np_hrv_coherence_result_t *result);

/*
 * Reset the Welch accumulator (call at session start).
 */
void np_hrv_coherence_reset(np_hrv_psd_t *psd);

/*
 * Interpolate an R-R buffer to a uniform 4 Hz float array of `out_len` points.
 * The interpolation spans the time window covered by the last `out_len` samples
 * of the uniform grid.  `out_len` must be ≤ NP_RR_INTERP_BUFFER_SIZE.
 *
 * Exposed for unit-testing.
 */
np_hrv_status_t np_hrv_rr_interpolate(const np_rr_buffer_t *buf,
                                        float                *out,
                                        uint16_t              out_len);

#endif /* NP_HRV_COHERENCE_H */
