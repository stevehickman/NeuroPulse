/*
 * NeurOne SW-02 — littlefs power-loss injection tests (claims L-1…L-4)
 * Document: NP-SOUP-LFS-001 Rev 3 §7.2, §12 — the NeurOne half of OI-LFS-02
 *
 * ── What this suite is for ───────────────────────────────────────────────────
 *
 * NP-SOUP-LFS-001 §3 states seven things NeurOne's specifications require of
 * littlefs.  Four of them are power-loss atomicity claims, and until this suite
 * existed all four were ASSERTED — by NP-FW-NVRAM-001 §4, by EMMC-FS-01 and by
 * NP-FW-HUB-001 §6.5 — against a component nothing had ever interrupted:
 *
 *   L-1  a flush commits the exact buffered tail and issues a block-device
 *        sync; at most the records appended since the last flush are lost
 *   L-2  the log file is append-mode, so a flush never rewrites a previously
 *        committed byte
 *   L-3  a write never destroys the live inventory record before its
 *        replacement is durable
 *   L-4  a torn Map 3 write costs one record, not all of them
 *
 * The old SOUP verification cell for this component read "Power-loss testing per
 * LittleFS test suite", which records that an upstream project tests its own
 * code.  This file is what replaces it: NeurOne's own interruption, of NeurOne's
 * own commit orderings, over the EMMC-FS-01 instance parameters the device will
 * actually mount — np_lfs_config_apply() and np_lfs_config_validate() are called
 * here exactly as the OI-LOG-05..07 glue must call them, so a geometry that
 * would be refused at mount is refused here too.
 *
 * ── How a power loss is modelled ─────────────────────────────────────────────
 *
 * np_lfs_powerbd.c.  Read its header: the short version is that the injector
 * applies the partial physical effect of the interrupted prog/erase and then
 * longjmp()s out of littlefs, because a power loss is not an error return.  The
 * media array survives the cut; every byte of littlefs's RAM state does not.
 *
 * Each scenario is swept: an uncut run establishes how many medium-touching ops
 * the sequence performs, and then the same sequence is re-run once per op index
 * per tear model, each time from the identical pre-state.  Every one of those
 * attempts is followed by a remount and a full verification.  The suite asserts
 * that every armed cut actually fired (case 6) — a sweep in which the injector
 * did nothing would otherwise report a clean pass having tested nothing.
 *
 * ── Falsified in both directions, per NP-CONV-001 §8 ─────────────────────────
 *
 * §8's rule is that a check must be falsified before it is trusted, and for a
 * power-loss test that means two different things, so both are done:
 *
 *   DIRECTION 1 — the checks can SEE a violation.  Cases 4 and 5 re-run the same
 *   sweeps against deliberately unsafe commit orderings — a log rewritten in
 *   place instead of appended, a blob removed before its replacement is durable,
 *   a journal truncated and rebuilt instead of extended — and REQUIRE the
 *   verifiers to catch them.  If an unsafe ordering survives a sweep, the
 *   corresponding safe result is vacuous and the suite fails.  The third of
 *   these is REQ-LFS-01's subject matter: it is discard-and-rebuild applied to a
 *   record store, which NP-SOUP-LFS-001 §6.2 says is data loss.
 *
 *   DIRECTION 2 — the injector really cuts.  Case 6 asserts a non-zero cut count
 *   for every sweep, that every armed index fired, and that the uncut baseline
 *   run of each scenario completes and produces exactly the expected content.
 *
 * ── What this suite does NOT prove ───────────────────────────────────────────
 *
 * That the eMMC honours the contract.  This device models struct lfs_config's
 * read/prog/erase/sync contract at prog_size granularity, which is the contract
 * L-1…L-4 are stated against.  NeurOne's medium is an eMMC behind an XTS layer,
 * whose FTL may tear inside a 512 B sector, may reorder, and whose erase
 * semantics are not the raw-flash semantics littlefs was written for (upstream
 * #1083 is exactly this question, unanswered).  No host test can reach that.
 * It is OI-LFS-07 and it needs hardware.
 *
 * That anything is integrated.  OI-LOG-05..07 are still unimplemented; this
 * suite supplies its own block device because the device's own does not exist.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "np_crypto.h"        /* np_crc32 — a checker independent of the component */
#include "np_lfs_config.h"
#include "np_lfs_instance.h"
#include "np_lfs_powerbd.h"

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));  \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

/* ── The instance under test is the real one ──────────────────────────────── */

#define MEDIA_BYTES ((size_t)NP_LFS_CFG_BLOCK_SIZE * NP_LFS_CFG_BLOCK_COUNT)

static uint8_t g_media[MEDIA_BYTES];
static uint8_t g_snapshot[MEDIA_BYTES];
static uint8_t g_dirty[(NP_LFS_CFG_BLOCK_COUNT + 7U) / 8U];

static lfs_t            g_lfs;
static struct lfs_config g_cfg;
static np_powerbd_t     g_bd;
static uint8_t          g_file_buffer[NP_LFS_FILE_BUFFER_SIZE];

