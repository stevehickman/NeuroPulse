/*
 * np_cfg_store.h — the Config-partition file store over littlefs
 * Document: NP-SOUP-LFS-001 Rev 4 §13 (OI-LFS-06, OI-LFS-08, OI-LFS-09,
 *           REQ-LFS-01 / OI-LFS-03), NP-FW-NVRAM-001 Rev 2 §3.3.1, §4
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * First-party NeurOne code; not part of the vendored component.
 *
 * ── What this file is for ─────────────────────────────────────────────────────
 * NP-SOUP-LFS-001 §11 evaluated littlefs v2.11.3's anomaly list and found that
 * what stops the applicable anomalies is not the component but CALLER rules —
 * and the callers did not exist.  This module is those callers for the Config
 * instance (EMMC-FS-01, partition 4).  Each rule is a property of this API, so
 * a caller cannot forget one:
 *
 *   OI-LFS-06  one open handle per file.  Every lfs_file_opencfg() on the
 *              instance goes through one static function that refuses a second
 *              handle on a file already open (NP_HUB_ERR_STORE_BUSY), and no
 *              handle outlives the call that opened it.  scripts/
 *              check-lfs-caller-rules.ts makes this checkable: no other
 *              translation unit in firmware/ may call littlefs at all.
 *
 *   OI-LFS-09  validate by content, on every read.  There is no unverified read
 *              and no stat: every read takes a mandatory content check and runs
 *              it on the bytes just read (upstream #1164 — a file can exist
 *              after a power loss with none of its data, so presence is not
 *              durability).  A block-device read error is reported as an
 *              absence, is NEVER retried, and forces a remount before the next
 *              operation (upstream #1205 — littlefs leaves stale bytes in its
 *              read cache after an error, and a retry through that cache
 *              returns them reporting success).
 *
 *   OI-LFS-08  bounded create/delete churn, and ukmd.rec on two entries.  The
 *              store never removes and never renames.  A whole-file replacement
 *              is an in-place O_TRUNC rewrite that littlefs commits atomically
 *              at close — so the Config directory sees one create per file for
 *              the life of the device and zero deletes, which is the bound
 *              upstream #1210's trigger (repeated create/delete of a lower-id
 *              file in the same metadata pair) cannot get past.  ukmd.rec is
 *              additionally held as two copies in two directories — two
 *              metadata pairs — so no single lost entry loses the record.
 *
 *   REQ-LFS-01 no emission-bounding value under a tail-additive policy.  Every
 *              file carries its policy in one table, and the policy decides
 *              which API may touch it: a rebuild-cache file cannot be appended
 *              to and a tail-additive journal cannot be read through the
 *              rebuild-cache reader.  Who may call the journal reader at all is
 *              the OI-LFS-03 gate's business, not this file's.
 *
 * ── What this file is NOT ─────────────────────────────────────────────────────
 * The block device.  read/prog/erase/sync over the XTS-mounted partition are
 * still OI-LOG-05..07 and still unwritten; this module takes a validated
 * struct lfs_config and does not care what is under it.  Nor is it the log
 * partitions: those are np_lfs_log_instance.h, and their files are the log
 * backend's.  Nor is anything here wired into bring-up yet — the HAL seams
 * np_hexmap_nvram_*() and np_uhdr_hal_config_*_ukmd() are the places it will
 * be bound, once a block device exists to mount (#340).
 */

#ifndef NP_CFG_STORE_H
#define NP_CFG_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "np_hub_types.h"
#include "lfs.h"

/* ── The files, and the policy each one is kept under ──────────────────────────
 * One table (np_cfg_store.c, s_files[]) is the only place a Config file is
 * named.  A file not in it cannot be opened through this API, and a new one
 * needs a policy — which is REQ-LFS-01's question asked at the one place a file
 * is added.
 */
typedef enum {
    NP_CFG_FILE_NPMP = 0,   /* "NPMP" module-map blob — np_module_map        */
    NP_CFG_FILE_MAP3,       /* Map 3 journal — NP-FW-NVRAM-001 §4.2          */
    NP_CFG_FILE_UKMD,       /* ukmd.rec — NP-FW-EMMC-002 §C.3, D-22          */
    NP_CFG_FILE_SESSION_COUNT, /* device session count — EMMC-SHDR-09, OI-LFS-12 */
    NP_CFG_FILE_WARRANTY_TOKEN, /* warranty token — NP-FW-EMMC-002 §A.2, OI-WA-03 */
    NP_CFG_FILE_COUNT
} np_cfg_file_t;

