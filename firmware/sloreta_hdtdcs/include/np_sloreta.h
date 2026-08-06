/*
 * NeurOne sLORETA Source Imaging — Public API
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

/* ── Context ─────────────────────────────────────────────────────────────────── */
/*
 * Transparent so callers may embed it by value — np_hd_session holds the single
 * static instance inside its session pool (NP-FW-HD-001 §8).  The struct is
 * dominated by the covariance matrices: one 21×21 broadband (~1.8 KB) plus four
 * 21×21 per-band (~7.1 KB), ~8.9 KB total.  It is placed in the embedder's
 * storage, not allocated here.  Do not touch members from outside this module —
 * use the np_sloreta_* API below.
 */
typedef struct np_sloreta_ctx {
    const float       *W;          /* weight matrix [n_voxels × NP_HD_SLORETA_N_CH] */
    const np_hd_mni_t *voxel_mni;  /* MNI coordinate lookup table                   */
    uint16_t           n_voxels;
    uint16_t           epoch_count;

    /* Epochs folded into cov_band[].  Tracked separately from epoch_count       */
    /* because the spectral path requires a full NP_HD_SLORETA_FFT_SIZE epoch     */
    /* and skips short ones, which the time-domain covariance still accepts.      */
    uint16_t           spectral_epochs;

    /* Running 21×21 broadband sample covariance (time domain, unwindowed).      */
    float cov[NP_HD_SLORETA_N_CH][NP_HD_SLORETA_N_CH];

    /* Running 21×21 cross-spectral covariance per band, indexed by np_hd_band_t. */
    /* C_band[i][j] = mean over epochs of  Σ_{k∈band} Re(X_i[k] · conj(X_j[k]))   */
    /* scaled to one-sided variance units.  See np_sloreta.c for the derivation.  */
    float cov_band[NP_HD_BAND_COUNT][NP_HD_SLORETA_N_CH][NP_HD_SLORETA_N_CH];

    /* Per-channel mean for epoch-mean subtraction.                              */
    float ch_mean[NP_HD_SLORETA_N_CH];
} np_sloreta_ctx_t;

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
 *
 * Two accumulators are updated from the same epoch:
 *   1. Broadband time-domain covariance C — mean-subtracted, unwindowed. Drives
 *      np_sloreta_compute_map() and therefore peak localisation.
 *   2. Per-band cross-spectral covariance C_band — mean-subtracted, Hann-
 *      windowed, one FFT per channel. Drives np_sloreta_band_power().
 * Accumulator 2 needs a full NP_HD_SLORETA_FFT_SIZE epoch and is skipped for a
 * short one; accumulator 1 still updates, and NP_HD_OK is still returned.
 *
 * samples: [channel][sample], channel-major, float, units µV.
 * n_samples: epoch length in samples (NP_HD_SLORETA_FFT_SIZE for band power).
 *
 * NOT REENTRANT and NOT ISR-SAFE.  The spectral path runs NP_HD_EEG_CHANNELS
 * 1024-point FFTs through module-static scratch buffers (~26 KB) — call it from
 * one task only, never from the ADS1299 DMA completion ISR.  Epochs arrive every
 * 2.048 s at 500 Hz, so the ~1.3 Mflop cost is ~0.1 % of one core.
 *
 * Returns NP_HD_OK when a covariance update was accepted,
 *         NP_HD_ERR_INVALID_ARG on a NULL/zero-length epoch,
 *         NP_HD_ERR_NO_WEIGHT_MATRIX if init() has not bound W.
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
 * Source power in each frequency band at a specified voxel.
 *
 * For each band b:  out[b] = W_v^T · C_band[b] · W_v
 * where C_band[b] is the cross-spectral covariance accumulated by
 * np_sloreta_push_epoch() over that band's FFT bins.  The four bands are
 * independent measurements over disjoint bin sets — this is a spectral
 * decomposition, not a partition of the broadband map, and it does NOT read the
 * output of np_sloreta_compute_map().
 *
 * Units are source-space and depend on the offline normalisation of W; see the
 * note on np_hd_band_power_t.  Band-edge leakage is bounded by the Hann window
 * (a tone within one bin of an edge contributes to the neighbouring band).
 *
 * Called internally by np_sloreta_find_peak; also exposed for per-region
 * analysis.  Values are non-negative by construction (each is |W_v · X|²
 * summed over bins).
 *
 * Returns NP_HD_OK with out->valid == true when spectra are available,
 *         NP_HD_ERR_NOT_READY (out zeroed, out->valid == false) if no full-length
 *           epoch has been accumulated,
 *         NP_HD_ERR_INVALID_ARG for NULL out or voxel_idx >= n_voxels,
 *         NP_HD_ERR_NO_WEIGHT_MATRIX if init() has not bound W.
 */
np_hd_status_t np_sloreta_band_power(np_sloreta_ctx_t    *ctx,
                                      uint16_t             voxel_idx,
                                      np_hd_band_power_t  *out);

/* ── Diagnostics ─────────────────────────────────────────────────────────────── */

/* Number of complete epochs accumulated since last reset. */
uint16_t np_sloreta_epoch_count(const np_sloreta_ctx_t *ctx);

/* Frobenius norm of the current covariance matrix (quality indicator). */
float np_sloreta_covariance_norm(const np_sloreta_ctx_t *ctx);

#endif /* NP_SLORETA_H */