/* LFS_THREADSAFE demands both; a longjmp out of littlefs leaves the lock held,
 * which is what the reset in attempt() is for.  On target this is a FreeRTOS
 * mutex — see np_lfs_config.h decision 4. */
static int g_lock_depth = 0;
static int host_lock(const struct lfs_config *c)   { (void)c; g_lock_depth++; return 0; }
static int host_unlock(const struct lfs_config *c) { (void)c; g_lock_depth--; return 0; }

static void build_config(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));

    /* The EMMC-FS-01 instance, from the same function the mount glue must use.
     * Not a geometry chosen to make the test quick. */
    if (np_lfs_config_apply(&g_cfg) != NP_HUB_OK) {
        printf("FAIL [build_config] np_lfs_config_apply refused\n");
        g_fail_count++;
    }

    np_powerbd_bind(&g_bd, &g_cfg, g_media,
                    NP_LFS_CFG_BLOCK_SIZE, NP_LFS_CFG_BLOCK_COUNT,
                    NP_LFS_CFG_PROG_SIZE, g_dirty, sizeof(g_dirty));

    g_cfg.lock   = host_lock;
    g_cfg.unlock = host_unlock;

    if (np_lfs_config_validate(&g_cfg) != NP_HUB_OK) {
        printf("FAIL [build_config] np_lfs_config_validate refused the config "
               "this suite runs against — every result below would be about a "
               "filesystem the device will not mount\n");
        g_fail_count++;
    }
}

/* ── Reporting ────────────────────────────────────────────────────────────────
 *
 * A falsification sweep EXPECTS findings — that is the whole point of running
 * it — and an unsafe ordering produces one at most cut points, so printing them
 * as FAIL lines would bury the real result under a thousand of them.  So a
 * finding is reported according to which kind of sweep asked for it: loud when
 * it should not have happened, and as a capped sample when it should.
 */
static bool g_expect_findings = false;
static int  g_findings_shown  = 0;

#define FINDINGS_SHOWN_MAX 3

