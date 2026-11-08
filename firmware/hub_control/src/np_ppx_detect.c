/*
 * NeurOne Hub Control Program — the photoparoxysmal detector. See np_ppx_detect.h for what it does and does not do.
 */
#include "np_ppx_detect.h"

#include <math.h>
#include <string.h>

typedef struct {
    float    prev, dc, var, bg;
    uint32_t warm;              /* samples seen, up to the warm-up length                       */
    uint32_t sat_hold;          /* samples left in which a saturation keeps the channel out    */
    uint32_t since_spike;       /* samples since the last spike                                */
    uint32_t spikes_at[8];      /* sample indices of the last spikes (a ring)                  */
    uint8_t  spike_n, spike_head;
} chan_t;

static np_ppx_params_t s_p;
static np_ppx_roles_t  s_roles;
static bool            s_stop_occ;
static chan_t          s_ch[NP_PPX_MAX_CH];
static uint8_t         s_nch;
static uint32_t        s_n;                 /* samples fed since init, per channel */
static bool            s_was_seeing;
static np_ppx_verdict_t s_latched;          /* a discharge, once found, until the next init */
static np_ppx_halt_fn  s_halt;
static uint8_t         s_sens;              /* the sensitivity level in force (0 is the validated setting) */

/* The factor each level above 0 applies to spike_k: more sensitive is a lower threshold. PROVISIONAL (UC-123): not derived,
 * measured or cited. The synthetic tests show the effect and the false-halt cost; they do not choose the numbers. */
static const float SENS_FACTOR[NP_PPX_SENS_MAX + 1U] = {
    1.0f,    /* level 0: the parameter set as validated */
    0.9f,    /* PROVISIONAL */
    0.8f,    /* PROVISIONAL */
    0.7f,    /* PROVISIONAL: on synthetic white noise this level already halts about one minute in five (np_ppx_tests) */
};

static uint32_t ms_to_samples(uint16_t ms) { return (uint32_t)((s_p.fs_hz * (float)ms) / 1000.0f + 0.5f); }

void np_ppx_params_provisional(np_ppx_params_t *out)
{
    /* PROVISIONAL (docs/status/unjustified-choices.md UC-122): none of these is derived, measured or cited. They exist so the
     * detector can run against synthetic signals; none is evidence, and the evidence level in force is NONE. */
    out->fs_hz              = 500.0f;
    out->spike_k            = 8.0f;     /* PROVISIONAL */
    out->bg_tau_ms          = 2000.0f;  /* PROVISIONAL */
    out->refractory_ms      = 40U;      /* PROVISIONAL */
    out->min_spikes         = 1U;       /* PROVISIONAL */
    out->window_ms          = 100U;     /* PROVISIONAL */
    out->min_other_channels = 2U;       /* PROVISIONAL */
    out->quality_ms         = 1000U;    /* PROVISIONAL */
    out->flat_floor_uv      = 0.5f;     /* PROVISIONAL */
    out->noise_ceiling_uv   = 200.0f;   /* PROVISIONAL */
    out->sat_ceiling_uv     = 150000.0f;/* PROVISIONAL: below the ADS1299's +-187.5 mV full scale at gain 24 */
}

void np_ppx_init(const np_ppx_params_t *params, np_ppx_roles_t roles, bool stop_on_occipital_only)
{
    s_p = *params;
    s_roles = roles;
    s_stop_occ = stop_on_occipital_only;
    memset(s_ch, 0, sizeof s_ch);
    s_nch = 0U;
    s_n = 0U;
    s_was_seeing = false;
    s_latched = NP_PPX_CLEAR;
    s_sens = 0U;     /* every session starts at the validated setting; its start sets the configured level */
}

void np_ppx_set_halt_fn(np_ppx_halt_fn fn) { s_halt = fn; }
void np_ppx_set_stop_on_occipital_only(bool on) { s_stop_occ = on; }

void np_ppx_set_sensitivity(uint8_t level)
{
    if (level > NP_PPX_SENS_MAX) { level = (uint8_t)NP_PPX_SENS_MAX; }
    if (level > s_sens) { s_sens = level; }      /* raises only: a detector already watching is never told to look less */
}

uint8_t np_ppx_sensitivity(void) { return s_sens; }

static bool trusted(const chan_t *c)
{
    if (c->warm < ms_to_samples(s_p.quality_ms) || c->sat_hold != 0U) { return false; }
    const float rms = sqrtf(c->var);
    return rms >= s_p.flat_floor_uv && rms <= s_p.noise_ceiling_uv;
}

bool np_ppx_channel_trusted(uint8_t ch) { return ch < s_nch && trusted(&s_ch[ch]); }

