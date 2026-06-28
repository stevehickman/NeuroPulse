/*
 * NeuroPulse HRV Biofeedback — Dual EEG + HRV Biofeedback
 * Document: NP-FW-HRV-001 Rev A §9
 *
 * Welch periodogram for EEG band power reuses the same radix-2 DIT FFT
 * structure as the HRV coherence module but operates on NP_EEG_FFT_SIZE
 * (1024) points at 500 Hz.
 */

#include "np_hrv_eeg_biofeedback.h"
#include <string.h>
#include <math.h>

/* ── Static tables ───────────────────────────────────────────────────────────── */
static float s_eeg_hann[NP_EEG_FFT_SIZE];
static float s_eeg_fft_re[NP_EEG_FFT_SIZE];
static float s_eeg_fft_im[NP_EEG_FFT_SIZE];
static bool  s_eeg_hann_ready = false;

/* ── Hann window for EEG FFT ─────────────────────────────────────────────────── */
static void eeg_hann_init(void)
{
    if (s_eeg_hann_ready) {
        return;
    }
    for (uint16_t i = 0U; i < NP_EEG_FFT_SIZE; i++) {
        float t       = (float)i / (float)(NP_EEG_FFT_SIZE - 1U);
        s_eeg_hann[i] = 0.5f * (1.0f - cosf(2.0f * 3.14159265358979f * t));
    }
    s_eeg_hann_ready = true;
}

/* ── Radix-2 DIT FFT (float32, N=1024) ─────────────────────────────────────── */
static void eeg_fft(float *re, float *im, uint16_t N)
{
    uint16_t j = 0U;
    for (uint16_t i = 1U; i < N; i++) {
        uint16_t bit = N >> 1U;
        while (j & bit) { j ^= bit; bit >>= 1U; }
        j ^= bit;
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }
    for (uint16_t len = 2U; len <= N; len <<= 1U) {
        float ang = -2.0f * 3.14159265358979f / (float)len;
        float wre = cosf(ang);
        float wim = sinf(ang);
        for (uint16_t i = 0U; i < N; i += len) {
            float cur = 1.0f, cui = 0.0f;
            uint16_t half = len >> 1U;
            for (uint16_t k = 0U; k < half; k++) {
                float ur = re[i+k];
                float ui = im[i+k];
                float vr = re[i+k+half]*cur - im[i+k+half]*cui;
                float vi = re[i+k+half]*cui + im[i+k+half]*cur;
                re[i+k]       = ur + vr;  im[i+k]       = ui + vi;
                re[i+k+half]  = ur - vr;  im[i+k+half]  = ui - vi;
                float nc = cur*wre - cui*wim;
                float ni = cur*wim + cui*wre;
                cur = nc; cui = ni;
            }
        }
    }
}

/* ── Band power for one channel (Welch, single segment) ─────────────────────── */
static void compute_band_power(const float *seg, float *delta, float *theta,
                                float *alpha, float *beta, float *gamma_out)
{
    float mean = 0.0f;
    for (uint16_t i = 0U; i < NP_EEG_FFT_SIZE; i++) { mean += seg[i]; }
    mean /= (float)NP_EEG_FFT_SIZE;

    for (uint16_t i = 0U; i < NP_EEG_FFT_SIZE; i++) {
        s_eeg_fft_re[i] = (seg[i] - mean) * s_eeg_hann[i];
        s_eeg_fft_im[i] = 0.0f;
    }

    eeg_fft(s_eeg_fft_re, s_eeg_fft_im, NP_EEG_FFT_SIZE);

    /* PSD normalization: 2/(N × Fs) × |X[k]|²   (µV²/Hz, one-sided). */
    float norm = 2.0f / ((float)NP_EEG_FFT_SIZE * (float)NP_EEG_SAMPLE_RATE_HZ);
    float freq_res = (float)NP_EEG_SAMPLE_RATE_HZ / (float)NP_EEG_FFT_SIZE;

    *delta = 0.0f; *theta = 0.0f; *alpha = 0.0f; *beta = 0.0f; *gamma_out = 0.0f;

    for (uint16_t k = 1U; k < NP_EEG_FFT_SIZE / 2U; k++) {
        float mag2 = (s_eeg_fft_re[k]*s_eeg_fft_re[k]
                    + s_eeg_fft_im[k]*s_eeg_fft_im[k]) * norm * freq_res;
        if (k >= NP_EEG_DELTA_BIN_LO && k <= NP_EEG_DELTA_BIN_HI) *delta     += mag2;
        if (k >= NP_EEG_THETA_BIN_LO && k <= NP_EEG_THETA_BIN_HI) *theta     += mag2;
        if (k >= NP_EEG_ALPHA_BIN_LO && k <= NP_EEG_ALPHA_BIN_HI) *alpha     += mag2;
        if (k >= NP_EEG_BETA_BIN_LO  && k <= NP_EEG_BETA_BIN_HI)  *beta      += mag2;
        if (k >= NP_EEG_GAMMA_BIN_LO && k <= NP_EEG_GAMMA_BIN_HI) *gamma_out += mag2;
    }
}

