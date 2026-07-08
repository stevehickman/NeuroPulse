/*
 * NeurOne HRV Biofeedback — Session Management
 * Document: NP-FW-HRV-001 Rev A §10
 *
 * Single static session pool (one active session at a time, matching the
 * single-user, single-headset device model).
 *
 * UHDR data produced here is returned via the end_cb's session_record argument.
 * Encryption and eMMC commit are handled by the storage layer above this module.
 * SHDR coherence trend write is also delegated to the caller via the end_cb.
 */

#include "np_hrv_session.h"
#include <string.h>

/* ── Static session context ─────────────────────────────────────────────────── */

struct np_hrv_session {
    bool                    allocated;
    bool                    running;
    np_hrv_session_config_t config;
    uint32_t                start_ms;
    uint32_t                end_ms;
    uint32_t                last_coherence_update_ms;
    uint32_t                last_adaptive_update_ms;

    np_rr_buffer_t          rr_buf;
    np_ppg_detector_t       ppg_det;
    np_hrv_psd_t            psd;
    np_hrv_coherence_result_t coherence;
    np_pacer_state_t        pacer;
    np_tavns_state_t        tavns;
    np_eeg_sample_buf_t     eeg_buf;
    np_eeg_bands_t          eeg_bands;
    np_dual_biofeedback_state_t dual_state;

    /* Running session statistics for UHDR record. */
    float    coherence_sum;
    float    coherence_min;
    float    coherence_max;
    float    rmssd_sum;
    uint32_t coherence_sample_count;

    np_hrv_display_cb_t     display_cb;
    np_hrv_session_end_cb_t end_cb;
};

static struct np_hrv_session s_session_pool;  /* single-instance pool */

/* ── taVNS callbacks (forwarded to safety MCU SPI driver) ───────────────────── */

static void tavns_enable(uint16_t freq_hz, uint16_t current_ua)
{
    /* Platform HAL call: programs VNS pulse generator and requests         */
    /* safety MCU SPI enable.  The response comes back asynchronously via     */
    /* np_hrv_tavns_safety_mcu_response().                                    */
    /* Stub — implemented by the platform HAL layer. */
    (void)freq_hz;
    (void)current_ua;
    /* np_platform_tavns_enable(freq_hz, current_ua); */
}

static void tavns_disable(void)
{
    /* np_platform_tavns_disable(); */
}

/* ── Pacer phase callback ────────────────────────────────────────────────────── */

static void pacer_phase_change(np_pacer_phase_t phase, uint32_t duration_ms)
{
    (void)phase;
    (void)duration_ms;
    /* Notify app and audio subsystem via platform HAL:                        */
    /* np_platform_pacer_phase_notify(phase, duration_ms);                     */
}

/* ── Session lifecycle ───────────────────────────────────────────────────────── */

np_hrv_session_t *np_hrv_session_create(const np_hrv_session_config_t *config,
                                          np_hrv_display_cb_t            display_cb,
                                          np_hrv_session_end_cb_t        end_cb,
                                          uint32_t                        now_ms)
{
    (void)now_ms;
    if (s_session_pool.allocated) {
        return &s_session_pool;  /* return existing context */
    }
    if (!end_cb) {
        return NULL;
    }

    memset(&s_session_pool, 0, sizeof(s_session_pool));
    s_session_pool.allocated  = true;
    s_session_pool.config     = *config;
    s_session_pool.display_cb = display_cb;
    s_session_pool.end_cb     = end_cb;
    s_session_pool.coherence_min = NP_COHERENCE_SCALE_MAX;
    s_session_pool.coherence_max = 0.0f;

    return &s_session_pool;
}