static void finding(const char *what, long cut_at, np_powerbd_tear_t tear,
                    const char *fmt, ...)
{
    if (g_expect_findings) {
        if (g_findings_shown >= FINDINGS_SHOWN_MAX) {
            return;
        }
        g_findings_shown++;
        printf("           caught: [%s] cut@%ld %s: ", what, cut_at,
               np_powerbd_tear_name(tear));
    } else {
        printf("FAIL [%s] cut@%ld %s: ", what, cut_at,
               np_powerbd_tear_name(tear));
    }

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

/* ── Record shapes ────────────────────────────────────────────────────────────
 *
 * 32 bytes, self-checking, which is the shape NP-FW-NVRAM-001 §4.2 D-5
 * specifies for Map 3 and NP-FW-HUB-001 §6.1 for the session log.  The CRC is
 * np_crc32 (firmware/crypto), NOT lfs_crc: a record checker that shares its
 * implementation with the component under test cannot witness that component
 * damaging a record.
 */
#define REC_SIZE      32U
#define REC_CRC_OFF   28U

static void record_build(uint8_t out[REC_SIZE], const char magic[4],
                         uint32_t ordinal, uint8_t generation)
{
    memcpy(out, magic, 4U);
    out[4] = (uint8_t)(ordinal & 0xFFU);
    out[5] = (uint8_t)((ordinal >> 8) & 0xFFU);
    out[6] = (uint8_t)((ordinal >> 16) & 0xFFU);
    out[7] = (uint8_t)((ordinal >> 24) & 0xFFU);
    for (unsigned i = 8U; i < REC_CRC_OFF; i++) {
        out[i] = (uint8_t)(generation ^ (ordinal * 31U + i));
    }
    uint32_t crc = np_crc32(out, REC_CRC_OFF);
    out[28] = (uint8_t)(crc & 0xFFU);
    out[29] = (uint8_t)((crc >> 8) & 0xFFU);
    out[30] = (uint8_t)((crc >> 16) & 0xFFU);
    out[31] = (uint8_t)((crc >> 24) & 0xFFU);
}

static bool record_valid(const uint8_t rec[REC_SIZE], const char magic[4],
                         uint32_t expect_ordinal)
{
    if (memcmp(rec, magic, 4U) != 0) {
        return false;
    }
    uint32_t ordinal = (uint32_t)rec[4] | ((uint32_t)rec[5] << 8) |
                       ((uint32_t)rec[6] << 16) | ((uint32_t)rec[7] << 24);
    if (ordinal != expect_ordinal) {
        return false;
    }
    uint32_t crc = np_crc32(rec, REC_CRC_OFF);
    uint32_t stored = (uint32_t)rec[28] | ((uint32_t)rec[29] << 8) |
                      ((uint32_t)rec[30] << 16) | ((uint32_t)rec[31] << 24);
    return crc == stored;
}

/* The "NPMP" module-map blob — NP-SOUP-LFS-001 §5.3's arithmetic exactly:
 * blob(n) = HDR(8) + n * 175 + CRC(4), at the n = 80 the Config partition
 * carries.  It is the one consumer family that can reach an emission, which is
 * why L-3 is tested against its shape rather than against a convenient one. */
#define BLOB_ELEMS  80U
#define BLOB_BYTES  (8U + (BLOB_ELEMS * 175U) + 4U)   /* 14,012 */

static uint8_t g_blob[BLOB_BYTES];

static void blob_build(uint8_t generation)
{
    memcpy(g_blob, "NPMP", 4U);
    g_blob[4] = generation;
    g_blob[5] = 0U;
    g_blob[6] = 0U;
    g_blob[7] = 0U;
    for (unsigned i = 8U; i < BLOB_BYTES - 4U; i++) {
        g_blob[i] = (uint8_t)(generation * 7U + (i & 0xFFU));
    }
    uint32_t crc = np_crc32(g_blob, BLOB_BYTES - 4U);
    g_blob[BLOB_BYTES - 4U] = (uint8_t)(crc & 0xFFU);
    g_blob[BLOB_BYTES - 3U] = (uint8_t)((crc >> 8) & 0xFFU);
    g_blob[BLOB_BYTES - 2U] = (uint8_t)((crc >> 16) & 0xFFU);
    g_blob[BLOB_BYTES - 1U] = (uint8_t)((crc >> 24) & 0xFFU);
}

static int blob_generation_of(const uint8_t *buf, size_t len)
{
    if (len != BLOB_BYTES || memcmp(buf, "NPMP", 4U) != 0) {
        return -1;
    }
    uint32_t crc = np_crc32(buf, (uint32_t)(BLOB_BYTES - 4U));
    uint32_t stored = (uint32_t)buf[BLOB_BYTES - 4U] |
                      ((uint32_t)buf[BLOB_BYTES - 3U] << 8) |
                      ((uint32_t)buf[BLOB_BYTES - 2U] << 16) |
                      ((uint32_t)buf[BLOB_BYTES - 1U] << 24);
    if (crc != stored) {
        return -1;
    }
    return (int)buf[4];
}

/* ── littlefs helpers ─────────────────────────────────────────────────────── */

static int mount_fresh(void)
{
    /* The lfs_t is RAM state.  A power loss destroys it; re-using the struct
     * across a cut would hand the remount a cache that the real device could
     * not possibly still hold. */
    memset(&g_lfs, 0, sizeof(g_lfs));
    return lfs_mount(&g_lfs, &g_cfg);
}

static int open_file(lfs_file_t *file, const char *path, int flags)
{
    static struct lfs_file_config fcfg;
    memset(&fcfg, 0, sizeof(fcfg));
    fcfg.buffer = g_file_buffer;   /* LFS_NO_MALLOC: lfs_file_open is unusable */
    return lfs_file_opencfg(&g_lfs, file, path, flags, &fcfg);
}

static void snapshot_take(void)
{
    memcpy(g_snapshot, g_media, MEDIA_BYTES);
    np_powerbd_dirty_clear(&g_bd);
}

/*
 * Put the medium back to the snapshot.  Only the blocks the last attempt
 * actually touched are copied — the Config instance is 16 MiB and the sweeps
 * below run thousands of attempts, so a whole-image restore would make the run
 * time a property of the partition size instead of the work under test.  The
 * bd's dirty map is what makes that safe: it is set by every prog and erase
 * that reached the medium, torn ones included.
 */
static void snapshot_restore(void)
{
    for (lfs_size_t b = 0U; b < NP_LFS_CFG_BLOCK_COUNT; b++) {
        if (np_powerbd_block_dirty(&g_bd, b)) {
            const size_t off = (size_t)b * NP_LFS_CFG_BLOCK_SIZE;
            memcpy(g_media + off, g_snapshot + off, NP_LFS_CFG_BLOCK_SIZE);
        }
    }
    np_powerbd_dirty_clear(&g_bd);
}

/* ── The sweep harness ────────────────────────────────────────────────────── */

static jmp_buf g_resume;

/* Parameters of the attempt in flight.  File-scope rather than locals of
 * attempt(): a local modified between setjmp() and longjmp() is undefined
 * unless it is volatile, and this is the cheaper way to be right about it. */
static void (*g_body)(void);
static long              g_cut_at;
static np_powerbd_tear_t g_tear;

/* Run `body` once from the snapshot, cutting at op `cut_at`.
 * Returns true if the cut fired. */
static bool attempt(long cut_at, np_powerbd_tear_t tear, void (*body)(void))
{
    g_body   = body;
    g_cut_at = cut_at;
    g_tear   = tear;

    snapshot_restore();
    g_lock_depth = 0;              /* the cut longjmp'd out with the lock held */
    np_powerbd_power_cycle(&g_bd);
    np_powerbd_arm(&g_bd, g_cut_at, g_tear, &g_resume);

    if (setjmp(g_resume) == 0) {
        g_body();
        return false;
    }
    return true;                   /* the processor stopped inside littlefs */
}

/* ── Scenario 1 — the session log (L-1, L-2) ──────────────────────────────── */

#define LOG_PATH        "session.log"
/* Sized so the file crosses block boundaries rather than living inside one
 * metadata pair: 160 records is 5,120 B against a 4,096 B block, so the append
 * under test allocates, programs and commits through the ctz skip-list — the
 * path L-1 and L-2 are actually about.  A scenario that fitted in one inline
 * metadata entry would have swept a different filesystem. */
#define LOG_BASE_RECS   160U       /* durable in the snapshot — 5,120 B */
#define LOG_BATCH       32U        /* records per flush in the mutation */
#define LOG_BATCHES     2U

static const char LOG_MAGIC[4] = { 'N', 'P', 'L', 'R' };

static void log_baseline(void)
{
    lfs_file_t file;
    uint8_t    rec[REC_SIZE];

    if (lfs_format(&g_lfs, &g_cfg) != 0 || mount_fresh() != 0) {
        printf("FAIL [log_baseline] format/mount\n");
        g_fail_count++;
        return;
    }
    if (open_file(&file, LOG_PATH, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND) != 0) {
        printf("FAIL [log_baseline] open\n");
        g_fail_count++;
        return;
    }
    for (uint32_t i = 0U; i < LOG_BASE_RECS; i++) {
        record_build(rec, LOG_MAGIC, i, 1U);
        lfs_file_write(&g_lfs, &file, rec, REC_SIZE);
    }
    lfs_file_sync(&g_lfs, &file);
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);
}

