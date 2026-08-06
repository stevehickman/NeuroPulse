/*
 * NeurOne sLORETA Source Imaging — Implementation
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
 *
 * Band power runs a SECOND accumulator over the same epochs (NP-FW-HD-001 §5.3):
 * one Hann-windowed 1024-point FFT per channel, from which a 21×21 cross-spectral
 * covariance is accumulated per band.  Band power at a voxel is then the same
 * quadratic form evaluated against the band's matrix instead of the broadband one.
 *
 * This replaces a placeholder that scaled the single broadband quadratic form by
 * four compile-time bin-count ratios.  Because those ratios were constants, the
 * four "bands" were always in fixed proportion: a 10 Hz alpha epoch and a 2 Hz
 * delta epoch produced byte-identical decompositions, with beta always largest
 * purely because it spans the most bins (35 of 61).  See FAI-HD01-E, whose
 * cross-stimulus ratio assertion the placeholder cannot satisfy under any tuning.
 */

#include "np_sloreta.h"
#include <string.h>
#include <math.h>

/* ── Internal context ────────────────────────────────────────────────────────── */
/* struct np_sloreta_ctx is defined transparently in np_sloreta.h so the session  */
/* pool can embed it by value.  The embedder owns the storage (LPSDR4/SRAM).       */

/* ── Spectral scratch and tables (module-static) ─────────────────────────────── */
/*
 * ~26 KB of .bss, shared by all contexts.  Only np_sloreta_push_epoch() touches
 * these, and only between entry and return — nothing here is per-session state,
 * so a single set serves the one static session pool.  The consequence is that
 * push_epoch() is not reentrant; see its header comment.
 *
 *   s_hann      4 KB   window, built once
 *   s_tw_*      4 KB   FFT twiddles, built once
 *   s_fft_*     8 KB   per-channel transform workspace
 *   s_spec_*   10 KB   band-span spectra for all 21 channels of one epoch
 */
static float   s_hann[NP_HD_SLORETA_FFT_SIZE];
static float   s_tw_re[NP_HD_SLORETA_FFT_SIZE / 2U];
static float   s_tw_im[NP_HD_SLORETA_FFT_SIZE / 2U];
static float   s_spectral_scale;      /* 2 / (N² · U) — one-sided variance units */
static bool    s_tables_ready = false;

static float s_fft_re[NP_HD_SLORETA_FFT_SIZE];
static float s_fft_im[NP_HD_SLORETA_FFT_SIZE];
static float s_spec_re[NP_HD_SLORETA_N_CH][NP_HD_BAND_BIN_COUNT];
static float s_spec_im[NP_HD_SLORETA_N_CH][NP_HD_BAND_BIN_COUNT];

/* π to float32 precision.  M_PI is POSIX, not ISO C, and this unit compiles
 * -std=c11 where glibc hides it (same constraint the FAI tests document). */
#define NP_HD_TWO_PI 6.283185307179586f

/*
 * Bin extent of each band, indexed by np_hd_band_t.  np_hd_config.h remains the
 * single source of truth; this table only puts the macros in np_hd_band_t order.
 *
 * Iterating per band over its own range — rather than sweeping every retained bin
 * and asking which band owns it — means there is no "bin belongs to no band" arm
 * to write, and therefore no arm that is unreachable while the bands happen to
 * tile the span.  Bands may be re-cut with gaps or overlaps with no code change.
 */
static const struct { uint16_t lo; uint16_t hi; } k_band_bins[NP_HD_BAND_COUNT] = {
    [NP_HD_BAND_DELTA] = { NP_HD_DELTA_BIN_LO, NP_HD_DELTA_BIN_HI },
    [NP_HD_BAND_THETA] = { NP_HD_THETA_BIN_LO, NP_HD_THETA_BIN_HI },
    [NP_HD_BAND_ALPHA] = { NP_HD_ALPHA_BIN_LO, NP_HD_ALPHA_BIN_HI },
    [NP_HD_BAND_BETA ] = { NP_HD_BETA_BIN_LO,  NP_HD_BETA_BIN_HI  },
};

/*
 * Every band must lie inside the retained span, because s_spec_*[][] is indexed
 * by (k − NP_HD_BAND_BIN_LO).  Enforced at compile time so a band edit that would
 * read past the retained spectra is a build error, not an out-of-bounds read.
 */
_Static_assert(NP_HD_DELTA_BIN_LO >= NP_HD_BAND_BIN_LO &&
               NP_HD_DELTA_BIN_HI <= NP_HD_BAND_BIN_HI, "delta band outside retained bin span");
_Static_assert(NP_HD_THETA_BIN_LO >= NP_HD_BAND_BIN_LO &&
               NP_HD_THETA_BIN_HI <= NP_HD_BAND_BIN_HI, "theta band outside retained bin span");
