/*
 * NeurOne Hub Control Program — Session Logger (UHDR / SHDR)
 * Document: NP-FW-HUB-001 Rev 1 §6
 *
 * All writes are buffered; np_log_flush() triggers eMMC writes.  A buffer is
 * always appended before it is synced (§6.5).
 * EEG sample blocks bypass the main buffer and go directly to HAL to avoid
 * copying 12 KB/s through a small intermediate buffer — after the buffered
 * UHDR records ahead of them, so a session file stays in time order.
 *
 * Data routing follows NP-FW-EMMC-001 Rev 1 §12:
 *  UHDR: session UUID+timestamps, dose, HRV waveforms, coherence, impedance,
 *        EEG band power, EEG waveforms, eye state, protocol parameters used.
 *        Commanded dose: every dispatched command (OI-FMEA-09).
 *  SHDR: device health metrics only — no HR values, no EEG amplitudes,
 *        no timestamps (only unsigned session count).  Session open/end
 *        markers pair by count to expose an unclean end (OI-FMEA-09).
 *
 * CONCURRENCY (OI-FWHUB-19, NP-FW-HUB-001 §6.7).  Four tasks call this logger.
 * Every entry point that touches logger state runs under one recursive lock
 * (a FreeRTOS recursive mutex on target, injected by np_log_set_lock()), so a
 * record is never torn and no buffered byte is lost to an interleaving.  The
 * one exception is np_log_shdr_fault(): the safety heartbeat calls it, and the
 * heartbeat must never wait behind an eMMC sync (REQ-FWHUB-02).  It copies the
 * record into a fixed queue under a short critical section and returns; the
 * next caller to take the lock moves the queue into s_shdr_buf.
 */

#include "np_session_log.h"
#include "np_adaptation_log.h"
#include "np_log_backend.h"    /* per-session UHDR files (OI-LFS-11) */
#include <string.h>

/* ── Internal buffers (flushed to eMMC by HAL) ───────────────────────────────── */

#define LOG_BUF_SIZE 4096U

static uint8_t  s_uhdr_buf[LOG_BUF_SIZE];
static size_t   s_uhdr_pos = 0U;

static uint8_t  s_shdr_buf[LOG_BUF_SIZE];
static size_t   s_shdr_pos = 0U;

static uint32_t s_device_session_count = 0U;

/* ── The logger lock (OI-FWHUB-19) ────────────────────────────────────────────── */

static np_log_lock_fn s_lock;
static np_log_lock_fn s_unlock;
static unsigned       s_depth;          /* nesting; touched only by the holder */

static void fault_queue_drain(void);

/* Take the lock.  The outermost take also moves any queued SHDR faults into
 * s_shdr_buf, so they are drained at the runner's tick rate during a session
 * and ahead of whatever record the caller is about to write. */
static void take(void)
{
    if (s_lock != NULL) { s_lock(); }
    if (++s_depth == 1U) {
        fault_queue_drain();
    }
}

static void give(void)
{
    s_depth--;
    if (s_unlock != NULL) { s_unlock(); }
}

void np_log_set_lock(np_log_lock_fn lock, np_log_lock_fn unlock)
{
    s_lock   = lock;
    s_unlock = unlock;
}

void np_log_lock(void)   { take(); }
void np_log_unlock(void) { give(); }

/* ── The SHDR fault queue (OI-FWHUB-19) ──────────────────────────────────────── */

typedef struct {
    uint32_t count;       /* the session count in force when the fault was logged */
    uint8_t  slot;
    uint8_t  type;
    uint8_t  code;
} fault_entry_t;

static fault_entry_t s_fq[NP_LOG_FAULT_QUEUE_MAX];
static uint32_t      s_fq_head;
static uint32_t      s_fq_len;
static uint32_t      s_fq_dropped;

static np_log_crit_enter_fn s_crit_enter;
static np_log_crit_exit_fn  s_crit_exit;

