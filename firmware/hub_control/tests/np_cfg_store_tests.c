/*
 * NeurOne SW-02 — Config store tests (OI-LFS-06, OI-LFS-08, OI-LFS-09)
 * Document: NP-SOUP-LFS-001 Rev 4 §13
 *
 * ── What this suite is for ───────────────────────────────────────────────────
 *
 * NP-SOUP-LFS-001 §11 found that littlefs v2.11.3's applicable anomalies are
 * stopped by caller rules, and that the callers did not exist.  np_cfg_store is
 * those callers for the Config instance.  This suite shows each rule holding,
 * and — per NP-CONV-001 §8 — shows each check able to fail:
 *
 *   1. policy        every API refuses a file kept under a different policy
 *   2. OI-LFS-06     a second handle on an open file is refused
 *   3. OI-LFS-09     every read is content-verified; presence is not durability
 *   4. OI-LFS-09     upstream #1205 REPRODUCED on the pinned version through raw
 *                    littlefs — a retry after a read error returns stale bytes
 *                    reporting success — and then shown NOT to reach a caller
 *                    of the store, which reports an absence and remounts
 *   5. OI-LFS-08     create/delete churn: one create per file for the life of
 *                    the partition, zero removes, over a thousand operations
 *   6. OI-LFS-08     ukmd.rec survives the loss of either entry and is repaired
 *                    from its twin the first time it is read
 *   7. L-3, L-4,     the store's own orderings swept under a power cut at every
 *      replicated    medium-touching op, three tear models: the in-place O_TRUNC
 *                    replacement (which replaces §12's write-temp-then-rename),
 *                    the journal append, and the two-copy write
 *   8. falsified     the same verifiers are required to catch the unsafe
 *                    orderings — remove-then-write, and both copies lost
 *
 * The instance is the real one: np_lfs_config_apply() / _validate(), EMMC-FS-01's
 * 4,096 x 4,096 geometry, over NeurOne's own block device (np_lfs_powerbd.c).
 *
 * ── What this suite does NOT prove ───────────────────────────────────────────
 *
 * That upstream #1210 cannot happen.  It was not reproduced on the pinned
 * version (NP-SOUP-LFS-001 §13.4 records the attempt), so this suite cannot
 * show the store preventing it.  What it shows is the two things the store
 * does about it: the trigger (create/delete churn) is bounded to a constant,
 * and the one file whose loss is unrecoverable no longer rests on one entry.
 * Nor the eMMC (OI-LFS-07), nor integration (OI-LOG-05..07).
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "np_cfg_store.h"
#include "np_crypto.h"        /* np_crc32 — a checker independent of the store */
#include "np_lfs_config.h"
#include "np_lfs_instance.h"
#include "np_lfs_powerbd.h"
#include "np_lfs_sweep.h"

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));  \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

/* ── The instance under test ──────────────────────────────────────────────── */

#define MEDIA_BYTES ((size_t)NP_LFS_CFG_BLOCK_SIZE * NP_LFS_CFG_BLOCK_COUNT)

static uint8_t           g_media[MEDIA_BYTES];
static uint8_t           g_dirty[(NP_LFS_CFG_BLOCK_COUNT + 7U) / 8U];
static lfs_t             g_lfs;
static struct lfs_config g_cfg;
static np_powerbd_t      g_bd;

static int  host_lock(const struct lfs_config *c)   { (void)c; return 0; }
static int  host_unlock(const struct lfs_config *c) { (void)c; return 0; }
static void store_lock(void)   { }
static void store_unlock(void) { }

/* A reboot: the store forgets everything it held in RAM. */
static void reboot(void)
{
    memset(&g_lfs, 0, sizeof(g_lfs));
    (void)np_cfg_store_bind(&g_lfs, &g_cfg, store_lock, store_unlock);
}

static void build(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    (void)np_lfs_config_apply(&g_cfg);
    np_powerbd_bind(&g_bd, &g_cfg, g_media, NP_LFS_CFG_BLOCK_SIZE,
                    NP_LFS_CFG_BLOCK_COUNT, NP_LFS_CFG_PROG_SIZE,
                    g_dirty, sizeof(g_dirty));
    g_cfg.lock   = host_lock;
    g_cfg.unlock = host_unlock;
    if (np_lfs_config_validate(&g_cfg) != NP_HUB_OK) {
        printf("FAIL [build] the store's instance is not the EMMC-FS-01 one\n");
        g_fail_count++;
    }
    np_sweep_bind(&g_bd, reboot);
}

/* Fresh partition, formatted and mounted through the store. */
static void fresh(void)
{
    np_powerbd_wipe(&g_bd);
    np_powerbd_power_cycle(&g_bd);
    reboot();
    ASSERT(np_cfg_store_format() == NP_HUB_OK, "format");
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "mount");
}

/* ── Content shapes ───────────────────────────────────────────────────────── */

/* The "NPMP" blob — NP-SOUP-LFS-001 §5.3: HDR(8) + 80 x 175 + CRC(4). */
#define BLOB_BYTES  (8U + (80U * 175U) + 4U)   /* 14,012 */

static uint8_t g_blob[BLOB_BYTES];
static uint8_t g_rb[BLOB_BYTES + 512U];

