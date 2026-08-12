/*
 * NeurOne HRV Biofeedback — Coherence Algorithm
 * Document: NP-FW-HRV-001 Rev 1 §6
 *
 * Self-contained radix-2 DIT FFT (float32, fixed size NP_HRV_FFT_SIZE = 256).
 * Welch's periodogram with Hann window and 50 % overlap.
 *
 * Memory: all large buffers are module-static (no stack allocation of arrays
 * larger than 64 bytes).
 */

#include "np_hrv_coherence.h"
#include "np_hrv_ppg.h"

/* Float alias used in PSD normalization. */
#define NP_HRV_INTERP_RATE_HZ_F  ((float)NP_RR_INTERP_RATE_HZ)
#include <string.h>
#include <math.h>

/* ── Static storage ─────────────────────────────────────────────────────────── */
static float s_hann[NP_HRV_FFT_SIZE];
static float s_interp_buf[NP_RR_INTERP_BUFFER_SIZE + NP_HRV_FFT_SIZE];
static float s_fft_re[NP_HRV_FFT_SIZE];
static float s_fft_im[NP_HRV_FFT_SIZE];
static bool  s_hann_ready = false;

/* ── Hann window pre-computation ─────────────────────────────────────────────── */
static void hann_init(void)
{
    if (s_hann_ready) {
        return;
    }
    for (uint16_t i = 0U; i < NP_HRV_FFT_SIZE; i++) {
        float t     = (float)i / (float)(NP_HRV_FFT_SIZE - 1U);
        s_hann[i]   = 0.5f * (1.0f - cosf(2.0f * 3.14159265358979f * t));
    }
    s_hann_ready = true;
}

/* ── Radix-2 DIT FFT (in-place, float32) ─────────────────────────────────────── */
/* N must be a power of 2.  re[] and im[] hold real and imaginary parts.         */
static void fft_r2dit(float *re, float *im, uint16_t N)
{
    /* Bit-reversal permutation */
    uint16_t j = 0U;
    for (uint16_t i = 1U; i < N; i++) {
        uint16_t bit = N >> 1U;
        while (j & bit) {
            j ^= bit;
            bit >>= 1U;
        }
        j ^= bit;
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }

    /* Cooley-Tukey butterfly stages */
    for (uint16_t len = 2U; len <= N; len <<= 1U) {
        float ang   = -2.0f * 3.14159265358979f / (float)len;
        float wre   = cosf(ang);
        float wim   = sinf(ang);
        for (uint16_t i = 0U; i < N; i += len) {
            float cur = 1.0f;
            float cui = 0.0f;
            uint16_t half = len >> 1U;
            for (uint16_t k = 0U; k < half; k++) {
                float ur = re[i + k];
                float ui = im[i + k];
                float vr = re[i + k + half] * cur - im[i + k + half] * cui;
                float vi = re[i + k + half] * cui + im[i + k + half] * cur;
                re[i + k]        = ur + vr;
                im[i + k]        = ui + vi;
                re[i + k + half] = ur - vr;
                im[i + k + half] = ui - vi;
                float new_cur = cur * wre - cui * wim;
                float new_cui = cur * wim + cui * wre;
                cur = new_cur;
                cui = new_cui;
            }
        }
    }
}

/* ── R-R interpolation to uniform 4 Hz grid ─────────────────────────────────── */
np_hrv_status_t np_hrv_rr_interpolate(const np_rr_buffer_t *buf,
                                        float                *out,
                                        uint16_t              out_len)
{
    if (buf->count < 2U || out_len == 0U) {
        return NP_HRV_ERR_NOT_READY;
    }

    /* Build cumulative time axis from the RR intervals. */
    /* Use the timestamp of the newest interval as the end anchor. */
    uint16_t n = buf->count;

    /* Reconstruct absolute timestamps from stored timestamps.                  */
    /* buf timestamps are stored at peak detection time for each R peak.        */
    /* t[i] = timestamp of the i-th R-R interval (its ending R peak).           */

    /* The interpolation output spans from t[n-out_len-1] to t[n-1] on a        */
    /* 4 Hz grid (step = 1000/4 = 250 ms).                                      */

    uint32_t t_end   = np_hrv_rr_get_timestamp(buf, (uint16_t)(n - 1U));
    uint32_t t_start = (t_end > (uint32_t)(out_len * 1000U / NP_RR_INTERP_RATE_HZ))
                       ? (t_end - (uint32_t)(out_len * 1000U / NP_RR_INTERP_RATE_HZ))
                       : 0U;

    uint32_t step_ms = 1000U / NP_RR_INTERP_RATE_HZ;  /* 250 ms */

    for (uint16_t s = 0U; s < out_len; s++) {
        uint32_t t_query = t_start + (uint32_t)s * step_ms;

        /* Find the two neighbouring R-peak timestamps that bracket t_query. */
        uint16_t lo = 0U;
        for (uint16_t k = 0U; k < n - 1U; k++) {
            if (np_hrv_rr_get_timestamp(buf, (uint16_t)(k + 1U)) >= t_query) {
                lo = k;
                break;
            }
            lo = (uint16_t)(n - 2U);
        }

        uint32_t t_lo = np_hrv_rr_get_timestamp(buf, lo);
        uint32_t t_hi = np_hrv_rr_get_timestamp(buf, (uint16_t)(lo + 1U));
        float    v_lo = (float)np_hrv_rr_get(buf, lo);
        float    v_hi = (float)np_hrv_rr_get(buf, (uint16_t)(lo + 1U));

        float alpha = 0.0f;
        if (t_hi > t_lo) {
            alpha = (float)(t_query - t_lo) / (float)(t_hi - t_lo);
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;
        }
        out[s] = v_lo + alpha * (v_hi - v_lo);
    }

    return NP_HRV_OK;
}