/* The mutation under test: append LOG_BATCHES batches, flushing after each.
 * L-1 says a flush commits the EXACT buffered tail, so after any cut the
 * recovered length must be one of the flush boundaries and never between them. */
static void log_append_batches(void)
{
    lfs_file_t file;
    uint8_t    rec[REC_SIZE];

    if (mount_fresh() != 0) { return; }
    if (open_file(&file, LOG_PATH, LFS_O_WRONLY | LFS_O_APPEND) != 0) {
        lfs_unmount(&g_lfs);
        return;
    }
    for (uint32_t b = 0U; b < LOG_BATCHES; b++) {
        for (uint32_t i = 0U; i < LOG_BATCH; i++) {
            uint32_t ordinal = LOG_BASE_RECS + (b * LOG_BATCH) + i;
            record_build(rec, LOG_MAGIC, ordinal, 1U);
            lfs_file_write(&g_lfs, &file, rec, REC_SIZE);
        }
        lfs_file_sync(&g_lfs, &file);
    }
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);
}

/* FALSIFICATION (direction 1) for L-2: the same log rewritten IN PLACE instead
 * of appended.  A previously committed byte changes, which is precisely what
 * L-2 forbids, and log_verify() must catch it. */
static void log_rewrite_in_place(void)
{
    lfs_file_t file;
    uint8_t    rec[REC_SIZE];

    if (mount_fresh() != 0) { return; }
    if (open_file(&file, LOG_PATH, LFS_O_WRONLY) != 0) {
        lfs_unmount(&g_lfs);
        return;
    }
    /* Eight records is enough to violate L-2, and a sweep costs one full re-run
     * of the sequence per op in it — rewriting all 160 would buy no additional
     * finding at several minutes of run time. */
    lfs_file_seek(&g_lfs, &file, 0, LFS_SEEK_SET);
    for (uint32_t i = 0U; i < 8U; i++) {
        record_build(rec, LOG_MAGIC, i, 9U);   /* different generation */
        lfs_file_write(&g_lfs, &file, rec, REC_SIZE);
        lfs_file_sync(&g_lfs, &file);
    }
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);
}

static uint8_t g_readback[REC_SIZE * (LOG_BASE_RECS + (LOG_BATCH * LOG_BATCHES))];

/*
 * Verify the log after a cut.  Returns the number of violations found.
 * Never prints on a clean result: the sweep prints a summary instead.
 */
