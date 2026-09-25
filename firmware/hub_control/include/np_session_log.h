/*
 * NeurOne Hub Control Program — Session Logger (UHDR / SHDR)
 * Document: NP-FW-HUB-001 Rev 1 §6
 *
 * Routes telemetry and session records to the correct eMMC partition per the
 * data classification table in NP-FW-EMMC-001 Rev 1 §12.
 * UHDR writes are AES-256-XTS encrypted with the user biometric-derived key.
 * SHDR writes use the HKDF manufacturing key.
 *
 * OI-LOG-01 through OI-LOG-04 (the four np_log_hal_* entry points below) are
 * implemented in np_log_backend.c as a block-coalescing append writer:
 *   OI-LOG-01: np_log_hal_uhdr_append(buf, len) → LittleFS UHDR partition
 *   OI-LOG-02: np_log_hal_shdr_append(buf, len) → LittleFS SHDR partition
 *   OI-LOG-03: np_log_hal_uhdr_flush()           → fsync UHDR
 *   OI-LOG-04: np_log_hal_shdr_flush()           → fsync SHDR
 * The residual hardware glue (the LittleFS-file open/append/sync on the mounted
 * partitions) is OI-LOG-05..07 in np_log_backend.h.  Call np_log_backend_init()
 * once at bring-up before np_log_init().
 */

#ifndef NP_SESSION_LOG_H
#define NP_SESSION_LOG_H

#include "np_hub_types.h"
#include "np_adaptation_log.h"

/* ── Log record type tags (written as first byte of each serialized record) ───── */

#define NP_LOG_TAG_UHDR_SESSION_START   0x10U
#define NP_LOG_TAG_UHDR_SESSION_END     0x11U
#define NP_LOG_TAG_UHDR_EEG_BAND        0x12U
#define NP_LOG_TAG_UHDR_PBM_DOSE        0x13U
#define NP_LOG_TAG_UHDR_VNS_HRV         0x14U
#define NP_LOG_TAG_UHDR_STIM            0x15U
#define NP_LOG_TAG_UHDR_VISUAL          0x16U
#define NP_LOG_TAG_UHDR_EEG_IMPEDANCE   0x17U
#define NP_LOG_TAG_UHDR_ADAPT_EVENT     0x18U  /* closed-loop adaptation event (STEP-33) */
#define NP_LOG_TAG_UHDR_COMMAND         0x19U  /* commanded dose: one dispatched command (OI-FMEA-09) */

#define NP_LOG_TAG_SHDR_SESSION_END     0x80U
#define NP_LOG_TAG_SHDR_PBM_HEALTH      0x81U
#define NP_LOG_TAG_SHDR_FAULT           0x82U
#define NP_LOG_TAG_SHDR_ZONE_AUTH       0x83U
#define NP_LOG_TAG_SHDR_NTC_PEAK        0x84U
#define NP_LOG_TAG_SHDR_EEG_CAL         0x85U
#define NP_LOG_TAG_SHDR_SESSION_OPEN    0x86U  /* dirty-session marker (OI-FMEA-09) */

/* ── API ─────────────────────────────────────────────────────────────────────── */

/*
 * np_log_init — initialize logger buffers; call once at hub startup.
 * Reads device_session_count from SHDR for SHDR record headers.
 */
void np_log_init(uint32_t device_session_count);

/*
 * np_log_session_start — open this session's UHDR file and write its
 * session-start record.  Call immediately before np_runner_run() begins
 * execution, after np_uhdr_key_unlock() has mounted UHDR.
 *
 * The device session count (seeded by np_log_init()) is incremented here —
 * EMMC-SHDR-09 — COMMITTED through the hook set by np_log_set_count_commit()
 * (np_session_count_commit(), OI-LFS-12), and only then used to name the file
 * (/uhdr/sessions/<count>, EMMC-UHDR-12).  Commit-before-create means no file
 * can exist whose count was not persisted first, so a reboot seeded from the
 * persisted count never collides.  If a file with the count exists anyway (the
 * persisted record was lost), the count advances to the next unused value, up
 * to NP_LOG_SESSION_PROBE_MAX tries, committing each: never reopened, never
 * overwritten, unique and monotonic (OI-LFS-11).  Past the bound this
 * session's UHDR log fails closed.  A failed commit does not stop the session.
 *
 * DIRTY-SESSION MARKER (OI-FMEA-09).  An SHDR SESSION_OPEN record carrying the
 * count is written here, and it and the UHDR start record are made durable
 * (appended, then synced) before this returns, so before any stimulation.  A
 * clean end writes SHDR SESSION_END for the same count.  An OPEN with no
 * matching END is therefore a session that ended uncleanly (power loss, hard
 * fault, watchdog reset).  Without the marker, a truncated log cannot tell "no
 * record was written" from "nothing happened", and a reader would infer state
 * from an absence (CLAUDE.md §5.1 rule 2).  The same holds for a UHDR session
 * file that has a start record and no end record.  The marker carries the
 * count only: no timestamp, no modality, nothing about the person.
 */