static uint32_t crit_enter(void) { return (s_crit_enter != NULL) ? s_crit_enter() : 0U; }
static void     crit_exit(uint32_t saved) { if (s_crit_exit != NULL) { s_crit_exit(saved); } }

void np_log_set_fault_crit(np_log_crit_enter_fn enter, np_log_crit_exit_fn exit_fn)
{
    s_crit_enter = enter;
    s_crit_exit  = exit_fn;
}

uint32_t np_log_shdr_fault_dropped(void)
{
    uint32_t saved = crit_enter();
    uint32_t n = s_fq_dropped;
    crit_exit(saved);
    return n;
}

/* ── Buffer hand-down ─────────────────────────────────────────────────────────── */

/* Hand everything buffered to the HAL, append only — no sync.  Used where
 * order matters but durability is not the point (the EEG direct path). */
static void uhdr_append_buffered(void)
{
    if (s_uhdr_pos > 0U) {
        np_log_hal_uhdr_append(s_uhdr_buf, s_uhdr_pos);
        s_uhdr_pos = 0U;
    }
}

static void shdr_append_buffered(void)
{
    if (s_shdr_pos > 0U) {
        np_log_hal_shdr_append(s_shdr_buf, s_shdr_pos);
        s_shdr_pos = 0U;
    }
}

/* Append, THEN sync: the sync covers the bytes just handed down (§6.5).  The
 * one path by which buffered records become durable — never sync first.
 * UHDR: also called at both session boundaries, so no record crosses into the
 * wrong session's file (OI-LFS-11). */
static void uhdr_drain(void)
{
    uhdr_append_buffered();
    np_log_hal_uhdr_flush();
}

static void shdr_drain(void)
{
    shdr_append_buffered();
    np_log_hal_shdr_flush();
}

/* ── Serialization helpers ────────────────────────────────────────────────────── */

static bool uhdr_write(const void *data, size_t len)
{
    if (len > LOG_BUF_SIZE) {
        return false;
    }
    if (s_uhdr_pos + len > LOG_BUF_SIZE) {
        uhdr_drain();
    }
    memcpy(s_uhdr_buf + s_uhdr_pos, data, len);
    s_uhdr_pos += len;
    return true;
}

static bool shdr_write(const void *data, size_t len)
{
    if (len > LOG_BUF_SIZE) {
        return false;
    }
    if (s_shdr_pos + len > LOG_BUF_SIZE) {
        shdr_drain();
    }
    memcpy(s_shdr_buf + s_shdr_pos, data, len);
    s_shdr_pos += len;
    return true;
}

static void uhdr_u8(uint8_t tag) { uhdr_write(&tag, 1U); }
static void shdr_u8(uint8_t tag) { shdr_write(&tag, 1U); }

/* ── Public API ───────────────────────────────────────────────────────────────── */

static np_log_count_commit_fn s_count_commit;    /* OI-LFS-12 */

uint32_t np_log_session_count(void)
{
    return s_device_session_count;
}

void np_log_set_count_commit(np_log_count_commit_fn fn)
{
    s_count_commit = fn;
}

void np_log_init(uint32_t device_session_count)
{
    take();
    s_uhdr_pos             = 0U;
    s_shdr_pos             = 0U;
    s_device_session_count = device_session_count;
    uint32_t saved = crit_enter();
    s_fq_head    = 0U;
    s_fq_len     = 0U;
    s_fq_dropped = 0U;
    crit_exit(saved);
    give();
}

