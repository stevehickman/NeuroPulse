/*
 * NeurOne Zone Module Announcement — Bone Conduction Audio Synthesis
 * Document: NP-FW-ZA-001 Rev 1 §7
 *
 * Produces the zone identification audio cue via the bone conduction
 * piezoelectric element at the mastoid (SAI3 I2S output, eDMA channel 5).
 *
 * Audio encoding per zone (RISK-15 Layer 5):
 *   Zone N plays N short beeps (zone position pulses) followed by a rising
 *   confirmation tone.  The pattern is distinct by beep count and requires no
 *   visual attention, covering blind users and those wearing eye shades.
 *
 *   Example — Frontal Left (zone 1):
 *     1× 440 Hz beep (120 ms) → 180 ms silence → 880 Hz confirm (250 ms)
 *
 *   Example — Parietal Right (zone 5):
 *     5× 440 Hz beeps (120 ms on / 80 ms off) → 180 ms silence → 880 Hz (250 ms)
 *
 *   Unknown zone (wiring fault):
 *     200 Hz (300 ms) → 100 ms silence → 150 Hz (300 ms)  [descending error pair]
 *
 * Synthesis: direct digital synthesis (DDS) from a 256-entry Q15 sine LUT.
 *   phase_inc = (freq_hz × 2^32) / NP_ZA_AUDIO_SAMPLE_RATE_HZ
 *   sample    = LUT[phase_acc >> NP_ZA_SINE_LUT_SHIFT]  (top 8 bits of Q32 acc)
 *
 * Amplitude ramp: linear 20 ms ramp up at start and down at end of each clip
 * to prevent piezo click artefacts and protect the silicone isolator mount.
 */

#include "np_zone_announce.h"
#include <string.h>

/* ── Q15 sine LUT (256 entries, amplitude ± 32767) ─────────────────────────── */

static const int16_t np_sine_lut[NP_ZA_SINE_LUT_SIZE] = {
       0,   804,  1608,  2410,  3212,  4011,  4808,  5602,
    6393,  7179,  7962,  8739,  9512, 10278, 11039, 11793,
   12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530,
   18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
   23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
   27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
   30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971,
   32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
   32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285,
   32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
   30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683,
   27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
   23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868,
   18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
   12539, 11793, 11039, 10278,  9512,  8739,  7962,  7179,
    6393,  5602,  4808,  4011,  3212,  2410,  1608,   804,
       0,  -804, -1608, -2410, -3212, -4011, -4808, -5602,
   -6393, -7179, -7962, -8739, -9512,-10278,-11039,-11793,
  -12539,-13279,-14010,-14732,-15446,-16151,-16846,-17530,
  -18204,-18868,-19519,-20159,-20787,-21403,-22005,-22594,
  -23170,-23731,-24279,-24811,-25329,-25832,-26319,-26790,
  -27245,-27683,-28105,-28510,-28898,-29268,-29621,-29956,
  -30273,-30571,-30852,-31113,-31356,-31580,-31785,-31971,
  -32137,-32285,-32412,-32521,-32609,-32678,-32728,-32757,
  -32767,-32757,-32728,-32678,-32609,-32521,-32412,-32285,
  -32137,-31971,-31785,-31580,-31356,-31113,-30852,-30571,
  -30273,-29956,-29621,-29268,-28898,-28510,-28105,-27683,
  -27245,-26790,-26319,-25832,-25329,-24811,-24279,-23731,
  -23170,-22594,-22005,-21403,-20787,-20159,-19519,-18868,
  -18204,-17530,-16846,-16151,-15446,-14732,-14010,-13279,
  -12539,-11793,-11039,-10278, -9512, -8739, -7962, -7179,
   -6393, -5602, -4808, -4011, -3212, -2410, -1608,  -804,
};

/* ── Clip definitions ────────────────────────────────────────────────────────── */

/* Segment building macros. */
#define BEEP(f, d)   { .freq_hz = (f), .duration_ms = (d) }
#define SILENCE(d)   { .freq_hz = 0U,  .duration_ms = (d) }

/* Common zone-beep sequence: N beeps at 440 Hz, then confirm at 880 Hz. */

static const np_za_tone_segment_t s_clip_zone1[] = {
    BEEP   (NP_ZA_TONE_BEEP_HZ,    NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_PRE_CONFIRM_GAP_MS),
    BEEP   (NP_ZA_TONE_CONFIRM_HZ, NP_ZA_CONFIRM_DURATION_MS),
};

static const np_za_tone_segment_t s_clip_zone2[] = {
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_PRE_CONFIRM_GAP_MS),
    BEEP   (NP_ZA_TONE_CONFIRM_HZ, NP_ZA_CONFIRM_DURATION_MS),
};

static const np_za_tone_segment_t s_clip_zone3[] = {
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_PRE_CONFIRM_GAP_MS),
    BEEP   (NP_ZA_TONE_CONFIRM_HZ, NP_ZA_CONFIRM_DURATION_MS),
};