/* ── Public API ──────────────────────────────────────────────────────────────── */

void np_hrv_eeg_init(np_eeg_sample_buf_t *eeg_buf)
{
    eeg_hann_init();
    memset(eeg_buf, 0, sizeof(*eeg_buf));
}

void np_hrv_eeg_push_sample(np_eeg_sample_buf_t *eeg_buf,
                              const float          samples_uv[NP_EEG_CHANNELS])
{
    for (uint8_t ch = 0U; ch < NP_EEG_CHANNELS; ch++) {
        eeg_buf->samples[ch][eeg_buf->head] = samples_uv[ch];
    }
    eeg_buf->head = (uint16_t)((eeg_buf->head + 1U) % NP_EEG_CH_BUF_SIZE);
    if (eeg_buf->count < NP_EEG_CH_BUF_SIZE) {
        eeg_buf->count++;
    }
}

np_hrv_status_t np_hrv_eeg_compute_bands(const np_eeg_sample_buf_t *eeg_buf,
                                           np_eeg_bands_t            *bands_out)
{
    if (eeg_buf->count < NP_EEG_FFT_SIZE) {
        return NP_HRV_ERR_NOT_READY;
    }

    /* Extract the most recent NP_EEG_FFT_SIZE samples per channel. */
    static float seg[NP_EEG_FFT_SIZE];

    for (uint8_t ch = 0U; ch < NP_EEG_CHANNELS; ch++) {
        /* Read from circular buffer starting at (head - FFT_SIZE). */
        uint16_t start = (uint16_t)((eeg_buf->head + NP_EEG_CH_BUF_SIZE
                                     - NP_EEG_FFT_SIZE) % NP_EEG_CH_BUF_SIZE);
        for (uint16_t i = 0U; i < NP_EEG_FFT_SIZE; i++) {
            uint16_t idx = (uint16_t)((start + i) % NP_EEG_CH_BUF_SIZE);
            seg[i]       = eeg_buf->samples[ch][idx];
        }
        compute_band_power(seg,
                           &bands_out->delta[ch],
                           &bands_out->theta[ch],
                           &bands_out->alpha[ch],
                           &bands_out->beta[ch],
                           &bands_out->gamma[ch]);
    }

    /* Frontal alpha/theta ratio: mean over F3 (ch 2) and F4 (ch 3). */
    float frontal_alpha = (bands_out->alpha[2] + bands_out->alpha[3]) * 0.5f;
    float frontal_theta = (bands_out->theta[2] + bands_out->theta[3]) * 0.5f;
    bands_out->alpha_theta_ratio = (frontal_theta > 0.0f)
                                   ? (frontal_alpha / frontal_theta) : 0.0f;
    bands_out->valid = true;

    return NP_HRV_OK;
}

float np_hrv_eeg_adaptive_step(const np_eeg_bands_t            *bands,
                                 const np_hrv_coherence_result_t *hrv,
                                 float                            current_rate_bpm)
{
    if (!bands->valid || !hrv->valid) {
        return 0.0f;
    }

    /* Rule 1: if coherence is below 5.0, nudge rate up to recover coherence. */
    if (hrv->coherence_score < 5.0f) {
        return NP_ADAPTIVE_PACER_STEP_BPM;
    }

    /* Rule 2: if alpha/theta is below target and coherence is maintained,
       nudge rate down (slower breath → more vagal → larger alpha). */
    if (bands->alpha_theta_ratio < NP_ADAPTIVE_ALPHA_THETA_TARGET) {
        return -NP_ADAPTIVE_PACER_STEP_BPM;
    }

    return 0.0f;
}

void np_hrv_dual_biofeedback_update(np_dual_biofeedback_state_t     *state,
                                     const np_hrv_coherence_result_t *hrv,
                                     const np_eeg_bands_t            *eeg,
                                     float                            pacer_rate_bpm,
                                     uint32_t                         now_ms)
{
    state->hrv                    = *hrv;
    state->eeg                    = *eeg;
    state->adaptive_pacer_rate_bpm = pacer_rate_bpm;
    state->last_update_ms         = now_ms;
}
