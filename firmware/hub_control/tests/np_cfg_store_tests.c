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
 *   9. OI-NVRAM-16   a Map 3 journal whose rows differ in length is read back
 *                    whole through np_cfg_store_journal_read_rows(), and the
 *                    fixed-length reader is shown losing rows on the same file
 *  10. OI-HEXMAP-01  np_module_map_persist()/_restore() end to end through
 *                    np_hexmap_nvram.c and this store: the blob survives a
 *                    reboot, a damaged or absent blob restores an EMPTY map,
 *                    a blob the map would refuse is never written, and a
 *                    power cut during persist leaves the old blob or the new
 *                    one (L-3, through the real caller)
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
#include <stdlib.h>
#include <string.h>

#include "np_cfg_store.h"
#include "np_factory_reset.h"
#include "np_reset_marker.h"
#include "np_crypto.h"        /* np_crc32 — a checker independent of the store */
#include "np_lfs_config.h"
#include "np_lfs_instance.h"
#include "np_lfs_powerbd.h"
#include "np_lfs_sweep.h"
#include "np_map3_record.h"
#include "np_module_map.h"
#include "np_session_count.h"
#include "np_hub_test_fixtures.h"

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));  \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

/* ── The instance under test ──────────────────────────────────────────────── */


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
    /* static: littlefs reads it again at sync/close (lfs.h lifetime rule). */
    static struct lfs_file_config fcfg;
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

    /* Both copies PRESENT and both damaged: not an absence.  A caller that
     * creates the record on absence (np_warranty_token, OI-WA-03) would
     * otherwise overwrite a record it had merely failed to read. */
    for (unsigned c = 0U; c < 2U; c++) {
        ASSERT(raw_open(&f, c == 0U ? "ra/ukmd.rec" : "rb/ukmd.rec",
                        LFS_O_WRONLY) == 0, "raw open for damage");
        ASSERT(lfs_file_seek(&g_lfs, &f, 40, LFS_SEEK_SET) == 40, "seek");
        ASSERT(lfs_file_write(&g_lfs, &f, junk, sizeof(junk)) == (lfs_ssize_t)sizeof(junk),
               "raw write");
        ASSERT(lfs_file_close(&g_lfs, &f) == 0, "raw close");
    }
    ASSERT(np_cfg_store_replicated_read(NP_CFG_FILE_UKMD, r, UKMD_BYTES)
               == NP_HUB_ERR_STORE_INTEGRITY,
           "two present-but-damaged copies were reported as something other than "
           "an integrity failure");

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

/*
 * What the store had ACKNOWLEDGED when the cut fired (OI-LFS-14).  Each is
 * set only when the store call returned NP_HUB_OK in the current attempt, and
 * each verifier then requires the acknowledged state: "old or new" is the
 * right answer only while the call is still in flight.  Globals, not locals:
 * the cut longjmps out of the mutation.  Every mutation resets the one its
 * verifier reads, falsifications included.
 */
static volatile bool     g_blob_acked;
static volatile uint32_t g_jrn_acked;
static volatile bool     g_ukmd_acked;
static volatile bool     g_count_acked;
static volatile bool     g_npmp_acked;

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
    g_blob_acked = false;
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    blob_build(g_blob, 2U);
    g_blob_acked = (np_cfg_store_replace(NP_CFG_FILE_NPMP, g_blob, BLOB_BYTES) ==
                    NP_HUB_OK);
    np_cfg_store_unmount();
}

/* FALSIFICATION: destroy the live record first — through raw littlefs,
 * because the store has no way to express it. */