void np_log_session_start(const np_session_uhdr_record_t *rec)
{
    if (rec == NULL) { return; }
    take();

    /* Anything still buffered belongs to the previous session's file. */
    uhdr_drain();

    /* EMMC-SHDR-09: the count increments at session start, and names this
     * session's UHDR file (EMMC-UHDR-12).  A file that already has the name
     * means the count was not carried across a reboot: step past it rather
     * than lose the session — and never reopen it (OI-LFS-11). */
    for (uint32_t probe = 0U; probe < NP_LOG_SESSION_PROBE_MAX; probe++) {
        s_device_session_count++;
        /* OI-LFS-12: persist first, create second.  A power loss between the
         * two costs an unused count — a gap, never a reused one. */
        if (s_count_commit != NULL) {
            (void)s_count_commit(s_device_session_count);
        }
        if (np_log_backend_session_begin((uint64_t)s_device_session_count)
                != NP_HUB_ERR_LOG_EXISTS) {
            break;
        }
    }

    /* UHDR: session start record */
    uhdr_u8(NP_LOG_TAG_UHDR_SESSION_START);
    uhdr_write(rec->session_uuid, NP_HUB_PROTO_UUID_LEN);
    uhdr_write(&rec->start_unix,  sizeof(rec->start_unix));
    uhdr_write(&rec->mods_active_mask, sizeof(rec->mods_active_mask));

    /* SHDR: the dirty-session marker (OI-FMEA-09).  The count only. */
    shdr_u8(NP_LOG_TAG_SHDR_SESSION_OPEN);
    shdr_write(&s_device_session_count, sizeof(s_device_session_count));

    /* Both durable before the caller starts stimulating: a marker that could
     * be lost with the session it marks would mark nothing. */
    uhdr_drain();
    shdr_drain();
    give();
}

void np_log_command(const np_session_cmd_t *cmd, uint32_t session_ms,
                    bool accepted)
{
    if (cmd == NULL || cmd->params_len > NP_HUB_PROTO_PARAMS_MAX) { return; }
    take();

    const uint8_t mod_type = (uint8_t)cmd->mod_type;
    const uint8_t acc      = accepted ? 1U : 0U;

    uhdr_u8(NP_LOG_TAG_UHDR_COMMAND);
    uhdr_write(&session_ms,        sizeof(session_ms));
    uhdr_write(&mod_type,          1U);
    uhdr_write(&cmd->target_kind,  1U);
    uhdr_write(&cmd->slot_id,      1U);
    uhdr_write(&acc,               1U);
    uhdr_write(&cmd->params_len,   sizeof(cmd->params_len));
    if (cmd->params_len > 0U) {
        uhdr_write(cmd->params, cmd->params_len);
    }
    if (cmd->target_kind == NP_PROTO_TARGET_SOCKET_MASK) {
        uhdr_write(cmd->socket_mask, NP_HUB_SOCKET_MASK_BYTES);
    }
    give();
}

void np_log_session_end(const np_session_uhdr_record_t *uhdr_rec,
                         const np_session_shdr_record_t *shdr_rec)
{
    /* Adaptation events still in the ring belong to this session: move them
     * into the buffer while its file is open, ahead of the session-end record.
     * Left for the caller's np_log_flush(), they would reach a closed file and
     * no file at all (OI-FWHUB-15).  The lock is taken first, which also moves
     * any queued SHDR fault into the buffer ahead of the session-end record. */
    take();
    np_adapt_log_flush();

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

    /* The session-end record is the last thing in this session's file. */
    uhdr_drain();
    (void)np_log_backend_session_end();
    if (s_shdr_pos > 0U) {
        shdr_drain();
    }
    give();
}

void np_log_telemetry(const np_telem_record_t *rec)
{
    if (rec == NULL) { return; }
    take();

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
    give();
}

static np_log_in_isr_fn s_in_isr;    /* OI-FWHUB-14 */

void np_log_set_isr_check(np_log_in_isr_fn fn)
{
    s_in_isr = fn;
}

