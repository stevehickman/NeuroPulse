/*
 * NeurOne Hub Control Program — Session Log eMMC Backing (OI-LOG-01..04)
 * Document: NP-FW-HUB-001 Rev 1 §6, NP-FW-EMMC-001 Rev 1 §4–§7, §12
 *
 * Backs the four session-logger HAL entry points declared in np_session_log.h:
 *
 *   OI-LOG-01  np_log_hal_uhdr_append(buf, len)  → UHDR partition log file
 *   OI-LOG-02  np_log_hal_shdr_append(buf, len)  → SHDR partition log file
 *   OI-LOG-03  np_log_hal_uhdr_flush()           → sync UHDR
 *   OI-LOG-04  np_log_hal_shdr_flush()           → sync SHDR
 *
 * These are now REAL logic (block-aligned staging + capacity/endurance tracking
 * + durable-sync barrier), not stubs.  The residual hardware glue is the lower
 * LittleFS-file HAL (OI-LOG-05..07) declared at the bottom of this header.
 *
 * ── Encryption boundary ──────────────────────────────────────────────────────
 * This layer writes PLAINTEXT records.  Encryption is transparent at the
 * mounted block-device layer, exactly as for every other file on these
 * partitions:
 *   - UHDR is mounted AES-256-XTS under the user biometric-derived UKMD by
 *     np_uhdr_key_unlock() → np_uhdr_hal_mount_uhdr() (NP-FW-EMMC-002 §C).
 *   - SHDR is mounted AES-256-XTS under the HKDF manufacturing key at boot
 *     (NP-FW-EMMC-001 §7).
 * The log backend never touches key material and must not — keeping the
 * UHDR key confined to np_uhdr_key is a privacy invariant.
 *
 * ── Durability model ──────────────────────────────────────────────────────────
 * Appends are coalesced into program-block-sized (NP_LOG_STAGE_BYTES) writes to
 * limit eMMC write amplification and wear.  Between flushes only whole blocks
 * are written; a flush commits the exact buffered tail (no padding) and issues a
 * block-device sync, so at most the records appended since the last flush are
 * lost on power loss.  Because the log file is append-mode, flush never rewrites
 * a previously committed byte — it writes only what is newly buffered.
 *
 * ── File layout (OI-LFS-11, NP-SOUP-LFS-001 Rev 7 §13.10) ─────────────────────
 * "The append-mode log file" was one file per partition until OI-LFS-11, and
 * littlefs caps a file at file_max = 2 GiB − 1 — under a third of the 6,903 MiB
 * UHDR partition.  The layout is now what NP-FW-EMMC-001 already specified:
 *
 *   UHDR  one file per SESSION — EMMC-UHDR-12 / -13: /uhdr/sessions/<counter>,
 *         the session counter zero-padded to 20 digits.  Opened by
 *         np_log_backend_session_begin() and closed by _session_end(); created
 *         EXCLUSIVELY, so an existing session file is never appended to or
 *         overwritten — a reused counter fails closed and loses that session's
 *         log, never an earlier one's.  Nothing is written to UHDR outside a
 *         session (NP_HUB_ERR_NO_SESSION), which also means UHDR is first opened
 *         after np_uhdr_key_unlock() has mounted it, not at boot.
 *   SHDR  one append file for the partition, as before.  It cannot reach
 *         file_max: the whole SHDR partition is 512 MiB.
 *
 * A single session file is capped at NP_LOG_SEGMENT_MAX_BYTES (= file_max).
 * Reaching it fails the rest of THAT session closed (NP_HUB_ERR_LOG_FULL) and
 * clears at the next session_begin.  At the EEG rate (≈12 kB/s) the cap is
 * ≈49 hours of one session.
 */

#ifndef NP_LOG_BACKEND_H
#define NP_LOG_BACKEND_H

#include "np_hub_types.h"

/* Coalescing granularity — one eMMC/LittleFS program block. */
#define NP_LOG_STAGE_BYTES 512U

/* Largest single log file littlefs will hold: file_max, EMMC-FS-01
 * (NP_LFS_LOG_FILE_MAX in np_lfs_log_instance.h).  OI-LFS-11. */
#define NP_LOG_SEGMENT_MAX_BYTES 2147483647ULL

/* Partition selector for the lower LittleFS-file HAL. */
typedef enum {
    NP_LOG_PART_UHDR = 0,
    NP_LOG_PART_SHDR = 1,
} np_log_part_t;

/*
 * np_log_backend_init — reset staging state and open the SHDR log file.
 * Call once at hub bring-up, before np_log_init().  Idempotent.
 * UHDR is NOT opened here: its files are per session (see "File layout").
 * Returns NP_HUB_OK if the SHDR log opened; the open error otherwise (SHDR is
 * then marked faulted and rejects appends).
 */