np_hrv_status_t np_hrv_session_start(np_hrv_session_t *sess, uint32_t now_ms)
{
    if (!sess || !sess->allocated) {
        return NP_HRV_ERR_INVALID_ARG;
    }
    if (sess->running) {
        return NP_HRV_ERR_SESSION_ACTIVE;
    }

    sess->running                  = true;
    sess->start_ms                 = now_ms;
    sess->last_coherence_update_ms = now_ms;
    sess->last_adaptive_update_ms  = now_ms;

    /* Initialize PPG peak detector (start threshold 2048 = mid-scale 12-bit). */
    np_hrv_ppg_init(&sess->ppg_det, 2048);

    /* Initialize Welch PSD accumulator. */
    np_hrv_coherence_init(&sess->psd);

    /* Initialize breathing pacer. */
    float init_rate = (sess->config.pacer_rate_bpm > 0.0f)
                       ? sess->config.pacer_rate_bpm : 0.0f;
    np_hrv_pacer_init(&sess->pacer, init_rate, 0.0f, pacer_phase_change);

    /* Initialize taVNS if needed. */
    if (sess->config.protocol == NP_HRV_PROTO_TAVNS_SYNC) {
        np_hrv_tavns_init(&sess->tavns,
                           sess->config.tavns_freq_hz   ? sess->config.tavns_freq_hz
                                                        : NP_TAVNS_DEFAULT_FREQ_HZ,
                           sess->config.tavns_current_ua ? sess->config.tavns_current_ua
                                                         : NP_TAVNS_DEFAULT_CURRENT_UA,
                           tavns_enable, tavns_disable);
    }

    /* Initialize EEG buffer for dual biofeedback. */
    if (sess->config.protocol == NP_HRV_PROTO_EEG_DUAL) {
        np_hrv_eeg_init(&sess->eeg_buf);
    }

    return NP_HRV_OK;
}

void np_hrv_session_push_ppg(np_hrv_session_t *sess,
                               int32_t           sample,
                               uint32_t          timestamp_ms)
{
    if (!sess || !sess->running) {
        return;
    }
    bool new_rr = np_hrv_ppg_process_sample(&sess->ppg_det, &sess->rr_buf,
                                              sample, timestamp_ms);
    if (new_rr && sess->config.protocol == NP_HRV_PROTO_TAVNS_SYNC) {
        uint16_t latest_rr = np_hrv_rr_get(&sess->rr_buf,
                                             (uint16_t)(sess->rr_buf.count - 1U));
        np_hrv_tavns_process_rr(&sess->tavns, latest_rr, timestamp_ms);
    }
}

void np_hrv_session_push_eeg(np_hrv_session_t *sess,
                               const float       samples_uv[NP_EEG_CHANNELS],
                               uint32_t          timestamp_ms)
{
    (void)timestamp_ms;
    if (!sess || !sess->running) {
        return;
    }
    if (sess->config.protocol != NP_HRV_PROTO_EEG_DUAL) {
        return;
    }
    np_hrv_eeg_push_sample(&sess->eeg_buf, samples_uv);
}

/* ── Main session tick ───────────────────────────────────────────────────────── */

