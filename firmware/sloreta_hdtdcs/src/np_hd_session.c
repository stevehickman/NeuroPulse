/*
 * NeuroPulse sLORETA-guided HD-tDCS — Session Orchestration
 * Document: NP-FW-HD-001 Rev A §8
 *
 * Drives the 5-stage workflow:
 *   EEG_ACQUIRE → SLORETA → MONTAGE → STIMULATION → COMPLETE
 *
 * UHDR and SHDR data routing follows NP-FW-EMMC-001 Rev A §12 classification.
 * UHDR data (user biology) is returned to caller via end_cb for encrypted write.
 * SHDR data (device metrics) is returned via shdr field for fleet telemetry.
 */

#include "np_hd_session.h"
#include <string.h>

/* ── Source power buffer (caller allocates in LPSDR4 or SRAM) ───────────────── */
/* 2447 voxels × float32 = 9.6 KB — placed in LPSDR4 (32 MB available).        */
/* For linker purposes, allocated as a file-scope array in LPSDR4 section.      */
static float s_source_power[NP_HD_SLORETA_N_VOXELS] __attribute__((section(".lpsdr4")));

/* ── Internal context ────────────────────────────────────────────────────────── */

struct np_hd_session {
    bool                     allocated;
    np_hd_stage_t            stage;
    np_hd_session_config_t   config;
    uint32_t                 stage_start_ms;

    /* sLORETA subsystem. */
    np_sloreta_ctx_t         sloreta;
    np_hd_sloreta_result_t   sloreta_result;

    /* Montage and stimulation. */
    np_hd_montage_t          montage;
    np_hd_stim_ctx_t         stim;

    /* Session record for UHDR (user biology — never transmitted by NeuroPulse). */
    np_hd_session_record_t   record;

    /* SHDR summary (device metrics — no user biology). */
    np_hd_shdr_summary_t     shdr;

    /* Display state. */
    np_hd_display_state_t    display;

    /* Callbacks. */
    np_hd_display_cb_t       display_cb;
    np_hd_session_end_cb_t   end_cb;
};

static struct np_hd_session s_session_pool;

/* ── Safety MCU response forwarding ─────────────────────────────────────────── */

static void stim_safety_cb(np_hd_stim_ctx_t *stim_ctx,
                             bool              granted,
                             const float       impedance_kohm[])
{
    (void)stim_ctx;
    /* Forward to session — session holds both stim and sloreta contexts.        */
    np_hd_session_safety_mcu_response(&s_session_pool, granted, impedance_kohm);
}

/* ── Platform HAL stubs ──────────────────────────────────────────────────────── */

static void platform_ads1299_start(void)
{
    /* OI-HD-01: configure ADS1299 for 21-ch acquisition at 500 Hz,              */
    /* enable DMA, route EEG data to np_hd_session_push_eeg().                  */
    /* np_platform_ads1299_start(); */
}

static void platform_ads1299_stop(void)
{
    /* np_platform_ads1299_stop(); */
}

static uint32_t platform_now_ms(void)
{
    /* OI-HD-02: return FreeRTOS tick count in ms.                               */
    /* return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);              */
    return 0U;
}

/* ── Lifecycle ───────────────────────────────────────────────────────────────── */

np_hd_session_t *np_hd_session_create(const float               *weight_matrix,
                                        const np_hd_mni_t         *voxel_mni,
                                        const np_hd_session_config_t *config,
                                        np_hd_display_cb_t         display_cb,
                                        np_hd_session_end_cb_t     end_cb)
{
    if (!end_cb || !weight_matrix || !voxel_mni || !config) {
        return NULL;
    }
    if (s_session_pool.allocated) {
        return &s_session_pool;
    }

    memset(&s_session_pool, 0, sizeof(s_session_pool));
    s_session_pool.allocated  = true;
    s_session_pool.stage      = NP_HD_STAGE_IDLE;
    s_session_pool.config     = *config;
    s_session_pool.display_cb = display_cb;
    s_session_pool.end_cb     = end_cb;

    np_sloreta_init(&s_session_pool.sloreta, weight_matrix, voxel_mni,
                    NP_HD_SLORETA_N_VOXELS);

    return &s_session_pool;
}

void np_hd_session_destroy(np_hd_session_t *sess)
{
    if (sess) {
        platform_ads1299_stop();
        np_hd_stim_deinit(&sess->stim);
        memset(sess, 0, sizeof(*sess));
    }
}

