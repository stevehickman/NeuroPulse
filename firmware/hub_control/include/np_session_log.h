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
#include "np_pbm_socket_telem.h"

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
#define NP_LOG_TAG_UHDR_PBM_SOCKET      0x1AU  /* one lattice socket's dose + NTC (OI-FWHUB-10, F1–F8) */

#define NP_LOG_TAG_SHDR_SESSION_END     0x80U
#define NP_LOG_TAG_SHDR_PBM_HEALTH      0x81U
#define NP_LOG_TAG_SHDR_FAULT           0x82U
#define NP_LOG_TAG_SHDR_ZONE_AUTH       0x83U
#define NP_LOG_TAG_SHDR_NTC_PEAK        0x84U
#define NP_LOG_TAG_SHDR_EEG_CAL         0x85U
#define NP_LOG_TAG_SHDR_SESSION_OPEN    0x86U  /* dirty-session marker (OI-FMEA-09) */
#define NP_LOG_TAG_SHDR_PBM_TILE_HEALTH 0x87U  /* idle pass, one occupied socket (OI-FWHUB-10, F10–F12, F14) */
#define NP_LOG_TAG_SHDR_PBM_COUNTS      0x88U  /* per-session PBM counts, no location (OI-FWHUB-10, F13) */

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
 * np_log_pbm_socket — one lattice socket's telemetry to UHDR (OI-FWHUB-10).
 * UHDR ONLY: which socket was lit, and what it delivered, is the treatment
 * montage (NP-FW-HUB-001 §6.8, F1–F4, F7, F8).
 *
 * Layout after NP_LOG_TAG_UHDR_PBM_SOCKET: session_ms (4), socket_id (1),
 * mod_type (1), flags (1, NP_PST_FLAG_*), ntc_c (4), ntc_peak_c (4),
 * dose_J_cm2[3] (12), irradiance_mW_cm2[3] (12) — 40 bytes with the tag.
 * A wavelength whose PD_VALID bit is clear was not measured; its irradiance
 * is written 0 and must not be read as darkness.
 */
void np_log_pbm_socket(const np_pst_socket_record_t *rec, uint32_t session_ms);

/*
 * np_log_shdr_pbm_socket_health — one occupied PBM tile's LATEST STATE to
 * SHDR (F10, F11b, F12, F14; §6.9).  Written by the idle pass over EVERY
 * occupied socket, never from session data and never per maintenance run, so
 * it says nothing about which sockets were lit or which a person chose to
 * test.  No module UID (OI-UPG-07), no timestamp, and no session count: the
 * fleet upserts one row per (device, socket), and a count would turn those
 * rows back into a timeline (principal 2026-09-27).
 *
 * Layout after NP_LOG_TAG_SHDR_PBM_TILE_HEALTH: socket_id (1), mod_type (1),
 * health_flags (1), verdict[4] (probe, cal, NTC, LED; np_maint_verdict_t),
 * cal_source (1), pd1_pct[3] (3) — 12 bytes with the tag.
 */
void np_log_shdr_pbm_socket_health(const np_pst_health_record_t *rec);

/*
 * np_log_shdr_pbm_session_counts — this session's PBM counts to SHDR (F13).
 * Written at EVERY session end, whether or not PBM ran, so its presence is
 * not a function of the modality (CLAUDE.md §5.1 rule 2).  Counts only.
 *
 * Layout after NP_LOG_TAG_SHDR_PBM_COUNTS: session_count (4),
 * throttle_events (2), predrive_refusals (2), drive_faults (2) — 11 bytes.
 */
void np_log_shdr_pbm_session_counts(const np_pst_counts_t *counts);

/*
 * np_log_eeg_sample_block — append a block of raw EEG samples to the UHDR
 * EDF+ channel file.  samples[] is 24-bit big-endian ADS1299 output,
 * n_samples × NP_EEG_CHANNELS × NP_EEG_SAMPLE_BYTES bytes.
 *
 * TASK CONTEXT ONLY — NEVER FROM AN ISR (OI-FWHUB-14, NP-FW-HUB-001 §8.2).
 * It drains the adaptation ring and s_uhdr_buf and appends to the backend's
 * staging, all shared with the task-side logger and none safe to touch from an
 * interrupt.  The EEG DMA ISR hands each completed buffer to a task through a
 * queue, and that task calls this.  Called from an ISR (as reported by the
 * hook set with np_log_set_isr_check()) it returns NP_HUB_ERR_GENERIC and
 * touches no logger state; the target's hook also asserts.
 *
 * Returns NP_HUB_OK when the block was handed down, NP_HUB_ERR_INVALID_ARG
 * for a NULL or empty block.
 */