void np_hrv_session_tick(np_hrv_session_t *sess, uint32_t now_ms)
{
    if (!sess || !sess->running) {
        return;
    }

    /* Check session duration limit. */
    uint32_t elapsed_s = (now_ms - sess->start_ms) / 1000U;
    if (elapsed_s >= sess->config.duration_s) {
        np_hrv_session_stop(sess, now_ms);
        return;
    }

    /* Pacer tick. */
    np_hrv_pacer_tick(&sess->pacer, now_ms);

    /* taVNS failsafe tick. */
    if (sess->config.protocol == NP_HRV_PROTO_TAVNS_SYNC) {
        np_hrv_tavns_tick(&sess->tavns, now_ms);
    }

    /* Coherence update interval. */
    uint32_t coh_elapsed = now_ms - sess->last_coherence_update_ms;
    if (coh_elapsed >= (uint32_t)NP_COHERENCE_UPDATE_INTERVAL_S * 1000U) {
        sess->last_coherence_update_ms = now_ms;

        if (np_hrv_coherence_update(&sess->rr_buf, &sess->psd) == NP_HRV_OK) {
            np_hrv_coherence_compute(&sess->psd, &sess->rr_buf, &sess->coherence);

            /* Accumulate session statistics. */
            if (sess->coherence.valid) {
                float sc = sess->coherence.coherence_score;
                sess->coherence_sum += sc;
                sess->coherence_sample_count++;
                if (sc < sess->coherence_min) sess->coherence_min = sc;
                if (sc > sess->coherence_max) sess->coherence_max = sc;
                sess->rmssd_sum += sess->coherence.rmssd_ms;

                /* Feed coherence into RF sweep evaluator. */
                np_hrv_pacer_sweep_coherence(&sess->pacer, sc);
            }
        }

        /* EEG band update for dual biofeedback. */
        if (sess->config.protocol == NP_HRV_PROTO_EEG_DUAL) {
            np_hrv_eeg_compute_bands(&sess->eeg_buf, &sess->eeg_bands);
        }

        /* Build combined display state. */
        np_hrv_dual_biofeedback_update(&sess->dual_state, &sess->coherence,
                                        &sess->eeg_bands,
                                        sess->pacer.target_rate_bpm, now_ms);

        if (sess->display_cb) {
            sess->display_cb(&sess->dual_state, sess->pacer.phase);
        }
    }

    /* Adaptive step for EEG dual mode. */
    if (sess->config.protocol == NP_HRV_PROTO_EEG_DUAL) {
        uint32_t adap_elapsed = now_ms - sess->last_adaptive_update_ms;
        if (adap_elapsed >= (uint32_t)NP_ADAPTIVE_UPDATE_INTERVAL_S * 1000U) {
            sess->last_adaptive_update_ms = now_ms;
            float nudge = np_hrv_eeg_adaptive_step(&sess->eeg_bands,
                                                     &sess->coherence,
                                                     sess->pacer.target_rate_bpm);
            if (nudge != 0.0f) {
                np_hrv_pacer_set_rate(&sess->pacer,
                                       sess->pacer.target_rate_bpm + nudge);
            }
        }
    }
}

void np_hrv_session_stop(np_hrv_session_t *sess, uint32_t now_ms)
{
    if (!sess || !sess->running) {
        return;
    }
    sess->running = false;
    sess->end_ms  = now_ms;

    /* Force-close taVNS gate. */
    if (sess->config.protocol == NP_HRV_PROTO_TAVNS_SYNC) {
        np_hrv_tavns_force_disable(&sess->tavns);
    }

    /* Finalise RF sweep if still active. */
    if (sess->pacer.sweep_active) {
        float best_rate;
        np_hrv_pacer_sweep_finalise(&sess->pacer, &best_rate);
        /* Platform should persist best_rate to Config partition. */
    }

    /* Build UHDR session record. */
    np_hrv_session_record_t record;
    memset(&record, 0, sizeof(record));
    record.duration_s        = (uint32_t)((now_ms - sess->start_ms) / 1000U);
    record.protocol          = sess->config.protocol;
    record.target_rate_bpm   = sess->pacer.personalized
                                ? sess->pacer.target_rate_bpm
                                : sess->config.pacer_rate_bpm;
    record.rr_sample_count   = sess->rr_buf.count;
    record.tavns_stim_count  = (uint16_t)(sess->tavns.stim_count < 0xFFFFU
                                           ? sess->tavns.stim_count : 0xFFFFU);
    if (sess->coherence_sample_count > 0U) {
        record.mean_coherence = sess->coherence_sum / (float)sess->coherence_sample_count;
        record.min_coherence  = sess->coherence_min;
        record.max_coherence  = sess->coherence_max;
        record.mean_rmssd_ms  = sess->rmssd_sum / (float)sess->coherence_sample_count;
    }

    sess->end_cb(&record, NP_HRV_OK);
}

void np_hrv_session_destroy(np_hrv_session_t *sess)
{
    if (sess) {
        memset(sess, 0, sizeof(*sess));
    }
}

np_hrv_status_t np_hrv_session_get_coherence(const np_hrv_session_t    *sess,
                                               np_hrv_coherence_result_t *out)
{
    if (!sess || !sess->running) {
        return NP_HRV_ERR_NO_SESSION;
    }
    if (!sess->coherence.valid) {
        return NP_HRV_ERR_NOT_READY;
    }
    *out = sess->coherence;
    return NP_HRV_OK;
}
