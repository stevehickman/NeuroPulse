/*
 * NeuroPulse HRV Biofeedback — Configuration Constants
 * Document: NP-FW-HRV-001 Rev A §3
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 *
 * All HRV analysis follows Task Force of ESC/NASPE (1996) frequency-domain
 * conventions.  EEG band definitions follow AASM/Kubicki (2008).
 */

#ifndef NP_HRV_CONFIG_H
#define NP_HRV_CONFIG_H

/* ── Hardware sample rates ──────────────────────────────────────────────────── */
#define NP_PPG_SAMPLE_RATE_HZ       200U    /* PPG ADC in VNS clip (808–830 nm)  */
#define NP_EEG_SAMPLE_RATE_HZ       500U    /* ADS1299 sample rate               */
#define NP_EEG_CHANNELS             8U      /* Fp1/2, F3/4, C3/4, P3/4          */

/* ── R-R interval processing ───────────────────────────────────────────────── */
#define NP_RR_INTERP_RATE_HZ        4U      /* uniform grid for HRV PSD          */
#define NP_RR_BUFFER_SIZE           256U    /* circular buffer depth (intervals) */
#define NP_RR_INTERP_BUFFER_SIZE    256U    /* interpolated samples for one FFT  */
#define NP_RR_MIN_MS                300U    /* 200 BPM upper HR limit            */
#define NP_RR_MAX_MS                2000U   /* 30 BPM lower HR limit             */
#define NP_RR_RMSSD_WINDOW          20U     /* consecutive intervals for RMSSD   */

/* ── HRV spectral analysis (Welch periodogram) ─────────────────────────────── */
/* 4 Hz / 256 pt = 0.015625 Hz/bin                                              */
#define NP_HRV_FFT_SIZE             256U
#define NP_HRV_WELCH_OVERLAP        128U    /* 50 % overlap                      */

/* LF band: 0.04–0.15 Hz → bins 3–9 */
#define NP_HRV_LF_BIN_LO            3U      /* 0.046875 Hz                       */
#define NP_HRV_LF_BIN_HI            9U      /* 0.140625 Hz                       */

/* HF band: 0.15–0.40 Hz → bins 10–25 */
#define NP_HRV_HF_BIN_LO            10U     /* 0.156250 Hz                       */
#define NP_HRV_HF_BIN_HI            25U     /* 0.390625 Hz                       */

/* Minimum RR intervals required before a coherence result is valid */
#define NP_HRV_MIN_RR_FOR_COHERENCE 64U     /* ~64 s at 60 BPM                   */

/* ── Coherence display ──────────────────────────────────────────────────────── */
#define NP_COHERENCE_SCALE_MAX      10.0f   /* raw ratio [0,1] mapped to [0,10]  */
#define NP_COHERENCE_UPDATE_INTERVAL_S  5U  /* seconds between display refresh   */

/* ── Breathing pacer ────────────────────────────────────────────────────────── */
#define NP_PACER_RATE_MIN_BPM       4.0f    /* breaths per minute (lower limit)  */
#define NP_PACER_RATE_MAX_BPM       7.0f    /* breaths per minute (upper limit)  */
#define NP_PACER_RATE_DEFAULT_BPM   6.0f    /* = 0.1 Hz resonance default        */
#define NP_PACER_SWEEP_STEP_BPM     0.5f    /* step size during RF sweep         */
#define NP_PACER_SWEEP_DWELL_S      120U    /* seconds at each sweep rate        */
#define NP_PACER_INSP_RATIO         0.4f    /* inspiration fraction of cycle     */

/* ── taVNS synchronization ──────────────────────────────────────────────────── */
/* Stim is gated ON during the inspiration phase detected from PPG RSA.          */
#define NP_TAVNS_DEFAULT_FREQ_HZ    25U     /* carrier frequency (within 1–25 Hz)*/
#define NP_TAVNS_DEFAULT_CURRENT_UA 500U    /* 0.5 mA (units: microamperes)      */
#define NP_TAVNS_MAX_CURRENT_UA     2000U   /* 2 mA absolute safety limit        */
#define NP_TAVNS_PULSE_WIDTH_US     250U    /* biphasic pulse half-period        */
#define NP_TAVNS_INSP_SLOPE_WIN     6U      /* RR intervals for slope estimate   */
#define NP_TAVNS_SLOPE_HYSTERESIS   5       /* ms/beat deadband to suppress noise*/
#define NP_TAVNS_MAX_INSP_DURATION_MS 6000U /* failsafe: 6 s max stim gate-on   */

/* ── EEG band power (Welch) ─────────────────────────────────────────────────── */
/* 500 Hz / 1024 pt = 0.4883 Hz/bin                                              */
#define NP_EEG_FFT_SIZE             1024U
#define NP_EEG_WELCH_OVERLAP        512U    /* 50 % overlap                      */

/* Delta  0.5–4 Hz  → bins 1–8   */
#define NP_EEG_DELTA_BIN_LO         1U
#define NP_EEG_DELTA_BIN_HI         8U
/* Theta  4–8 Hz   → bins 9–16  */
#define NP_EEG_THETA_BIN_LO         9U
#define NP_EEG_THETA_BIN_HI         16U
/* Alpha  8–13 Hz  → bins 17–26 */
#define NP_EEG_ALPHA_BIN_LO         17U
#define NP_EEG_ALPHA_BIN_HI         26U
/* Beta  13–30 Hz  → bins 27–61 */
#define NP_EEG_BETA_BIN_LO          27U
#define NP_EEG_BETA_BIN_HI          61U
/* Gamma 30–40 Hz  → bins 62–81 */
#define NP_EEG_GAMMA_BIN_LO         62U
#define NP_EEG_GAMMA_BIN_HI         81U

/* EEG channel input buffer — holds two Welch windows */
#define NP_EEG_CH_BUF_SIZE          (NP_EEG_FFT_SIZE + NP_EEG_WELCH_OVERLAP)

/* ── Closed-loop EEG-adaptive frequency ─────────────────────────────────────── */
/* Target: maximise alpha/theta ratio while maintaining HRV coherence ≥ 5.0.    */
#define NP_ADAPTIVE_ALPHA_THETA_TARGET  1.5f
#define NP_ADAPTIVE_PACER_STEP_BPM      0.25f   /* rate nudge per update cycle  */
#define NP_ADAPTIVE_UPDATE_INTERVAL_S   30U     /* seconds between adaptive step */

/* ── Session parameters ─────────────────────────────────────────────────────── */
#define NP_HRV_SESSION_MIN_S        300U    /* 5 minutes                         */
#define NP_HRV_SESSION_MAX_S        1200U   /* 20 minutes                        */
#define NP_HRV_TREND_SESSIONS       30U     /* sessions kept in trend graph      */

/* ── PPG peak detection ─────────────────────────────────────────────────────── */
#define NP_PPG_PEAK_MIN_SEPARATION_MS  300U /* matches NP_RR_MIN_MS              */
#define NP_PPG_ADAPTIVE_THRESH_ALPHA   0.125f /* IIR smoothing for threshold     */
#define NP_PPG_REFRACTORY_SAMPLES      (NP_PPG_PEAK_MIN_SEPARATION_MS * \
                                        NP_PPG_SAMPLE_RATE_HZ / 1000U)

/* ── SHDR coherence trend ───────────────────────────────────────────────────── */
/* Linear regression slope of session-mean coherence over NP_HRV_TREND_SESSIONS */
/* Written to SHDR after each session (no user biology — slope only).           */
#define NP_SHDR_COHERENCE_TREND_KEY  "hrv_coherence_slope_v1"

#endif /* NP_HRV_CONFIG_H */
