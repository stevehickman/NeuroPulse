/* np_audio_synth.c — see np_audio_synth.h.  REQ-AUDIO-29. */
#include "np_audio_synth.h"
#include <math.h>

#define TWO32 4294967296.0

/* Whole cycles per second x 2^32 per sample; refused if it would not fit (at or above half the sample rate). */
static int increment(uint64_t freq_mhz, uint32_t fs, uint32_t *inc)
{
    /* inc = freq * 2^32 / fs, with freq in millihertz: (freq_mhz << 32) / (fs * 1000).  freq_mhz < 2^32, so the
     * product is below 2^64. */
    uint64_t v = (freq_mhz << 32) / ((uint64_t)fs * 1000ULL);
    if (v >= 0x80000000ULL) { return 0; }
    *inc = (uint32_t)v;
    return 1;
}

np_hub_status_t np_audio_synth_init(np_audio_synth_t *s, const np_audio_synth_cfg_t *cfg)
{
    if (s == NULL || cfg == NULL) { return NP_HUB_ERR_INVALID_ARG; }
    if (cfg->sample_rate_hz == 0U || cfg->carrier_hz == 0U || cfg->rate_mhz == 0U) { return NP_HUB_ERR_INVALID_ARG; }
    if (cfg->duty_permille == 0U || cfg->duty_permille > 1000U || cfg->amplitude <= 0) { return NP_HUB_ERR_INVALID_ARG; }

    uint32_t cinc, pinc;
    if (!increment((uint64_t)cfg->carrier_hz * 1000ULL, cfg->sample_rate_hz, &cinc)) { return NP_HUB_ERR_INVALID_ARG; }
    if (!increment(cfg->rate_mhz, cfg->sample_rate_hz, &pinc) || pinc == 0U) { return NP_HUB_ERR_INVALID_ARG; }

    /* Shares of the period: on = duty; ramp = ramp_us x rate (rate in mHz, so x 1e-9). */
    float on_frac   = (float)cfg->duty_permille / 1000.0f;
    float ramp_frac = (float)((double)cfg->ramp_us * (double)cfg->rate_mhz * 1e-9);
    if (ramp_frac > on_frac * 0.5f) { return NP_HUB_ERR_INVALID_ARG; }

    s->cfg = *cfg;
    s->carrier_phase = 0U;
    s->carrier_inc = cinc;
    s->pulse_phase = 0U;
    s->pulse_inc = pinc;
    s->on_frac = on_frac;
    s->ramp_frac = ramp_frac;
    return NP_HUB_OK;
}

/* The pulse envelope at position t in [0,1): 0 off, 1 full, a raised-cosine ramp on each edge. */
static float envelope(const np_audio_synth_t *s, float t)
{
    if (t >= s->on_frac) { return 0.0f; }
    float x = 1.0f;
    if (s->ramp_frac > 0.0f) {
        float up = t / s->ramp_frac;
        float down = (s->on_frac - t) / s->ramp_frac;
        x = up < down ? up : down;
        if (x > 1.0f) { x = 1.0f; }
    }
    return 0.5f - 0.5f * cosf(3.14159265f * x);
}

void np_audio_synth_render(np_audio_synth_t *s, int16_t *out, uint32_t n)
{
    for (uint32_t i = 0U; i < n; i++) {
        float t = (float)((double)s->pulse_phase / TWO32);
        float g = envelope(s, t);
        float c = sinf((float)((double)s->carrier_phase / TWO32 * 6.283185307179586));
        float v = g * c * (float)s->cfg.amplitude;
        out[i] = (int16_t)lrintf(v);
        s->carrier_phase += s->carrier_inc;
        s->pulse_phase += s->pulse_inc;
    }
}
