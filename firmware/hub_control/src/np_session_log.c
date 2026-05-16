/*
 * NeuroPulse Hub Control Program — Session Logger (UHDR / SHDR)
 * Document: NP-FW-HUB-001 Rev A §6
 *
 * All writes are buffered; np_log_flush() triggers eMMC writes.
 * EEG sample blocks bypass the main buffer and go directly to HAL to avoid
 * copying 12 KB/s through a small intermediate buffer.
 *
 * Data routing follows NP-FW-EMMC-001 Rev A §12:
 *  UHDR: session UUID+timestamps, dose, HRV waveforms, coherence, impedance,
 *        EEG band power, EEG waveforms, eye state, protocol parameters used.
 *  SHDR: device health metrics only — no HR values, no EEG amplitudes,
 *        no timestamps (only unsigned session count).
 */

#include "np_session_log.h"
#include <string.h>

/* ── Internal buffers (flushed to eMMC by HAL) ───────────────────────────────── */

#define LOG_BUF_SIZE 4096U

static uint8_t  s_uhdr_buf[LOG_BUF_SIZE];
static size_t   s_uhdr_pos = 0U;

static uint8_t  s_shdr_buf[LOG_BUF_SIZE];
static size_t   s_shdr_pos = 0U;

static uint32_t s_device_session_count = 0U;

/* ── Serialisation helpers ────────────────────────────────────────────────────── */

static bool uhdr_write(const void *data, size_t len)
{
    if (s_uhdr_pos + len > LOG_BUF_SIZE) {
        np_log_hal_uhdr_flush();
        np_log_hal_uhdr_append(s_uhdr_buf, s_uhdr_pos);
        s_uhdr_pos = 0U;
    }
    if (len > LOG_BUF_SIZE) {
        return false;
    }
    memcpy(s_uhdr_buf + s_uhdr_pos, data, len);
    s_uhdr_pos += len;
    return true;
}

static bool shdr_write(const void *data, size_t len)
{
    if (s_shdr_pos + len > LOG_BUF_SIZE) {
        np_log_hal_shdr_flush();
        np_log_hal_shdr_append(s_shdr_buf, s_shdr_pos);
        s_shdr_pos = 0U;
    }
    if (len > LOG_BUF_SIZE) {
        return false;
    }
    memcpy(s_shdr_buf + s_shdr_pos, data, len);
    s_shdr_pos += len;
    return true;
}

static void uhdr_u8(uint8_t tag) { uhdr_write(&tag, 1U); }
static void shdr_u8(uint8_t tag) { shdr_write(&tag, 1U); }

/* ── Public API ───────────────────────────────────────────────────────────────── */

void np_log_init(uint32_t device_session_count)
{
    s_uhdr_pos             = 0U;
    s_shdr_pos             = 0U;
    s_device_session_count = device_session_count;
}

void np_log_session_start(const np_session_uhdr_record_t *rec)
{
    if (rec == NULL) { return; }

    /* UHDR: session start record */
    uhdr_u8(NP_LOG_TAG_UHDR_SESSION_START);
    uhdr_write(rec->session_uuid, NP_HUB_PROTO_UUID_LEN);
    uhdr_write(&rec->start_unix,  sizeof(rec->start_unix));
    uhdr_write(&rec->mods_active_mask, sizeof(rec->mods_active_mask));
}

void np_log_session_end(const np_session_uhdr_record_t *uhdr_rec,
                         const np_session_shdr_record_t *shdr_rec)
{
    if (uhdr_rec != NULL) {
        uhdr_u8(NP_LOG_TAG_UHDR_SESSION_END);
        uhdr_write(uhdr_rec->session_uuid, NP_HUB_PROTO_UUID_LEN);
        uhdr_write(&uhdr_rec->duration_s,    sizeof(uhdr_rec->duration_s));
        uhdr_write(&uhdr_rec->abort_reason,  sizeof(uhdr_rec->abort_reason));
        uhdr_write(&uhdr_rec->mods_active_mask, sizeof(uhdr_rec->mods_active_mask));
    }

    if (shdr_rec != NULL) {
        /* SHDR: session count, duration, firmware version — no user biology. */
        shdr_u8(NP_LOG_TAG_SHDR_SESSION_END);
        shdr_write(&s_device_session_count,  sizeof(s_device_session_count));
        shdr_write(&shdr_rec->duration_s,    sizeof(shdr_rec->duration_s));
        shdr_write(&shdr_rec->abort_reason,  sizeof(shdr_rec->abort_reason));
        shdr_write(&shdr_rec->mods_active_mask, sizeof(shdr_rec->mods_active_mask));
        shdr_write(shdr_rec->firmware_version, sizeof(shdr_rec->firmware_version));
    }
}