np_hub_status_t np_log_eeg_sample_block(const uint8_t *samples,
                                         uint16_t       n_samples,
                                         uint32_t       session_ms)
{
    /* OI-FWHUB-14: refused from an ISR before anything is touched.  The ring,
     * s_uhdr_buf and the backend staging below are unsynchronized state the
     * task-side logger may be part-way through; the DMA ISR queues the block
     * to a task instead (§8.2). */
    if (s_in_isr != NULL && s_in_isr()) { return NP_HUB_ERR_GENERIC; }
    if (samples == NULL || n_samples == 0U) { return NP_HUB_ERR_INVALID_ARG; }
    take();

    /* Every UHDR record logged before this block goes to the file before it —
     * the adaptation ring first, since its events reach s_uhdr_buf only when
     * drained — so a session file stays in time order.  Append only: order,
     * not durability, is what this path owes, and a sync here would cost one
     * per block. */
    np_adapt_log_flush();
    uhdr_append_buffered();

    /* The samples themselves bypass the buffer — avoids a 12 KB/s copy. */
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
    give();
    return NP_HUB_OK;
}

void np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass)
{
    take();
    shdr_u8(NP_LOG_TAG_SHDR_ZONE_AUTH);
    shdr_write(&s_device_session_count, sizeof(s_device_session_count));
    shdr_write(&slot, 1U);
    uint8_t t = (uint8_t)type;
    shdr_write(&t, 1U);
    uint8_t p = pass ? 1U : 0U;
    shdr_write(&p, 1U);
    give();
}

void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type,
                        uint8_t fault_code, uint32_t session_ms)
{
    /* session_ms is NOT written to SHDR — SHDR never contains timestamps.
     * Only the session count (monotonic device state) is logged. */
    (void)session_ms;

    /* Never takes the logger lock (OI-FWHUB-19): the heartbeat calls this, and
     * the lock can be held across an eMMC sync.  The record is queued under a
     * critical section a few stores long, with the session count in force now,
     * and written by the next take().  A full queue keeps the faults already in
     * it — the first of a burst is the diagnostic one — and counts the rest. */
    fault_entry_t e;
    e.count = s_device_session_count;
    e.slot  = slot;
    e.type  = (uint8_t)type;
    e.code  = fault_code;

    uint32_t saved = crit_enter();
    if (s_fq_len < NP_LOG_FAULT_QUEUE_MAX) {
        s_fq[(s_fq_head + s_fq_len) % NP_LOG_FAULT_QUEUE_MAX] = e;
        s_fq_len++;
    } else {
        s_fq_dropped++;
    }
    crit_exit(saved);
}

/* Called with the lock held, by the outermost take() only.  Each entry leaves
 * the queue under the critical section and is serialized outside it, so a
 * fault logged mid-drain is queued, not lost; the record layout is unchanged. */
static void fault_queue_drain(void)
{
    for (;;) {
        fault_entry_t e;
        uint32_t saved = crit_enter();
        if (s_fq_len == 0U) {
            crit_exit(saved);
            return;
        }
        e = s_fq[s_fq_head];
        s_fq_head = (s_fq_head + 1U) % NP_LOG_FAULT_QUEUE_MAX;
        s_fq_len--;
        crit_exit(saved);

        shdr_u8(NP_LOG_TAG_SHDR_FAULT);
        shdr_write(&e.count, sizeof(e.count));
        shdr_write(&e.slot, 1U);
        shdr_write(&e.type, 1U);
        shdr_write(&e.code, 1U);
    }
}

void np_log_adapt_event(const np_adaptation_event_t *event)
{
    if (event == NULL) { return; }
    take();
    /* UHDR only — no SHDR routing for adaptation events (NP-PRIV-REM-001 STEP-33). */
    uhdr_u8(NP_LOG_TAG_UHDR_ADAPT_EVENT);
    uhdr_write(event, sizeof(np_adaptation_event_t));
    give();
}

void np_log_flush(void)
{
    take();                 /* also moves queued SHDR faults into the buffer */
    np_adapt_log_flush();   /* drain the adaptation ring buffer first */
    /* UHDR syncs whether or not s_uhdr_buf holds anything: EEG sample blocks
     * reach the HAL without passing through it, so an empty buffer does not
     * mean nothing is waiting to be made durable (§6.5). */
    uhdr_drain();
    if (s_shdr_pos > 0U) {
        shdr_drain();
    }
    give();
}