/* ── Stage 1: EEG acquisition ────────────────────────────────────────────────── */

np_hd_status_t np_hd_session_start_eeg(np_hd_session_t *sess, uint32_t now_ms)
{
    if (!sess || !sess->allocated) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (sess->stage != NP_HD_STAGE_IDLE) {
        return NP_HD_ERR_SESSION_ACTIVE;
    }

    np_sloreta_reset(&sess->sloreta);
    sess->stage          = NP_HD_STAGE_EEG_ACQUIRE;
    sess->stage_start_ms = now_ms;
    sess->record.session_start_unix = now_ms / 1000U;  /* placeholder: use RTC */

    platform_ads1299_start();

    sess->display.stage = NP_HD_STAGE_EEG_ACQUIRE;
    if (sess->display_cb) {
        sess->display_cb(&sess->display);
    }

    return NP_HD_OK;
}

void np_hd_session_push_eeg(np_hd_session_t *sess,
                              const float      samples[][NP_HD_SLORETA_FFT_SIZE],
                              uint16_t         n_samples,
                              uint32_t         timestamp_ms)
{
    (void)timestamp_ms;
    if (!sess || sess->stage != NP_HD_STAGE_EEG_ACQUIRE) {
        return;
    }
    np_sloreta_push_epoch(&sess->sloreta, samples, n_samples);
}

/* ── Stage 2: sLORETA computation ───────────────────────────────────────────── */

np_hd_status_t np_hd_session_compute_sloreta(np_hd_session_t *sess, uint32_t now_ms)
{
    if (!sess || !sess->allocated) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (np_sloreta_epoch_count(&sess->sloreta) < NP_HD_SLORETA_EPOCHS) {
        return NP_HD_ERR_NOT_READY;
    }

    sess->stage          = NP_HD_STAGE_SLORETA;
    sess->stage_start_ms = now_ms;

    platform_ads1299_stop();

    np_hd_status_t ret = np_sloreta_compute_map(&sess->sloreta,
                                                  s_source_power,
                                                  NP_HD_SLORETA_N_VOXELS);
    if (ret != NP_HD_OK) {
        np_hd_session_abort(sess, ret, now_ms);
        return ret;
    }

    ret = np_sloreta_find_peak(&sess->sloreta, s_source_power,
                                NP_HD_SLORETA_N_VOXELS, &sess->sloreta_result);
    if (ret != NP_HD_OK) {
        np_hd_session_abort(sess, ret, now_ms);
        return ret;
    }

    /* Store sLORETA peak in UHDR record. */
    sess->record.peak_source_power = sess->sloreta_result.peak_source_power;
    sess->record.target_mni_x      = sess->sloreta_result.peak_mni.x;
    sess->record.target_mni_y      = sess->sloreta_result.peak_mni.y;
    sess->record.target_mni_z      = sess->sloreta_result.peak_mni.z;
    sess->record.eeg_duration_s    = (uint32_t)((now_ms - sess->stage_start_ms)
                                                  / 1000U + NP_HD_RESTING_DURATION_S);

    sess->stage = NP_HD_STAGE_MONTAGE;
    sess->display.stage   = NP_HD_STAGE_SLORETA;
    sess->display.sloreta = sess->sloreta_result;
    if (sess->display_cb) {
        sess->display_cb(&sess->display);
    }

    return NP_HD_OK;
}

np_hd_status_t np_hd_session_get_sloreta_result(const np_hd_session_t  *sess,
                                                  np_hd_sloreta_result_t *out)
{
    if (!sess || !out) {
        return NP_HD_ERR_INVALID_ARG;
    }
    if (!sess->sloreta_result.valid) {
        return NP_HD_ERR_NOT_READY;
    }
    *out = sess->sloreta_result;
    return NP_HD_OK;
}

/* ── Stage 3: Montage selection ──────────────────────────────────────────────── */