void np_log_telemetry(const np_telem_record_t *rec)
{
    if (rec == NULL) { return; }

    switch (rec->mod_type) {

    case NP_MOD_PBM_BASE:
    case NP_MOD_PBM_SMART: {
        const np_telem_pbm_t *p = &rec->data.pbm;
        /* UHDR: dose per zone — reveals treatment received. */
        uhdr_u8(NP_LOG_TAG_UHDR_PBM_DOSE);
        uhdr_write(&rec->session_ms,       sizeof(rec->session_ms));
        uhdr_write(&rec->slot,             sizeof(rec->slot));
        uhdr_write(p->dose_J_cm2,          sizeof(p->dose_J_cm2));
        uhdr_write(&p->irradiance_mW_cm2,  sizeof(p->irradiance_mW_cm2));

        /* SHDR: PD ratio (LED health), NTC peaks, fault mask — device condition. */
        shdr_u8(NP_LOG_TAG_SHDR_PBM_HEALTH);
        shdr_write(&s_device_session_count, sizeof(s_device_session_count));
        shdr_write(&rec->slot,              sizeof(rec->slot));
        shdr_write(p->pd_ratio,             sizeof(p->pd_ratio));
        shdr_write(p->ntc_peak_c,           sizeof(p->ntc_peak_c));
        shdr_write(&p->throttle_events,     sizeof(p->throttle_events));
        shdr_write(&p->fault_mask,          sizeof(p->fault_mask));
        break;
    }

    case NP_MOD_EEG: {
        const np_telem_eeg_t *e = &rec->data.eeg;
        /* UHDR: impedance per channel and band power — user physiology. */
        uhdr_u8(NP_LOG_TAG_UHDR_EEG_IMPEDANCE);
        uhdr_write(&rec->session_ms,        sizeof(rec->session_ms));
        uhdr_write(e->impedance_kohm,       sizeof(e->impedance_kohm));
        uhdr_u8(NP_LOG_TAG_UHDR_EEG_BAND);
        uhdr_write(&rec->session_ms,        sizeof(rec->session_ms));
        uhdr_write(&e->dominant_band,       sizeof(e->dominant_band));
        uhdr_write(e->band_power_db,        sizeof(e->band_power_db));
        /* ADS1299 calibration coefficients → SHDR (device health). */
        shdr_u8(NP_LOG_TAG_SHDR_EEG_CAL);
        shdr_write(&s_device_session_count, sizeof(s_device_session_count));
        /* Calibration coefficients are written at session start via a separate
         * SHDR record from np_mod_eeg_init(); nothing extra here. */
        break;
    }

    case NP_MOD_VNS_HRV: {
        const np_telem_vns_hrv_t *h = &rec->data.vns_hrv;
        /* UHDR: HRV time series, coherence, HR, impedance — user physiology. */
        uhdr_u8(NP_LOG_TAG_UHDR_VNS_HRV);
        uhdr_write(&rec->session_ms,        sizeof(rec->session_ms));
        uhdr_write(&h->rmssd_ms,            sizeof(h->rmssd_ms));
        uhdr_write(&h->coherence_score,     sizeof(h->coherence_score));
        uhdr_write(&h->hr_bpm,              sizeof(h->hr_bpm));
        uhdr_write(&h->impedance_ohm,       sizeof(h->impedance_ohm));
        /* SHDR: contact pass/fail only — no HR, no impedance raw value. */
        /* (contact_confirmed bool is written in SHDR fault record on disconnect) */
        break;
    }

    case NP_MOD_BES_TACS:
    case NP_MOD_TDCS: {
        const np_telem_stim_t *s = &rec->data.stim;
        /* UHDR: delivered current and charge — treatment parameters received. */
        uhdr_u8(NP_LOG_TAG_UHDR_STIM);
        uhdr_write(&rec->session_ms,        sizeof(rec->session_ms));
        uhdr_write(&rec->slot,              sizeof(rec->slot));
        uhdr_write(&s->current_ua,          sizeof(s->current_ua));
        uhdr_write(&s->charge_uC,           sizeof(s->charge_uC));
        uhdr_write(&s->impedance_kohm,      sizeof(s->impedance_kohm));
        break;
    }

    case NP_MOD_VISUAL: {
        const np_telem_visual_t *v = &rec->data.visual;
        /* UHDR: eye_open state — reveals user physiology during session. */
        uhdr_u8(NP_LOG_TAG_UHDR_VISUAL);
        uhdr_write(&rec->session_ms,        sizeof(rec->session_ms));
        uhdr_write(&v->eye_open,            sizeof(v->eye_open));
        /* SHDR: hall state and ppx_halt — device safety logs only. */
        if (v->ppx_halt) {
            shdr_u8(NP_LOG_TAG_SHDR_FAULT);
            shdr_write(&s_device_session_count, sizeof(s_device_session_count));
            uint8_t slot = rec->slot;
            uint8_t code = 0xEEU; /* EEG PPX halt code */
            shdr_write(&slot, 1U);
            shdr_write(&code, 1U);
        }
        break;
    }

    default:
        break;
    }
}

