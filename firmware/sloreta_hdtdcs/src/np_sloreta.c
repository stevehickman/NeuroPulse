/*
 * NeuroPulse sLORETA Source Imaging — Implementation
 * Document: NP-FW-HD-001 Rev A §5
 *
 * Scalar sLORETA source power estimator.
 *
 * Algorithm (offline precomputation → on-device runtime):
 *
 * Offline (app/PC):
 *   T = (K K^T + λ² H)^{-1} K · W_norm^{-1}
 *   where K = lead field matrix (21 × N_voxels),
 *         H = graph Laplacian regularizer, λ² = regularization parameter.
 *   Result: scalar weight matrix W (N_voxels × 21, one row per voxel,
 *           dominant orientation pre-selected), stored in Config partition.
 *
 * On-device runtime:
 *   1. Accumulate 21×21 sample covariance C from N_EPOCHS EEG windows.
 *   2. For each voxel v: P[v] = W_v^T · C · W_v  (scalar quadratic form).
 *   3. Peak of P[] → target MNI coordinate.
 *
 * Cortex-M7 FPU throughput: 2447 voxels × 441 mults ≈ 1.1M flops;
 * at 600 MHz with FPU pipeline: < 20 ms.
 */

#include "np_sloreta.h"
#include <string.h>
#include <math.h>

/* ── Internal context ────────────────────────────────────────────────────────── */

struct np_sloreta_ctx {
    const float       *W;          /* weight matrix [n_voxels × NP_HD_SLORETA_N_CH] */
    const np_hd_mni_t *voxel_mni;  /* MNI coordinate lookup table                   */
    uint16_t           n_voxels;
    uint16_t           epoch_count;

    /* Running 21×21 sample covariance (upper triangle, row-major).              */
    float cov[NP_HD_SLORETA_N_CH][NP_HD_SLORETA_N_CH];

    /* Per-channel mean for epoch-mean subtraction.                              */
    float ch_mean[NP_HD_SLORETA_N_CH];
};

static struct np_sloreta_ctx s_ctx;

/* ── Compile-time size guard ─────────────────────────────────────────────────── */
_Static_assert(sizeof(struct np_sloreta_ctx) <= NP_SLORETA_CTX_SIZE_BYTES,
               "np_sloreta_ctx exceeds declared NP_SLORETA_CTX_SIZE_BYTES");

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

np_hd_status_t np_sloreta_init(np_sloreta_ctx_t   *ctx,
                                const float        *weight_matrix,
                                const np_hd_mni_t  *voxel_mni,
                                uint16_t            n_voxels)
{
    if (!ctx || !weight_matrix || !voxel_mni || n_voxels == 0U) {
        return NP_HD_ERR_INVALID_ARG;
    }
    memset(ctx, 0, sizeof(struct np_sloreta_ctx));
    ctx->W         = weight_matrix;
    ctx->voxel_mni = voxel_mni;
    ctx->n_voxels  = n_voxels;
    return NP_HD_OK;
}

void np_sloreta_reset(np_sloreta_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    memset(ctx->cov, 0, sizeof(ctx->cov));
    memset(ctx->ch_mean, 0, sizeof(ctx->ch_mean));
    ctx->epoch_count = 0U;
}

/* ── Epoch accumulation ──────────────────────────────────────────────────────── */

np_hd_status_t np_sloreta_push_epoch(np_sloreta_ctx_t *ctx,
                                      const float       samples[][NP_HD_SLORETA_FFT_SIZE],
                                      uint16_t          n_samples)
{
    if (!ctx || !samples || n_samples == 0U) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (!ctx->W) {
        return NP_HD_ERR_NO_WEIGHT_MATRIX;
    }

    /* Compute channel means for this epoch. */
    float means[NP_HD_SLORETA_N_CH];
    for (uint8_t ch = 0U; ch < NP_HD_SLORETA_N_CH; ch++) {
        float sum = 0.0f;
        for (uint16_t s = 0U; s < n_samples; s++) {
            sum += samples[ch][s];
        }
        means[ch] = sum / (float)n_samples;
    }

    /* Accumulate outer product of mean-subtracted epoch mean into covariance.  */
    /* Use epoch mean vector (one sample per channel after mean subtraction)    */
    /* as a rank-1 update: C += (x - mean)(x - mean)^T per sample.             */
    /* For efficiency: accumulate full sample covariance as running mean.        */

    float alpha = 1.0f / (float)(ctx->epoch_count + 1U);
    float beta  = 1.0f - alpha;

    for (uint16_t s = 0U; s < n_samples; s++) {
        for (uint8_t i = 0U; i < NP_HD_SLORETA_N_CH; i++) {
            float xi = samples[i][s] - means[i];
            for (uint8_t j = i; j < NP_HD_SLORETA_N_CH; j++) {
                float xj = samples[j][s] - means[j];
                float outer = xi * xj / (float)n_samples;
                /* Welford-style running mean update across epochs.             */
                ctx->cov[i][j] = beta * ctx->cov[i][j] + alpha * outer;
                ctx->cov[j][i] = ctx->cov[i][j];  /* symmetric                */
            }
        }
    }

    /* Update global channel means (running mean for diagnostics). */
    for (uint8_t ch = 0U; ch < NP_HD_SLORETA_N_CH; ch++) {
        ctx->ch_mean[ch] = beta * ctx->ch_mean[ch] + alpha * means[ch];
    }

    ctx->epoch_count++;
    return NP_HD_OK;
}

/* ── Source power computation ────────────────────────────────────────────────── */

