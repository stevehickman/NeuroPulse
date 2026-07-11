/*
 * NeurOne Hub Control Program — Session Log eMMC Backing (OI-LOG-01..04)
 * Document: NP-FW-HUB-001 Rev A §6, NP-FW-EMMC-001 Rev A §4–§7, §12
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
 */

#ifndef NP_LOG_BACKEND_H
#define NP_LOG_BACKEND_H

#include "np_hub_types.h"

/* Coalescing granularity — one eMMC/LittleFS program block. */
#define NP_LOG_STAGE_BYTES 512U

/* Partition selector for the lower LittleFS-file HAL. */
typedef enum {
    NP_LOG_PART_UHDR = 0,
    NP_LOG_PART_SHDR = 1,
} np_log_part_t;

/*
 * np_log_backend_init — open both partition log files and reset staging state.
 * Call once at hub bring-up, before np_log_init().  Idempotent.
 * Returns NP_HUB_OK if both log files opened; the first open error otherwise
 * (that partition is then marked faulted and rejects appends).
 */
np_hub_status_t np_log_backend_init(void);

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
 */

/* OI-LOG-05: open (create if absent) the append-mode log file on `part`.       */
extern np_hub_status_t np_log_hal_part_open(np_log_part_t part);

/* OI-LOG-06: append `len` bytes to the open log file on `part` (lfs_file_write).*/
extern np_hub_status_t np_log_hal_part_append(np_log_part_t part,
                                              const uint8_t *buf, size_t len);

/* OI-LOG-07: durably sync the log file + block device on `part` (lfs_file_sync).*/
extern np_hub_status_t np_log_hal_part_sync(np_log_part_t part);

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
#endif

#endif /* NP_LOG_BACKEND_H */