static const np_za_tone_segment_t s_clip_zone4[] = {
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_PRE_CONFIRM_GAP_MS),
    BEEP   (NP_ZA_TONE_CONFIRM_HZ, NP_ZA_CONFIRM_DURATION_MS),
};

static const np_za_tone_segment_t s_clip_zone5[] = {
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_BEEP_GAP_MS),
    BEEP   (NP_ZA_TONE_BEEP_HZ, NP_ZA_BEEP_DURATION_MS),
    SILENCE(NP_ZA_PRE_CONFIRM_GAP_MS),
    BEEP   (NP_ZA_TONE_CONFIRM_HZ, NP_ZA_CONFIRM_DURATION_MS),
};

/* Descending two-tone fault cue for unknown zone. */
static const np_za_tone_segment_t s_clip_error[] = {
    BEEP   (NP_ZA_TONE_ERROR_LO_HZ, NP_ZA_ERROR_DURATION_MS),
    SILENCE(NP_ZA_ERROR_GAP_MS),
    BEEP   (NP_ZA_TONE_ERROR_HI_HZ, NP_ZA_ERROR_DURATION_MS),
};

#undef BEEP
#undef SILENCE

/* Lookup table indexed by np_zone_id_t. */
typedef struct {
    const np_za_tone_segment_t *segs;
    uint8_t                     count;
} np_za_clip_def_t;

static const np_za_clip_def_t s_clips[NP_ZONE_UNKNOWN + 1U] = {
    [NP_ZONE_NONE]           = { NULL,          0U },
    [NP_ZONE_FRONTAL_LEFT]   = { s_clip_zone1,  (uint8_t)(sizeof(s_clip_zone1)  / sizeof(s_clip_zone1[0]))  },
    [NP_ZONE_FRONTAL_RIGHT]  = { s_clip_zone2,  (uint8_t)(sizeof(s_clip_zone2)  / sizeof(s_clip_zone2[0]))  },
    [NP_ZONE_VERTEX]         = { s_clip_zone3,  (uint8_t)(sizeof(s_clip_zone3)  / sizeof(s_clip_zone3[0]))  },
    [NP_ZONE_PARIETAL_LEFT]  = { s_clip_zone4,  (uint8_t)(sizeof(s_clip_zone4)  / sizeof(s_clip_zone4[0]))  },
    [NP_ZONE_PARIETAL_RIGHT] = { s_clip_zone5,  (uint8_t)(sizeof(s_clip_zone5)  / sizeof(s_clip_zone5[0]))  },
    [NP_ZONE_UNKNOWN]        = { s_clip_error,  (uint8_t)(sizeof(s_clip_error)  / sizeof(s_clip_error[0]))  },
};

/* ── Module-internal audio state (points into shared np_za_ctx_t) ────────────── */

static np_za_dds_t         *s_dds    = NULL;
static np_za_clip_player_t *s_player = NULL;
static int16_t             *s_dma_buf = NULL;

/* ── Ramp helpers ────────────────────────────────────────────────────────────── */

/* Samples in the 20 ms amplitude ramp. */
#define RAMP_SAMPLES    ((NP_ZA_AUDIO_RAMP_MS * NP_ZA_AUDIO_SAMPLE_RATE_HZ) / 1000U)

/* Amplitude step per sample during ramp (Q15). */
#define RAMP_STEP       ((uint32_t)NP_ZA_AUDIO_AMPLITUDE_Q15 / RAMP_SAMPLES)

/* ── Phase increment calculation ─────────────────────────────────────────────── */

/* Δφ = (freq_hz × 2^32) / Fs.  Result fits in uint32_t for Fs=8000, freq≤4000. */
static uint32_t freq_to_phase_inc(uint16_t freq_hz)
{
    /* Use 64-bit intermediate to avoid overflow. */
    return (uint32_t)(((uint64_t)freq_hz << 32U) / NP_ZA_AUDIO_SAMPLE_RATE_HZ);
}

/* ── Load a clip ─────────────────────────────────────────────────────────────── */

/*
 * Arm the clip player with the segments for the given zone.
 * Starts SAI3 DMA after pre-filling both halves of the ping-pong buffer.
 */
np_za_status_t np_za_audio_play(np_za_ctx_t *ctx, np_zone_id_t zone)
{
    if (!ctx || zone > NP_ZONE_UNKNOWN) {
        return NP_ZA_ERR_INVALID_ARG;
    }

    const np_za_clip_def_t *def = &s_clips[zone];
    if (!def->segs || def->count == 0U) {
        return NP_ZA_ERR_INVALID_ARG;
    }

    s_dds    = &ctx->dds;
    s_player = &ctx->player;
    s_dma_buf = ctx->dma_buf;

    /* Reset DDS. */
    memset(s_dds, 0, sizeof(*s_dds));

    /* Arm player at first segment. */
    s_player->segments = def->segs;
    s_player->seg_count = def->count;
    s_player->seg_idx = 0U;

    const np_za_tone_segment_t *seg0 = &def->segs[0];
    s_player->seg_samples_remaining =
        (uint32_t)seg0->duration_ms * NP_ZA_AUDIO_SAMPLE_RATE_HZ / 1000U;
    s_player->active = true;

    /* Initialize DDS for first segment. */
    s_dds->phase_inc   = freq_to_phase_inc(seg0->freq_hz);
    s_dds->target_amp  = (seg0->freq_hz > 0U) ? NP_ZA_AUDIO_AMPLITUDE_Q15 : 0;
    s_dds->amplitude   = 0;    /* start at zero, ramp up */
    s_dds->ramp_step   = RAMP_STEP;

    if (!ctx->audio_initialized) {
        if (!np_za_platform_sai_init(s_dma_buf, NP_ZA_DMA_BUF_SAMPLES)) {
            return NP_ZA_ERR_AUDIO_INIT;
        }
        ctx->audio_initialized = true;
    }

    /* Pre-fill both halves before starting DMA to avoid initial under-run. */
    np_za_audio_tick(0U);
    np_za_audio_tick(1U);

    np_za_platform_sai_start();
    return NP_ZA_OK;
}