#define NP_LOG_SESSION_PROBE_MAX  1024U

void np_log_session_start(const np_session_uhdr_record_t *rec);

/* The device session count in force — the current session's once started. */
uint32_t np_log_session_count(void);

/* Where np_log_session_start() persists each new count before using it
 * (OI-LFS-12).  NULL (the default) persists nothing. */
typedef np_hub_status_t (*np_log_count_commit_fn)(uint32_t count);
void np_log_set_count_commit(np_log_count_commit_fn fn);

/*
 * np_log_session_end — drain the adaptation ring, write UHDR session-end and
 * SHDR session-end records, flush both, and close the session's UHDR file
 * (OI-LFS-11, OI-FWHUB-15).
 * Call after np_runner_run() returns.
 */
void np_log_session_end(const np_session_uhdr_record_t *uhdr_rec,
                         const np_session_shdr_record_t *shdr_rec);

/*
 * np_log_command — write one dispatched session command to UHDR: the
 * commanded dose, which the device log could not reconstruct until OI-FMEA-09
 * (np_log_session_start() writes no protocol parameters).
 *
 * Layout after NP_LOG_TAG_UHDR_COMMAND: session_ms (4), mod_type (1),
 * target_kind (1), slot_id (1), accepted (1), params_len (2), params
 * (params_len), then socket_mask (NP_HUB_SOCKET_MASK_BYTES) for a
 * socket-addressed command only.  `params` are as authored and signed.  The
 * module caps are fixed firmware constants applied on top, so the commanded
 * current the safety MCU integrated is reproducible from this record and the
 * firmware version.  `accepted` is false when the registry refused the
 * command, so a refused drive is recorded as refused, never as delivered.
 *
 * UHDR only: it is the treatment the person was given.
 */
void np_log_command(const np_session_cmd_t *cmd, uint32_t session_ms,
                    bool accepted);

/*
 * np_log_telemetry — route a module telemetry snapshot to the correct partition.
 * EEG waveform data is logged separately via np_log_eeg_sample_block().
 */
void np_log_telemetry(const np_telem_record_t *rec);

/*
 * np_log_eeg_sample_block — append a block of raw EEG samples to the UHDR
 * EDF+ channel file.  samples[] is 24-bit big-endian ADS1299 output,
 * n_samples × NP_EEG_CHANNELS × NP_EEG_SAMPLE_BYTES bytes.
 */
void np_log_eeg_sample_block(const uint8_t *samples,
                              uint16_t       n_samples,
                              uint32_t       session_ms);

/*
 * np_log_shdr_zone_auth — write zone module authentication result to SHDR.
 * Called by accessory drivers that authenticate (intranasal, cervical VNS).
 */
void np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass);

/*
 * np_log_shdr_fault — write a module fault event to SHDR.
 * Contains only device-condition data: no HR, no EEG values.
 */
void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type,
                        uint8_t fault_code, uint32_t session_ms);

/*
 * np_log_adapt_event — write one closed-loop adaptation event to UHDR.
 * UHDR-only: no SHDR routing (NP-PRIV-REM-001 STEP-33).
 * Called by np_adapt_log_flush() which drains the ring buffer.
 */
void np_log_adapt_event(const np_adaptation_event_t *event);

/*
 * np_log_flush — flush any pending UHDR and SHDR buffers to eMMC.
 * Drains the adaptation ring buffer (np_adapt_log_flush) first.
 * Called on session end and periodically by the hub control task.
 */
void np_log_flush(void);

/* ── HAL stubs (OI-LOG-01 through OI-LOG-04) ─────────────────────────────────── */

extern np_hub_status_t np_log_hal_uhdr_append(const uint8_t *buf, size_t len);
extern np_hub_status_t np_log_hal_shdr_append(const uint8_t *buf, size_t len);
extern np_hub_status_t np_log_hal_uhdr_flush(void);
extern np_hub_status_t np_log_hal_shdr_flush(void);

#endif /* NP_SESSION_LOG_H */
