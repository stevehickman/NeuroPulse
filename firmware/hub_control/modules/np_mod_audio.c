/*
 * NeurOne Hub Control — Audio Driver
 * Document: NP-FW-HUB-001 Rev A §8.5
 *
 * Controls:
 *   - Over-ear planar magnetic driver (binaural beats, isochronic tones, noise)
 *   - Mastoid bone conduction piezoelectric (breathing cue, sync audio)
 * Both driven from the hub's I2S audio codec via SAI peripheral.
 *
 * EEG-adaptive mode: when eeg_adaptive=1, the carrier frequency for binaural
 * beats is adjusted every NP_AUDIO_ADAPT_INTERVAL_MS to track the dominant EEG
 * band.  The EEG module posts its dominant band via np_mod_audio_set_eeg_band().
 *
 * HAL stubs:
 *   OI-AUDIO-01: np_mod_audio_hal_codec_init()
 *   OI-AUDIO-02: np_mod_audio_hal_set_binaural(carrier_hz, beat_mhz, vol_pct)
 *   OI-AUDIO-03: np_mod_audio_hal_set_isochronic(freq_hz, vol_pct)
 *   OI-AUDIO-04: np_mod_audio_hal_set_noise(type, vol_pct)  — 0=pink, 1=brown
 *   OI-AUDIO-05: np_mod_audio_hal_stop_planar()
 *   OI-AUDIO-06: np_mod_audio_hal_bone_set(freq_hz, vol_pct) — bone conduction
 *   OI-AUDIO-07: np_mod_audio_hal_bone_stop()
 *   OI-AUDIO-08: np_mod_audio_hal_mesh_impedance() → float ohm (fouling detect)
 */

#include "np_hub_types.h"
#include "np_module_registry.h"
#include <string.h>

/* EEG band → suggested binaural beat frequency mapping (mHz). */
static const uint16_t k_band_beat_mhz[5] = {
    2000U,   /* delta: 2Hz */
    6000U,   /* theta: 6Hz */
    10000U,  /* alpha: 10Hz */
    20000U,  /* beta: 20Hz */
    40000U,  /* gamma: 40Hz */
};

extern np_hub_status_t np_mod_audio_hal_codec_init(void);
extern void np_mod_audio_hal_set_binaural(uint16_t carrier_hz,
                                           uint16_t beat_mhz, uint8_t vol_pct);
extern void np_mod_audio_hal_set_isochronic(uint16_t freq_hz, uint8_t vol_pct);
extern void np_mod_audio_hal_set_noise(uint8_t type, uint8_t vol_pct);
extern void np_mod_audio_hal_stop_planar(void);
extern void np_mod_audio_hal_bone_set(uint16_t freq_hz, uint8_t vol_pct);
extern void np_mod_audio_hal_bone_stop(void);
extern float np_mod_audio_hal_mesh_impedance(void);

/* ── State ───────────────────────────────────────────────────────────────────── */

typedef struct {
    uint8_t  mode;
    uint16_t carrier_hz;
    uint16_t beat_mhz;
    uint8_t  vol_pct;
    bool     bone_on;
    bool     eeg_adaptive;
    bool     active;
} np_mod_audio_state_t;

static np_mod_audio_state_t s_state;

/* ── EEG band update (called from EEG telemetry path) ───────────────────────── */

void np_mod_audio_set_eeg_band(uint8_t band)
{
    if (!s_state.active || !s_state.eeg_adaptive) { return; }
    if (band >= 5U) { return; }
    s_state.beat_mhz = k_band_beat_mhz[band];
    np_mod_audio_hal_set_binaural(s_state.carrier_hz, s_state.beat_mhz,
                                   s_state.vol_pct);
}

/* ── Detect ──────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_audio_detect(uint8_t slot, np_hub_mod_type_t *type_out)
{
    (void)slot;
    /* Audio codec is fixed silicon — always present. */
    *type_out = NP_MOD_AUDIO;
    return NP_HUB_OK;
}

/* ── Init ────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_audio_init(uint8_t slot)
{
    (void)slot;
    memset(&s_state, 0, sizeof(s_state));
    return np_mod_audio_hal_codec_init();
}

/* ── Control ─────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_audio_control(uint8_t slot, const void *params, uint16_t len)
{
    (void)slot;

    /* Stop */
    if (params == NULL || len == 0U) {
        np_mod_audio_hal_stop_planar();
        if (s_state.bone_on) {
            np_mod_audio_hal_bone_stop();
            s_state.bone_on = false;
        }
        s_state.active = false;
        return NP_HUB_OK;
    }

    if (len < sizeof(np_mod_audio_params_t)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    const np_mod_audio_params_t *p = (const np_mod_audio_params_t *)params;

    s_state.mode         = p->mode;
    s_state.carrier_hz   = p->carrier_hz;
    s_state.beat_mhz     = p->beat_mhz;
    s_state.vol_pct      = p->volume_pct;
    s_state.eeg_adaptive = (p->eeg_adaptive != 0U);

    switch (p->mode) {
    case 0: /* binaural */
        np_mod_audio_hal_set_binaural(p->carrier_hz, p->beat_mhz, p->volume_pct);
        break;
    case 1: /* isochronic */
        np_mod_audio_hal_set_isochronic(p->carrier_hz, p->volume_pct);
        break;
    case 2: /* pink noise */
        np_mod_audio_hal_set_noise(0U, p->volume_pct);
        break;
    case 3: /* brown noise */
        np_mod_audio_hal_set_noise(1U, p->volume_pct);
        break;
    default:
        np_mod_audio_hal_stop_planar();
        break;
    }

    if (p->bone_conduct_en) {
        /* Bone conduction at carrier frequency for breathing pacer / sync cue */
        np_mod_audio_hal_bone_set(p->carrier_hz, p->volume_pct);
        s_state.bone_on = true;
    } else if (s_state.bone_on) {
        np_mod_audio_hal_bone_stop();
        s_state.bone_on = false;
    }

    s_state.active = true;
    return NP_HUB_OK;
}

/* ── Telemetry ───────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_audio_telemetry(uint8_t slot, np_telem_record_t *out)
{
    (void)slot;
    if (out == NULL) { return NP_HUB_ERR_INVALID_ARG; }
    /* Audio telemetry: mesh impedance (fouling/RF shielding) written to SHDR
     * by the session log in response to an out-of-range impedance reading.
     * No user-biology content in audio telemetry. */
    float mesh_ohm = np_mod_audio_hal_mesh_impedance();
    (void)mesh_ohm; /* Threshold comparison and SHDR log handled by session_log. */

    /* We return OK with no data — audio has no UHDR per-tick record. */
    out->mod_type = NP_MOD_AUDIO;
    out->slot     = NP_HUB_SLOT_AUDIO;
    return NP_HUB_OK;
}

/* ── Shutdown ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_mod_audio_shutdown(uint8_t slot)
{
    return np_mod_audio_control(slot, NULL, 0U);
}