static void blob_build(uint8_t *out, uint8_t generation)
{
    memcpy(out, "NPMP", 4U);
    out[4] = generation;
    out[5] = out[6] = out[7] = 0U;
    for (unsigned i = 8U; i < BLOB_BYTES - 4U; i++) {
        /* Position-unique within each 256 B, so a block's bytes can be found
         * on the medium and two different offsets never read the same. */
        out[i] = (uint8_t)((generation * 7U) ^ (i & 0xFFU) ^ ((i >> 8) * 13U));
    }
    uint32_t crc = np_crc32(out, BLOB_BYTES - 4U);
    out[BLOB_BYTES - 4U] = (uint8_t)(crc & 0xFFU);
    out[BLOB_BYTES - 3U] = (uint8_t)((crc >> 8) & 0xFFU);
    out[BLOB_BYTES - 2U] = (uint8_t)((crc >> 16) & 0xFFU);
    out[BLOB_BYTES - 1U] = (uint8_t)((crc >> 24) & 0xFFU);
}

/* The rebuild-cache content check a real np_module_map caller would supply. */
static bool blob_verify(const uint8_t *buf, size_t len, void *ctx)
{
    (void)ctx;
    if (len != BLOB_BYTES || memcmp(buf, "NPMP", 4U) != 0) {
        return false;
    }
    uint32_t crc = np_crc32(buf, (uint32_t)(BLOB_BYTES - 4U));
    uint32_t stored = (uint32_t)buf[BLOB_BYTES - 4U] |
                      ((uint32_t)buf[BLOB_BYTES - 3U] << 8) |
                      ((uint32_t)buf[BLOB_BYTES - 2U] << 16) |
                      ((uint32_t)buf[BLOB_BYTES - 1U] << 24);
    return crc == stored;
}

static bool accept_anything(const uint8_t *buf, size_t len, void *ctx)
{
    (void)buf; (void)len; (void)ctx;
    return true;
}

/* 32-byte self-checking journal record — NP-FW-NVRAM-001 §4.2 D-5. */
#define REC_SIZE 32U

static void rec_build(uint8_t out[REC_SIZE], uint32_t ordinal)
{
    memcpy(out, "NPM3", 4U);
    out[4] = (uint8_t)(ordinal & 0xFFU);
    out[5] = (uint8_t)((ordinal >> 8) & 0xFFU);
    out[6] = (uint8_t)((ordinal >> 16) & 0xFFU);
    out[7] = (uint8_t)((ordinal >> 24) & 0xFFU);
    for (unsigned i = 8U; i < 28U; i++) {
        out[i] = (uint8_t)(ordinal * 31U + i);
    }
    uint32_t crc = np_crc32(out, 28U);
    out[28] = (uint8_t)(crc & 0xFFU);
    out[29] = (uint8_t)((crc >> 8) & 0xFFU);
    out[30] = (uint8_t)((crc >> 16) & 0xFFU);
    out[31] = (uint8_t)((crc >> 24) & 0xFFU);
}

static bool rec_verify(const uint8_t *rec, size_t rec_len, uint32_t index,
                       void *ctx)
{
    (void)ctx;
    uint8_t expect[REC_SIZE];
    if (rec_len != REC_SIZE) {
        return false;
    }
    rec_build(expect, index);
    return memcmp(rec, expect, REC_SIZE) == 0;
}

/* ukmd.rec's shape: 192 bytes (np_ukmd_record_t).  The store cannot check the
 * GCM tag — np_uhdr_key does, at every unlock — so the payload is opaque here. */
#define UKMD_BYTES 192U

static void ukmd_build(uint8_t out[UKMD_BYTES], uint8_t v)
{
    for (unsigned i = 0U; i < UKMD_BYTES; i++) {
        out[i] = (uint8_t)(v ^ (i * 5U));
    }
}

/* ── Raw littlefs access, for the tests that need to act BEHIND the store ────
 * Test code is outside the gate's scope (scripts/check-lfs-caller-rules.ts
 * excludes tests/): these helpers stand in for a filesystem defect or a torn
 * medium, which is exactly what the store must survive.
 */
static uint8_t g_raw_cache[NP_LFS_FILE_BUFFER_SIZE];

static int raw_open(lfs_file_t *f, const char *path, int flags)
{
    struct lfs_file_config fcfg;
    memset(&fcfg, 0, sizeof(fcfg));
    fcfg.buffer = g_raw_cache;
    return lfs_file_opencfg(&g_lfs, f, path, flags, &fcfg);
}

static bool raw_exists(const char *path)
{
    struct lfs_info info;
    return lfs_stat(&g_lfs, path, &info) == 0;
}

/* Find which block of the medium holds `needle`. */
static long find_block(const uint8_t *needle, size_t n)
{
    for (size_t b = 0U; b < NP_LFS_CFG_BLOCK_COUNT; b++) {
        const uint8_t *blk = g_media + (b * NP_LFS_CFG_BLOCK_SIZE);
        for (size_t o = 0U; o + n <= NP_LFS_CFG_BLOCK_SIZE; o++) {
            if (blk[o] == needle[0] && memcmp(blk + o, needle, n) == 0) {
                return (long)b;
            }
        }
    }
    return -1;
}

/* ── 1. Policy ────────────────────────────────────────────────────────────── */