_Static_assert(NP_HD_ALPHA_BIN_LO >= NP_HD_BAND_BIN_LO &&
               NP_HD_ALPHA_BIN_HI <= NP_HD_BAND_BIN_HI, "alpha band outside retained bin span");
_Static_assert(NP_HD_BETA_BIN_LO  >= NP_HD_BAND_BIN_LO &&
               NP_HD_BETA_BIN_HI  <= NP_HD_BAND_BIN_HI, "beta band outside retained bin span");
/* Bin 0 is DC and NP_HD_BAND_BIN_HI must stay below Nyquist for the one-sided
 * factor of 2 in s_spectral_scale to be correct. */
_Static_assert(NP_HD_BAND_BIN_LO >= 1U, "bin 0 is DC and has no one-sided mirror");
_Static_assert(NP_HD_BAND_BIN_HI < NP_HD_SLORETA_FFT_SIZE / 2U,
               "retained bins must stay below Nyquist");

/*
 * Build the Hann window, the FFT twiddle table, the bin→band map, and the PSD
 * scale factor.  Idempotent; all four are input-independent.
 *
 * PERIODIC Hann (denominator N), deliberately NOT the symmetric N-1 form used by
 * firmware/hrv_biofeedback/src/np_hrv_eeg_biofeedback.c.  Periodic is the correct
 * convention for FFT-based spectral estimation: w[n] = 0.5 − 0.25(e^{i2πn/N} +
 * e^{−i2πn/N}) has exactly three non-zero DFT samples, so a bin-centred tone
 * lands in exactly three bins with no sidelobe leakage at all.  That is what
 * makes FAI-HD01-E's magnitude tolerance a derivation rather than a tuned number.
 *
 * Scale derivation.  Parseval for the DFT gives the windowed signal's variance as
 * (1/N²)·Σ_k |X[k]|² over all N bins.  Summing only positive frequencies halves
 * it, hence the factor 2; dividing by the window power U = (1/N)·Σ w[n]² removes
 * the window's own attenuation.  So
 *
 *     C_band[i][j] = (2 / (N² · U)) · Σ_{k ∈ band} Re(X_i[k] · conj(X_j[k]))
 *
 * and for a coherent tone of amplitude A sitting inside one band, that band's
 * diagonal entry evaluates to exactly A²/2 — the tone's variance, matching what
 * the time-domain covariance reports for the same input.  FAI-HD01-E asserts both
 * that identity and the band-sum-vs-variance reconciliation that follows from it.
 */
static void sloreta_tables_init(void)
{
    if (s_tables_ready) {
        return;
    }

    float sum_sq = 0.0f;
    for (uint16_t n = 0U; n < NP_HD_SLORETA_FFT_SIZE; n++) {
        float w   = 0.5f * (1.0f - cosf(NP_HD_TWO_PI * (float)n /
                                        (float)NP_HD_SLORETA_FFT_SIZE));
        s_hann[n] = w;
        sum_sq   += w * w;
    }
    float u = sum_sq / (float)NP_HD_SLORETA_FFT_SIZE;   /* measured, not assumed */

    s_spectral_scale = 2.0f / ((float)NP_HD_SLORETA_FFT_SIZE *
                               (float)NP_HD_SLORETA_FFT_SIZE * u);

    for (uint16_t m = 0U; m < NP_HD_SLORETA_FFT_SIZE / 2U; m++) {
        float ang = -NP_HD_TWO_PI * (float)m / (float)NP_HD_SLORETA_FFT_SIZE;
        s_tw_re[m] = cosf(ang);
        s_tw_im[m] = sinf(ang);
    }

    s_tables_ready = true;
}

/*
 * In-place iterative radix-2 decimation-in-time FFT over NP_HD_SLORETA_FFT_SIZE
 * points.  Twiddles come from the precomputed table rather than the per-butterfly
 * complex recurrence used in the HRV module: the recurrence compounds float32
 * rotation error across 512 steps in the final stage, and a spectral value that
 * feeds a clinical targeting decision should not carry avoidable drift.
 */
