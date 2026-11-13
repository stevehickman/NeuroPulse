/*
 * NeurOne Firmware-in-the-Loop Simulator — the HRV session's missing feeders (NP-SIM-FW-001 §32, OI-SIMFW-60)
 * HOST ONLY.
 *
 * np_mod_vns.c creates and starts the HRV biofeedback session (firmware/hrv_biofeedback/) when a VNS command names a protocol, and nothing
 * under firmware/ outside that library ever feeds it: no caller of np_hrv_session_push_ppg, np_hrv_session_push_eeg or np_hrv_session_tick
 * exists (OI-SIMFW-78).  The device's driver says the HRV module "owns its own FreeRTOS task"; the library has none.  So the library's DSP
 * (the peak detector, the coherence estimate, the pacer, the taVNS gate, the dual-biofeedback adaptive step) is reached here only by playing
 * the three feeders it was written for, against the session the DRIVER created:
 *
 *   - the PPG ADC interrupt: a 200 Hz sample stream (NP_PPG_SAMPLE_RATE_HZ), here a synthetic pulse train;
 *   - the EEG stream: a 500 Hz sample vector (NP_EEG_SAMPLE_RATE_HZ), here two sinusoids;
 *   - the HRV task: np_hrv_session_tick at 10 Hz (the header asks for at least 10 Hz).
 *
 * The session handle is the library's own: np_hrv_session_create returns the existing context while one is allocated, so a call with no
 * end callback gets the driver's session, or NULL when there is none.  Nothing in firmware/ is edited (REQ-SIMFW-03).
 *
 * The signals are SYNTHETIC test data, never a person's (REQ-SIMFW-08).  A run is not verification evidence (REQ-SIMFW-06).
 *
 *   hrv_run <ms> <bpm> <rsa_ms> <drift_ms> [<alpha_uv> <theta_uv>]
 *       advance virtual time by <ms>, a millisecond at a time, playing the three feeders.  The heart period is 60000/<bpm> ms, plus a
 *       sinusoid of amplitude <rsa_ms> at 0.1 Hz (the resonance rate: a stand-in for respiratory sinus arrhythmia), plus <drift_ms> per beat
 *       (negative shortens: an inspiration onset for the taVNS gate).  With <alpha_uv> and <theta_uv> an 8-channel EEG at 10 Hz and 6 Hz is
 *       pushed as well.
 *   hrv_state       the library's own coherence result for the running session, and whether there is one
 *   hrv_reset       forget the beat train's phase and its drift (call it when a new session starts)
 *
 * Two commands call the library's own public functions directly, because the session keeps their results to itself (its display callback is
 * NULL in the driver and its context is opaque), so no scenario through the driver could see them:
 *
 *   hrv_pacer <ms> <peak_bpm> [<rate_bpm> <personalised_bpm>]
 *                                  a pacer (given rates default to 0: none) with no personalised rate (so it sweeps), ticked every 100 ms for <ms>, fed a synthetic coherence
 *                                  curve that peaks at <peak_bpm>; reports the breath phases it announced, where the sweep stands, and what
 *                                  np_hrv_pacer_sweep_finalise says
 *   hrv_eeg_bands <alpha_uv> <theta_uv> <hrv_valid 0|1> <score>
 *                                  three seconds of 8-channel synthetic EEG through np_hrv_eeg_compute_bands, then the adaptive step and the
 *                                  dual-biofeedback state with the HRV coherence given
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_hrv_session.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PPG_PERIOD_MS   5U      /* 200 Hz */
#define EEG_PERIOD_MS   2U      /* 500 Hz */
#define TICK_PERIOD_MS  100U    /* 10 Hz: the least np_hrv_session_tick asks for */
#define PPG_BASE        2048.0  /* mid-scale 12-bit, where the session starts its threshold */
#define PPG_PULSE       1500.0  /* the pulse's height above it */
#define PPG_PULSE_MS    40.0    /* the pulse's width (a Gaussian's sigma) */
#define RESONANCE_HZ    0.1
#define HRV_RUN_MAX_MS  1300000UL   /* one session's longest length (NP_HRV_SESSION_MAX_S) and a little more */

static double g_next_beat_ms;
static double g_last_beat_ms;
static double g_beats;
static bool   g_phase_set;

void np_simh_hrv_init(void) { g_phase_set = false; g_beats = 0.0; }

static np_hrv_session_t *session(void)
{
    static const np_hrv_session_config_t none;   /* never read: only an allocated session is returned without an end callback */
    return np_hrv_session_create(&none, NULL, NULL, 0U);
}

/* A period is never shorter than 250 ms: a drift that ran on for long enough would make it zero and the beat loop would not end. */
#define MIN_PERIOD_MS  250.0