static void test_policy_decides_the_api(void)
{
    uint8_t  buf[64];
    size_t   len = 0U;
    uint32_t n   = 0U;

    fresh();

    ASSERT(np_cfg_store_policy(NP_CFG_FILE_NPMP) == NP_CFG_POLICY_REBUILD,
           "npmp.bin must be a rebuild cache — it is the file that carries "
           "safe ranges (NP-SOUP-LFS-001 §5.3)");
    ASSERT(np_cfg_store_policy(NP_CFG_FILE_MAP3) == NP_CFG_POLICY_TAIL_ADDITIVE,
           "Map 3's journal must be tail-additive (NP-FW-NVRAM-001 §4.2 D-5)");
    ASSERT(np_cfg_store_policy(NP_CFG_FILE_UKMD) == NP_CFG_POLICY_REPLICATED,
           "ukmd.rec must be replicated (OI-LFS-08)");
    ASSERT(np_cfg_store_policy(NP_CFG_FILE_COUNT) == NP_CFG_POLICY_INVALID,
           "an id outside the table must have no policy");

    /* REQ-LFS-01's API half: the journal cannot be read through the reader a
     * limit's consumer uses, and a rebuild cache cannot be appended to. */
    ASSERT(np_cfg_store_read(NP_CFG_FILE_MAP3, buf, sizeof(buf), &len,
                             accept_anything, NULL) == NP_HUB_ERR_INVALID_ARG,
           "the rebuild-cache reader accepted the Map 3 journal");
    ASSERT(np_cfg_store_journal_append(NP_CFG_FILE_NPMP, buf, 8U)
               == NP_HUB_ERR_INVALID_ARG,
           "a rebuild cache accepted a tail-additive append");
    ASSERT(np_cfg_store_journal_read(NP_CFG_FILE_NPMP, buf, sizeof(buf),
                                     REC_SIZE, &n, rec_verify, NULL)
               == NP_HUB_ERR_INVALID_ARG,
           "the journal reader accepted a rebuild cache");
    ASSERT(np_cfg_store_replace(NP_CFG_FILE_UKMD, buf, 8U) == NP_HUB_ERR_INVALID_ARG,
           "ukmd.rec was writable as ONE entry — the thing OI-LFS-08 forbids");
    ASSERT(np_cfg_store_replicated_write(NP_CFG_FILE_NPMP, buf, 8U)
               == NP_HUB_ERR_INVALID_ARG,
           "the replicated writer accepted a rebuild cache");

    /* OI-LFS-09: there is no unverified read. */
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, buf, sizeof(buf), &len, NULL, NULL)
               == NP_HUB_ERR_INVALID_ARG,
           "a read without a content check was accepted");
    ASSERT(np_cfg_store_journal_read(NP_CFG_FILE_MAP3, buf, sizeof(buf),
                                     REC_SIZE, &n, NULL, NULL)
               == NP_HUB_ERR_INVALID_ARG,
           "a journal read without a per-record check was accepted");

    np_cfg_store_unmount();
}

/* ── 2. OI-LFS-06 — one open handle per file ──────────────────────────────── */

static void test_second_handle_is_refused(void)
{
    np_cfg_store_stats_t st;

    fresh();
    blob_build(g_blob, 1U);
    ASSERT(np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES) == NP_HUB_OK,
           "replace");
    ASSERT(np_cfg_store_test_double_open(NP_CFG_FILE_NPMP) == NP_HUB_ERR_STORE_BUSY,
           "a second handle on npmp.bin was opened — upstream 488e84bb's "
           "condition is reachable through the store");
    np_cfg_store_stats(&st);
    ASSERT(st.busy_refusals == 1U, "the refusal was not counted");

    /* And the registry released the slot: the next ordinary read works. */
    size_t len = 0U;
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_OK,
           "the registry leaked a slot after refusing");
    np_cfg_store_unmount();
}

/* ── 3. OI-LFS-09 — content, not presence ─────────────────────────────────── */