static void s_blob_remove_then_write(void)
{
    g_blob_acked = false;   /* judged on the window, as before */
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
    if (g_blob_acked && g_rb[4] != 2U) {
        finding(what, cut, tear, "the replace had returned OK and npmp.bin is "
                "still generation 1 — it was not durable (L-3)");
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
    g_jrn_acked = 0U;
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    for (uint32_t i = 0U; i < JRN_ADD; i++) {
        rec_build(rec, JRN_BASE + i);
        if (np_cfg_store_journal_append(NP_CFG_FILE_MAP3, rec, REC_SIZE) ==
            NP_HUB_OK) {
            g_jrn_acked++;
        }
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
    if (st != NP_HUB_OK || n < JRN_BASE + g_jrn_acked) {
        finding(what, cut, tear, "%u of %u durable records survived (status %d) "
                "(L-4)", (unsigned)n, JRN_BASE + g_jrn_acked, st);
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
    g_ukmd_acked = false;
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    g_ukmd_acked = (np_cfg_store_replicated_write(NP_CFG_FILE_UKMD, g_u2,
                                                  UKMD_BYTES) == NP_HUB_OK);
    np_cfg_store_unmount();
}

/* FALSIFICATION: both copies destroyed before either is rewritten. */
static void s_ukmd_remove_both_then_write(void)
{
    g_ukmd_acked = false;   /* judged on the window, as before */
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
    if (g_ukmd_acked && memcmp(r, g_u2, UKMD_BYTES) != 0) {
        np_cfg_store_unmount();
        finding(what, cut, tear, "the replicated write had returned OK and "
                "ukmd.rec is still the old record — it was not durable");
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

/* ── 9. OI-LFS-12 — the persisted device session count ────────────────── */

static void s_count_baseline(void)
{
    ASSERT(np_cfg_store_format() == NP_HUB_OK && np_cfg_store_mount() == NP_HUB_OK,
           "baseline format/mount");
    (void)np_session_count_commit(41U);
    np_cfg_store_unmount();
}

static void s_count_advance(void)
{
    g_count_acked = false;
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    g_count_acked = (np_session_count_commit(42U) == NP_HUB_OK);
    np_cfg_store_unmount();
}

static int v_count(const char *what, long cut, np_powerbd_tear_t tear)
{
    uint32_t c = 0U;
    np_hub_status_t st = NP_HUB_ERR_GENERIC;
    if (np_cfg_store_mount() == NP_HUB_OK) {
        st = np_session_count_load(&c);
        np_cfg_store_unmount();
    }
    if (st == NP_HUB_OK && (c == 42U || (c == 41U && !g_count_acked))) {
        return 0;
    }
    finding(what, cut, tear, "session count is %u (status %d) — it went "
            "backwards or was lost", (unsigned)c, st);
    return 1;
}

static void test_session_count_is_persisted(void)
{
    uint32_t c = 99U;

    fresh();
    ASSERT(np_session_count_load(&c) == NP_HUB_ERR_NOT_PRESENT && c == 0U,
           "a new device's count must read as absent and seed 0");
    ASSERT(np_session_count_commit(5U) == NP_HUB_OK, "commit 5");
    ASSERT(np_session_count_load(&c) == NP_HUB_OK && c == 5U, "count reads back");

    /* Across a reboot — which is the whole point of OI-LFS-12. */
    np_cfg_store_unmount();
    reboot();
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "remount");
    ASSERT(np_session_count_load(&c) == NP_HUB_OK && c == 5U,
           "the count did not survive a reboot");

    /* One lost entry (upstream #1210's consequence) does not lose the count. */
    ASSERT(np_session_count_commit(6U) == NP_HUB_OK, "commit 6");
    ASSERT(lfs_remove(&g_lfs, "ra/sesscnt.rec") == 0, "raw remove of copy A");
    ASSERT(np_session_count_load(&c) == NP_HUB_OK && c == 6U,
           "losing one entry lost the count");
    np_cfg_store_unmount();

    /* A power loss during a commit leaves the old count or the new one. */
    np_sweep_result_t r = run("session count commit", s_count_baseline,
                              s_count_advance, v_count, false);
    ASSERT(r.ops > 0 && r.missed_cuts == 0, "count sweep did not run as measured");
    ASSERT(r.violations == 0,
           "a power loss during the count commit made it go backwards or lost it");
}

/* ── OI-NVRAM-05 option A: the durable factory-reset marker ─────────────────
 * NP-FW-NVRAM-001 §3.4.1.  What np_factory_reset_boot_check() completes a
 * reset on must be readable from the medium after a power loss, and what it
 * must NOT complete a reset on (an unreadable medium) must stay distinct. */

static np_fr_marker_state_t marker_state_after_reboot(void)
{
    np_cfg_store_unmount();
    np_powerbd_power_cycle(&g_bd);
    reboot();
    np_fr_marker_state_t m = np_factory_reset_hal_marker_state();
    np_cfg_store_unmount();
    return m;
}

static void s_marker_baseline(void)
{
    ASSERT(np_cfg_store_format() == NP_HUB_OK && np_cfg_store_mount() == NP_HUB_OK,
           "baseline format/mount");
    np_cfg_store_unmount();
}

/* np_factory_reset.h: NP_RESET_OK "only once it is durable" (OI-LFS-14). */
static volatile bool g_marker_acked;

static void s_marker_write(void)
{
    g_marker_acked = false;
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    g_marker_acked = (np_factory_reset_hal_marker_write() == NP_RESET_OK);
    np_cfg_store_unmount();
}

static int v_marker(const char *what, long cut, np_powerbd_tear_t tear)
{
    /* A cut during R-3 happens before any erase.  Either the reset had not
     * started (ABSENT, and the user re-issues it) or it had (PRESENT, and the
     * boot completes it).  Anything else would either erase on an unreadable
     * store or lose the store the reset has not yet erased. */
    np_fr_marker_state_t m = np_factory_reset_hal_marker_state();
    np_cfg_store_unmount();
    if (m == NP_FR_MARKER_PRESENT || (m == NP_FR_MARKER_ABSENT && !g_marker_acked)) {
        return 0;
    }
    if (m == NP_FR_MARKER_ABSENT) {
        finding(what, cut, tear, "the marker write had returned NP_RESET_OK and "
                "the marker is absent — an acknowledged reset would be dropped");
        return 1;
    }
    finding(what, cut, tear, "marker state %d after a cut during its write — "
            "neither absent nor present", (int)m);
    return 1;
}

static void test_reset_marker_is_durable(void)
{
    /* The decision table, without a medium. */
    ASSERT(np_reset_marker_classify(NP_HUB_ERR_STORE_INTEGRITY, NP_HUB_OK) ==
           NP_FR_MARKER_NO_STORE, "no filesystem must classify as NO_STORE");
    ASSERT(np_reset_marker_classify(NP_HUB_ERR_STORE_IO, NP_HUB_OK) ==
           NP_FR_MARKER_UNKNOWN, "an unreadable medium must classify as UNKNOWN");
    ASSERT(np_reset_marker_classify(NP_HUB_OK, NP_HUB_ERR_NOT_PRESENT) ==
           NP_FR_MARKER_ABSENT, "no marker must classify as ABSENT");
    ASSERT(np_reset_marker_classify(NP_HUB_OK, NP_HUB_OK) ==
           NP_FR_MARKER_PRESENT, "a marker must classify as PRESENT");
    ASSERT(np_reset_marker_classify(NP_HUB_OK, NP_HUB_ERR_STORE_INTEGRITY) ==
           NP_FR_MARKER_PRESENT, "a refused copy is a begun write: PRESENT");
    ASSERT(np_reset_marker_classify(NP_HUB_OK, NP_HUB_ERR_STORE_IO) ==
           NP_FR_MARKER_UNKNOWN, "a read fault must classify as UNKNOWN");

    /* A formatted Config holds no marker. */
    fresh();
    ASSERT(marker_state_after_reboot() == NP_FR_MARKER_ABSENT,
           "a freshly formatted Config reads as a running reset");

    /* R-3 writes it, and it survives a power cycle — the property LPGPR1
     * does not have. */
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "mount");
    ASSERT(np_factory_reset_hal_marker_write() == NP_RESET_OK, "marker write");
    ASSERT(marker_state_after_reboot() == NP_FR_MARKER_PRESENT,
           "the marker did not survive a power cycle");

    /* One lost entry (#1210) does not lose the evidence. */
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "mount");
    ASSERT(lfs_remove(&g_lfs, "ra/reset.mrk") == 0, "raw remove of copy A");
    ASSERT(marker_state_after_reboot() == NP_FR_MARKER_PRESENT,
           "losing one copy lost the marker");

    /* R-7 zeroes the partition: no filesystem, which completes the reset. */
    memset(g_media, 0x00, sizeof(g_media));
    ASSERT(marker_state_after_reboot() == NP_FR_MARKER_NO_STORE,
           "a zeroed Config did not read as NO_STORE");
    np_powerbd_wipe(&g_bd);
    ASSERT(marker_state_after_reboot() == NP_FR_MARKER_NO_STORE,
           "an erased Config did not read as NO_STORE");

    /* An unreadable medium is NOT an empty one.  Before 2026-09-26 the store
     * reported both as NP_HUB_ERR_STORE_IO, and option A's boot rule would
     * have erased a device on a read fault. */
    fresh();
    ASSERT(np_factory_reset_hal_marker_write() == NP_RESET_OK, "marker write");
    np_cfg_store_unmount();
    np_powerbd_power_cycle(&g_bd);
    reboot();
    np_powerbd_fail_reads(&g_bd, 0, 1000);
    np_powerbd_fail_reads(&g_bd, 1, 1000);
    np_fr_marker_state_t m = np_factory_reset_hal_marker_state();
    np_powerbd_fail_reads(&g_bd, 0, 0);
    np_powerbd_fail_reads(&g_bd, 1, 0);
    np_cfg_store_unmount();
    ASSERT(m == NP_FR_MARKER_UNKNOWN,
           "a read fault on the superblock was reported as a state to act on");

    /* R-3's read-back reads the MEDIUM, in the same boot as the write
     * (NP-SOUP-LFS-001 §13.18, RISK-LFS-09).  np_factory_reset_execute()
     * refuses to start R-5 unless marker_state() says PRESENT straight after
     * the write, so marker_state() must see a write the medium did not keep —
     * not the store's or littlefs's memory of having made it.  The medium is
     * put back to its pre-write bytes after an acknowledged write: an E4
     * rollback or an OI-LFS-07 dropped program, seen from the medium. */
    {
        uint8_t *before = malloc(sizeof(g_media));
        ASSERT(before != NULL, "host out of memory");
        if (before != NULL) {
            fresh();
            ASSERT(np_cfg_store_mount() == NP_HUB_OK, "mount");
            memcpy(before, g_media, sizeof(g_media));
            ASSERT(np_factory_reset_hal_marker_write() == NP_RESET_OK, "marker write");
            ASSERT(np_factory_reset_hal_marker_state() == NP_FR_MARKER_PRESENT,
                   "a kept marker did not read back PRESENT in the same boot");
            memcpy(g_media, before, sizeof(g_media));     /* the write is lost */
            ASSERT(np_factory_reset_hal_marker_state() == NP_FR_MARKER_ABSENT,
                   "a marker the medium did not keep still read back PRESENT — "
                   "R-3's read-back would let R-5 purge UHDR on it");
            np_cfg_store_unmount();
            free(before);
        }
    }

    /* A power loss during the marker write: absent or present, nothing else. */
    np_sweep_result_t r = run("R-3 reset marker write", s_marker_baseline,
                              s_marker_write, v_marker, false);
    ASSERT(r.ops > 0 && r.missed_cuts == 0, "marker sweep did not run as measured");
    ASSERT(r.violations == 0,
           "a power loss during the marker write left Config in neither state");
}

/* ── 9. OI-NVRAM-16 — a journal whose rows differ in length ─────────────────
 *
 * NP-FW-NVRAM-001 D-24 budgets a later Map 3 row of 40 bytes, and D-25 makes
 * each row carry its own length.  Here a version-2 row (the v1 row with an
 * 8-byte tail before the CRC, built as §7.2 rule 1 permits) is appended
 * between version-1 rows, through the store, and read back two ways.
 */

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static size_t map3_row(uint32_t i, uint8_t tail, uint8_t *out)
{
    np_map3_rec_t r;
    memset(&r, 0, sizeof(r));
    for (unsigned k = 0U; k < NP_MAP3_UID_LEN; k++) {
        r.uid[k] = (uint8_t)(0x30U + i + k);
    }
    r.seq = 100U + i;
    r.session_count = (uint16_t)i;
    if (np_map3_encode(&r, out, NP_MAP3_V1_BYTES) != NP_MAP3_V1_BYTES) {
        return 0U;
    }
    if (tail == 0U) {
        return NP_MAP3_V1_BYTES;
    }
    size_t len = NP_MAP3_V1_BYTES + tail;
    for (size_t k = 0U; k < tail; k++) {
        out[NP_MAP3_V1_FIELDS_END + k] = (uint8_t)(0xC0U + k);
    }
    out[0] = (uint8_t)len;
    out[1] = 2U;
    put_le32(&out[len - NP_MAP3_CRC_BYTES],
             np_crc32(out, (uint32_t)(len - NP_MAP3_CRC_BYTES)));
    return len;
}

/* The step Map 3's owner hands the store. */
static size_t map3_step(const uint8_t *rec, size_t left, uint32_t index,
                        void *ctx)
{
    (void)index;
    (void)ctx;
    return np_map3_row_len(rec, left);
}

/* The only check the fixed-length reader can make: a whole row in the slice. */
static bool map3_fixed_verify(const uint8_t *rec, size_t rec_len,
                              uint32_t index, void *ctx)
{
    (void)index;
    (void)ctx;
    return np_map3_row_len(rec, rec_len) == rec_len;
}

static size_t step_zero(const uint8_t *rec, size_t left, uint32_t i, void *c)
{
    (void)rec; (void)left; (void)i; (void)c;
    return 0U;
}

static size_t step_overrun(const uint8_t *rec, size_t left, uint32_t i, void *c)
{
    (void)rec; (void)i; (void)c;
    return left + 1U;
}

static bool seq_in_order(const np_map3_row_t *row, void *ctx)
{
    uint32_t *next = (uint32_t *)ctx;
    if (row->rec.seq != 100U + *next) {
        return false;
    }
    (*next)++;
    return true;
}

static void test_journal_rows_of_differing_length(void)
{
    enum { ROWS = 10U, GROWN = 4U, TAIL = 8U };
    static uint8_t jrn[1024];
    uint8_t row[NP_MAP3_MAX_BYTES];
    size_t want_bytes = 0U;

    printf("[OI-NVRAM-16] a Map 3 journal with a grown row is read whole\n");
    fresh();
    for (uint32_t i = 0U; i < ROWS; i++) {
        size_t n = map3_row(i, (i == GROWN) ? (uint8_t)TAIL : 0U, row);
        ASSERT(n != 0U, "row encode");
        ASSERT(np_cfg_store_journal_append(NP_CFG_FILE_MAP3, row, n) == NP_HUB_OK,
               "append");
        want_bytes += n;
    }
    ASSERT(want_bytes == ROWS * NP_MAP3_V1_BYTES + TAIL, "the fixture has one grown row");

    /* The reader stepping by each row's own length gets every row back. */
    uint32_t count = 0U;
    size_t bytes = 0U;
    ASSERT(np_cfg_store_journal_read_rows(NP_CFG_FILE_MAP3, jrn, sizeof(jrn),
                                          &count, &bytes, map3_step, NULL)
               == NP_HUB_OK, "variable-length read");
    ASSERT(count == ROWS, "a row was lost across the grown row");
    ASSERT(bytes == want_bytes, "the valid prefix does not end at the journal's end");
    uint32_t next = 0U;
    np_map3_scan_t sc;
    np_map3_scan(jrn, bytes, seq_in_order, &next, &sc);
    ASSERT(sc.rows == ROWS && sc.end == NP_MAP3_END_CLEAN && next == ROWS,
           "the rows read back are not the rows written, in order");

    /* Falsified: the fixed-length reader on the same file stops at the grown
     * row, which is the defect OI-NVRAM-16 recorded.  If this ever reads
     * every row, the fixture no longer contains a grown row and the check
     * above proves nothing. */
    uint32_t fixed = 0U;
    ASSERT(np_cfg_store_journal_read(NP_CFG_FILE_MAP3, jrn, sizeof(jrn),
                                     NP_MAP3_V1_BYTES, &fixed,
                                     map3_fixed_verify, NULL) == NP_HUB_OK,
           "fixed-length read");
    ASSERT(fixed == GROWN, "the fixed-length reader did not lose the rows after the "
           "grown one — the fixture does not exercise OI-NVRAM-16");

    /* A step that verifies nothing, or claims more than remains, ends the
     * prefix at once; neither may walk the store past what it read. */
    ASSERT(np_cfg_store_journal_read_rows(NP_CFG_FILE_MAP3, jrn, sizeof(jrn),
                                          &count, &bytes, step_zero, NULL)
               == NP_HUB_OK && count == 0U && bytes == 0U,
           "a step of 0 did not end the prefix");
    ASSERT(np_cfg_store_journal_read_rows(NP_CFG_FILE_MAP3, jrn, sizeof(jrn),
                                          &count, &bytes, step_overrun, NULL)
               == NP_HUB_OK && count == 0U && bytes == 0U,
           "a step past the end was accepted");

    /* A torn tail costs the torn row only (L-4). */
    size_t n = map3_row(ROWS, 0U, row);
    ASSERT(np_cfg_store_journal_append(NP_CFG_FILE_MAP3, row, n / 2U) == NP_HUB_OK,
           "append half a row");
    ASSERT(np_cfg_store_journal_read_rows(NP_CFG_FILE_MAP3, jrn, sizeof(jrn),
                                          &count, &bytes, map3_step, NULL)
               == NP_HUB_OK && count == ROWS && bytes == want_bytes,
           "a torn tail cost more than the torn row");

    /* The policy and the mandatory check hold for this reader too. */
    ASSERT(np_cfg_store_journal_read_rows(NP_CFG_FILE_NPMP, jrn, sizeof(jrn),
                                          &count, &bytes, map3_step, NULL)
               == NP_HUB_ERR_INVALID_ARG,
           "the variable-length reader accepted a rebuild cache");
    ASSERT(np_cfg_store_journal_read_rows(NP_CFG_FILE_MAP3, jrn, sizeof(jrn),
                                          &count, &bytes, NULL, NULL)
               == NP_HUB_ERR_INVALID_ARG,
           "a variable-length read without a per-record check was accepted");

    np_cfg_store_unmount();
}

/* ── 10. OI-HEXMAP-01 — the module map's blob, through the real caller ─────
 * NP-FW-NVRAM-001 D-1/D-2; NP-SOUP-LFS-001 REQ-LFS-02.  Everything above
 * tested npmp.bin with a synthetic blob written straight to the store.  This
 * case drives np_module_map_persist() and _restore(), which reach the store
 * only through np_hexmap_nvram_read/write — the binding OI-HEXMAP-01 asked
 * for — on the real helmet's 80 sockets (14,012 bytes). */

#define MAP_SOCKETS 80U

static np_socket_geom_t g_geom[MAP_SOCKETS];
static int              g_map_inv_calls;
static uint8_t          g_npmp_old[NP_HEXMAP_NVRAM_MAX_BYTES];
static uint8_t          g_npmp_new[NP_HEXMAP_NVRAM_MAX_BYTES];
static uint8_t          g_npmp_rb[NP_HEXMAP_NVRAM_MAX_BYTES];
static size_t           g_npmp_len;

static np_hub_status_t map_inv(uint16_t socket_id, void *ctx, uint8_t *types_out,
                               uint8_t max, uint8_t *count_out)
{
    (void)socket_id;
    (void)ctx;
    static const uint8_t TILE[] = { NP_ELEM_LED_660, NP_ELEM_LED_808,
                                    NP_ELEM_NTC, NP_ELEM_PD_FORWARD };
    g_map_inv_calls++;
    if (max < sizeof(TILE)) {
        return NP_HUB_ERR_CMD_TOO_MANY;
    }
    memcpy(types_out, TILE, sizeof(TILE));
    *count_out = (uint8_t)sizeof(TILE);
    return NP_HUB_OK;
}

static np_module_uid_t map_uid(uint8_t seed)
{
    np_module_uid_t u;
    for (unsigned i = 0U; i < NP_HEXMAP_UID_LEN; i++) {
        u.b[i] = (uint8_t)(seed + 3U * i);
    }
    return u;
}

/* Power-on: bind the geometry, empty map. */
static void map_boot(void)
{
    for (unsigned s = 0U; s < MAP_SOCKETS; s++) {
        g_geom[s].present_in_helmet = true;
        g_geom[s].x_mm = (int16_t)s;
        g_geom[s].y_mm = 0;
    }
    (void)np_module_map_init(g_geom, (uint16_t)MAP_SOCKETS);
}

/* Seat generation `gen` of the helmet: a module in every even socket, its
 * UID and calibration both derived from gen, so two generations differ in
 * every seated record. */
static void map_seat(uint8_t gen)
{
    map_boot();
    for (unsigned s = 0U; s < MAP_SOCKETS; s += 2U) {
        np_module_uid_t u = map_uid((uint8_t)(gen * 0x40U + s));
        (void)np_module_map_apply_poll((uint16_t)s, &u, 0U, map_inv, NULL, NULL);
        float cal[NP_HEXMAP_CAL_FLOATS];
        for (unsigned c = 0U; c < NP_HEXMAP_CAL_FLOATS; c++) {
            cal[c] = 0.1f * (float)gen + 0.001f * (float)(c + 1U);
        }
        (void)np_module_map_set_cal(&u, cal);
    }
}

/* The power-on poll re-reports socket 0's generation-`gen` module; returns
 * the inventory calls it cost (0 = the restored record was confirmed). */
static int map_confirm0(uint8_t gen)
{
    np_module_uid_t u = map_uid((uint8_t)(gen * 0x40U));
    g_map_inv_calls = 0;
    (void)np_module_map_apply_poll(0U, &u, 0U, map_inv, NULL, NULL);
    return g_map_inv_calls;
}

static void s_npmp_baseline(void)
{
    ASSERT(np_cfg_store_format() == NP_HUB_OK && np_cfg_store_mount() == NP_HUB_OK,
           "baseline format/mount");
    map_seat(1U);
    ASSERT(np_module_map_persist() == NP_HUB_OK, "baseline persist");
    np_cfg_store_unmount();
}

static void s_npmp_persist(void)
{
    g_npmp_acked = false;
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    map_seat(2U);
    g_npmp_acked = (np_module_map_persist() == NP_HUB_OK);
    np_cfg_store_unmount();
}

static int v_npmp(const char *what, long cut, np_powerbd_tear_t tear)
{
    if (np_cfg_store_mount() != NP_HUB_OK) {
        finding(what, cut, tear, "MOUNT FAILED after the cut");
        return 1;
    }
    size_t len = 0U;
    np_hub_status_t rd = np_hexmap_nvram_read(g_npmp_rb, sizeof(g_npmp_rb), &len);
    map_boot();
    np_hub_status_t rs = np_module_map_restore();
    np_cfg_store_unmount();
    if (rd != NP_HUB_OK || rs != NP_HUB_OK || len != g_npmp_len) {
        finding(what, cut, tear, "npmp.bin lost (read %d, restore %d) — the "
                "live blob went before its replacement was durable (L-3)", rd, rs);
        return 1;
    }
    bool is_old = (memcmp(g_npmp_rb, g_npmp_old, len) == 0);
    bool is_new = (memcmp(g_npmp_rb, g_npmp_new, len) == 0);
    if (!is_old && !is_new) {
        finding(what, cut, tear, "npmp.bin is neither generation (L-3)");
        return 1;
    }
    if (g_npmp_acked && !is_new) {
        finding(what, cut, tear, "persist returned OK and npmp.bin is still "
                "the old blob — it was not durable");
        return 1;
    }
    return 0;
}

static void test_module_map_blob_through_the_store(void)
{
    np_physical_loc_t loc;
    const np_hex_addr_t a0 = { 0U, 1U };
    size_t len = 0U;

    /* The two generations' exact bytes, for the sweep's verifier. */
    map_seat(1U);
    int n = np_module_map_serialize(g_npmp_old, sizeof(g_npmp_old));
    map_seat(2U);
    ASSERT(np_module_map_serialize(g_npmp_new, sizeof(g_npmp_new)) == n && n > 0,
           "serialize both generations");
    g_npmp_len = (size_t)n;
    ASSERT(g_npmp_len == 14012U, "80 sockets serialize to 14,012 bytes "
           "(NP-FW-NVRAM-001 OI-NVRAM-11)");

    /* A new device: nothing stored, and restore leaves the map empty. */
    fresh();
    map_boot();
    ASSERT(np_module_map_restore() == NP_HUB_ERR_NOT_PRESENT,
           "a never-written blob must restore as absent");
    ASSERT(map_confirm0(1U) == 1, "after a failed restore the poll must "
           "re-inventory: the map was not empty");

    /* Persist, reboot, restore: the cache survives, and REQ-LFS-02 holds. */
    map_seat(1U);
    ASSERT(np_module_map_persist() == NP_HUB_OK, "persist through the store");
    np_cfg_store_unmount();
    reboot();
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "remount");
    map_boot();
    ASSERT(np_module_map_restore() == NP_HUB_OK, "restore after a reboot");
    ASSERT(np_module_map_resolve(a0, &loc) == NP_HUB_ERR_NOT_PRESENT,
           "REQ-LFS-02: a restored record resolved before the poll confirmed it");
    ASSERT(map_confirm0(1U) == 0, "the poll re-inventoried an unchanged module "
           "— the restored cache was not used");
    ASSERT(np_module_map_resolve(a0, &loc) == NP_HUB_OK &&
           loc.elem_type == NP_ELEM_LED_808, "the confirmed record resolves");
    np_module_uid_t u0 = map_uid(0x40U);
    float cal[NP_HEXMAP_CAL_FLOATS];
    ASSERT(np_module_map_get_cal(&u0, cal) == NP_HUB_OK &&
           cal[0] == 0.1f + 0.001f, "the calibration survived the reboot");

    /* A module swapped while the hub was off: the stale record is replaced,
     * and its calibration reaches neither module. */
    map_boot();
    ASSERT(np_module_map_restore() == NP_HUB_OK, "restore again");
    ASSERT(map_confirm0(3U) == 1, "a different module was confirmed from the "
           "cache instead of being inventoried");
    np_module_uid_t u3 = map_uid(0xC0U);
    ASSERT(np_module_map_get_cal(&u3, cal) == NP_HUB_ERR_NOT_PRESENT &&
           np_module_map_get_cal(&u0, cal) == NP_HUB_ERR_NOT_PRESENT,
           "a stored calibration outlived the swap");

    /* A blob the map would refuse is never written, and the live one stays. */
    uint8_t junk[64];
    memset(junk, 0xA5, sizeof(junk));
    ASSERT(np_hexmap_nvram_write(junk, sizeof(junk)) == NP_HUB_ERR_INVALID_ARG,
           "a blob with no valid header was written");
    ASSERT(np_hexmap_nvram_write(NULL, 0U) == NP_HUB_ERR_INVALID_ARG,
           "a NULL blob was written");
    ASSERT(np_hexmap_nvram_read(g_npmp_rb, sizeof(g_npmp_rb), &len) == NP_HUB_OK &&
           len == g_npmp_len && memcmp(g_npmp_rb, g_npmp_old, len) == 0,
           "a refused write disturbed the live blob");
    ASSERT(np_hexmap_nvram_read(NULL, 0U, &len) == NP_HUB_ERR_INVALID_ARG &&
           np_hexmap_nvram_read(g_npmp_rb, sizeof(g_npmp_rb), NULL)
               == NP_HUB_ERR_INVALID_ARG, "a NULL read argument was accepted");

    /* Too small a buffer is a refusal, never a truncated success. */
    ASSERT(np_hexmap_nvram_read(g_npmp_rb, g_npmp_len - 1U, &len) != NP_HUB_OK &&
           len == 0U, "a short buffer read back part of the blob as success");

    /* Damage on the medium (OI-LFS-09): the content check refuses, and the
     * map comes back EMPTY rather than holding what was read. */
    np_module_uid_t probe = map_uid(0x40U + 2U);   /* socket 2, generation 1 */
    long blk = find_block(probe.b, NP_HEXMAP_UID_LEN);
    ASSERT(blk >= 0, "the blob's bytes were not found on the medium");
    if (blk >= 0) {
        uint8_t *p = g_media + ((size_t)blk * NP_LFS_CFG_BLOCK_SIZE);
        for (size_t o = 0U; o + NP_HEXMAP_UID_LEN <= NP_LFS_CFG_BLOCK_SIZE; o++) {
            if (memcmp(p + o, probe.b, NP_HEXMAP_UID_LEN) == 0) {
                p[o] ^= 0x01U;
                break;
            }
        }
    }
    np_cfg_store_unmount();
    reboot();
    ASSERT(np_cfg_store_mount() == NP_HUB_OK, "remount after damage");
    map_boot();
    ASSERT(np_module_map_restore() == NP_HUB_ERR_STORE_INTEGRITY,
           "a damaged blob was not refused by its content check");
    ASSERT(map_confirm0(1U) == 1, "a refused blob left records in the map");
    np_cfg_store_unmount();

    /* A power cut at every op of np_module_map_persist(). */
    np_sweep_result_t r = run("module-map persist", s_npmp_baseline,
                              s_npmp_persist, v_npmp, false);
    ASSERT(r.ops > 0 && r.missed_cuts == 0, "persist sweep did not run as measured");
    ASSERT(r.violations == 0,
           "a power loss during np_module_map_persist() lost or tore the blob");
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
    test_session_count_is_persisted();
    test_reset_marker_is_durable();
    test_journal_rows_of_differing_length();
    test_module_map_blob_through_the_store();

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
