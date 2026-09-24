/*
 * np_lfs_log_instance.h — the UHDR and SHDR littlefs instances (OI-LFS-05)
 * Document: NP-SOUP-LFS-001 Rev 4 §13.1 (OI-LFS-05, claim L-5 for the logs),
 *           NP-FW-EMMC-001 EMMC-FS-01, EMMC-UHDR-05, EMMC-SHDR-03
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * First-party NeurOne code; not part of the vendored component.
 *
 * ── What was open ─────────────────────────────────────────────────────────────
 * OI-LFS-05 asked for the log partitions' instance parameters, on the reading
 * that EMMC-FS-01 states only the Config partition's.  That reading was wrong:
 * EMMC-FS-01's table (NP-FW-EMMC-001 §5.2) has a UHDR and an SHDR column with
 * all twelve parameters.  So the decision is not "choose twelve values" but
 * "take EMMC-FS-01's, and find out whether they survive" — and one pair does
 * not.  NP-SOUP-LFS-001 §13.1 has the field-by-field disposition; the short
 * form is below, beside each value.
 *
 * ── The one deviation: read_size and prog_size are 512, not 256 ───────────────
 * EMMC-FS-01 prints 256.  EMMC-UHDR-05 (and EMMC-SHDR-03 by reference) sets the
 * AES-XTS data unit to 512 bytes "matching the LittleFS read_size and
 * prog_size".  Both cannot hold, and the clause that states its reason wins:
 * XTS encrypts a whole data unit under one tweak, so a 256-byte program into a
 * 512-byte unit forces the encryption layer to read, decrypt, merge,
 * re-encrypt and rewrite the WHOLE unit.  A power loss inside that rewrite
 * damages the other 256 bytes — bytes littlefs already committed and synced,
 * and relies on.  That breaks the one property of struct lfs_config that
 * L-1…L-4 rest on (a program does not disturb anything outside itself), and
 * np_lfs_log_instance_tests shows it: under a 512-byte read-modify-write
 * model, the append sweep that passes at prog_size 512 FAILS at 256.  Raised
 * against NP-FW-EMMC-001 as ECR-EMMC-002 (NP-SOUP-LFS-001 §13.1.3); the same
 * finding landed on the Config instance as OI-LFS-10, which moved it to 512
 * too (NP-SOUP-LFS-001 §13.8).  ECR-EMMC-002 was APPLIED 2026-09-24:
 * NP-FW-EMMC-001 Rev 3 prints 512 in all three columns
 * (editscripts/patch_emmc_ecr002_prog512.py), so 512 is now the specified
 * value, not a deviation from it.
 *
 * ── Taken from EMMC-FS-01 as written ──────────────────────────────────────────
 * block_size, block_count, cache_size, lookahead_size, block_cycles, file_max,
 * name_max, attr_max, metadata_max.  Two of them were examined hard and kept:
 *
 * lookahead_size (UHDR 512 B, SHDR 256 B) covers 4,096 and 2,048 blocks — a
 * 16 MiB / 8 MiB allocation window over partitions of 1,767,168 and 131,072
 * blocks.  Each time a window is used up littlefs traverses the WHOLE
 * filesystem (lfs_alloc_scan), at a cost proportional to the blocks in use,
 * and the writer waits for it.  Full coverage would move every traversal to
 * mount — and cost 220,896 B + 16,384 B of static RAM on a part with no
 * external SDRAM (NP-SW-CI-001 §4.13, OI-SWCI-46).  What fails if the window
 * stays small is an unmeasured stall of the session logger, whose duration is
 * a property of the eMMC (per-read latency × blocks in use) that no host can
 * measure.  CLAUDE.md §18: no constraint without a failure it prevents, so the
 * specified value stands, the read COUNT per traversal is measured on the
 * host (np_lfs_log_instance_tests), and the TIME is a bring-up measurement
 * under OI-LFS-07.  What is done now costs nothing: np_lfs_log_prime_allocator()
 * runs the first traversal at mount, not at a session's first append.
 *
 * file_max LFS_FILE_MAX (2 GiB − 1) is right for EMMC-UHDR-12's one-file-per-
 * session layout and WRONG for np_log_backend.h's "the append-mode log file"
 * — a single file cannot exceed 2 GiB, a third of the UHDR partition.  That is
 * a finding against the log backend's file layout, not against this value;
 * carried to OI-LOG-05 (NP-SOUP-LFS-001 §13.1.4).
 */

