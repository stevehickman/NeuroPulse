/*
 * NeuroPulse HRV Biofeedback — Breathing Pacer
 * Document: NP-FW-HRV-001 Rev A §7
 */

#include "np_hrv_pacer.h"
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────────────── */

static float clamp_rate(float rate_bpm)
{
    if (rate_bpm < NP_PACER_RATE_MIN_BPM) return NP_PACER_RATE_MIN_BPM;
    if (rate_bpm > NP_PACER_RATE_MAX_BPM) return NP_PACER_RATE_MAX_BPM;
    return rate_bpm;
}

static void update_phase_durations(np_pacer_state_t *pacer)
{
    /* Convert breaths/min to ms per breath, then split by inspiration ratio. */
    float period_ms   = 60000.0f / pacer->target_rate_bpm;
    pacer->inspire_duration_ms = (uint32_t)(period_ms * NP_PACER_INSP_RATIO);
    pacer->expire_duration_ms  = (uint32_t)(period_ms * (1.0f - NP_PACER_INSP_RATIO));
}

/* ── Module-level callback pointer ──────────────────────────────────────────── */
static np_pacer_phase_cb_t s_phase_cb;

/* ── Sweep rate table ────────────────────────────────────────────────────────── */

static uint8_t sweep_steps(void)
{
    float range = NP_PACER_RATE_MAX_BPM - NP_PACER_RATE_MIN_BPM;
    uint8_t n   = (uint8_t)(range / NP_PACER_SWEEP_STEP_BPM) + 1U;
    if (n > 8U) n = 8U;  /* clamp to coherence array size in np_pacer_state_t */
    return n;
}

static float sweep_rate_for_step(uint8_t idx)
{
    return NP_PACER_RATE_MIN_BPM + (float)idx * NP_PACER_SWEEP_STEP_BPM;
}

/* ── Public API ──────────────────────────────────────────────────────────────── */

void np_hrv_pacer_init(np_pacer_state_t  *pacer,
                        float              rate_bpm,
                        float              personalised_rate_bpm,
                        np_pacer_phase_cb_t phase_cb)
{
    memset(pacer, 0, sizeof(*pacer));
    s_phase_cb = phase_cb;

    if (personalised_rate_bpm >= NP_PACER_RATE_MIN_BPM &&
        personalised_rate_bpm <= NP_PACER_RATE_MAX_BPM) {
        pacer->target_rate_bpm = personalised_rate_bpm;
        pacer->personalised    = true;
    } else if (rate_bpm >= NP_PACER_RATE_MIN_BPM &&
               rate_bpm <= NP_PACER_RATE_MAX_BPM) {
        pacer->target_rate_bpm = rate_bpm;
        pacer->personalised    = false;
        /* Start RF sweep on first session where no personalised rate exists. */
        pacer->sweep_active    = true;
        pacer->sweep_step_idx  = 0U;
        pacer->sweep_rate_bpm  = NP_PACER_RATE_MIN_BPM;
        pacer->target_rate_bpm = NP_PACER_RATE_MIN_BPM;
    } else {
        pacer->target_rate_bpm = NP_PACER_RATE_DEFAULT_BPM;
        pacer->personalised    = false;
        pacer->sweep_active    = true;
        pacer->sweep_step_idx  = 0U;
        pacer->sweep_rate_bpm  = NP_PACER_RATE_MIN_BPM;
        pacer->target_rate_bpm = NP_PACER_RATE_MIN_BPM;
    }

    update_phase_durations(pacer);
    pacer->phase = NP_PACER_PHASE_INSPIRE;
}