static int log_verify(const char *what, long cut_at, np_powerbd_tear_t tear)
{
    lfs_file_t file;
    int        violations = 0;

    np_powerbd_power_cycle(&g_bd);

    if (mount_fresh() != 0) {
        finding(what, cut_at, tear, "MOUNT FAILED after the cut — littlefs did "
                "not fall back to a last known good state");
        return 1;
    }

    if (open_file(&file, LOG_PATH, LFS_O_RDONLY) != 0) {
        finding(what, cut_at, tear, "the log file is GONE — L-1's committed "
                "prefix did not survive");
        lfs_unmount(&g_lfs);
        return 1;
    }

    lfs_ssize_t got = lfs_file_read(&g_lfs, &file, g_readback, sizeof(g_readback));
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);

    if (got < 0) {
        finding(what, cut_at, tear, "read returned %d", (int)got);
        return 1;
    }

    size_t bytes = (size_t)got;

    /* L-1: a flush commits the exact buffered tail.  The only lengths that may
     * survive are flush boundaries — 8, 12 or 16 records.  A length between two
     * boundaries would mean a partial batch became visible. */
    if ((bytes % REC_SIZE) != 0U) {
        finding(what, cut_at, tear, "length %zu is not a whole number of "
                "records", bytes);
        violations++;
    }
    size_t recs = bytes / REC_SIZE;
    bool boundary = false;
    for (uint32_t b = 0U; b <= LOG_BATCHES; b++) {
        if (recs == (size_t)(LOG_BASE_RECS + (b * LOG_BATCH))) {
            boundary = true;
        }
    }
    if (!boundary) {
        finding(what, cut_at, tear, "%zu records survived — not a flush "
                "boundary (L-1: a flush commits the exact buffered tail)", recs);
        violations++;
    }
    if (recs < LOG_BASE_RECS) {
        finding(what, cut_at, tear, "only %zu of %u previously flushed records "
                "survived (L-1)", recs, LOG_BASE_RECS);
        violations++;
    }

    /* L-2: every record that was committed before this run must be byte-identical
     * to what was committed.  This is the observable form of "append never
     * rewrites a previously committed byte" — the block-level form is not
     * checkable, because copy-on-write legitimately reprograms a block once its
     * contents have been relocated. */
    uint8_t expect[REC_SIZE];
    size_t  checked = (recs < LOG_BASE_RECS) ? recs : LOG_BASE_RECS;
    for (size_t i = 0U; i < checked; i++) {
        record_build(expect, LOG_MAGIC, (uint32_t)i, 1U);
        if (memcmp(g_readback + (i * REC_SIZE), expect, REC_SIZE) != 0) {
            finding(what, cut_at, tear, "record %zu changed after it had been "
                    "committed (L-2)", i);
            violations++;
            break;
        }
    }

    /* Whatever survived must be intact and in order — no torn record, no gap. */
    for (size_t i = checked; i < recs; i++) {
        if (!record_valid(g_readback + (i * REC_SIZE), LOG_MAGIC, (uint32_t)i)) {
            finding(what, cut_at, tear, "record %zu is torn or out of order", i);
            violations++;
            break;
        }
    }

    return violations;
}

/* ── Scenario 2 — atomic replacement of the "NPMP" blob (L-3) ─────────────── */

#define BLOB_PATH  "npmp.bin"
#define BLOB_TMP   "npmp.tmp"

static void blob_write(const char *path, uint8_t generation, int flags)
{
    lfs_file_t file;
    blob_build(generation);
    if (open_file(&file, path, flags) != 0) {
        return;
    }
    lfs_file_write(&g_lfs, &file, g_blob, BLOB_BYTES);
    lfs_file_sync(&g_lfs, &file);
    lfs_file_close(&g_lfs, &file);
}

static void blob_baseline(void)
{
    if (lfs_format(&g_lfs, &g_cfg) != 0 || mount_fresh() != 0) {
        printf("FAIL [blob_baseline] format/mount\n");
        g_fail_count++;
        return;
    }
    blob_write(BLOB_PATH, 1U, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    lfs_unmount(&g_lfs);
}

/* The safe ordering: the replacement is made durable under a second name, and
 * only then does the live name move.  This is what L-3 requires of the caller,
 * and it is the ordering the OI-LOG-05..07 glue must use. */
static void blob_replace_atomic(void)
{
    if (mount_fresh() != 0) { return; }
    blob_write(BLOB_TMP, 2U, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    lfs_rename(&g_lfs, BLOB_TMP, BLOB_PATH);
    lfs_unmount(&g_lfs);
}

/* FALSIFICATION (direction 1) for L-3: destroy the live record first.  There is
 * a window in which npmp.bin does not exist, and blob_verify() must find it. */
static void blob_replace_unsafe(void)
{
    if (mount_fresh() != 0) { return; }
    lfs_remove(&g_lfs, BLOB_PATH);
    blob_write(BLOB_PATH, 2U, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    lfs_unmount(&g_lfs);
}

static uint8_t g_blob_readback[BLOB_BYTES];

static int blob_verify(const char *what, long cut_at, np_powerbd_tear_t tear)
{
    lfs_file_t file;

    np_powerbd_power_cycle(&g_bd);

    if (mount_fresh() != 0) {
        finding(what, cut_at, tear, "MOUNT FAILED after the cut");
        return 1;
    }

    if (open_file(&file, BLOB_PATH, LFS_O_RDONLY) != 0) {
        finding(what, cut_at, tear, "npmp.bin is ABSENT — the live inventory "
                "record was destroyed before its replacement was durable (L-3)");
        lfs_unmount(&g_lfs);
        return 1;
    }

    lfs_ssize_t got = lfs_file_read(&g_lfs, &file, g_blob_readback, BLOB_BYTES);
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);

    int gen = (got < 0) ? -1 : blob_generation_of(g_blob_readback, (size_t)got);
    if (gen != 1 && gen != 2) {
        finding(what, cut_at, tear, "npmp.bin is neither the old blob nor the "
                "new one (read %d bytes, generation %d) (L-3)", (int)got, gen);
        return 1;
    }
    return 0;
}

/* ── Scenario 3 — the Map 3 journal (L-4) ─────────────────────────────────── */

#define JRN_PATH       "map3.jrn"
#define JRN_BASE_RECS  200U        /* 6,400 B — more than one block */
#define JRN_APPENDS    4U

static const char JRN_MAGIC[4] = { 'N', 'P', 'M', '3' };

static void jrn_baseline(void)
{
    lfs_file_t file;
    uint8_t    rec[REC_SIZE];

    if (lfs_format(&g_lfs, &g_cfg) != 0 || mount_fresh() != 0) {
        printf("FAIL [jrn_baseline] format/mount\n");
        g_fail_count++;
        return;
    }
    if (open_file(&file, JRN_PATH, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND) != 0) {
        printf("FAIL [jrn_baseline] open\n");
        g_fail_count++;
        return;
    }
    for (uint32_t i = 0U; i < JRN_BASE_RECS; i++) {
        record_build(rec, JRN_MAGIC, i, 1U);
        lfs_file_write(&g_lfs, &file, rec, REC_SIZE);
    }
    lfs_file_sync(&g_lfs, &file);
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);
}

/* Tail-additive, one flush per record — NP-FW-NVRAM-001 §4.2 D-5. */
static void jrn_append(void)
{
    lfs_file_t file;
    uint8_t    rec[REC_SIZE];

    if (mount_fresh() != 0) { return; }
    if (open_file(&file, JRN_PATH, LFS_O_WRONLY | LFS_O_APPEND) != 0) {
        lfs_unmount(&g_lfs);
        return;
    }
    for (uint32_t i = 0U; i < JRN_APPENDS; i++) {
        record_build(rec, JRN_MAGIC, JRN_BASE_RECS + i, 1U);
        lfs_file_write(&g_lfs, &file, rec, REC_SIZE);
        lfs_file_sync(&g_lfs, &file);
    }
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);
}