#ifndef NP_LFS_LOG_INSTANCE_H
#define NP_LFS_LOG_INSTANCE_H

#include "np_hub_types.h"
#include "np_log_backend.h"   /* np_log_part_t */
#include "lfs.h"

/* ── Common to both log instances ─────────────────────────────────────────── */
#define NP_LFS_LOG_BLOCK_SIZE      4096u    /* EMMC-FS-01, EMMC-FS-02            */
#define NP_LFS_LOG_READ_SIZE       512u     /* EMMC-FS-01 Rev 3 (ECR-EMMC-002)   */
#define NP_LFS_LOG_PROG_SIZE       512u     /* EMMC-FS-01 Rev 3 (ECR-EMMC-002)   */
#define NP_LFS_LOG_CACHE_SIZE      4096u    /* EMMC-FS-01                        */
#define NP_LFS_LOG_BLOCK_CYCLES    500      /* EMMC-FS-01; L-6 (and OI-LFS-07)   */
#define NP_LFS_LOG_FILE_MAX        2147483647u /* EMMC-FS-01 = LFS_FILE_MAX      */
#define NP_LFS_LOG_NAME_MAX        255u     /* EMMC-FS-01 = LFS_NAME_MAX         */
#define NP_LFS_LOG_ATTR_MAX        1022u    /* EMMC-FS-01 = LFS_ATTR_MAX         */
#define NP_LFS_LOG_METADATA_MAX    4096u    /* EMMC-FS-01 (= block_size)         */

/* ── Per partition ────────────────────────────────────────────────────────── */
#define NP_LFS_LOG_UHDR_BLOCK_COUNT  1767168u  /* 6,903 MiB / 4 KiB — EMMC-FS-01 */
#define NP_LFS_LOG_SHDR_BLOCK_COUNT  131072u   /*   512 MiB / 4 KiB — EMMC-FS-01 */
#define NP_LFS_LOG_UHDR_LOOKAHEAD    512u      /* EMMC-FS-01 — see header        */
#define NP_LFS_LOG_SHDR_LOOKAHEAD    256u      /* EMMC-FS-01 — see header        */

/* Per-open-file cache: cache_size, caller-owned under LFS_NO_MALLOC. */
#define NP_LFS_LOG_FILE_BUFFER_SIZE  NP_LFS_LOG_CACHE_SIZE

/* Static RAM both instances cost (read + prog cache + lookahead, each), for
 * the NP-SW-CI-001 §4.10 budget: 2 x 8,192 + 512 + 256 = 17,152 B. */
#define NP_LFS_LOG_STATIC_RAM_BYTES \
    ((2u * 2u * NP_LFS_LOG_CACHE_SIZE) + NP_LFS_LOG_UHDR_LOOKAHEAD + \
     NP_LFS_LOG_SHDR_LOOKAHEAD)

/* Blocks in `part`'s instance, or 0 for an unknown partition. */
lfs_size_t np_lfs_log_block_count(np_log_part_t part);

/*
 * Fill the littlefs-owned fields of `cfg` for `part` and point its buffers at
 * that partition's static storage.  Leaves context, read/prog/erase/sync
 * (OI-LOG-05..07) and lock/unlock to the caller, exactly as
 * np_lfs_config_apply() does for Config.
 * NP_HUB_ERR_INVALID_ARG for a NULL cfg or an unknown partition.
 */
np_hub_status_t np_lfs_log_config_apply(np_log_part_t part,
                                        struct lfs_config *cfg);

/*
 * The mount-time check (claim L-5, now for the logs).  Every field above by
 * VALUE; the buffers present; the block device and lock bound.
 * NP_HUB_ERR_BAD_VERSION for any configuration that is not `part`'s —
 * including the Config instance's, and including EMMC-FS-01 Rev 2's 256.
 */
np_hub_status_t np_lfs_log_config_validate(np_log_part_t part,
                                           const struct lfs_config *cfg);

/*
 * Call once, immediately after a successful lfs_mount() of a log instance and
 * before the first append: runs lfs_fs_gc(), which performs littlefs's first
 * allocator traversal NOW — at mount, where a delay costs nothing — instead of
 * inside the first session's first append.  It is not a substitute for the
 * OI-LFS-07 timing measurement; it moves the one traversal that is certain.
 * NP_HUB_ERR_STORE_IO if the traversal failed (a read error: remount).
 */
np_hub_status_t np_lfs_log_prime_allocator(lfs_t *lfs);

#endif /* NP_LFS_LOG_INSTANCE_H */