static double period_ms(double bpm, double rsa_ms, double drift_ms, double beat_time_ms, double beats)
{
    const double p = 60000.0 / bpm + rsa_ms * sin(2.0 * M_PI * RESONANCE_HZ * beat_time_ms / 1000.0) + drift_ms * beats;
    return p < MIN_PERIOD_MS ? MIN_PERIOD_MS : p;
}

static void emit_state(const char *cmd)
{
    np_hrv_session_t *s = session();
    np_hrv_coherence_result_t c;
    memset(&c, 0, sizeof c);
    const np_hrv_status_t rc = (s != NULL) ? np_hrv_session_get_coherence(s, &c) : NP_HRV_ERR_NO_SESSION;
    np_sim_emit("{\"ev\":\"hrv_state\",\"cmd\":\"%s\",\"t\":%u,\"has_session\":%s,\"coherence_code\":%d,\"valid\":%s,"
                "\"coherence\":%.3f,\"rmssd_ms\":%.2f,\"lf_hf\":%.3f,\"lf_total\":%.2f,\"hf_total\":%.2f}",
                cmd, (unsigned)np_sim_now_ms(), s != NULL ? "true" : "false", (int)rc, (rc == NP_HRV_OK && c.valid) ? "true" : "false",
                (double)c.coherence_score, (double)c.rmssd_ms, (double)c.lf_hf_ratio, (double)c.lf_total_power, (double)c.hf_total_power);
}

static double ppg_sample(double now_ms)
{
    const double d1 = fabs(now_ms - g_last_beat_ms), d2 = fabs(now_ms - g_next_beat_ms);
    const double d = d1 < d2 ? d1 : d2;
    return PPG_BASE + PPG_PULSE * exp(-(d / PPG_PULSE_MS) * (d / PPG_PULSE_MS));
}

static unsigned g_transitions, g_inspires;
static void count_phase(np_pacer_phase_t phase, uint32_t duration_ms)
{
    (void)duration_ms;
    g_transitions++;
    if (phase == NP_PACER_PHASE_INSPIRE) { g_inspires++; }
}

static void pacer_scenario(unsigned long ms, double peak_bpm, double rate_bpm, double personal_bpm)
{
    np_pacer_state_t p;
    g_transitions = 0U;
    g_inspires = 0U;
    np_hrv_pacer_init(&p, (float)rate_bpm, (float)personal_bpm, count_phase);
    const float start_rate = p.target_rate_bpm;
    uint32_t t = 1000U;
    uint32_t next_score = t + (uint32_t)NP_COHERENCE_UPDATE_INTERVAL_S * 1000U;
    for (unsigned long i = 0UL; i < ms / TICK_PERIOD_MS; i++) {
        t += TICK_PERIOD_MS;
        np_hrv_pacer_tick(&p, t);
        if (t >= next_score) {                     /* the session's own cadence: one coherence update per 5 s */
            next_score += (uint32_t)NP_COHERENCE_UPDATE_INTERVAL_S * 1000U;
            const double score = 10.0 - 2.0 * fabs((double)p.sweep_rate_bpm - peak_bpm);
            np_hrv_pacer_sweep_coherence(&p, (float)(score > 0.5 ? score : 0.5));
        }
    }
    float best = 0.0f;
    const np_hrv_status_t fin = np_hrv_pacer_sweep_finalise(&p, &best);
    np_sim_emit("{\"ev\":\"hrv_pacer\",\"start_rate_bpm\":%.2f,\"transitions\":%u,\"inspires\":%u,\"sweep_active\":%s,\"sweep_step\":%u,"
                "\"target_rate_bpm\":%.2f,\"personalized\":%s,\"finalise_code\":%d,\"best_rate_bpm\":%.2f}",
                (double)start_rate, g_transitions, g_inspires, p.sweep_active ? "true" : "false", (unsigned)p.sweep_step_idx,
                (double)p.target_rate_bpm, p.personalized ? "true" : "false", (int)fin, (double)best);
}

static void eeg_scenario(double alpha, double theta, bool hrv_valid, double score)
{
    static np_eeg_sample_buf_t buf;
    np_hrv_eeg_init(&buf);
    for (unsigned n = 0U; n < NP_EEG_CH_BUF_SIZE + 64U; n++) {
        float v[NP_EEG_CHANNELS];
        const double t = (double)n / 500.0;
        for (unsigned ch = 0U; ch < NP_EEG_CHANNELS; ch++) {
            v[ch] = (float)(alpha * sin(2.0 * M_PI * 10.0 * t) + theta * sin(2.0 * M_PI * 6.0 * t));
        }
        np_hrv_eeg_push_sample(&buf, v);
    }
    np_eeg_bands_t bands;
    memset(&bands, 0, sizeof bands);
    const np_hrv_status_t rc = np_hrv_eeg_compute_bands(&buf, &bands);
    np_hrv_coherence_result_t hrv;
    memset(&hrv, 0, sizeof hrv);
    hrv.valid = hrv_valid;
    hrv.coherence_score = (float)score;
    const float nudge = np_hrv_eeg_adaptive_step(&bands, &hrv, NP_PACER_RATE_DEFAULT_BPM);
    np_dual_biofeedback_state_t st;
    memset(&st, 0, sizeof st);
    np_hrv_dual_biofeedback_update(&st, &hrv, &bands, NP_PACER_RATE_DEFAULT_BPM, 1000U);
    np_sim_emit("{\"ev\":\"hrv_eeg_bands\",\"code\":%d,\"bands_valid\":%s,\"alpha\":%.4f,\"theta\":%.4f,\"beta\":%.4f,\"alpha_theta_ratio\":%.3f,"
                "\"nudge_bpm\":%.3f,\"dual_valid\":%s,\"dual_rate_bpm\":%.2f}",
                (int)rc, bands.valid ? "true" : "false", (double)bands.alpha[2], (double)bands.theta[2], (double)bands.beta[2],
                (double)bands.alpha_theta_ratio, (double)nudge, st.eeg.valid ? "true" : "false", (double)st.adaptive_pacer_rate_bpm);
}