static void test_every_read_is_content_verified(void)
{
    size_t len = 0U;
    np_cfg_store_stats_t st;

    fresh();
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_ERR_NOT_PRESENT,
           "a never-written file must read as absent");

    blob_build(g_blob, 1U);
    ASSERT(np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES) == NP_HUB_OK,
           "replace");
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_OK && len == BLOB_BYTES &&
           memcmp(g_rb, g_blob, BLOB_BYTES) == 0,
           "a good blob did not read back");

    /* Upstream #1164: the file exists and holds nothing.  Made behind the
     * store's back, because the store can never produce it. */
    lfs_file_t f;
    ASSERT(raw_open(&f, "npmp.bin", LFS_O_WRONLY | LFS_O_TRUNC) == 0, "raw open");
    ASSERT(lfs_file_close(&g_lfs, &f) == 0, "raw close");
    ASSERT(raw_exists("npmp.bin"), "the empty file should still exist");
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_ERR_STORE_INTEGRITY,
           "an EMPTY npmp.bin was accepted — presence was treated as durability "
           "(upstream #1164)");

    /* A wrong byte on the medium, with littlefs none the wiser: data blocks
     * carry no littlefs CRC, so only the caller's check can see it. */
    blob_build(g_blob, 2U);
    ASSERT(np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES) == NP_HUB_OK,
           "replace");
    long blk = find_block(g_blob + 5000U, 64U);
    ASSERT(blk >= 0, "could not locate the blob's data on the medium");
    if (blk >= 0) {
        uint8_t *p = g_media + ((size_t)blk * NP_LFS_CFG_BLOCK_SIZE);
        for (size_t o = 0U; o + 64U <= NP_LFS_CFG_BLOCK_SIZE; o++) {
            if (memcmp(p + o, g_blob + 5000U, 64U) == 0) {
                p[o + 10U] ^= 0x01U;
                break;
            }
        }
    }
    /* A fresh mount so nothing cached hides the flipped bit. */
    np_cfg_store_unmount();
    reboot();
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "remount");
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_ERR_STORE_INTEGRITY,
           "a blob with a flipped data bit was accepted");

    /* Falsification of the check itself: with a verifier that accepts
     * anything, the same corrupted read comes back OK — so the refusal above
     * is the verifier's doing, not an accident of the read path. */
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             accept_anything, NULL) == NP_HUB_OK,
           "the corrupted read failed for a reason other than its content");
    np_cfg_store_stats(&st);
    /* One, not two: the reboot above reset the counters, as a power cycle
     * does, and the empty-file refusal was counted before it. */
    ASSERT(st.integrity_fails == 1U, "the flipped-bit refusal was not counted");
    np_cfg_store_unmount();
}

/* ── 4. OI-LFS-09 — upstream #1205, reproduced, and stopped ───────────────── */

static void test_1205_stale_cache_reproduced_and_stopped(void)
{
    np_cfg_store_stats_t st;
    size_t len = 0U;

    fresh();
    blob_build(g_blob, 3U);
    ASSERT(np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES) == NP_HUB_OK,
           "replace");
    np_cfg_store_unmount();

    /* The blob spans four blocks.  Y is the one holding file offset 9,000. */
    long y = find_block(g_blob + 9000U, 64U);
    ASSERT(y >= 0, "could not locate the second data block");
    if (y < 0) {
        return;
    }

    /* (a) RAW littlefs — what the anomaly does without the store.
     *     Read a little from the front (the file cache now holds block X's
     *     bytes), seek into block Y, fail ONE read of Y, then retry. */
    memset(&g_lfs, 0, sizeof(g_lfs));
    ASSERT(lfs_mount(&g_lfs, &g_cfg) == 0, "raw mount");
    lfs_file_t f;
    uint8_t    first[32];
    uint8_t    retry[32];
    ASSERT(raw_open(&f, "npmp.bin", LFS_O_RDONLY) == 0, "raw open");
    ASSERT(lfs_file_read(&g_lfs, &f, first, sizeof(first)) == (lfs_ssize_t)sizeof(first),
           "raw read of the first block");
    ASSERT(lfs_file_seek(&g_lfs, &f, 9000, LFS_SEEK_SET) == 9000, "seek");
    np_powerbd_fail_reads(&g_bd, (lfs_block_t)y, 1);
    lfs_ssize_t e1 = lfs_file_read(&g_lfs, &f, retry, sizeof(retry));
    lfs_ssize_t e2 = lfs_file_read(&g_lfs, &f, retry, sizeof(retry));
    (void)lfs_file_close(&g_lfs, &f);
    (void)lfs_unmount(&g_lfs);

    bool reproduced = (e1 == LFS_ERR_IO) && (e2 == (lfs_ssize_t)sizeof(retry)) &&
                      (memcmp(retry, g_blob + 9000U, sizeof(retry)) != 0);
    printf("  #1205    raw retry after a read error: first %d, retry %d, "
           "retry bytes %s\n", (int)e1, (int)e2,
           reproduced ? "STALE (reported as success) — reproduced on v2.11.3"
                      : "correct");
    ASSERT(e1 == LFS_ERR_IO, "the injected read error did not surface");
    ASSERT(reproduced,
           "upstream #1205 did not reproduce.  Either the pinned version no "
           "longer has it (re-run NP-SOUP-LFS-001 §11) or this scenario no "
           "longer reaches it — either way the store's mitigation below is "
           "now shown against nothing");

    /* (b) Through the STORE.  The same fault on the same block. */
    reboot();
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "mount");
    np_powerbd_fail_reads(&g_bd, (lfs_block_t)y, 1);
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_ERR_STORE_IO,
           "a read error was not reported as an absence");
    /* The fault has cleared; the next read must be CORRECT, which needs the
     * cache the error poisoned to be gone. */
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_OK &&
           memcmp(g_rb, g_blob, BLOB_BYTES) == 0,
           "after a read error the next read was not the stored blob");
    np_cfg_store_stats(&st);
    ASSERT(st.read_errors == 1U, "read error not counted");
    ASSERT(st.remounts == 1U,
           "no remount followed the read error — the poisoned cache was reused");

    /* (c) A persistent fault: every read an absence, never a wrong value. */
    np_powerbd_fail_reads(&g_bd, (lfs_block_t)y, 1000000L);
    for (int i = 0; i < 4; i++) {
        np_hub_status_t r = np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb),
                                              &len, blob_verify, NULL);
        ASSERT(r == NP_HUB_ERR_STORE_IO,
               "a persistent read fault produced something other than an absence");
    }
    np_powerbd_fail_reads(&g_bd, 0, 0);

    /* (d) The half of #1205 the remount is for.  (b) is a data-block fault,
     *     and the store opens a fresh handle per call, so the poisoned FILE
     *     cache dies with the handle whether or not the instance remounts.
     *     A fault on the root METADATA pair poisons littlefs's own read cache,
     *     which outlives every handle: found by hand-falsifying the remount
     *     (NP-SOUP-LFS-001 §13.4, P1) — without it, one failed read of the
     *     pair made the NEXT, fault-free read report npmp.bin as absent.  So
     *     for each block of the root pair: one failure, then two clean reads,
     *     both of which must be the stored blob. */
    for (lfs_block_t mblk = 0U; mblk < 2U; mblk++) {
        np_powerbd_fail_reads(&g_bd, mblk, 1);
        (void)np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                                blob_verify, NULL);
        np_powerbd_fail_reads(&g_bd, 0, 0);
        for (int i = 0; i < 2; i++) {
            ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                                     blob_verify, NULL) == NP_HUB_OK &&
                   memcmp(g_rb, g_blob, BLOB_BYTES) == 0,
                   "after a failed read of the root metadata pair, a fault-free "
                   "read did not return the stored blob — littlefs's read "
                   "cache outlived the error (#1205)");
        }
    }

    /* (e) The remount itself meets the fault.  Found when (d) first ran: (c)
     *     left a remount pending, the remount's own lfs_mount() read the
     *     faulted pair, and the store stayed unmounted for the rest of the
     *     power cycle — every later read an absence, with nothing wrong.  A
     *     transient fault must cost the operations it touches, not the
     *     instance. */
    np_powerbd_fail_reads(&g_bd, (lfs_block_t)y, 1);
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_ERR_STORE_IO,
           "data-block fault not reported");
    np_powerbd_fail_reads(&g_bd, 0, 4);      /* the remount will read block 0 */
    np_hub_status_t during = np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb,
                                               sizeof(g_rb), &len, blob_verify,
                                               NULL);
    np_powerbd_fail_reads(&g_bd, 1, 4);
    (void)np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                            blob_verify, NULL);
    np_powerbd_fail_reads(&g_bd, 0, 0);
    ASSERT(during != NP_HUB_OK || memcmp(g_rb, g_blob, BLOB_BYTES) == 0,
           "a read during a failing remount returned something wrong");
    ASSERT(np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb), &len,
                             blob_verify, NULL) == NP_HUB_OK &&
           memcmp(g_rb, g_blob, BLOB_BYTES) == 0,
           "once the fault cleared the store did not recover — a failed "
           "remount left the instance unmounted");
    np_cfg_store_unmount();
}