/*
 * FALSIFICATION (direction 1) for L-4, and it is REQ-LFS-01's own subject:
 * discard-and-rebuild applied to a record store.  np_module_map may do this to
 * the "NPMP" blob because the sockets will re-answer; nothing will re-answer
 * Map 3 (NP-SOUP-LFS-001 §6.2).  Truncating and rebuilding opens a window in
 * which most of the journal is simply gone, and jrn_verify() must see it.
 */
static void jrn_truncate_and_rebuild(void)
{
    lfs_file_t file;
    uint8_t    rec[REC_SIZE];

    if (mount_fresh() != 0) { return; }
    if (open_file(&file, JRN_PATH, LFS_O_WRONLY | LFS_O_TRUNC) != 0) {
        lfs_unmount(&g_lfs);
        return;
    }
    lfs_file_sync(&g_lfs, &file);                 /* the truncation is committed */
    for (uint32_t i = 0U; i < JRN_BASE_RECS + JRN_APPENDS; i++) {
        record_build(rec, JRN_MAGIC, i, 1U);
        lfs_file_write(&g_lfs, &file, rec, REC_SIZE);
    }
    lfs_file_sync(&g_lfs, &file);
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);
}

static uint8_t g_jrn_readback[REC_SIZE * (JRN_BASE_RECS + JRN_APPENDS)];

static int jrn_verify(const char *what, long cut_at, np_powerbd_tear_t tear)
{
    lfs_file_t file;
    int        violations = 0;

    np_powerbd_power_cycle(&g_bd);

    if (mount_fresh() != 0) {
        finding(what, cut_at, tear, "MOUNT FAILED after the cut");
        return 1;
    }
    if (open_file(&file, JRN_PATH, LFS_O_RDONLY) != 0) {
        finding(what, cut_at, tear, "the journal is GONE — a torn write cost "
                "ALL the records (L-4)");
        lfs_unmount(&g_lfs);
        return 1;
    }

    lfs_ssize_t got = lfs_file_read(&g_lfs, &file, g_jrn_readback,
                                    sizeof(g_jrn_readback));
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);

    if (got < 0) {
        finding(what, cut_at, tear, "read returned %d", (int)got);
        return 1;
    }

    size_t recs = (size_t)got / REC_SIZE;

    /* L-4: the loss is bounded to the record in flight.  Every record that was
     * already durable must still be there, intact and in order. */
    if (recs < JRN_BASE_RECS) {
        finding(what, cut_at, tear, "%zu of %u durable records survived — a "
                "torn write cost more than one record (L-4)", recs, JRN_BASE_RECS);
        violations++;
    }
    for (size_t i = 0U; i < recs; i++) {
        if (!record_valid(g_jrn_readback + (i * REC_SIZE), JRN_MAGIC, (uint32_t)i)) {
            finding(what, cut_at, tear, "journal record %zu is torn or out of "
                    "order (L-4)", i);
            violations++;
            break;
        }
    }
    return violations;
}

/* ── The sweep ────────────────────────────────────────────────────────────── */

typedef int (*verify_fn)(const char *what, long cut_at, np_powerbd_tear_t tear);

typedef struct {
    long ops;          /* medium-touching ops in an uncut run    */
    long cuts;         /* cuts that actually fired               */
    long attempts;     /* sweep attempts                         */
    long violations;   /* verifier findings                      */
    long missed_cuts;  /* armed indices that did not fire        */
} sweep_result_t;

/*
 * Sweep every op index of `mutate`, once per tear model, verifying after each.
 * `baseline` must leave the medium in the pre-state; it is run once and
 * snapshotted.  Findings are COUNTED, not asserted here — the safe scenarios
 * require zero and the falsification scenarios require more than zero, and it
 * is the caller that knows which it is.
 */