np_hub_status_t np_log_backend_init(void);

/*
 * np_log_backend_session_begin — end any open UHDR session file (flushing its
 * tail into IT, not into the new one) and create the file for
 * `session_counter`.  Clears a UHDR fault left by the previous session.
 * Returns the open error if the file could not be created —
 * NP_HUB_ERR_LOG_EXISTS when it already exists — and UHDR appends then fail
 * until the next session_begin.
 */
np_hub_status_t np_log_backend_session_begin(uint64_t session_counter);

/*
 * np_log_backend_session_end — flush the UHDR tail, sync and close the session
 * file.  UHDR appends then return NP_HUB_ERR_NO_SESSION.  Idempotent.
 */
np_hub_status_t np_log_backend_session_end(void);

/* ── Session-logger HAL entry points (OI-LOG-01..04) ──────────────────────────
 *
 * Implemented in np_log_backend.c; also declared (identically) in
 * np_session_log.h, which is their caller.  Appends are plaintext; the mounted
 * partition encrypts at rest (see file banner).
 */
np_hub_status_t np_log_hal_uhdr_append(const uint8_t *buf, size_t len); /* OI-LOG-01 */
np_hub_status_t np_log_hal_shdr_append(const uint8_t *buf, size_t len); /* OI-LOG-02 */
np_hub_status_t np_log_hal_uhdr_flush(void);                            /* OI-LOG-03 */
np_hub_status_t np_log_hal_shdr_flush(void);                            /* OI-LOG-04 */

/* ── Lower LittleFS-file HAL (OI-LOG-05..07) ─────────────────────────────────
 *
 * Implemented by the platform LittleFS glue on target; host-modeled in
 * np_log_backend.c under NPTEST_HOST.  These operate on the XTS-mounted
 * partition, so bytes are encrypted at rest without this layer seeing keys.
 *
 * NP-SOUP-LFS-001 Rev 4 §13 binds that glue three ways.  It mounts each
 * partition with np_lfs_log_config_apply()/_validate() and calls
 * np_lfs_log_prime_allocator() once after mount (np_lfs_log_instance.h).  It
 * must be added to LFS_CALLERS in scripts/check-lfs-caller-rules.ts, with its
 * reason, or CI fails — the one-handle-per-file rule (OI-LFS-06) is enforced
 * there.  UHDR opens are exclusive creates of one session file each (see
 * "File layout" above, OI-LFS-11); SHDR is one file.
 */

/* OI-LOG-05: open the append-mode log file on `part`.
 *   UHDR: CREATE /uhdr/sessions/<segment, %020llu> — exclusively; an existing
 *         file returns NP_HUB_ERR_LOG_EXISTS and is never reopened
 *         (EMMC-UHDR-12/-13, OI-LFS-11).
 *   SHDR: open (create if absent) the partition's single log file; `segment`
 *         is ignored and is passed as 0.                                       */
extern np_hub_status_t np_log_hal_part_open(np_log_part_t part, uint64_t segment);

/* OI-LOG-06: append `len` bytes to the open log file on `part` (lfs_file_write).*/
extern np_hub_status_t np_log_hal_part_append(np_log_part_t part,
                                              const uint8_t *buf, size_t len);

/* OI-LOG-07: durably sync the log file + block device on `part` (lfs_file_sync).*/
extern np_hub_status_t np_log_hal_part_sync(np_log_part_t part);

/* OI-LFS-11: sync and close the open log file on `part` (lfs_file_close).       */
extern np_hub_status_t np_log_hal_part_close(np_log_part_t part);

#ifdef NPTEST_HOST
/* ── Host-test inspection hooks (NPTEST_HOST only) ────────────────────────────
 * Let the unit tests read back what the backend committed and how many syncs
 * it issued, without a real eMMC.  Never compiled into the target image.
 */
void        np_log_test_reset(void);                 /* clear captured state    */
size_t      np_log_test_captured_len(np_log_part_t part);
const uint8_t *np_log_test_captured(np_log_part_t part);
unsigned    np_log_test_sync_count(np_log_part_t part);
void        np_log_test_fail_next_append(np_log_part_t part); /* inject 1 error  */
void        np_log_test_set_capacity(np_log_part_t part, uint64_t bytes);
void        np_log_test_set_segment_cap(np_log_part_t part, uint64_t bytes);
unsigned    np_log_test_open_count(np_log_part_t part);   /* HAL opens        */
unsigned    np_log_test_close_count(np_log_part_t part);  /* HAL closes       */
uint64_t    np_log_test_last_segment(np_log_part_t part); /* last opened id   */
/* Bytes captured into the file opened `nth` (0-based) on `part`. */
size_t      np_log_test_segment_len(np_log_part_t part, unsigned nth);
#endif

#endif /* NP_LOG_BACKEND_H */
