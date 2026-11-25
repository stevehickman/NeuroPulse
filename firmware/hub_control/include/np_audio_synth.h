/*
 * np_audio_synth.h — isochronic tone synthesis: a carrier gated at a pulse rate (NP-HW-AUDIO-001 REQ-AUDIO-29).
 *
 * Pure C, no HAL and no hardware figure.  The codec, its sample rate and the amplifier are undecided
 * (OI-AUDIOHW-14), so the caller states the sample rate; nothing here is a default.  Neither is the pulse shape: the
 * duty cycle and the edge ramp have no source in the documents (OI-AUDIOHW-18), so the caller states both and
 * np_audio_synth_init() refuses a shape it cannot render.  Nothing is clamped to fit.
 */
#ifndef NP_AUDIO_SYNTH_H
#define NP_AUDIO_SYNTH_H

#include <stdint.h>
#include "np_hub_types.h"

typedef struct {
    uint32_t sample_rate_hz;   /* the codec's rate, stated by the caller */
    uint16_t carrier_hz;       /* the tone that is pulsed */
    uint32_t rate_mhz;         /* pulses per second x 1000, as np_mod_audio_params_t.beat_mhz */
    uint16_t duty_permille;    /* the share of each period the pulse is on, 1..1000 */
    uint32_t ramp_us;          /* the rise and fall time of each edge; 0 is a hard edge */
    int16_t  amplitude;        /* the peak sample, 1..32767; the level calibration is the HAL's */
} np_audio_synth_cfg_t;

typedef struct {
    np_audio_synth_cfg_t cfg;
    uint32_t carrier_phase;    /* 2^32 = one carrier cycle */
    uint32_t carrier_inc;
    uint32_t pulse_phase;      /* 2^32 = one pulse period */
    uint32_t pulse_inc;
    float    on_frac;          /* the pulse's on share of the period */
    float    ramp_frac;        /* the ramp's share of the period */
} np_audio_synth_t;

/* Refuses with NP_HUB_ERR_INVALID_ARG, and leaves *s untouched, a configuration it cannot render: a zero sample rate,
 * carrier or pulse rate; a carrier or pulse rate at or above half the sample rate (Nyquist); a duty of 0 or above
 * 1000; an amplitude of 0 or below; a ramp longer than half the on-time (the pulse would never reach full level). */
np_hub_status_t np_audio_synth_init(np_audio_synth_t *s, const np_audio_synth_cfg_t *cfg);

/* Renders n mono samples.  Continuous across calls. */
void np_audio_synth_render(np_audio_synth_t *s, int16_t *out, uint32_t n);

#endif