static bool involved(const chan_t *c)
{
    const uint32_t win = ms_to_samples(s_p.window_ms);
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < c->spike_n; i++) {
        if (s_n - c->spikes_at[i] <= win) { n++; }
    }
    return n >= s_p.min_spikes && n > 0U;
}

static uint8_t popcount8(uint8_t v) { uint8_t n = 0U; while (v) { n += (uint8_t)(v & 1U); v >>= 1U; } return n; }

static bool seeing_now(void)
{
    if (s_roles.occipital_mask == 0U || popcount8(s_roles.other_mask) < s_p.min_other_channels || s_p.min_other_channels == 0U) {
        return false;     /* no roles: nothing is known about what a channel is, so nothing can be seen */
    }
    bool occ = false;
    uint8_t others = 0U;
    for (uint8_t c = 0U; c < s_nch; c++) {
        if (!trusted(&s_ch[c])) { continue; }
        if (s_roles.occipital_mask & (1U << c)) { occ = true; }
        if (s_roles.other_mask & (1U << c)) { others++; }
    }
    return occ && others >= s_p.min_other_channels;
}

static void note_spike(chan_t *c)
{
    c->spikes_at[c->spike_head] = s_n;
    c->spike_head = (uint8_t)((c->spike_head + 1U) & 7U);
    if (c->spike_n < 8U) { c->spike_n++; }
    c->since_spike = 0U;
}

static void judge(void)
{
    uint8_t others = 0U;
    bool occ = false;
    for (uint8_t c = 0U; c < s_nch; c++) {
        if (!trusted(&s_ch[c]) || !involved(&s_ch[c])) { continue; }
        if (s_roles.occipital_mask & (1U << c)) { occ = true; }
        if (s_roles.other_mask & (1U << c)) { others++; }
    }
    np_ppx_verdict_t v = NP_PPX_CLEAR;
    if (others >= s_p.min_other_channels) { v = NP_PPX_DISCHARGE_GENERALISED; }
    else if (occ) { v = NP_PPX_DISCHARGE_OCCIPITAL; }
    if (v == NP_PPX_CLEAR || s_latched != NP_PPX_CLEAR) { return; }
    if (v == NP_PPX_DISCHARGE_OCCIPITAL && !s_stop_occ) { return; }   /* the clinician's switch: not a reason to stop */
    s_latched = v;
    if (s_halt != NULL) { s_halt(v); }
}

void np_ppx_feed(const float *uv, uint16_t n_samples, uint8_t n_ch)
{
    if (n_ch > NP_PPX_MAX_CH) { n_ch = NP_PPX_MAX_CH; }
    s_nch = n_ch;
    const float dt_ms = 1000.0f / s_p.fs_hz;
    const float a_bg = dt_ms / s_p.bg_tau_ms;
    const float a_q  = dt_ms / (float)s_p.quality_ms;
    const uint32_t warm_n = ms_to_samples(s_p.quality_ms);
    const uint32_t refr = ms_to_samples(s_p.refractory_ms);

    for (uint16_t s = 0U; s < n_samples; s++) {
        s_n++;
        for (uint8_t k = 0U; k < n_ch; k++) {
            chan_t *c = &s_ch[k];
            const float x = uv[(uint32_t)s * n_ch + k];
            const float d = fabsf(x - c->prev);
            c->prev = x;
            if (c->warm == 0U) { c->dc = x; c->var = 0.0f; c->bg = 0.0f; }
            if (fabsf(x) >= s_p.sat_ceiling_uv) { c->sat_hold = warm_n; } else if (c->sat_hold > 0U) { c->sat_hold--; }
            c->dc  += (x - c->dc) * a_q;
            c->var += (((x - c->dc) * (x - c->dc)) - c->var) * a_q;
            c->since_spike++;
            if (c->warm < warm_n) {
                c->warm++;
                c->bg += (d - c->bg) / (float)c->warm;       /* warm-up: a plain mean of the steps, no spike judged */
                continue;
            }
            if (c->bg > 0.0f && d > s_p.spike_k * SENS_FACTOR[s_sens] * c->bg) {
                if (c->since_spike >= refr) { note_spike(c); }
            } else {
                c->bg += (d - c->bg) * a_bg;                  /* a spike is never folded into the background */
            }
        }
        judge();
    }

    const bool seeing = seeing_now();
    if (s_was_seeing && !seeing && s_latched == NP_PPX_CLEAR && s_halt != NULL) {
        s_halt(NP_PPX_BLIND);                                 /* it could see and cannot now: a drive in progress stops */
    }
    s_was_seeing = seeing;
}

bool np_ppx_seeing(void) { return seeing_now(); }

np_ppx_verdict_t np_ppx_verdict(void)
{
    if (s_latched != NP_PPX_CLEAR) { return s_latched; }
    return seeing_now() ? NP_PPX_CLEAR : NP_PPX_BLIND;
}