bool np_simh_hrv_line(const char *cmd, char *a, char *b, char *c, char *d, char *e)
{
    if (strcmp(cmd, "hrv_pacer") == 0 && a && b) {
        const unsigned long ms = strtoul(a, NULL, 10);
        pacer_scenario(ms > 1300000UL ? 1300000UL : ms, atof(b), c ? atof(c) : 0.0, d ? atof(d) : 0.0);
        return true;
    }
    if (strcmp(cmd, "hrv_eeg_bands") == 0 && a && b && c && d) { eeg_scenario(atof(a), atof(b), atoi(c) != 0, atof(d)); return true; }
    if (strcmp(cmd, "hrv_state") == 0) { emit_state(cmd); return true; }
    if (strcmp(cmd, "hrv_reset") == 0) { np_simh_hrv_init(); return true; }
    if (strcmp(cmd, "hrv_run") != 0) { return false; }
    np_hrv_session_t *s = session();
    if (s == NULL) {
        np_sim_emit("{\"ev\":\"error\",\"what\":\"hrv_run: no HRV session (a vns command with an hrv_proto above 0 starts one)\"}");
        return true;
    }
    const unsigned long ms = a ? strtoul(a, NULL, 10) : 0UL;
    const double bpm = b ? atof(b) : 0.0, rsa = c ? atof(c) : 0.0, drift = d ? atof(d) : 0.0;
    double alpha = 0.0, theta = 0.0;
    const bool eeg = (e != NULL && sscanf(e, "%lf %lf", &alpha, &theta) == 2);
    if (a == NULL || b == NULL || c == NULL || d == NULL || ms == 0UL || ms > HRV_RUN_MAX_MS || bpm < 20.0 || bpm > 250.0 || rsa < 0.0 || rsa > 400.0 ||
        drift < -50.0 || drift > 50.0) {
        np_sim_emit("{\"ev\":\"error\",\"what\":\"hrv_run: needs <ms 1..1300000> <bpm 20..250> <rsa_ms 0..400> <drift_ms -50..50> [<alpha_uv> <theta_uv>]\"}");
        return true;
    }
    if (!g_phase_set) {
        /* The beat train starts on the 5 ms sample grid, a fixed 1 ms off it, whatever the clock's epoch (the host's uptime when it first
         * became virtual): a run's signal must not depend on when the process started. */
        g_last_beat_ms = (double)(np_sim_now_ms() / PPG_PERIOD_MS * PPG_PERIOD_MS) + 1.0;
        g_next_beat_ms = g_last_beat_ms + period_ms(bpm, rsa, drift, 0.0, 0.0);
        g_phase_set = true;
    }
    for (unsigned long i = 0UL; i < ms; i++) {
        np_sim_advance_ms(1U);
        const uint32_t now = np_sim_now_ms();
        while ((double)now >= g_next_beat_ms) {
            g_last_beat_ms = g_next_beat_ms;
            g_beats += 1.0;
            g_next_beat_ms += period_ms(bpm, rsa, drift, g_last_beat_ms, g_beats);
        }
        if (now % PPG_PERIOD_MS == 0U) { np_hrv_session_push_ppg(s, (int32_t)ppg_sample((double)now), now); }
        if (eeg && now % EEG_PERIOD_MS == 0U) {
            float v[NP_EEG_CHANNELS];
            const double t = (double)now / 1000.0;
            for (unsigned ch = 0U; ch < NP_EEG_CHANNELS; ch++) {
                v[ch] = (float)(alpha * sin(2.0 * M_PI * 10.0 * t) + theta * sin(2.0 * M_PI * 6.0 * t));
            }
            np_hrv_session_push_eeg(s, v, now);
        }
        if (now % TICK_PERIOD_MS == 0U) { np_hrv_session_tick(s, now); }
    }
    emit_state(cmd);
    return true;
}