static sweep_result_t sweep(const char *what, void (*baseline)(void),
                            void (*mutate)(void), verify_fn verify,
                            bool expect_findings)
{
    sweep_result_t r;
    memset(&r, 0, sizeof(r));

    g_expect_findings = expect_findings;
    g_findings_shown  = 0;

    /* Pre-state, and the uncut reference run that establishes the op count. */
    np_powerbd_wipe(&g_bd);
    np_powerbd_power_cycle(&g_bd);
    baseline();
    snapshot_take();

    (void)attempt(NP_POWERBD_NO_CUT, NP_POWERBD_TEAR_NONE, mutate);
    r.ops = np_powerbd_ops_seen(&g_bd);

    for (int t = 0; t < (int)NP_POWERBD_TEAR_MODEL_COUNT; t++) {
        for (long cut = 0; cut < r.ops; cut++) {
            bool fired = attempt(cut, (np_powerbd_tear_t)t, mutate);
            r.attempts++;
            if (fired) {
                r.cuts++;
            } else {
                r.missed_cuts++;
            }
            r.violations += verify(what, cut, (np_powerbd_tear_t)t);
        }
    }

    g_expect_findings = false;
    return r;
}

/* ── 1. L-1 and L-2 — the session log ─────────────────────────────────────── */

static sweep_result_t g_log_safe;

static void test_log_append_is_power_safe(void)
{
    g_log_safe = sweep("L-1/L-2 append", log_baseline, log_append_batches,
                       log_verify, false);

    printf("  L-1/L-2  %ld ops swept x 3 tear models = %ld attempts, "
           "%ld cuts, %ld violations\n",
           g_log_safe.ops, g_log_safe.attempts, g_log_safe.cuts,
           g_log_safe.violations);

    ASSERT(g_log_safe.ops > 0,
           "the append sequence touched the medium zero times — the scenario "
           "did not run");
    ASSERT(g_log_safe.violations == 0,
           "L-1/L-2: a power loss during an append damaged or lost previously "
           "flushed records. NP-FW-HUB-001 §6.5's durability model does not "
           "hold as written");
}

/* ── 2. L-3 — atomic replacement of the inventory blob ────────────────────── */

static sweep_result_t g_blob_safe;

static void test_blob_replace_is_atomic(void)
{
    g_blob_safe = sweep("L-3 atomic replace", blob_baseline,
                        blob_replace_atomic, blob_verify, false);

    printf("  L-3      %ld ops swept x 3 tear models = %ld attempts, "
           "%ld cuts, %ld violations\n",
           g_blob_safe.ops, g_blob_safe.attempts, g_blob_safe.cuts,
           g_blob_safe.violations);

    ASSERT(g_blob_safe.ops > 0, "the replace sequence touched the medium zero times");
    ASSERT(g_blob_safe.violations == 0,
           "L-3: a power loss during the write-temp-then-rename replacement left "
           "npmp.bin absent or invalid. NP-FW-NVRAM-001 §4.2's atomicity does "
           "not hold as written");
}

/* ── 3. L-4 — the Map 3 journal ───────────────────────────────────────────── */

static sweep_result_t g_jrn_safe;

static void test_journal_torn_write_costs_one_record(void)
{
    g_jrn_safe = sweep("L-4 journal", jrn_baseline, jrn_append, jrn_verify, false);

    printf("  L-4      %ld ops swept x 3 tear models = %ld attempts, "
           "%ld cuts, %ld violations\n",
           g_jrn_safe.ops, g_jrn_safe.attempts, g_jrn_safe.cuts,
           g_jrn_safe.violations);

    ASSERT(g_jrn_safe.ops > 0, "the journal append touched the medium zero times");
    ASSERT(g_jrn_safe.violations == 0,
           "L-4: a power loss during a tail-additive journal append cost more "
           "than the record in flight");
}

/* ── 4. Falsification, direction 1 — the verifiers can see a violation ────── */

static void test_falsification_unsafe_orderings_are_caught(void)
{
    sweep_result_t rewrite = sweep("FALSIFY L-2 in-place rewrite", log_baseline,
                                   log_rewrite_in_place, log_verify, true);
    printf("  falsify  in-place log rewrite: %ld attempts, %ld cuts, "
           "%ld violations seen\n",
           rewrite.attempts, rewrite.cuts, rewrite.violations);
    ASSERT(rewrite.violations > 0,
           "L-2's check did not notice a log rewritten in place. It cannot "
           "distinguish append from overwrite, so the L-1/L-2 result above is "
           "vacuous (NP-CONV-001 §8)");

    sweep_result_t unsafe_blob = sweep("FALSIFY L-3 remove-then-write",
                                       blob_baseline, blob_replace_unsafe,
                                       blob_verify, true);
    printf("  falsify  remove-then-write blob: %ld attempts, %ld cuts, "
           "%ld violations seen\n",
           unsafe_blob.attempts, unsafe_blob.cuts, unsafe_blob.violations);
    ASSERT(unsafe_blob.violations > 0,
           "L-3's check did not notice the live blob being destroyed before its "
           "replacement was durable. The L-3 result above is vacuous");

    sweep_result_t rebuild = sweep("FALSIFY L-4 truncate-and-rebuild",
                                   jrn_baseline, jrn_truncate_and_rebuild,
                                   jrn_verify, true);
    printf("  falsify  truncate-and-rebuild journal: %ld attempts, %ld cuts, "
           "%ld violations seen\n",
           rebuild.attempts, rebuild.cuts, rebuild.violations);
    ASSERT(rebuild.violations > 0,
           "L-4's check did not notice discard-and-rebuild applied to a record "
           "store — which is exactly what REQ-LFS-01 exists to forbid. The L-4 "
           "result above is vacuous");
}