/* ── Welch periodogram accumulation ─────────────────────────────────────────── */

void np_hrv_coherence_init(np_hrv_psd_t *psd)
{
    hann_init();
    np_hrv_coherence_reset(psd);
}

void np_hrv_coherence_reset(np_hrv_psd_t *psd)
{
    memset(psd->psd, 0, sizeof(psd->psd));
    psd->num_averages = 0U;
    psd->valid        = false;
}

np_hrv_status_t np_hrv_coherence_update(const np_rr_buffer_t *buf,
                                         np_hrv_psd_t         *psd)
{
    if (buf->count < NP_HRV_MIN_RR_FOR_COHERENCE) {
        return NP_HRV_ERR_NOT_READY;
    }

    /* Interpolate buffer to 4 Hz grid — fill the entire extended interp buf.  */
    uint16_t interp_len = (uint16_t)(NP_RR_INTERP_BUFFER_SIZE + NP_HRV_FFT_SIZE);
    np_hrv_status_t ret = np_hrv_rr_interpolate(buf, s_interp_buf, interp_len);
    if (ret != NP_HRV_OK) {
        return ret;
    }

    /* Detrend: subtract mean of the FFT window. */
    float mean = 0.0f;
    for (uint16_t i = 0U; i < NP_HRV_FFT_SIZE; i++) {
        mean += s_interp_buf[NP_RR_INTERP_BUFFER_SIZE + i];
    }
    mean /= (float)NP_HRV_FFT_SIZE;

    /* Apply Hann window and load FFT input. */
    for (uint16_t i = 0U; i < NP_HRV_FFT_SIZE; i++) {
        s_fft_re[i] = (s_interp_buf[NP_RR_INTERP_BUFFER_SIZE + i] - mean) * s_hann[i];
        s_fft_im[i] = 0.0f;
    }

    /* In-place radix-2 FFT. */
    fft_r2dit(s_fft_re, s_fft_im, NP_HRV_FFT_SIZE);

    /* Hann window power normalization factor: sum(w²) = N/2 for Hann. */
    float norm = 2.0f / ((float)NP_HRV_FFT_SIZE * (float)NP_HRV_INTERP_RATE_HZ_F);

    /* Accumulate one-sided PSD into psd->psd[]. */
    for (uint16_t k = 0U; k < NP_HRV_FFT_SIZE / 2U; k++) {
        float mag2 = s_fft_re[k] * s_fft_re[k] + s_fft_im[k] * s_fft_im[k];
        /* Double for one-sided (except DC and Nyquist). */
        float scale = (k == 0U || k == NP_HRV_FFT_SIZE / 2U) ? norm : 2.0f * norm;
        psd->psd[k] = (psd->psd[k] * (float)psd->num_averages + mag2 * scale)
                       / (float)(psd->num_averages + 1U);
    }

    if (psd->num_averages < 0xFFFFU) {
        psd->num_averages++;
    }
    psd->valid = true;

    return NP_HRV_OK;
}

/* ── Coherence result extraction ─────────────────────────────────────────────── */

np_hrv_status_t np_hrv_coherence_compute(const np_hrv_psd_t   *psd,
                                          const np_rr_buffer_t *rr_buf,
                                          np_hrv_coherence_result_t *result)
{
    if (!psd->valid) {
        return NP_HRV_ERR_NOT_READY;
    }

    /* LF band: integrated power and peak. */
    float lf_total = 0.0f;
    float lf_peak  = 0.0f;
    float freq_res = (float)NP_RR_INTERP_RATE_HZ / (float)NP_HRV_FFT_SIZE;

    for (uint16_t k = NP_HRV_LF_BIN_LO; k <= NP_HRV_LF_BIN_HI; k++) {
        lf_total += psd->psd[k] * freq_res;
        if (psd->psd[k] > lf_peak) {
            lf_peak = psd->psd[k];
        }
    }

    /* HF band: integrated power. */
    float hf_total = 0.0f;
    for (uint16_t k = NP_HRV_HF_BIN_LO; k <= NP_HRV_HF_BIN_HI; k++) {
        hf_total += psd->psd[k] * freq_res;
    }

    float total_power = lf_total + hf_total;
    float coherence   = (total_power > 0.0f)
                        ? (lf_peak / total_power) * NP_COHERENCE_SCALE_MAX
                        : 0.0f;
    if (coherence > NP_COHERENCE_SCALE_MAX) {
        coherence = NP_COHERENCE_SCALE_MAX;
    }

    result->lf_peak_power  = lf_peak;
    result->lf_total_power = lf_total;
    result->hf_total_power = hf_total;
    result->lf_hf_ratio    = (hf_total > 0.0f) ? (lf_total / hf_total) : 0.0f;
    result->coherence_score = coherence;
    result->valid           = true;

    np_hrv_ppg_rmssd(rr_buf, &result->rmssd_ms);

    return NP_HRV_OK;
}