np_hd_status_t np_hd_session_select_montage(np_hd_session_t *sess, uint32_t now_ms)
{
    if (!sess || sess->stage != NP_HD_STAGE_MONTAGE) {
        return NP_HD_ERR_INVALID_ARG;
    }

    np_hd_mni_t target;
    if (sess->config.target == NP_HD_TARGET_CUSTOM) {
        target = sess->config.custom_target_mni;
    } else {
        np_hd_status_t ret = np_hd_clinical_target_mni(sess->config.target, &target);
        if (ret != NP_HD_OK) {
            np_hd_session_abort(sess, ret, now_ms);
            return ret;
        }
    }

    np_hd_status_t ret = np_hd_montage_from_mni(&target,
                                                   sess->config.montage_type,
                                                   &sess->montage);
    if (ret != NP_HD_OK) {
        np_hd_session_abort(sess, ret, now_ms);
        return ret;
    }

    sess->montage.anode_current_ua = sess->config.stim_current_ua
                                     ? sess->config.stim_current_ua
                                     : NP_HD_DEFAULT_CURRENT_UA;

    /* Populate UHDR record with montage details. */
    sess->record.montage_type      = (uint8_t)sess->montage.type;
    sess->record.center_electrode  = (uint8_t)sess->montage.center;
    sess->record.target_mni_x      = target.x;
    sess->record.target_mni_y      = target.y;
    sess->record.target_mni_z      = target.z;
    sess->record.anode_current_ua  = sess->montage.anode_current_ua;
    for (uint8_t k = 0U; k < NP_HD_RING_CATHODE_COUNT; k++) {
        sess->record.cathode_electrodes[k] = (uint8_t)sess->montage.cathodes[k];
    }

    /* SHDR montage summary (no user biology — electrode position is device metric). */
    sess->shdr.montage_type     = (uint8_t)sess->montage.type;
    sess->shdr.center_electrode = (uint8_t)sess->montage.center;
    sess->shdr.anode_current_ua = sess->montage.anode_current_ua;

    sess->display.montage = sess->montage;
    sess->display.stage   = NP_HD_STAGE_MONTAGE;
    if (sess->display_cb) {
        sess->display_cb(&sess->display);
    }

    return NP_HD_OK;
}

np_hd_status_t np_hd_session_override_target(np_hd_session_t   *sess,
                                               const np_hd_mni_t *target_mni)
{
    if (!sess || !target_mni || sess->stage != NP_HD_STAGE_MONTAGE) {
        return NP_HD_ERR_INVALID_ARG;
    }
    sess->config.target            = NP_HD_TARGET_CUSTOM;
    sess->config.custom_target_mni = *target_mni;
    return NP_HD_OK;
}

np_hd_status_t np_hd_session_check_impedance(np_hd_session_t *sess, uint32_t now_ms)
{
    (void)now_ms;
    if (!sess || sess->stage != NP_HD_STAGE_MONTAGE) {
        return NP_HD_ERR_INVALID_ARG;
    }

    np_hd_status_t ret = np_hd_stim_init(&sess->stim, &sess->montage, stim_safety_cb);
    if (ret != NP_HD_OK) {
        return ret;
    }

    return np_hd_stim_request_impedance_check(&sess->stim);
}

/* ── Stage 4: Stimulation ────────────────────────────────────────────────────── */

np_hd_status_t np_hd_session_start_stim(np_hd_session_t *sess, uint32_t now_ms)
{
    if (!sess || sess->stage != NP_HD_STAGE_MONTAGE) {
        return NP_HD_ERR_INVALID_ARG;
    }

    uint16_t current_ua = sess->montage.anode_current_ua;
    uint16_t duration_s = sess->config.stim_duration_s
                          ? sess->config.stim_duration_s : NP_HD_MIN_SESSION_S;

    np_hd_status_t ret = np_hd_stim_start(&sess->stim, current_ua, duration_s, now_ms);
    if (ret != NP_HD_OK) {
        return ret;
    }

    sess->stage          = NP_HD_STAGE_STIMULATION;
    sess->stage_start_ms = now_ms;
    sess->display.stage  = NP_HD_STAGE_STIMULATION;
    return NP_HD_OK;
}

void np_hd_session_safety_mcu_response(np_hd_session_t *sess,
                                        bool             granted,
                                        const float      impedance_kohm[])
{
    if (!sess) {
        return;
    }
    np_hd_stim_safety_mcu_response(&sess->stim, granted, impedance_kohm);

    if (impedance_kohm) {
        /* Record impedance in UHDR and SHDR. */
        float mean = 0.0f;
        for (uint8_t i = 0U; i < NP_HD_RING_ELECTRODE_COUNT; i++) {
            mean += impedance_kohm[i];
        }
        mean /= (float)NP_HD_RING_ELECTRODE_COUNT;
        sess->record.mean_impedance_kohm = mean;
        sess->shdr.mean_impedance_kohm   = mean;
        sess->shdr.impedance_check_pass  = granted ? 1U : 0U;
        sess->display.mean_impedance_kohm = mean;
    }
}

