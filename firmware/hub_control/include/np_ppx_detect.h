/*
 * NeurOne Hub Control Program — the photoparoxysmal detector (OI-FWHUB-29 option C, REQ-FWHUB-67 / -69)
 *
 * WHAT IT DETECTS (the principal's ruling, 2026-11-05): the first appearance of a GENERALISED epileptiform discharge during
 * photic stimulation, whether or not it would end by itself; occipital-only discharge only when the clinician's switch says so.
 * Engineering has turned that into something a program can compute, and every number in it is a PARAMETER, not a finding:
 *
 *   - a SPIKE on a channel is a sample-to-sample step larger than `spike_k` times that channel's own running background of
 *     steps (the background is not updated by a spike, so a burst does not hide itself);
 *   - a channel is INVOLVED while it has had at least `min_spikes` spikes in the last `window_ms`;
 *   - the discharge is GENERALISED when at least `min_other_channels` channels that are not occipital are involved at once,
 *     and OCCIPITAL-ONLY when an occipital channel is involved and that is not so.
 *
 * It looks at the spike component only. It does not look for the slow wave of a spike-and-wave complex, so a sharp artefact
 * on many channels (a movement, a loose lead) is read as a discharge. That errs toward stopping, which is the ruling for an
 * automated halt, and what it costs in false halts is what the validation of REQ-FWHUB-70 has to measure.
 *
 * WHEN IT CANNOT SEE (REQ-FWHUB-69): a channel is not trusted while it is warming up, flat, saturated or noisier than a
 * ceiling. The detector sees only when at least one occipital channel and at least `min_other_channels` other channels are
 * trusted. With no channel roles set it sees nothing. Whoever drives the goggles photically asks np_ppx_seeing() first and
 * refuses to start when it is false, and a drive already running is halted when it turns false.
 *
 * NOTHING HERE IS VALIDATED. The parameter set the hub ships is marked PROVISIONAL (docs/status/unjustified-choices.md) and
 * the evidence level in force is NONE (np_ppx_policy.h), so the detector is never relied on and the refusal of REQ-FWHUB-65 /
 * -66 stands. The synthetic-signal tests are the harness that validation would use; they are not the validation.
 */
#ifndef NP_PPX_DETECT_H
#define NP_PPX_DETECT_H

#include <stdbool.h>
#include <stdint.h>

#define NP_PPX_MAX_CH   8U

typedef struct {
    float    fs_hz;               /* sample rate; the ADS1299 runs at 500 Hz                                              */
    float    spike_k;             /* a step this many times the channel's background is a spike                            */
    float    bg_tau_ms;           /* time constant of the background of steps                                              */
    uint16_t refractory_ms;       /* after a spike the same channel cannot spike again for this long                       */
    uint8_t  min_spikes;          /* spikes in the window that make a channel involved                                     */
    uint16_t window_ms;           /* the window an involvement lasts                                                       */
    uint8_t  min_other_channels;  /* non-occipital channels involved at once that make a discharge generalised             */
    uint16_t quality_ms;          /* warm-up, and the time constant of a channel's level and spread                        */
    float    flat_floor_uv;       /* a channel whose spread (rms about its mean) is below this is flat: not trusted         */
    float    noise_ceiling_uv;    /* ... above this is noise: not trusted                                                  */
    float    sat_ceiling_uv;      /* a sample at or beyond this is saturated: the channel is not trusted for quality_ms     */
} np_ppx_params_t;

typedef struct {
    uint8_t occipital_mask;       /* bit n set: channel n is an occipital electrode                                        */
    uint8_t other_mask;           /* bit n set: channel n is some other electrode, used to judge a discharge generalised   */
} np_ppx_roles_t;

typedef enum {
    NP_PPX_CLEAR                 = 0,
    NP_PPX_BLIND                 = 1,   /* the detector cannot see: not an answer, an absence of one                      */
    NP_PPX_DISCHARGE_OCCIPITAL   = 2,
    NP_PPX_DISCHARGE_GENERALISED = 3
} np_ppx_verdict_t;

/* The halt hook. Called from np_ppx_feed() (task context) when the verdict becomes BLIND after it was seeing, or a discharge
 * is found that the clinician's switch says to stop on. The receiver decides whether anything photic is being driven. */
typedef void (*np_ppx_halt_fn)(np_ppx_verdict_t why);

/*
 * SENSITIVITY (REQ-FWHUB-68). Level 0 is the parameter set as validated, and there is NO level below it: a clinician can make
 * the detector more sensitive and never less, because the floor of what the validation supports is the validated setting
 * itself. Each level above 0 lowers the spike threshold (spike_k) by a fixed factor, so it halts on smaller steps: more sessions
 * stopped for harmless signals, never fewer for real ones. The factors are PROVISIONAL (UC-123).
 *
 * np_ppx_set_sensitivity() only ever RAISES the level. A detector already watching is not made to look less: lowering takes
 * effect at the next session, when np_ppx_init() starts again from level 0 and the session's start sets the configured level.
 */
#define NP_PPX_SENS_MAX   3U

/* The parameter set the hub ships: PROVISIONAL, evidence NONE. */
void np_ppx_params_provisional(np_ppx_params_t *out);

/* Start (a session starts): clears every channel, the latch and the verdict. */
void np_ppx_init(const np_ppx_params_t *params, np_ppx_roles_t roles, bool stop_on_occipital_only);
void np_ppx_set_halt_fn(np_ppx_halt_fn fn);
void np_ppx_set_stop_on_occipital_only(bool on);
void    np_ppx_set_sensitivity(uint8_t level);   /* raises at once; never lowers (see above); clamped to NP_PPX_SENS_MAX */
uint8_t np_ppx_sensitivity(void);

/* Samples in microvolts, interleaved: sample s of channel c is uv[s * n_ch + c]. n_ch is at most NP_PPX_MAX_CH. */
void np_ppx_feed(const float *uv, uint16_t n_samples, uint8_t n_ch);

bool             np_ppx_seeing(void);
np_ppx_verdict_t np_ppx_verdict(void);       /* the latched discharge if there is one, else BLIND or CLEAR */
bool             np_ppx_channel_trusted(uint8_t ch);

#endif /* NP_PPX_DETECT_H */