/* ── 5. OI-LFS-08 — churn is bounded ──────────────────────────────────────── */

static void test_churn_is_bounded(void)
{
    np_cfg_store_stats_t st;
    uint8_t rec[REC_SIZE];
    uint8_t ukmd[UKMD_BYTES];

    fresh();
    np_cfg_store_stats(&st);
    uint32_t creates_after_mount = st.creates;     /* the two replica dirs */

    for (unsigned i = 0U; i < 400U; i++) {
        blob_build(g_blob, (uint8_t)i);
        ASSERT(np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES) == NP_HUB_OK,
               "replace");
        rec_build(rec, i);
        ASSERT(np_cfg_store_journal_append(NP_CFG_FILE_MAP3, rec, REC_SIZE)
                   == NP_HUB_OK, "append");
        if ((i % 8U) == 0U) {
            ukmd_build(ukmd, (uint8_t)i);
            ASSERT(np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, ukmd,
                                                 UKMD_BYTES) == NP_HUB_OK,
                   "replicated write");
        }
        if ((i % 100U) == 99U) {
            np_cfg_store_unmount();
            reboot();
            ASSERT(np_cfg_store_mount() == NP_HUB_OK, "remount");
        }
    }
    np_cfg_store_stats(&st);
    printf("  churn    850 writes: %u creates (since the last reboot), "
           "%u removes/renames\n", (unsigned)st.creates, (unsigned)st.removes);
    ASSERT(st.removes == 0U, "the store removed or renamed something");
    ASSERT(st.creates == 0U,
           "after a reboot, nothing may be created: every file already exists, "
           "so a create here means a file was lost and re-made");
    ASSERT(creates_after_mount == 2U, "format did not lay out both replica dirs");

    /* Count the whole partition's creates from scratch: exactly one per
     * directory and one per file (npmp, map3, two ukmd copies). */
    fresh();
    blob_build(g_blob, 1U);
    rec_build(rec, 0U);
    ukmd_build(ukmd, 1U);
    for (unsigned i = 0U; i < 50U; i++) {
        (void)np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES);
        (void)np_cfg_store_journal_append(NP_CFG_FILE_MAP3, rec, REC_SIZE);
        (void)np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, ukmd, UKMD_BYTES);
    }
    np_cfg_store_stats(&st);
    ASSERT(st.creates == 2U + 1U + 1U + 2U,
           "create count is not one per directory and file — churn is not bounded");
    np_cfg_store_unmount();
}

/* ── 6. OI-LFS-08 — ukmd.rec on two entries ───────────────────────────────── */