/* ── Main tick ───────────────────────────────────────────────────────────────── */

void np_hd_session_tick(np_hd_session_t *sess, uint32_t now_ms)
{
    if (!sess || !sess->allocated) {
        return;
    }

    if (sess->stage == NP_HD_STAGE_EEG_ACQUIRE) {
        /* Auto-advance after resting-state duration. */
        uint32_t elapsed = now_ms - sess->stage_start_ms;
        if (elapsed >= (uint32_t)NP_HD_RESTING_DURATION_S * 1000U) {
            if (np_sloreta_epoch_count(&sess->sloreta) >= NP_HD_SLORETA_EPOCHS) {
                np_hd_session_compute_sloreta(sess, now_ms);
            }
        }
        return;
    }

    if (sess->stage == NP_HD_STAGE_STIMULATION) {
        np_hd_stim_tick(&sess->stim, now_ms);

        np_hd_stim_phase_t sp = np_hd_stim_phase(&sess->stim);

        /* Update display state. */
        uint32_t ramp_ms  = (uint32_t)NP_HD_RAMP_DURATION_S * 1000U;
        uint32_t total_ms = (uint32_t)sess->config.stim_duration_s * 1000U;
        uint32_t elapsed  = now_ms - sess->stage_start_ms;
        float progress    = (total_ms > 0U)
                            ? (float)elapsed / (float)total_ms * 100.0f : 0.0f;

        sess->display.stim_phase        = sp;
        sess->display.stim_progress_pct = (progress > 100.0f) ? 100.0f : progress;
        sess->display.anode_current_ua  = (float)sess->stim.elec[0].actual_current_ua;

        if (sess->display_cb) {
            sess->display_cb(&sess->display);
        }

        if (sp == NP_HD_STIM_DONE) {
            /* Stimulation complete: build UHDR record and call end_cb. */
            sess->record.stim_duration_s =
                (uint32_t)((now_ms - sess->stage_start_ms) / 1000U);
            sess->record.total_charge_ua_s =
                np_hd_stim_anode_charge_ua_s(&sess->stim);
            sess->record.abort_reason = 0U;

            sess->shdr.stim_duration_s = sess->record.stim_duration_s;
            sess->shdr.abort_reason    = 0U;

            sess->stage = NP_HD_STAGE_COMPLETE;
            if (sess->end_cb) {
                sess->end_cb(&sess->record, NP_HD_OK);
            }
            sess->allocated = false;

        } else if (sp == NP_HD_STIM_FAULT) {
            np_hd_session_abort(sess, NP_HD_ERR_SAFETY_REJECTED, now_ms);
        }

        /* Safety timeout: abort if stimulation stalls for > 5 s. */
        if (sp == NP_HD_STIM_RAMP_UP && elapsed < ramp_ms &&
            !sess->stim.safety_mcu_granted &&
            elapsed > 5000U) {
            np_hd_session_abort(sess, NP_HD_ERR_SAFETY_REJECTED, now_ms);
        }
    }
}

/* ── Abort ───────────────────────────────────────────────────────────────────── */

void np_hd_session_abort(np_hd_session_t *sess,
                           np_hd_status_t   reason,
                           uint32_t         now_ms)
{
    if (!sess || !sess->allocated) {
        return;
    }
    np_hd_stim_stop(&sess->stim);
    platform_ads1299_stop();

    sess->record.abort_reason     = (uint8_t)(-(int8_t)reason);
    sess->record.stim_duration_s  =
        (uint32_t)((now_ms - sess->stage_start_ms) / 1000U);
    sess->shdr.abort_reason = sess->record.abort_reason;

    sess->stage = NP_HD_STAGE_ABORTED;
    if (sess->end_cb) {
        sess->end_cb(&sess->record, reason);
    }
    sess->allocated = false;
}

/* ── Queries ─────────────────────────────────────────────────────────────────── */

np_hd_stage_t np_hd_session_stage(const np_hd_session_t *sess)
{
    return sess ? sess->stage : NP_HD_STAGE_IDLE;
}

const np_hd_montage_t *np_hd_session_montage(const np_hd_session_t *sess)
{
    return sess ? &sess->montage : NULL;
}