np_hub_status_t np_log_eeg_sample_block(const uint8_t *samples,
                                         uint16_t       n_samples,
                                         uint32_t       session_ms);

/* Reports whether the caller is running in interrupt context (OI-FWHUB-14).
 * NULL (the default, and host) reports task context. */
typedef bool (*np_log_in_isr_fn)(void);
void np_log_set_isr_check(np_log_in_isr_fn fn);

/*
 * np_log_shdr_zone_auth — write zone module authentication result to SHDR.
 * Called by accessory drivers that authenticate (intranasal, cervical VNS).
 */
void np_log_shdr_zone_auth(uint8_t slot, np_hub_mod_type_t type, bool pass);

/*
 * np_log_shdr_fault — write a module fault event to SHDR.
 * Contains only device-condition data: no HR, no EEG values.
 *
 * NEVER BLOCKS (OI-FWHUB-19).  The safety heartbeat calls this, so it does not
 * take the logger lock.  It copies the record into a queue of
 * NP_LOG_FAULT_QUEUE_MAX entries under the critical section set with
 * np_log_set_fault_crit(), stamped with the session count in force now, and
 * the next logger call to take the lock writes it to s_shdr_buf.  The SHDR
 * record layout is unchanged.  A full queue keeps what it holds and counts the
 * newer faults it could not take (np_log_shdr_fault_dropped()).
 */
#define NP_LOG_FAULT_QUEUE_MAX  32U

void np_log_shdr_fault(uint8_t slot, np_hub_mod_type_t type,
                        uint8_t fault_code, uint32_t session_ms);

/* Faults refused by a full queue since np_log_init().  Not written to SHDR. */
uint32_t np_log_shdr_fault_dropped(void);

/* The fault queue's critical section.  On target it masks the kernel-aware
 * interrupts (portSET_INTERRUPT_MASK_FROM_ISR), which is legal from a task or
 * from an ISR at or below configMAX_SYSCALL_INTERRUPT_PRIORITY.  NULL (the
 * default, and host) masks nothing. */
typedef uint32_t (*np_log_crit_enter_fn)(void);
typedef void     (*np_log_crit_exit_fn)(uint32_t saved);
void np_log_set_fault_crit(np_log_crit_enter_fn enter, np_log_crit_exit_fn exit_fn);

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

/*
 * THE LOGGER LOCK (OI-FWHUB-19, NP-FW-HUB-001 §6.7).  Four tasks call the
 * logger: the runner (records, session boundaries), task_telemetry (the
 * periodic flush), task_module_detect (accessory auth records) and the safety
 * heartbeat (cVNS fault records).  Every entry point above except
 * np_log_session_count(), the np_log_set_*() hooks and np_log_shdr_fault()
 * holds this lock for its whole body, and so do the adaptation ring's
 * functions.  It must be RECURSIVE: np_log_flush() drains the ring, which
 * writes each event through np_log_adapt_event().  On target it is a FreeRTOS
 * recursive mutex, installed by np_hub_control_app_main() before the scheduler
 * starts; NULL (the default, and host) locks nothing.  Never take it from an
 * ISR, and never from task_safety_heartbeat: the holder can be inside an eMMC
 * sync (REQ-FWHUB-02).  np_log_lock() / np_log_unlock() are for
 * np_adaptation_log.c.
 */
typedef void (*np_log_lock_fn)(void);
void np_log_set_lock(np_log_lock_fn lock, np_log_lock_fn unlock);
void np_log_lock(void);
void np_log_unlock(void);

/* ── HAL stubs (OI-LOG-01 through OI-LOG-04) ─────────────────────────────────── */

extern np_hub_status_t np_log_hal_uhdr_append(const uint8_t *buf, size_t len);
extern np_hub_status_t np_log_hal_shdr_append(const uint8_t *buf, size_t len);
extern np_hub_status_t np_log_hal_uhdr_flush(void);
extern np_hub_status_t np_log_hal_shdr_flush(void);

#endif /* NP_SESSION_LOG_H */