void np_log_eeg_sample_block(const uint8_t *samples,
                              uint16_t       n_samples,
                              uint32_t       session_ms)
{
    if (samples == NULL || n_samples == 0U) { return; }
    /* Write directly to UHDR without buffering — avoids a 12 KB/s copy overhead. */
    uint8_t hdr[7];
    hdr[0] = 0xEEU; /* EEG raw sample block tag */
    hdr[1] = (uint8_t)(session_ms >> 24);
    hdr[2] = (uint8_t)(session_ms >> 16);
    hdr[3] = (uint8_t)(session_ms >> 8);
    hdr[4] = (uint8_t)(session_ms);
    hdr[5] = (uint8_t)(n_samples >> 8);
    hdr[6] = (uint8_t)(n_samples);
    np_log_hal_uhdr_append(hdr, sizeof(hdr));
    np_log_hal_uhdr_append(samples,
                           (size_t)n_samples * NP_EEG_CHANNELS * NP_EEG_SAMPLE_BYTES);
}

void np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass)
{
    shdr_u8(NP_LOG_TAG_SHDR_ZONE_AUTH);
    shdr_write(&s_device_session_count, sizeof(s_device_session_count));
    shdr_write(&slot, 1U);
    uint8_t t = (uint8_t)type;
    shdr_write(&t, 1U);
    uint8_t p = pass ? 1U : 0U;
    shdr_write(&p, 1U);
}

void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type,
                        uint8_t fault_code, uint32_t session_ms)
{
    /* session_ms is NOT written to SHDR — SHDR never contains timestamps.
     * Only the session count (monotonic device state) is logged. */
    (void)session_ms;
    shdr_u8(NP_LOG_TAG_SHDR_FAULT);
    shdr_write(&s_device_session_count, sizeof(s_device_session_count));
    shdr_write(&slot, 1U);
    uint8_t t = (uint8_t)type;
    shdr_write(&t, 1U);
    shdr_write(&fault_code, 1U);
}

void np_log_flush(void)
{
    if (s_uhdr_pos > 0U) {
        np_log_hal_uhdr_append(s_uhdr_buf, s_uhdr_pos);
        s_uhdr_pos = 0U;
        np_log_hal_uhdr_flush();
    }
    if (s_shdr_pos > 0U) {
        np_log_hal_shdr_append(s_shdr_buf, s_shdr_pos);
        s_shdr_pos = 0U;
        np_log_hal_shdr_flush();
    }
}