/*
 * Stop playback and silence the bone conduction output.
 */
void np_za_audio_stop(np_za_ctx_t *ctx)
{
    if (s_player) {
        s_player->active = false;
    }
    np_za_platform_sai_stop();
    if (ctx) {
        memset(ctx->dma_buf, 0, sizeof(ctx->dma_buf));
    }
}

/*
 * Returns true while a clip is still playing.
 */
bool np_za_audio_busy(void)
{
    return (s_player && s_player->active);
}

/* ── DMA half-transfer ISR fill ──────────────────────────────────────────────── */

/*
 * Called from eDMA half-complete and transfer-complete ISR.
 * Fills half the ping-pong DMA buffer with synthesized samples.
 *
 * The DMA buffer is 16-bit stereo for SAI3 compatibility; bone conduction
 * is mono — the same sample is written to both L and R channels.
 * We store interleaved stereo as int16_t pairs in dma_buf[].
 *
 * @param half  0 = fill first NP_ZA_DMA_HALF_BUF_SAMPLES frames,
 *              1 = fill second NP_ZA_DMA_HALF_BUF_SAMPLES frames.
 */
void np_za_audio_tick(uint8_t half)
{
    if (!s_player || !s_player->active || !s_dds || !s_dma_buf) {
        return;
    }

    /* Stereo interleave: each frame = 2 × int16_t (L, R). */
    int16_t *buf = s_dma_buf + (half ? NP_ZA_DMA_HALF_BUF_SAMPLES : 0U);
    uint16_t frames = NP_ZA_DMA_HALF_BUF_SAMPLES / 2U;  /* stereo pairs */

    for (uint16_t f = 0U; f < frames; f++) {

        /* ── Ramp amplitude toward target ──────────────────────────────── */
        if (s_dds->amplitude < s_dds->target_amp) {
            s_dds->amplitude += (int16_t)s_dds->ramp_step;
            if (s_dds->amplitude > s_dds->target_amp) {
                s_dds->amplitude = s_dds->target_amp;
            }
        } else if (s_dds->amplitude > s_dds->target_amp) {
            s_dds->amplitude -= (int16_t)s_dds->ramp_step;
            if (s_dds->amplitude < s_dds->target_amp) {
                s_dds->amplitude = s_dds->target_amp;
            }
        }

        /* ── DDS sample ────────────────────────────────────────────────── */
        int16_t raw;
        if (s_dds->phase_inc == 0U) {
            raw = 0;    /* silence segment */
        } else {
            uint8_t lut_idx = (uint8_t)(s_dds->phase_acc >> NP_ZA_SINE_LUT_SHIFT);
            raw = (int16_t)(((int32_t)np_sine_lut[lut_idx] * s_dds->amplitude) >> 15);
            s_dds->phase_acc += s_dds->phase_inc;
        }

        buf[f * 2U]       = raw;  /* L */
        buf[f * 2U + 1U]  = raw;  /* R (same for mono bone conduction)     */

        /* ── Advance segment ────────────────────────────────────────────── */
        if (s_player->seg_samples_remaining > 0U) {
            s_player->seg_samples_remaining--;
        }

        if (s_player->seg_samples_remaining == 0U) {
            uint8_t next = s_player->seg_idx + 1U;

            if (next >= s_player->seg_count) {
                /* Clip complete — stop. */
                s_player->active = false;
                np_za_platform_sai_stop();
                return;
            }

            /* Advance to next segment. */
            s_player->seg_idx = next;
            const np_za_tone_segment_t *seg = &s_player->segments[next];

            s_player->seg_samples_remaining =
                (uint32_t)seg->duration_ms * NP_ZA_AUDIO_SAMPLE_RATE_HZ / 1000U;

            /* Reconfigure DDS for new segment. */
            s_dds->phase_inc  = freq_to_phase_inc(seg->freq_hz);
            s_dds->phase_acc  = 0U;  /* restart phase for clean tone onset    */
            s_dds->target_amp = (seg->freq_hz > 0U) ? NP_ZA_AUDIO_AMPLITUDE_Q15 : 0;
        }
    }
}