typedef enum {
    /* A CACHE of facts something will re-answer.  Replaced whole; on any
     * integrity failure the caller rebuilds (§5.3 step 2), so a wrong value
     * becomes an empty one.  The only policy under which a value that bounds
     * an emission may be stored. */
    NP_CFG_POLICY_REBUILD = 0,
    /* A RECORD of facts nothing will re-answer.  Appended, never rewritten,
     * never truncated (D-5).  REQ-LFS-01: nothing stored under it may ever be
     * read as a limit. */
    NP_CFG_POLICY_TAIL_ADDITIVE,
    /* A fixed-size record whose loss is unrecoverable and whose integrity the
     * store cannot judge by meaning (ukmd.rec's GCM tag needs the user's key).
     * Two copies in two metadata pairs, each in a checksummed envelope. */
    NP_CFG_POLICY_REPLICATED,
    /* Returned for a file id outside the table.  Matches no API. */
    NP_CFG_POLICY_INVALID = -1,
} np_cfg_policy_t;

/* The policy a file is kept under, or NP_CFG_POLICY_INVALID. */
np_cfg_policy_t np_cfg_store_policy(np_cfg_file_t file);

/* ── Content checks (OI-LFS-09) ────────────────────────────────────────────────
 * Mandatory on every read.  NULL is refused with NP_HUB_ERR_INVALID_ARG: an
 * unverified read is not a mode this store has.
 */

/* Whole-content check for a REBUILD file: the blob CRC, magic, length. */
typedef bool (*np_cfg_verify_fn)(const uint8_t *buf, size_t len, void *ctx);

/* Per-record check for a TAIL_ADDITIVE journal.  Called once per fixed-size
 * record, in order; the first record that fails ends the valid prefix. */
typedef bool (*np_cfg_record_verify_fn)(const uint8_t *rec, size_t rec_len,
                                        uint32_t index, void *ctx);

/* ── Lifecycle ─────────────────────────────────────────────────────────────────
 *
 * np_cfg_store_bind() is a REBOOT: it forgets every piece of RAM state —
 * mounted flag, open handles, remount-pending flag, cached generations —
 * exactly as a power cycle does.  `cfg` must already have its block device and
 * lock bound; np_cfg_store_mount() validates it with np_lfs_config_validate()
 * and refuses anything that is not the EMMC-FS-01 instance.
 *
 * `lock`/`unlock` serialise the store's own state (the handle registry and the
 * remount flag) across the three specified writers, which are not one task.
 * They are NOT cfg->lock: littlefs takes that one inside every lfs_* call, and
 * a store lock held across an lfs_* call must be a different mutex or the call
 * deadlocks.  Both are required.
 */
typedef void (*np_cfg_lock_fn)(void);

np_hub_status_t np_cfg_store_bind(lfs_t *lfs, const struct lfs_config *cfg,
                                  np_cfg_lock_fn lock, np_cfg_lock_fn unlock);

/*
 * Manufacture-time only: format the partition and lay out the directories
 * the replicated copies live in.  Never called in the field — a field format
 * is a factory reset and is np_factory_reset's decision, not this module's.
 */
np_hub_status_t np_cfg_store_format(void);

/*
 * Validate the instance, mount it, and confirm both replica directories exist
 * (creating any that are missing — which happens at most once per directory
 * for the life of the partition, and is counted as churn).
 */
np_hub_status_t np_cfg_store_mount(void);

void np_cfg_store_unmount(void);

/* ── REBUILD files ─────────────────────────────────────────────────────────────
 *
 * np_cfg_store_read: open, read all of it, close, THEN run `verify` over what
 * was read.  NP_HUB_OK only if the read completed and `verify` accepted.
 *   NP_HUB_ERR_NOT_PRESENT      the file has never been written
 *   NP_HUB_ERR_STORE_IO         a read failed — absence; remount pending
 *   NP_HUB_ERR_STORE_INTEGRITY  `verify` refused, or the file exceeds `cap`
 * Every one of them means "rebuild"; none of them means "use what you have".
 *
 * np_cfg_store_replace: O_CREAT|O_TRUNC in place, write, close.  littlefs
 * commits the truncation and the new contents together at close, so a power
 * loss leaves the old file or the new one (claim L-3, re-swept against this
 * ordering in np_cfg_store_tests).  No temp file, no rename, no remove.
 */
np_hub_status_t np_cfg_store_read(np_cfg_file_t file, uint8_t *buf, size_t cap,
                                  size_t *out_len, np_cfg_verify_fn verify,
                                  void *ctx);

np_hub_status_t np_cfg_store_replace(np_cfg_file_t file, const uint8_t *buf,
                                     size_t len);