np_hd_status_t np_sloreta_compute_map(np_sloreta_ctx_t *ctx,
                                       float            *source_power_out,
                                       uint16_t          n_voxels)
{
    if (!ctx || !source_power_out) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (!ctx->W) {
        return NP_HD_ERR_NO_WEIGHT_MATRIX;
    }
    if (ctx->epoch_count < NP_HD_SLORETA_EPOCHS) {
        return NP_HD_ERR_NOT_READY;
    }

    uint16_t nv = (n_voxels < ctx->n_voxels) ? n_voxels : ctx->n_voxels;
    uint8_t  nc = NP_HD_SLORETA_N_CH;

    for (uint16_t v = 0U; v < nv; v++) {
        const float *Wv = &ctx->W[v * nc];  /* row v of weight matrix             */

        /* Compute W_v^T · C · W_v (scalar quadratic form).                     */
        /* Step 1: tmp = C · W_v  (matrix-vector product, nc² ops per voxel).   */
        float tmp[NP_HD_SLORETA_N_CH];
        for (uint8_t i = 0U; i < nc; i++) {
            float acc = 0.0f;
            for (uint8_t j = 0U; j < nc; j++) {
                acc += ctx->cov[i][j] * Wv[j];
            }
            tmp[i] = acc;
        }

        /* Step 2: P[v] = W_v · tmp  (dot product, nc ops per voxel).           */
        float p = 0.0f;
        for (uint8_t i = 0U; i < nc; i++) {
            p += Wv[i] * tmp[i];
        }
        source_power_out[v] = p;
    }

    return NP_HD_OK;
}

np_hd_status_t np_sloreta_find_peak(np_sloreta_ctx_t      *ctx,
                                     const float           *source_power,
                                     uint16_t               n_voxels,
                                     np_hd_sloreta_result_t *out)
{
    if (!ctx || !source_power || !out || n_voxels == 0U) {
        return NP_HD_ERR_INVALID_ARG;
    }

    uint16_t peak_idx  = 0U;
    float    peak_val  = source_power[0];

    for (uint16_t v = 1U; v < n_voxels; v++) {
        if (source_power[v] > peak_val) {
            peak_val = source_power[v];
            peak_idx = v;
        }
    }

    out->peak_voxel       = peak_idx;
    out->peak_mni         = ctx->voxel_mni[peak_idx];
    out->peak_source_power = peak_val;
    out->valid            = true;

    /* Band decomposition at peak voxel. */
    np_sloreta_band_power(ctx, peak_idx, source_power, &out->peak_bands);

    return NP_HD_OK;
}

np_hd_status_t np_sloreta_band_power(np_sloreta_ctx_t    *ctx,
                                      uint16_t             voxel_idx,
                                      const float         *source_power,
                                      np_hd_band_power_t  *out)
{
    (void)source_power;  /* source_power reserved for future per-band sLORETA   */

    if (!ctx || !out) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (voxel_idx >= ctx->n_voxels) {
        return NP_HD_ERR_INVALID_ARG;
    }

    const float *Wv = &ctx->W[voxel_idx * NP_HD_SLORETA_N_CH];

    /* Project EEG band power through weight vector.                            */
    /* The covariance matrix encodes broadband power; band-specific covariance  */
    /* is approximated via the diagonal of each bin's spectral covariance.      */
    /* Implementation: W_v^T · diag(band_psd) · W_v for each band.             */
    /* Here we use the broadband covariance (already accumulated) scaled by      */
    /* relative bin counts as a first-order band estimate.                       */

    float total_bins = (float)(NP_HD_BETA_BIN_HI - NP_HD_DELTA_BIN_LO + 1U);
    float w_delta = (float)(NP_HD_DELTA_BIN_HI - NP_HD_DELTA_BIN_LO + 1U) / total_bins;
    float w_theta = (float)(NP_HD_THETA_BIN_HI - NP_HD_THETA_BIN_LO + 1U) / total_bins;
    float w_alpha = (float)(NP_HD_ALPHA_BIN_HI - NP_HD_ALPHA_BIN_LO + 1U) / total_bins;
    float w_beta  = (float)(NP_HD_BETA_BIN_HI  - NP_HD_BETA_BIN_LO  + 1U) / total_bins;

    /* Quadratic form W_v^T C W_v for full broadband. */
    float p_total = 0.0f;
    for (uint8_t i = 0U; i < NP_HD_SLORETA_N_CH; i++) {
        float tmp = 0.0f;
        for (uint8_t j = 0U; j < NP_HD_SLORETA_N_CH; j++) {
            tmp += ctx->cov[i][j] * Wv[j];
        }
        p_total += Wv[i] * tmp;
    }

    out->delta = p_total * w_delta;
    out->theta = p_total * w_theta;
    out->alpha = p_total * w_alpha;
    out->beta  = p_total * w_beta;

    return NP_HD_OK;
}

/* ── Diagnostics ─────────────────────────────────────────────────────────────── */

uint16_t np_sloreta_epoch_count(const np_sloreta_ctx_t *ctx)
{
    return ctx ? ctx->epoch_count : 0U;
}

float np_sloreta_covariance_norm(const np_sloreta_ctx_t *ctx)
{
    if (!ctx) {
        return 0.0f;
    }
    float sum_sq = 0.0f;
    for (uint8_t i = 0U; i < NP_HD_SLORETA_N_CH; i++) {
        for (uint8_t j = 0U; j < NP_HD_SLORETA_N_CH; j++) {
            sum_sq += ctx->cov[i][j] * ctx->cov[i][j];
        }
    }
    return sqrtf(sum_sq);
}
