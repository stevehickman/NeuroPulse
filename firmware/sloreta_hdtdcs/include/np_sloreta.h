/*
 * NeuroPulse sLORETA Source Imaging — Public API
 * Document: NP-FW-HD-001 Rev A §5
 *
 * Scalar sLORETA: weight matrix W (N_voxels × 21) precomputed offline from
 * BEM forward model on MNI152 head.  Loaded from Config partition at session
 * start into LPSDR4 (32 MB).  On-device runtime: covariance accumulation +
 * matrix–vector multiply gives source power per voxel.
 */

#ifndef NP_SLORETA_H
#define NP_SLORETA_H

#include "np_hd_types.h"

/* ── Context (opaque — allocate via np_sloreta_init) ─────────────────────────── */
typedef struct np_sloreta_ctx np_sloreta_ctx_t;

/*
 * Maximum static context size for linker reservation.  Actual struct is defined
 * in np_sloreta.c; this size must be kept in sync.
 */
#define NP_SLORETA_CTX_SIZE_BYTES   (48U)

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

/*
 * Initialize the sLORETA context.
 *
 * weight_matrix: pointer to the precomputed W matrix in LPSDR4.
 *   Layout: W[voxel * NP_HD_EEG_CHANNELS + channel], row-major, float32.
 *   Must remain valid for the lifetime of ctx.
 * voxel_mni: pointer to MNI coordinate lookup table (one entry per voxel).
 *   Same voxel ordering as weight_matrix.  Stored in flash.
 * n_voxels: number of source voxels (NP_HD_SLORETA_N_VOXELS).
 */
np_hd_status_t np_sloreta_init(np_sloreta_ctx_t   *ctx,
                                const float        *weight_matrix,
                                const np_hd_mni_t  *voxel_mni,
                                uint16_t            n_voxels);

/* Reset accumulated covariance.  Call between sessions. */
void np_sloreta_reset(np_sloreta_ctx_t *ctx);

/* ── EEG epoch accumulation ──────────────────────────────────────────────────── */

/*
 * Push one epoch of EEG data (NP_HD_EEG_CHANNELS channels, n_samples samples).
 * Internally: compute x = mean of epoch (or use full epoch covariance update).
 * Updates the running sample covariance C = (1/N) Σ x_i x_i^T.
 * Call at 500 Hz from the ADS1299 DMA completion ISR (or from task).
 *
 * samples: [channel][sample], channel-major, float, units µV.
 * n_samples: epoch length in samples (NP_HD_SLORETA_FFT_SIZE recommended).
 *
 * Returns NP_HD_OK when a covariance update was accepted,
 *         NP_HD_ERR_NOT_READY if epoch was rejected (not enough samples).
 */
np_hd_status_t np_sloreta_push_epoch(np_sloreta_ctx_t *ctx,
                                      const float       samples[][NP_HD_SLORETA_FFT_SIZE],
                                      uint16_t          n_samples);

/* ── Source power computation ────────────────────────────────────────────────── */

/*
 * Compute source power map from accumulated covariance.
 * Requires at least NP_HD_SLORETA_EPOCHS epochs.
 *
 * For each voxel v: P[v] = W_v^T · C · W_v
 *   where W_v is row v of the weight matrix (21-vector),
 *   and C is the 21×21 sample covariance matrix.
 *
 * source_power_out: caller-supplied buffer, length n_voxels floats.
 *   Caller must allocate in LPSDR4 (2447 floats ≈ 9.6 KB).
 */
np_hd_status_t np_sloreta_compute_map(np_sloreta_ctx_t *ctx,
                                       float            *source_power_out,
                                       uint16_t          n_voxels);

/*
 * Find peak source power and return result structure including MNI coordinates.
 * Must be called after np_sloreta_compute_map().
 *
 * source_power: the array returned by np_sloreta_compute_map().
 * out: filled with peak voxel, MNI coordinates, and band decomposition.
 */
np_hd_status_t np_sloreta_find_peak(np_sloreta_ctx_t      *ctx,
                                     const float           *source_power,
                                     uint16_t               n_voxels,
                                     np_hd_sloreta_result_t *out);

/*
 * Decompose source power into frequency bands at a specified voxel.
 * Uses the Welch periodogram accumulated in ctx for the voxel's virtual
 * channel (W_v · x_epoch).  Called internally by np_sloreta_find_peak;
 * also exposed for per-region band analysis.
 */
np_hd_status_t np_sloreta_band_power(np_sloreta_ctx_t    *ctx,
                                      uint16_t             voxel_idx,
                                      const float         *source_power,
                                      np_hd_band_power_t  *out);

/* ── Diagnostics ─────────────────────────────────────────────────────────────── */

/* Number of complete epochs accumulated since last reset. */
uint16_t np_sloreta_epoch_count(const np_sloreta_ctx_t *ctx);

/* Frobenius norm of the current covariance matrix (quality indicator). */
float np_sloreta_covariance_norm(const np_sloreta_ctx_t *ctx);

#endif /* NP_SLORETA_H */