/* ── TAIL_ADDITIVE journals ────────────────────────────────────────────────────
 *
 * np_cfg_store_journal_append: O_APPEND, write one record, close (which
 * syncs).  One flush per record, D-5.
 *
 * np_cfg_store_journal_read: read the whole journal and run `verify` over each
 * `rec_len`-byte record in order.  `*out_count` is the length of the valid
 * prefix, in records; a torn tail ends the prefix and is not an error (L-4:
 * a torn write costs one record).  The bytes past the valid prefix in `buf`
 * are unspecified and must not be read.
 *
 * REQ-LFS-01: the output of this function is HISTORY.  It must never become
 * the source of a limit.  scripts/check-lfs-caller-rules.ts restricts who may
 * call it, and each permitted caller is listed there with the reason it reads
 * history rather than a bound.
 */
np_hub_status_t np_cfg_store_journal_append(np_cfg_file_t file,
                                            const uint8_t *rec, size_t rec_len);

np_hub_status_t np_cfg_store_journal_read(np_cfg_file_t file, uint8_t *buf,
                                          size_t cap, size_t rec_len,
                                          uint32_t *out_count,
                                          np_cfg_record_verify_fn verify,
                                          void *ctx);

/* ── REPLICATED records ────────────────────────────────────────────────────────
 *
 * Fixed-size payload, NP_CFG_REPLICA_MAX_PAYLOAD bytes at most.  Each copy is
 * stored in an envelope — magic, generation, length, payload, CRC-32 over all
 * of it — because the store must be able to tell a good copy from a bad one
 * without the key that gives the payload meaning.  The payload's own integrity
 * (ukmd.rec's AES-GCM tag) is still checked by its owner at every use; the
 * envelope does not replace that, it decides which copy to hand over.
 *
 * np_cfg_store_replicated_write: generation = newest valid + 1; write copy A,
 * then copy B, each an atomic in-place replacement.  A power loss between the
 * two leaves A newer than B, which the next read resolves.
 *
 * np_cfg_store_replicated_read: read both, keep the valid copy with the
 * higher generation (A on a tie), and REPAIR the other one if it is missing,
 * invalid or older — so a copy lost to upstream #1210, a torn write or a bad
 * block is restored the first time the record is used, rather than being
 * discovered when its twin fails too.  A failed repair does not fail the
 * read: the caller already has a verified record, and the repair is retried
 * on the next read.  When neither copy is valid the status says why:
 *   NP_HUB_ERR_NOT_PRESENT      neither entry exists — never written, or both
 *                               entries lost
 *   NP_HUB_ERR_STORE_IO         a copy could not be read (remount pending)
 *   NP_HUB_ERR_STORE_INTEGRITY  every copy that exists was read and refused
 * Only the first is an absence.  A caller that creates the record when it is
 * absent (np_warranty_token) must treat the other two as "unknown", never as
 * "absent" — until 2026-09-25 all three were reported as NP_HUB_ERR_NOT_PRESENT.
 */
#define NP_CFG_REPLICA_MAX_PAYLOAD   256U

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file,
                                              const uint8_t *payload,
                                              size_t len);

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file,
                                             uint8_t *payload, size_t len);

/* ── Observability ─────────────────────────────────────────────────────────────
 * Counters since the last np_cfg_store_bind().  They are what makes OI-LFS-08's
 * bound and OI-LFS-09's remount observable rather than asserted: a test (and,
 * later, the SHDR predictive-maintenance record) reads them.  Every field is a
 * device-condition count — no file contents, no timing — and is SHDR-safe
 * under CLAUDE.md §5.1's defining test.
 */
typedef struct {
    uint32_t creates;          /* files or directories brought into existence */
    uint32_t removes;          /* lfs_remove / lfs_rename calls — always 0     */
    uint32_t read_errors;      /* block-device read failures seen              */
    uint32_t remounts;         /* remounts forced by a read error              */
    uint32_t integrity_fails;  /* content checks that refused                  */
    uint32_t repairs;          /* replicated copies rewritten from their twin  */
    uint32_t busy_refusals;    /* second-handle opens refused                  */
} np_cfg_store_stats_t;

void np_cfg_store_stats(np_cfg_store_stats_t *out);

#ifdef NPTEST_HOST
/* Host-test hook: attempt to open `file` (copy 0) while holding it open, to
 * prove the registry refuses the second handle.  Returns the status of the
 * SECOND open.  Never compiled into a target image. */
np_hub_status_t np_cfg_store_test_double_open(np_cfg_file_t file);
#endif

#endif /* NP_CFG_STORE_H */