static void test_replicated_record_survives_one_lost_entry(void)
{
    np_cfg_store_stats_t st;
    uint8_t w1[UKMD_BYTES];
    uint8_t w2[UKMD_BYTES];
    uint8_t r[UKMD_BYTES];

    fresh();
    ASSERT(np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, r, UKMD_BYTES)
               == NP_HUB_ERR_NOT_PRESENT, "never-written record must be absent");

    ukmd_build(w1, 0x11U);
    ASSERT(np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, w1, UKMD_BYTES) == NP_HUB_OK,
           "write");
    np_cfg_store_unmount();

    /* #1210's consequence, applied directly: copy A's entry is GONE. */
    memset(&g_lfs, 0, sizeof(g_lfs));
    ASSERT(lfs_mount(&g_lfs, &g_cfg) == 0, "raw mount");
    ASSERT(lfs_remove(&g_lfs, "ra/ukmd.rec") == 0, "raw remove of copy A");
    (void)lfs_unmount(&g_lfs);

    reboot();
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "mount");
    ASSERT(np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, r, UKMD_BYTES) == NP_HUB_OK &&
           memcmp(r, w1, UKMD_BYTES) == 0,
           "losing copy A lost the record — ukmd.rec still rests on one entry");
    np_cfg_store_stats(&st);
    ASSERT(st.repairs == 1U, "copy A was not repaired on first use");
    ASSERT(raw_exists("ra/ukmd.rec"), "copy A is still missing after the repair");

    /* The repair is not repeated when nothing is wrong. */
    ASSERT(np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, r, UKMD_BYTES) == NP_HUB_OK,
           "read");
    np_cfg_store_stats(&st);
    ASSERT(st.repairs == 1U, "a healthy pair was 'repaired'");

    /* Copy B's CONTENT damaged (a torn write, a bad block): its envelope CRC
     * fails, A is served, B is rewritten. */
    lfs_file_t f;
    uint8_t junk[16];
    memset(junk, 0xEE, sizeof(junk));
    ASSERT(raw_open(&f, "rb/ukmd.rec", LFS_O_WRONLY) == 0, "raw open B");
    ASSERT(lfs_file_seek(&g_lfs, &f, 40, LFS_SEEK_SET) == 40, "seek");
    ASSERT(lfs_file_write(&g_lfs, &f, junk, sizeof(junk)) == (lfs_ssize_t)sizeof(junk),
           "raw write");
    ASSERT(lfs_file_close(&g_lfs, &f) == 0, "raw close");
    ASSERT(np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, r, UKMD_BYTES) == NP_HUB_OK &&
           memcmp(r, w1, UKMD_BYTES) == 0, "a damaged copy B was served");
    np_cfg_store_stats(&st);
    ASSERT(st.repairs == 2U, "copy B was not repaired");

    /* Generations: a newer write wins over an older copy left behind by a
     * power loss between the two copy writes. */
    ukmd_build(w2, 0x22U);
    ASSERT(np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, w2, UKMD_BYTES) == NP_HUB_OK,
           "second write");
    /* Put copy A back to the OLD record, as if B's write landed and A's did
     * not — the reverse of the store's order, the harder case to resolve. */
    uint8_t env[12U + UKMD_BYTES + 4U];
    ASSERT(raw_open(&f, "rb/ukmd.rec", LFS_O_RDONLY) == 0, "raw open B");
    ASSERT(lfs_file_read(&g_lfs, &f, env, sizeof(env)) == (lfs_ssize_t)sizeof(env),
           "raw read B");
    (void)lfs_file_close(&g_lfs, &f);
    /* Rebuild an older-generation envelope for A. */
    env[4] = (uint8_t)(env[4] - 1U);
    memcpy(env + 12U, w1, UKMD_BYTES);
    uint32_t crc = np_crc32(env, 12U + UKMD_BYTES);
    env[12U + UKMD_BYTES]      = (uint8_t)(crc & 0xFFU);
    env[12U + UKMD_BYTES + 1U] = (uint8_t)((crc >> 8) & 0xFFU);
    env[12U + UKMD_BYTES + 2U] = (uint8_t)((crc >> 16) & 0xFFU);
    env[12U + UKMD_BYTES + 3U] = (uint8_t)((crc >> 24) & 0xFFU);
    ASSERT(raw_open(&f, "ra/ukmd.rec", LFS_O_WRONLY | LFS_O_TRUNC) == 0, "raw open A");
    ASSERT(lfs_file_write(&g_lfs, &f, env, sizeof(env)) == (lfs_ssize_t)sizeof(env),
           "raw write A");
    (void)lfs_file_close(&g_lfs, &f);
    ASSERT(np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, r, UKMD_BYTES) == NP_HUB_OK &&
           memcmp(r, w2, UKMD_BYTES) == 0,
           "the older copy was served over the newer one");
    np_cfg_store_stats(&st);
    ASSERT(st.repairs == 3U, "the stale copy was not brought forward");

    /* Both entries gone: the one outcome replication cannot prevent, and it
     * must be reported as an absence, never as some other record. */
    ASSERT(lfs_remove(&g_lfs, "ra/ukmd.rec") == 0, "remove A");
    ASSERT(lfs_remove(&g_lfs, "rb/ukmd.rec") == 0, "remove B");
    ASSERT(np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, r, UKMD_BYTES)
               == NP_HUB_ERR_NOT_PRESENT,
           "with both copies gone the read did not report absence");
    np_cfg_store_unmount();
}