/* ── 5. Falsification, direction 2 — the injector really cuts ─────────────── */

static void test_falsification_the_injector_fires(void)
{
    ASSERT(g_bd.total_cuts > 0,
           "no cut fired anywhere in this suite — every scenario above ran to "
           "completion and proved nothing");

    ASSERT(g_log_safe.cuts == g_log_safe.attempts &&
           g_blob_safe.cuts == g_blob_safe.attempts &&
           g_jrn_safe.cuts == g_jrn_safe.attempts,
           "an armed cut index did not fire. The sweep does not cover every "
           "medium-touching op, so its coverage claim is false");

    ASSERT(g_bd.total_progs > 0 && g_bd.total_erases > 0 && g_bd.total_syncs > 0,
           "the block device saw no program, no erase or no sync — the instance "
           "is not being exercised");

    /* And the uncut run must produce the WHOLE expected content: a scenario that
     * silently failed to write anything would pass every check above by having
     * nothing to lose. */
    np_powerbd_wipe(&g_bd);
    np_powerbd_power_cycle(&g_bd);
    log_baseline();
    snapshot_take();
    (void)attempt(NP_POWERBD_NO_CUT, NP_POWERBD_TEAR_NONE, log_append_batches);

    np_powerbd_power_cycle(&g_bd);
    ASSERT(mount_fresh() == 0, "the uncut run must leave a mountable filesystem");

    lfs_file_t file;
    ASSERT(open_file(&file, LOG_PATH, LFS_O_RDONLY) == 0,
           "the uncut run must leave the log present");
    lfs_ssize_t got = lfs_file_read(&g_lfs, &file, g_readback, sizeof(g_readback));
    lfs_file_close(&g_lfs, &file);
    lfs_unmount(&g_lfs);

    ASSERT(got == (lfs_ssize_t)(REC_SIZE * (LOG_BASE_RECS + LOG_BATCH * LOG_BATCHES)),
           "the uncut run did not write every record — the sweeps above were "
           "cutting into a sequence that does less than it claims");

    bool all_good = (got > 0);
    for (size_t i = 0U; got > 0 && i < (size_t)got / REC_SIZE; i++) {
        if (!record_valid(g_readback + (i * REC_SIZE), LOG_MAGIC, (uint32_t)i)) {
            all_good = false;
            break;
        }
    }
    ASSERT(all_good, "the uncut run's records do not all verify");
}

/* ── 6. The instance under test is the one the device will mount ──────────── */

static void test_instance_is_the_emmc_fs_01_instance(void)
{
    ASSERT(g_cfg.block_size  == NP_LFS_CFG_BLOCK_SIZE &&
           g_cfg.block_count == NP_LFS_CFG_BLOCK_COUNT &&
           g_cfg.prog_size   == NP_LFS_CFG_PROG_SIZE &&
           g_cfg.read_size   == NP_LFS_CFG_READ_SIZE &&
           g_cfg.cache_size  == NP_LFS_CFG_CACHE_SIZE,
           "the sweeps must run against EMMC-FS-01's geometry, not a geometry "
           "chosen to make them quick");

    ASSERT(np_lfs_config_validate(&g_cfg) == NP_HUB_OK,
           "np_lfs_config_validate must accept the configuration these results "
           "are about");

    ASSERT(g_lock_depth == 0,
           "LFS_THREADSAFE lock/unlock are unbalanced at the end of the suite");
}

int main(void)
{
    printf("── littlefs power-loss injection tests, claims L-1…L-4 "
           "(NP-SOUP-LFS-001 §12) ──\n");

    build_config();

    test_log_append_is_power_safe();
    test_blob_replace_is_atomic();
    test_journal_torn_write_costs_one_record();
    test_falsification_unsafe_orderings_are_caught();
    test_falsification_the_injector_fires();
    test_instance_is_the_emmc_fs_01_instance();

    printf("  totals   %ld progs, %ld erases, %ld syncs, %ld reads, %ld cuts\n",
           g_bd.total_progs, g_bd.total_erases, g_bd.total_syncs,
           g_bd.total_reads, g_bd.total_cuts);

    if (g_fail_count == 0) {
        printf("PASS — all checks\n");
    } else {
        printf("FAILED — %d check(s)\n", g_fail_count);
    }
    return g_fail_count;
}