static void sloreta_fft(float *re, float *im)
{
    const uint16_t n = NP_HD_SLORETA_FFT_SIZE;

    /* Bit-reversal permutation. */
    uint16_t j = 0U;
    for (uint16_t i = 1U; i < n; i++) {
        uint16_t bit = (uint16_t)(n >> 1U);
        while (j & bit) {
            j   = (uint16_t)(j ^ bit);
            bit = (uint16_t)(bit >> 1U);
        }
        j = (uint16_t)(j ^ bit);
        if (i < j) {
            float t;
            t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }

    for (uint16_t len = 2U; len <= n; len = (uint16_t)(len << 1U)) {
        uint16_t half = (uint16_t)(len >> 1U);
        uint16_t step = (uint16_t)(n / len);      /* stride into the N/2 table */
        for (uint16_t base = 0U; base < n; base = (uint16_t)(base + len)) {
            for (uint16_t k = 0U; k < half; k++) {
                uint16_t a  = (uint16_t)(base + k);
                uint16_t b  = (uint16_t)(a + half);
                float    wr = s_tw_re[k * step];
                float    wi = s_tw_im[k * step];
                float    vr = re[b] * wr - im[b] * wi;
                float    vi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - vr;  im[b] = im[a] - vi;
                re[a] = re[a] + vr;  im[a] = im[a] + vi;
            }
        }
    }
}

/*
 * Fold one full-length epoch into the per-band cross-spectral covariances.
 * means[] is the epoch mean already computed by the caller, so the spectral and
 * time-domain accumulators see identically detrended input.
 */
static void sloreta_accumulate_spectra(np_sloreta_ctx_t *ctx,
                                        const float       samples[][NP_HD_SLORETA_FFT_SIZE],
                                        const float      *means)
{
    sloreta_tables_init();

    /* One windowed transform per channel; keep only the band-span bins. */
    for (uint8_t ch = 0U; ch < NP_HD_SLORETA_N_CH; ch++) {
        for (uint16_t s = 0U; s < NP_HD_SLORETA_FFT_SIZE; s++) {
            s_fft_re[s] = (samples[ch][s] - means[ch]) * s_hann[s];
            s_fft_im[s] = 0.0f;
        }
        sloreta_fft(s_fft_re, s_fft_im);

        for (uint16_t b = 0U; b < NP_HD_BAND_BIN_COUNT; b++) {
            uint16_t k      = (uint16_t)(NP_HD_BAND_BIN_LO + b);
            s_spec_re[ch][b] = s_fft_re[k];
            s_spec_im[ch][b] = s_fft_im[k];
        }
    }

    /* Same once-per-epoch blend as the time-domain path — see push_epoch. */
    float alpha = 1.0f / (float)(ctx->spectral_epochs + 1U);
    float beta  = 1.0f - alpha;

    for (uint8_t i = 0U; i < NP_HD_SLORETA_N_CH; i++) {
        for (uint8_t j = i; j < NP_HD_SLORETA_N_CH; j++) {
            for (uint8_t band = 0U; band < NP_HD_BAND_COUNT; band++) {
                /* Re(X_i · conj(X_j)) = re_i·re_j + im_i·im_j, over this band. */
                float acc = 0.0f;
                for (uint16_t k = k_band_bins[band].lo;
                     k <= k_band_bins[band].hi; k++) {
                    uint16_t b = (uint16_t)(k - NP_HD_BAND_BIN_LO);
                    acc += s_spec_re[i][b] * s_spec_re[j][b] +
                           s_spec_im[i][b] * s_spec_im[j][b];
                }

                float c_e = acc * s_spectral_scale;
                ctx->cov_band[band][i][j] = beta * ctx->cov_band[band][i][j] +
                                            alpha * c_e;
                ctx->cov_band[band][j][i] = ctx->cov_band[band][i][j];
            }
        }
    }

    ctx->spectral_epochs++;
}

/* W_v^T · M · W_v for a symmetric 21×21 M.  Non-negative when M is a covariance. */
static float sloreta_quadratic_form(const float  m[NP_HD_SLORETA_N_CH][NP_HD_SLORETA_N_CH],
                                     const float *wv)
{
    float p = 0.0f;
    for (uint8_t i = 0U; i < NP_HD_SLORETA_N_CH; i++) {
        float tmp = 0.0f;
        for (uint8_t j = 0U; j < NP_HD_SLORETA_N_CH; j++) {
            tmp += m[i][j] * wv[j];
        }
        p += wv[i] * tmp;
    }
    return p;
}

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
    memset(ctx->cov_band, 0, sizeof(ctx->cov_band));
    memset(ctx->ch_mean, 0, sizeof(ctx->ch_mean));
    ctx->epoch_count     = 0U;
    ctx->spectral_epochs = 0U;
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

    /* This epoch's sample covariance is C_e = (1/N) Σ_s (x_s - μ)(x_s - μ)^T.  */
    /* Fold it into the running across-epoch mean with a single blend per epoch: */
    /*                                                                          */
    /*     C ← β·C + α·C_e,   α = 1/(epoch_count + 1),  β = 1 − α               */
    /*                                                                          */
    /* The blend MUST happen once per epoch, not once per sample.  Applying it   */
    /* inside the sample loop turns it into a geometric decay with rate β per    */
    /* sample: with N = 1024 samples, β^N ≈ 0 for every α, so all but the last   */
    /* handful of samples are annihilated and the result is low by ~1/N.  On the */
    /* first epoch (α = 1, β = 0) it collapses to the final sample alone.        */
    /*                                                                          */
    /* Pair-outer/sample-inner ordering lets each channel pair accumulate into a */
    /* scalar, so no 21×21 epoch-covariance temporary is needed on the stack.    */

    float alpha = 1.0f / (float)(ctx->epoch_count + 1U);
    float beta  = 1.0f - alpha;

    for (uint8_t i = 0U; i < NP_HD_SLORETA_N_CH; i++) {
        for (uint8_t j = i; j < NP_HD_SLORETA_N_CH; j++) {
            float acc = 0.0f;
            for (uint16_t s = 0U; s < n_samples; s++) {
                acc += (samples[i][s] - means[i]) * (samples[j][s] - means[j]);
            }
            float c_e = acc / (float)n_samples;
            ctx->cov[i][j] = beta * ctx->cov[i][j] + alpha * c_e;
            ctx->cov[j][i] = ctx->cov[i][j];  /* symmetric                     */
        }
    }

    /* Update global channel means (running mean for diagnostics). */
    for (uint8_t ch = 0U; ch < NP_HD_SLORETA_N_CH; ch++) {
        ctx->ch_mean[ch] = beta * ctx->ch_mean[ch] + alpha * means[ch];
    }

    /*
     * Second accumulator: per-band cross-spectral covariance (NP-FW-HD-001 §5.3).
     *
     * Kept strictly AFTER the time-domain loop above and operating on its own
     * matrices, so nothing here can perturb the broadband covariance that drives
     * peak localisation — the estimator FAI-HD01-D pins to its variance floor.
     *
     * Requires the full transform length.  A short epoch still contributes to the
     * broadband covariance (which is defined for any n_samples) but is skipped
     * here rather than zero-padded: padding would silently rescale the spectrum
     * and bias every band low, and reporting a biased band power is worse than
     * reporting none.  np_sloreta_band_power() answers NP_HD_ERR_NOT_READY until
     * at least one full epoch has landed.
     */
    if (n_samples == NP_HD_SLORETA_FFT_SIZE) {
        sloreta_accumulate_spectra(ctx, samples, means);
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

        /* P[v] = W_v^T · C · W_v (scalar quadratic form, nc² + nc ops).        */
        source_power_out[v] = sloreta_quadratic_form(ctx->cov, Wv);
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

    /*
     * Band decomposition at the peak voxel.  A failure here leaves peak_bands
     * zeroed with valid == false and does NOT fail peak localisation: the peak
     * comes from the broadband covariance and stands on its own.  Consumers must
     * check peak_bands.valid — zeros are "not measured", not "no activity".
     */
    (void)np_sloreta_band_power(ctx, peak_idx, &out->peak_bands);

    return NP_HD_OK;
}

np_hd_status_t np_sloreta_band_power(np_sloreta_ctx_t    *ctx,
                                      uint16_t             voxel_idx,
                                      np_hd_band_power_t  *out)
{
    if (!ctx || !out) {
        return NP_HD_ERR_INVALID_ARG;
    }

    /* Clear first: every early return below must leave a struct that reads as
     * "no measurement", never as a measurement of zero activity. */
    memset(out, 0, sizeof(*out));

    if (voxel_idx >= ctx->n_voxels) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (!ctx->W) {
        return NP_HD_ERR_NO_WEIGHT_MATRIX;
    }
    if (ctx->spectral_epochs == 0U) {
        return NP_HD_ERR_NOT_READY;
    }

    const float *Wv = &ctx->W[voxel_idx * NP_HD_SLORETA_N_CH];

    /*
     * Four independent quadratic forms over four independent cross-spectral
     * covariances: P_band[v] = W_v^T · C_band · W_v.
     *
     * Each is |W_v · X[k]|² summed over the band's bins and averaged over epochs,
     * so each is non-negative by construction and no band's value can be derived
     * from another's.  Nothing here reads the broadband source-power map.
     */
    out->delta = sloreta_quadratic_form(ctx->cov_band[NP_HD_BAND_DELTA], Wv);
    out->theta = sloreta_quadratic_form(ctx->cov_band[NP_HD_BAND_THETA], Wv);
    out->alpha = sloreta_quadratic_form(ctx->cov_band[NP_HD_BAND_ALPHA], Wv);
    out->beta  = sloreta_quadratic_form(ctx->cov_band[NP_HD_BAND_BETA],  Wv);
    out->valid = true;

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