/* ── 7/8. Power-loss sweeps of the store's own orderings ──────────────────── */

static int  g_findings_shown;
static bool g_expect;

static void finding(const char *what, long cut, np_powerbd_tear_t tear,
                    const char *fmt, ...)
{
    if (g_expect) {
        if (g_findings_shown >= 2) {
            return;
        }
        g_findings_shown++;
        printf("           caught: [%s] cut@%ld %s: ", what, cut,
               np_powerbd_tear_name(tear));
    } else {
        printf("FAIL [%s] cut@%ld %s: ", what, cut, np_powerbd_tear_name(tear));
    }
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

/* L-3 — the in-place replacement. */

static void s_blob_baseline(void)
{
    ASSERT(np_cfg_store_format() == NP_HUB_OK && np_cfg_store_mount() == NP_HUB_OK,
           "baseline format/mount");
    blob_build(g_blob, 1U);
    (void)np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES);
    np_cfg_store_unmount();
}

static void s_blob_replace(void)
{
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    blob_build(g_blob, 2U);
    (void)np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES);
    np_cfg_store_unmount();
}

/* FALSIFICATION: destroy the live record first — through raw littlefs,
 * because the store has no way to express it. */
static void s_blob_remove_then_write(void)
{
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    (void)lfs_remove(&g_lfs, "npmp.bin");
    blob_build(g_blob, 2U);
    (void)np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES);
    np_cfg_store_unmount();
}

static int v_blob(const char *what, long cut, np_powerbd_tear_t tear)
{
    size_t len = 0U;
    if (np_cfg_store_mount() != NP_HUB_OK) {
        finding(what, cut, tear, "MOUNT FAILED after the cut");
        return 1;
    }
    np_hub_status_t st = np_cfg_store_read(NP_CFG_FILE_NPMP, g_rb, sizeof(g_rb),
                                           &len, blob_verify, NULL);
    np_cfg_store_unmount();
    if (st != NP_HUB_OK) {
        finding(what, cut, tear, "npmp.bin unreadable (status %d) — the live "
                "record was lost before its replacement was durable (L-3)", st);
        return 1;
    }
    if (g_rb[4] != 1U && g_rb[4] != 2U) {
        finding(what, cut, tear, "npmp.bin is neither generation (L-3)");
        return 1;
    }
    return 0;
}

/* L-4 — the journal append, through the store. */

#define JRN_BASE 200U
#define JRN_ADD  4U
static uint8_t g_jrn[REC_SIZE * (JRN_BASE + JRN_ADD) + 512U];

static void s_jrn_baseline(void)
{
    uint8_t rec[REC_SIZE];
    ASSERT(np_cfg_store_format() == NP_HUB_OK && np_cfg_store_mount() == NP_HUB_OK,
           "baseline format/mount");
    for (uint32_t i = 0U; i < JRN_BASE; i++) {
        rec_build(rec, i);
        (void)np_cfg_store_journal_append(NP_CFG_FILE_MAP3, rec, REC_SIZE);
    }
    np_cfg_store_unmount();
}

static void s_jrn_append(void)
{
    uint8_t rec[REC_SIZE];
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    for (uint32_t i = 0U; i < JRN_ADD; i++) {
        rec_build(rec, JRN_BASE + i);
        (void)np_cfg_store_journal_append(NP_CFG_FILE_MAP3, rec, REC_SIZE);
    }
    np_cfg_store_unmount();
}

static int v_jrn(const char *what, long cut, np_powerbd_tear_t tear)
{
    uint32_t n = 0U;
    if (np_cfg_store_mount() != NP_HUB_OK) {
        finding(what, cut, tear, "MOUNT FAILED after the cut");
        return 1;
    }
    np_hub_status_t st = np_cfg_store_journal_read(NP_CFG_FILE_MAP3, g_jrn,
                                                   sizeof(g_jrn), REC_SIZE, &n,
                                                   rec_verify, NULL);
    np_cfg_store_unmount();
    if (st != NP_HUB_OK || n < JRN_BASE) {
        finding(what, cut, tear, "%u of %u durable records survived (status %d) "
                "(L-4)", (unsigned)n, JRN_BASE, st);
        return 1;
    }
    return 0;
}

/* The replicated write. */

static uint8_t g_u1[UKMD_BYTES];
static uint8_t g_u2[UKMD_BYTES];

static void s_ukmd_baseline(void)
{
    ASSERT(np_cfg_store_format() == NP_HUB_OK && np_cfg_store_mount() == NP_HUB_OK,
           "baseline format/mount");
    (void)np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, g_u1, UKMD_BYTES);
    np_cfg_store_unmount();
}

static void s_ukmd_write(void)
{
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    (void)np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, g_u2, UKMD_BYTES);
    np_cfg_store_unmount();
}

/* FALSIFICATION: both copies destroyed before either is rewritten. */
static void s_ukmd_remove_both_then_write(void)
{
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    (void)lfs_remove(&g_lfs, "ra/ukmd.rec");
    (void)lfs_remove(&g_lfs, "rb/ukmd.rec");
    (void)np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, g_u2, UKMD_BYTES);
    np_cfg_store_unmount();
}