void np_hrv_pacer_tick(np_pacer_state_t *pacer, uint32_t now_ms)
{
    if (pacer->phase_start_ms == 0U) {
        /* First tick: initialise phase start. */
        pacer->phase_start_ms = now_ms;
        if (s_phase_cb) {
            s_phase_cb(pacer->phase, pacer->inspire_duration_ms);
        }
        return;
    }

    uint32_t current_duration = (pacer->phase == NP_PACER_PHASE_INSPIRE)
                                 ? pacer->inspire_duration_ms
                                 : pacer->expire_duration_ms;

    if ((now_ms - pacer->phase_start_ms) >= current_duration) {
        /* Phase transition. */
        if (pacer->phase == NP_PACER_PHASE_INSPIRE) {
            pacer->phase = NP_PACER_PHASE_EXPIRE;
        } else {
            pacer->phase = NP_PACER_PHASE_INSPIRE;

            /* RF sweep: advance step after NP_PACER_SWEEP_DWELL_S have elapsed. */
            if (pacer->sweep_active) {
                uint32_t dwell_ms = (uint32_t)NP_PACER_SWEEP_DWELL_S * 1000U;
                /* step_idx advances when the breath count at this rate × period ≥ dwell */
                /* Simple implementation: each expiration tick = one breath cycle done.  */
                static uint32_t s_sweep_step_start_ms = 0U;
                if (s_sweep_step_start_ms == 0U) {
                    s_sweep_step_start_ms = now_ms;
                }
                if ((now_ms - s_sweep_step_start_ms) >= dwell_ms) {
                    pacer->sweep_step_idx++;
                    s_sweep_step_start_ms = now_ms;
                    uint8_t steps = sweep_steps();
                    if (pacer->sweep_step_idx < steps) {
                        pacer->sweep_rate_bpm  = sweep_rate_for_step(pacer->sweep_step_idx);
                        pacer->target_rate_bpm = pacer->sweep_rate_bpm;
                        update_phase_durations(pacer);
                    } else {
                        /* Sweep complete — finalisation via np_hrv_pacer_sweep_finalise. */
                        pacer->sweep_active = false;
                        s_sweep_step_start_ms = 0U;
                    }
                }
            }
        }

        pacer->phase_start_ms = now_ms;
        uint32_t next_dur = (pacer->phase == NP_PACER_PHASE_INSPIRE)
                             ? pacer->inspire_duration_ms
                             : pacer->expire_duration_ms;
        if (s_phase_cb) {
            s_phase_cb(pacer->phase, next_dur);
        }
    }
}

void np_hrv_pacer_sweep_coherence(np_pacer_state_t *pacer, float coherence_score)
{
    if (!pacer->sweep_active) {
        return;
    }
    uint8_t idx = pacer->sweep_step_idx;
    if (idx < 8U) {
        /* Running mean: accumulate into slot (simple first-seen assignment    */
        /* works fine — one sample per dwell window is sufficient for RF find).*/
        if (pacer->sweep_coherence[idx] == 0.0f) {
            pacer->sweep_coherence[idx] = coherence_score;
        } else {
            pacer->sweep_coherence[idx] = (pacer->sweep_coherence[idx] + coherence_score) * 0.5f;
        }
    }
}

np_hrv_status_t np_hrv_pacer_sweep_finalise(np_pacer_state_t *pacer,
                                              float            *best_rate_bpm_out)
{
    if (pacer->sweep_active) {
        return NP_HRV_ERR_NOT_READY;
    }

    uint8_t steps    = sweep_steps();
    uint8_t best_idx = 0U;
    float   best_coh = pacer->sweep_coherence[0];

    for (uint8_t i = 1U; i < steps; i++) {
        if (pacer->sweep_coherence[i] > best_coh) {
            best_coh = pacer->sweep_coherence[i];
            best_idx = i;
        }
    }

    float best_rate          = sweep_rate_for_step(best_idx);
    pacer->target_rate_bpm   = best_rate;
    pacer->personalised      = true;
    *best_rate_bpm_out        = best_rate;
    update_phase_durations(pacer);

    return NP_HRV_OK;
}

void np_hrv_pacer_set_rate(np_pacer_state_t *pacer, float rate_bpm)
{
    pacer->target_rate_bpm = clamp_rate(rate_bpm);
    update_phase_durations(pacer);
}