static int v_ukmd(const char *what, long cut, np_powerbd_tear_t tear)
{
    uint8_t r[UKMD_BYTES];
    uint8_t again[UKMD_BYTES];
    if (np_cfg_store_mount() != NP_HUB_OK) {
        finding(what, cut, tear, "MOUNT FAILED after the cut");
        return 1;
    }
    np_hub_status_t st = np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, r,
                                                      UKMD_BYTES);
    if (st != NP_HUB_OK) {
        np_cfg_store_unmount();
        finding(what, cut, tear, "ukmd.rec LOST (status %d) — the user's UHDR "
                "would be permanently unmountable", st);
        return 1;
    }
    if (memcmp(r, g_u1, UKMD_BYTES) != 0 && memcmp(r, g_u2, UKMD_BYTES) != 0) {
        np_cfg_store_unmount();
        finding(what, cut, tear, "ukmd.rec is neither the old record nor the new");
        return 1;
    }
    /* After the first read has repaired the pair, a second read must agree
     * and must find nothing further to repair. */
    np_cfg_store_stats_t before;
    np_cfg_store_stats_t after;
    np_cfg_store_stats(&before);
    st = np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, again, UKMD_BYTES);
    np_cfg_store_stats(&after);
    np_cfg_store_unmount();
    if (st != NP_HUB_OK || memcmp(r, again, UKMD_BYTES) != 0 ||
        after.repairs != before.repairs) {
        finding(what, cut, tear, "the pair did not converge after one read");
        return 1;
    }
    return 0;
}

static np_sweep_result_t run(const char *label, void (*base)(void),
                             void (*mut)(void), np_sweep_verify_fn v,
                             bool expect)
{
    g_expect         = expect;
    g_findings_shown = 0;
    np_sweep_result_t r = np_sweep_run(base, mut, label, v);
    g_expect = false;
    printf("  %-26s %3ld ops x 3 tear models = %4ld attempts, %4ld cuts, "
           "%4ld violations\n", label, r.ops, r.attempts, r.cuts, r.violations);
    return r;
}

static void test_store_orderings_survive_power_loss(void)
{
    ukmd_build(g_u1, 0x31U);
    ukmd_build(g_u2, 0x32U);

    np_sweep_result_t blob = run("L-3 in-place replace", s_blob_baseline,
                                 s_blob_replace, v_blob, false);
    np_sweep_result_t jrn  = run("L-4 journal append", s_jrn_baseline,
                                 s_jrn_append, v_jrn, false);
    np_sweep_result_t ukmd = run("replicated write", s_ukmd_baseline,
                                 s_ukmd_write, v_ukmd, false);

    ASSERT(blob.ops > 0 && jrn.ops > 0 && ukmd.ops > 0,
           "a store ordering touched the medium zero times");
    ASSERT(blob.violations == 0,
           "L-3 does not hold for the in-place O_TRUNC replacement — the store's "
           "churn bound (OI-LFS-08) cannot use it");
    ASSERT(jrn.violations == 0, "L-4 does not hold through the store's append");
    ASSERT(ukmd.violations == 0,
           "a power loss during the replicated write lost ukmd.rec");
    ASSERT(blob.missed_cuts == 0 && jrn.missed_cuts == 0 && ukmd.missed_cuts == 0,
           "an armed cut did not fire — the op sequence is not the one measured");

    /* Direction 1 — the verifiers can see what they are for. */
    np_sweep_result_t f1 = run("FALSIFY remove-then-write", s_blob_baseline,
                               s_blob_remove_then_write, v_blob, true);
    ASSERT(f1.violations > 0,
           "v_blob did not notice the live blob destroyed before its "
           "replacement — the L-3 result above is vacuous");
    np_sweep_result_t f2 = run("FALSIFY lose both copies", s_ukmd_baseline,
                               s_ukmd_remove_both_then_write, v_ukmd, true);
    ASSERT(f2.violations > 0,
           "v_ukmd did not notice both copies destroyed — the replicated result "
           "above is vacuous");

    /* Direction 2 — the injector really cut. */
    ASSERT(g_bd.total_cuts > 0, "no cut fired anywhere in this suite");
}

int main(void)
{
    printf("np_cfg_store_tests — NP-SOUP-LFS-001 Rev 4 §13 "
           "(OI-LFS-06, OI-LFS-08, OI-LFS-09)\n");
    build();

    test_policy_decides_the_api();
    test_second_handle_is_refused();
    test_every_read_is_content_verified();
    test_1205_stale_cache_reproduced_and_stopped();
    test_churn_is_bounded();
    test_replicated_record_survives_one_lost_entry();
    test_store_orderings_survive_power_loss();

    np_sweep_release();
    printf("  totals   %ld programs, %ld erases, %ld syncs, %ld reads, %ld cuts\n",
           g_bd.total_progs, g_bd.total_erases, g_bd.total_syncs,
           g_bd.total_reads, g_bd.total_cuts);

    if (g_fail_count == 0) {
        printf("PASS\n");
    } else {
        printf("%d FAILURE(S)\n", g_fail_count);
    }
    return g_fail_count;
}
