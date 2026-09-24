/*
 * NeurOne SW-02 — UHDR / SHDR littlefs instance tests (OI-LFS-05)
 * Document: NP-SOUP-LFS-001 Rev 4 §13.1
 *
 * ── What this suite is for ───────────────────────────────────────────────────
 *
 * L-1 and L-2 — the two claims NP-FW-HUB-001 §6.5's durability model rests on —
 * are claims about the LOG partitions, and §12 exercised them on the Config
 * instance because it was the only one with parameters.  OI-LFS-05 closed by
 * taking EMMC-FS-01's UHDR and SHDR columns, with one deviation.  This suite:
 *
 *   1. pins every parameter and requires each single-field perturbation —
 *      EMMC-FS-01's printed read/prog 256 among them — to be refused
 *   2. mounts the REAL geometry: 1,767,168 blocks for UHDR, 131,072 for SHDR,
 *      over a sparse medium, because a result on a smaller partition would be
 *      a result about a filesystem the device does not mount
 *   3. sweeps L-1/L-2 on both, three tear models, every op
 *   4. is the evidence for the deviation (ECR-EMMC-002): under a 512-byte
 *      read-modify-write encryption unit (EMMC-UHDR-05), the same sweep passes
 *      at prog_size 512 and FAILS at 256 — for the log instance as EMMC-FS-01
 *      prints it, and for the Config instance as it is built today (OI-LFS-10)
 *   5. measures, in reads, what the specified lookahead costs: how often the
 *      allocator traverses the filesystem during a session, and how many reads
 *      one traversal takes at a given fill.  The TIME is the eMMC's and is
 *      OI-LFS-07's to measure.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "np_cfg_store.h"
#include "np_crypto.h"
#include "np_lfs_config.h"
#include "np_lfs_instance.h"
#include "np_lfs_log_instance.h"
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

static int host_lock(const struct lfs_config *c)   { (void)c; return 0; }
static int host_unlock(const struct lfs_config *c) { (void)c; return 0; }

/* The medium under test.  Sparse for the log instances (UHDR is 6,903 MiB). */
static np_powerbd_t      g_bd;
static struct lfs_config g_cfg;
static lfs_t             g_lfs;
static uint8_t         **g_table;
static uint8_t          *g_dirty;
static uint8_t           g_fcache[NP_LFS_LOG_FILE_BUFFER_SIZE];

static void reboot_raw(void)
{
    memset(&g_lfs, 0, sizeof(g_lfs));
}

/* Bind a sparse medium of `blocks` blocks under `cfg`'s callbacks. */
static void bind_sparse(lfs_size_t blocks, lfs_size_t prog, lfs_size_t rmw)
{
    np_powerbd_sparse_free(&g_bd);
    free(g_table);
    free(g_dirty);
    g_table = calloc(blocks, sizeof(*g_table));
    g_dirty = calloc((blocks + 7U) / 8U, 1U);
    np_powerbd_bind_sparse(&g_bd, &g_cfg, g_table, NP_LFS_LOG_BLOCK_SIZE,
                           blocks, prog, g_dirty, (blocks + 7U) / 8U);
    g_bd.rmw_unit = rmw;
    g_cfg.lock    = host_lock;
    g_cfg.unlock  = host_unlock;
}

static bool build_log(np_log_part_t part, lfs_size_t rmw)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    if (np_lfs_log_config_apply(part, &g_cfg) != NP_HUB_OK) {
        return false;
    }
    bind_sparse(g_cfg.block_count, g_cfg.prog_size, rmw);
    np_sweep_bind(&g_bd, reboot_raw);
    return np_lfs_log_config_validate(part, &g_cfg) == NP_HUB_OK;
}

static int open_file(lfs_file_t *f, const char *path, int flags)
{
    struct lfs_file_config fcfg;
    memset(&fcfg, 0, sizeof(fcfg));
    fcfg.buffer = g_fcache;
    return lfs_file_opencfg(&g_lfs, f, path, flags, &fcfg);
}

/* ── 1. The parameters, and the validator's refusals ──────────────────────── */

typedef void (*perturb_fn)(struct lfs_config *c);

static void p_read256(struct lfs_config *c)  { c->read_size = 256U; }
static void p_prog256(struct lfs_config *c)  { c->prog_size = 256U; }
static void p_both256(struct lfs_config *c)  { c->read_size = 256U; c->prog_size = 256U; }
static void p_block(struct lfs_config *c)    { c->block_size = 8192U; }
static void p_count(struct lfs_config *c)    { c->block_count -= 1U; }
static void p_cache(struct lfs_config *c)    { c->cache_size = 2048U; }
static void p_look(struct lfs_config *c)     { c->lookahead_size *= 2U; }
static void p_cyc_m1(struct lfs_config *c)   { c->block_cycles = -1; }
static void p_cyc200(struct lfs_config *c)   { c->block_cycles = 200; }
static void p_filemax(struct lfs_config *c)  { c->file_max = 65536U; }
static void p_namemax(struct lfs_config *c)  { c->name_max = 64U; }
static void p_attrmax(struct lfs_config *c)  { c->attr_max = 256U; }
static void p_metamax(struct lfs_config *c)  { c->metadata_max = 0U; }
static void p_nobuf(struct lfs_config *c)    { c->lookahead_buffer = NULL; }
static void p_noprog(struct lfs_config *c)   { c->prog = NULL; }
static void p_nolock(struct lfs_config *c)   { c->lock = NULL; }

static void test_parameters_are_pinned(void)
{
    static const struct { const char *name; perturb_fn fn; } cases[] = {
        { "read_size 256 (EMMC-FS-01 as printed)",   p_read256 },
        { "prog_size 256 (EMMC-FS-01 as printed)",   p_prog256 },
        { "read+prog 256 (EMMC-FS-01 as printed)",   p_both256 },
        { "block_size 8192",                         p_block   },
        { "block_count - 1",                         p_count   },
        { "cache_size 2048",                         p_cache   },
        { "lookahead_size x 2",                      p_look    },
        { "block_cycles -1 (disables L-6)",          p_cyc_m1  },
        { "block_cycles 200 (Config's)",             p_cyc200  },
        { "file_max 65536 (Config's)",               p_filemax },
        { "name_max 64 (Config's per EMMC-FS-01)",   p_namemax },
        { "attr_max 256 (Config's per EMMC-FS-01)",  p_attrmax },
        { "metadata_max 0 (default)",                p_metamax },
        { "lookahead_buffer NULL",                   p_nobuf   },
        { "prog callback NULL",                      p_noprog  },
        { "lock NULL",                               p_nolock  },
    };
    const np_log_part_t parts[2] = { NP_LOG_PART_UHDR, NP_LOG_PART_SHDR };

    ASSERT(np_lfs_log_block_count(NP_LOG_PART_UHDR) == 1767168U,
           "UHDR block_count is EMMC-FS-01's 1,767,168");
    ASSERT(np_lfs_log_block_count(NP_LOG_PART_SHDR) == 131072U,
           "SHDR block_count is EMMC-FS-01's 131,072");
    ASSERT(NP_LFS_LOG_PROG_SIZE == 512U && NP_LFS_LOG_READ_SIZE == 512U,
           "read/prog must be the XTS data unit (EMMC-UHDR-05)");
    ASSERT(NP_LFS_LOG_UHDR_LOOKAHEAD == 512U && NP_LFS_LOG_SHDR_LOOKAHEAD == 256U,
           "lookahead is EMMC-FS-01's — a change needs the OI-LFS-07 measurement");

    int refused = 0;
    for (unsigned p = 0U; p < 2U; p++) {
        ASSERT(build_log(parts[p], 0U), "the real log config was refused");

        for (unsigned i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
            struct lfs_config c = g_cfg;
            cases[i].fn(&c);
            if (np_lfs_log_config_validate(parts[p], &c) == NP_HUB_ERR_BAD_VERSION) {
                refused++;
            } else {
                printf("FAIL [%s] %s accepted a perturbed config: %s\n", __func__,
                       p == 0U ? "UHDR" : "SHDR", cases[i].name);
                g_fail_count++;
            }
        }
    }
    printf("  L-5      %d single-field perturbations refused across both "
           "log instances\n", refused);

    /* Cross-instance: each partition's config is refused by the other's
     * validator, and the Config instance's by both. */
    struct lfs_config u;
    struct lfs_config s;
    struct lfs_config k;
    memset(&u, 0, sizeof(u));
    memset(&s, 0, sizeof(s));
    memset(&k, 0, sizeof(k));
    (void)np_lfs_log_config_apply(NP_LOG_PART_UHDR, &u);
    (void)np_lfs_log_config_apply(NP_LOG_PART_SHDR, &s);
    (void)np_lfs_config_apply(&k);
    u.read = s.read = k.read = g_cfg.read;
    u.prog = s.prog = k.prog = g_cfg.prog;
    u.erase = s.erase = k.erase = g_cfg.erase;
    u.sync = s.sync = k.sync = g_cfg.sync;
    u.lock = s.lock = k.lock = host_lock;
    u.unlock = s.unlock = k.unlock = host_unlock;
    ASSERT(np_lfs_log_config_validate(NP_LOG_PART_SHDR, &u) == NP_HUB_ERR_BAD_VERSION,
           "the SHDR validator accepted the UHDR config");
    ASSERT(np_lfs_log_config_validate(NP_LOG_PART_UHDR, &s) == NP_HUB_ERR_BAD_VERSION,
           "the UHDR validator accepted the SHDR config");
    ASSERT(np_lfs_log_config_validate(NP_LOG_PART_UHDR, &k) == NP_HUB_ERR_BAD_VERSION &&
           np_lfs_log_config_validate(NP_LOG_PART_SHDR, &k) == NP_HUB_ERR_BAD_VERSION,
           "a log validator accepted the Config instance");
    ASSERT(np_lfs_config_validate(&u) == NP_HUB_ERR_BAD_VERSION,
           "the Config validator accepted a log instance");
    ASSERT(np_lfs_log_config_apply((np_log_part_t)7, &u) == NP_HUB_ERR_INVALID_ARG &&
           np_lfs_log_config_validate((np_log_part_t)7, &u) == NP_HUB_ERR_INVALID_ARG,
           "an unknown partition was accepted");
}

/* ── 2/3. The real geometry mounts, and L-1/L-2 hold on it ────────────────── */

#define REC_SIZE   32U
#define LOG_BASE   160U
#define LOG_BATCH  32U
#define LOG_BATCHES 4U
#define LOG_PATH   "session.log"

static void rec_build(uint8_t out[REC_SIZE], uint32_t ordinal, uint8_t gen)
{
    memcpy(out, "NPLR", 4U);
    out[4] = (uint8_t)(ordinal & 0xFFU);
    out[5] = (uint8_t)((ordinal >> 8) & 0xFFU);
    out[6] = (uint8_t)((ordinal >> 16) & 0xFFU);
    out[7] = (uint8_t)((ordinal >> 24) & 0xFFU);
    for (unsigned i = 8U; i < 28U; i++) {
        out[i] = (uint8_t)(gen ^ (ordinal * 31U + i));
    }
    uint32_t crc = np_crc32(out, 28U);
    out[28] = (uint8_t)(crc & 0xFFU);
    out[29] = (uint8_t)((crc >> 8) & 0xFFU);
    out[30] = (uint8_t)((crc >> 16) & 0xFFU);
    out[31] = (uint8_t)((crc >> 24) & 0xFFU);
}

static void log_baseline(void)
{
    lfs_file_t f;
    uint8_t rec[REC_SIZE];
    reboot_raw();
    if (lfs_format(&g_lfs, &g_cfg) != 0 || lfs_mount(&g_lfs, &g_cfg) != 0 ||
        open_file(&f, LOG_PATH, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND) != 0) {
        printf("FAIL [log_baseline] format/mount/open\n");
        g_fail_count++;
        return;
    }
    for (uint32_t i = 0U; i < LOG_BASE; i++) {
        rec_build(rec, i, 1U);
        (void)lfs_file_write(&g_lfs, &f, rec, REC_SIZE);
    }
    (void)lfs_file_close(&g_lfs, &f);
    (void)lfs_unmount(&g_lfs);
}

static void log_append(void)
{
    lfs_file_t f;
    uint8_t rec[REC_SIZE];
    if (lfs_mount(&g_lfs, &g_cfg) != 0) { return; }
    if (open_file(&f, LOG_PATH, LFS_O_WRONLY | LFS_O_APPEND) != 0) {
        (void)lfs_unmount(&g_lfs);
        return;
    }
    for (uint32_t b = 0U; b < LOG_BATCHES; b++) {
        for (uint32_t i = 0U; i < LOG_BATCH; i++) {
            rec_build(rec, LOG_BASE + (b * LOG_BATCH) + i, 1U);
            (void)lfs_file_write(&g_lfs, &f, rec, REC_SIZE);
        }
        (void)lfs_file_sync(&g_lfs, &f);
    }
    (void)lfs_file_close(&g_lfs, &f);
    (void)lfs_unmount(&g_lfs);
}

static bool g_expect;
static int  g_shown;

static int log_verify(const char *what, long cut, np_powerbd_tear_t tear)
{
    static uint8_t rb[REC_SIZE * (LOG_BASE + LOG_BATCH * LOG_BATCHES)];
    lfs_file_t f;
    const char *why = NULL;

    if (lfs_mount(&g_lfs, &g_cfg) != 0) {
        why = "MOUNT FAILED";
    } else if (open_file(&f, LOG_PATH, LFS_O_RDONLY) != 0) {
        why = "the log file is gone";
        (void)lfs_unmount(&g_lfs);
    } else {
        lfs_ssize_t got = lfs_file_read(&g_lfs, &f, rb, sizeof(rb));
        (void)lfs_file_close(&g_lfs, &f);
        (void)lfs_unmount(&g_lfs);
        size_t recs = (got < 0) ? 0U : (size_t)got / REC_SIZE;
        bool boundary = false;
        for (uint32_t b = 0U; b <= LOG_BATCHES; b++) {
            boundary = boundary || (recs == (size_t)(LOG_BASE + b * LOG_BATCH));
        }
        if (got < 0 || ((size_t)got % REC_SIZE) != 0U) {
            why = "read failed or not a whole number of records";
        } else if (!boundary) {
            why = "not a flush boundary (L-1)";
        } else {
            uint8_t expect[REC_SIZE];
            for (size_t i = 0U; i < recs && why == NULL; i++) {
                rec_build(expect, (uint32_t)i, 1U);
                if (memcmp(rb + i * REC_SIZE, expect, REC_SIZE) != 0) {
                    why = (i < LOG_BASE)
                        ? "a record committed before the run changed (L-2)"
                        : "a surviving appended record is damaged";
                }
            }
        }
    }
    if (why == NULL) {
        return 0;
    }
    if (!g_expect) {
        printf("FAIL [%s] cut@%ld %s: %s\n", what, cut,
               np_powerbd_tear_name(tear), why);
    } else if (g_shown < 2) {
        g_shown++;
        printf("           caught: [%s] cut@%ld %s: %s\n", what, cut,
               np_powerbd_tear_name(tear), why);
    }
    return 1;
}

static np_sweep_result_t sweep(const char *label, bool expect)
{
    g_expect = expect;
    g_shown  = 0;
    np_sweep_result_t r = np_sweep_run(log_baseline, log_append, label,
                                       log_verify);
    g_expect = false;
    printf("  %-34s %3ld ops x 3 = %4ld attempts, %4ld cuts, %4ld violations\n",
           label, r.ops, r.attempts, r.cuts, r.violations);
    return r;
}

static void test_log_instances_hold_l1_l2(void)
{
    ASSERT(build_log(NP_LOG_PART_UHDR, 0U), "UHDR config refused");
    np_sweep_result_t u = sweep("UHDR L-1/L-2 (1,767,168 blocks)", false);
    ASSERT(u.ops > 0 && u.missed_cuts == 0, "UHDR sweep did not run as measured");
    ASSERT(u.violations == 0, "L-1/L-2 do not hold on the UHDR instance");

    ASSERT(build_log(NP_LOG_PART_SHDR, 0U), "SHDR config refused");
    np_sweep_result_t s = sweep("SHDR L-1/L-2 (131,072 blocks)", false);
    ASSERT(s.ops > 0 && s.missed_cuts == 0, "SHDR sweep did not run as measured");
    ASSERT(s.violations == 0, "L-1/L-2 do not hold on the SHDR instance");
}

/* ── 4. ECR-EMMC-002 — a program smaller than the XTS unit ─────────────────── */

/* The Config instance, through the store's journal (L-4), on a dense medium. */
static uint8_t          *g_cmedia;
static uint8_t           g_cdirty[(NP_LFS_CFG_BLOCK_COUNT + 7U) / 8U];
static void store_lock(void) { }

static void reboot_store(void)
{
    memset(&g_lfs, 0, sizeof(g_lfs));
    (void)np_cfg_store_bind(&g_lfs, &g_cfg, store_lock, store_lock);
}

static bool rec_ok(const uint8_t *rec, size_t len, uint32_t idx, void *ctx)
{
    uint8_t e[REC_SIZE];
    (void)ctx;
    rec_build(e, idx, 3U);
    return len == REC_SIZE && memcmp(rec, e, REC_SIZE) == 0;
}

static void jrn_baseline(void)
{
    uint8_t rec[REC_SIZE];
    (void)np_cfg_store_format();
    (void)np_cfg_store_mount();
    for (uint32_t i = 0U; i < 40U; i++) {
        rec_build(rec, i, 3U);
        (void)np_cfg_store_journal_append(NP_CFG_FILE_MAP3, rec, REC_SIZE);
    }
    np_cfg_store_unmount();
}

static void jrn_append(void)
{
    uint8_t rec[REC_SIZE];
    if (np_cfg_store_mount() != NP_HUB_OK) { return; }
    for (uint32_t i = 40U; i < 44U; i++) {
        rec_build(rec, i, 3U);
        (void)np_cfg_store_journal_append(NP_CFG_FILE_MAP3, rec, REC_SIZE);
    }
    np_cfg_store_unmount();
}

static int jrn_verify(const char *what, long cut, np_powerbd_tear_t tear)
{
    static uint8_t rb[REC_SIZE * 48U];
    uint32_t n = 0U;
    np_hub_status_t st = NP_HUB_ERR_GENERIC;
    if (np_cfg_store_mount() == NP_HUB_OK) {
        st = np_cfg_store_journal_read(NP_CFG_FILE_MAP3, rb, sizeof(rb), REC_SIZE,
                                       &n, rec_ok, NULL);
        np_cfg_store_unmount();
    }
    if (st == NP_HUB_OK && n >= 40U) {
        return 0;
    }
    if (g_expect && g_shown < 2) {
        g_shown++;
        printf("           caught: [%s] cut@%ld %s: %u of 40 durable records "
               "(status %d)\n", what, cut, np_powerbd_tear_name(tear),
               (unsigned)n, st);
    } else if (!g_expect) {
        printf("FAIL [%s] cut@%ld: %u of 40 durable records\n", what, cut,
               (unsigned)n);
    }
    return 1;
}

static np_sweep_result_t config_journal_sweep(lfs_size_t rmw, const char *label)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    (void)np_lfs_config_apply(&g_cfg);
    np_powerbd_bind(&g_bd, &g_cfg, g_cmedia, NP_LFS_CFG_BLOCK_SIZE,
                    NP_LFS_CFG_BLOCK_COUNT, NP_LFS_CFG_PROG_SIZE,
                    g_cdirty, sizeof(g_cdirty));
    g_bd.rmw_unit = rmw;
    g_cfg.lock    = host_lock;
    g_cfg.unlock  = host_unlock;
    np_sweep_bind(&g_bd, reboot_store);
    g_expect = (rmw != 0U);
    g_shown  = 0;
    np_sweep_result_t r = np_sweep_run(jrn_baseline, jrn_append, label, jrn_verify);
    g_expect = false;
    printf("  %-34s %3ld ops x 3 = %4ld attempts, %4ld cuts, %4ld violations\n",
           label, r.ops, r.attempts, r.cuts, r.violations);
    return r;
}

static void test_prog_smaller_than_xts_unit_breaks_the_contract(void)
{
    /* (a) The log instance as decided: prog 512 over a 512-byte RMW unit. */
    ASSERT(build_log(NP_LOG_PART_SHDR, 512U), "SHDR config refused");
    np_sweep_result_t ok = sweep("SHDR prog 512 / XTS RMW 512", false);
    ASSERT(ok.violations == 0,
           "prog_size 512 over a 512-byte RMW unit lost committed data — the "
           "deviation does not buy what §13.1.3 says it buys");

    /* (b) The log instance as EMMC-FS-01 PRINTS it: read/prog 256. */
    memset(&g_cfg, 0, sizeof(g_cfg));
    (void)np_lfs_log_config_apply(NP_LOG_PART_SHDR, &g_cfg);
    g_cfg.read_size = 256U;
    g_cfg.prog_size = 256U;
    bind_sparse(g_cfg.block_count, 256U, 512U);
    np_sweep_bind(&g_bd, reboot_raw);
    np_sweep_result_t bad = sweep("SHDR prog 256 / XTS RMW 512", true);
    ASSERT(bad.violations > 0,
           "prog_size 256 under a 512-byte RMW unit lost nothing.  Either the "
           "model no longer tears the neighbouring half-unit, or the deviation "
           "in np_lfs_log_instance.h has no evidence behind it");

    /* (c) The contract-only control: prog 256 with NO RMW unit passes, so (b)
     *     fails because of the unit and not because 256 is broken per se. */
    bind_sparse(g_cfg.block_count, 256U, 0U);
    np_sweep_bind(&g_bd, reboot_raw);
    np_sweep_result_t ctl = sweep("SHDR prog 256 / no RMW (control)", false);
    ASSERT(ctl.violations == 0,
           "prog_size 256 fails even without an RMW unit — (b) proves nothing "
           "about the XTS unit");

    /* (d) OI-LFS-10: the Config instance, built today at prog 256 and mounted
     *     on an XTS-encrypted partition, through the store's own journal. */
    g_cmedia = malloc((size_t)NP_LFS_CFG_BLOCK_SIZE * NP_LFS_CFG_BLOCK_COUNT);
    ASSERT(g_cmedia != NULL, "host out of memory");
    if (g_cmedia == NULL) {
        return;
    }
    np_sweep_result_t c0 = config_journal_sweep(0U, "Config L-4, no RMW (control)");
    np_sweep_result_t c1 = config_journal_sweep(512U, "Config L-4 / XTS RMW 512");
    ASSERT(c0.violations == 0, "the Config journal fails even without RMW");
    ASSERT(c1.violations > 0,
           "the Config instance's prog 256 survived a 512-byte RMW unit — "
           "OI-LFS-10's finding is not reproduced; re-examine it");
    np_sweep_release();
    free(g_cmedia);
    g_cmedia = NULL;
}

/* ── 5. What the specified lookahead costs, in reads ──────────────────────── */

static uint8_t g_chunk[NP_LFS_LOG_BLOCK_SIZE];

/* Write `blocks` blocks of data as session files of `per_file` blocks. */
static void fill(lfs_size_t blocks, lfs_size_t per_file)
{
    char name[40];
    lfs_file_t f;
    for (lfs_size_t done = 0U, n = 0U; done < blocks; n++) {
        (void)snprintf(name, sizeof(name), "%020lu", (unsigned long)n);
        if (open_file(&f, name, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_EXCL) != 0) {
            break;
        }
        for (lfs_size_t i = 0U; i < per_file && done < blocks; i++, done++) {
            g_chunk[0] = (uint8_t)done;
            (void)lfs_file_write(&g_lfs, &f, g_chunk, sizeof(g_chunk));
        }
        (void)lfs_file_close(&g_lfs, &f);
    }
}

/* Append `blocks` blocks to a new session file and report the largest number
 * of block-device reads any single one-block write took, and how many writes
 * took more than `spike` reads (i.e. carried an allocator traversal). */
static void session(lfs_size_t blocks, long spike, long *max_reads, long *spikes)
{
    lfs_file_t f;
    *max_reads = 0;
    *spikes    = 0;
    if (open_file(&f, "session.new", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != 0) {
        return;
    }
    for (lfs_size_t i = 0U; i < blocks; i++) {
        long before = g_bd.total_reads;
        (void)lfs_file_write(&g_lfs, &f, g_chunk, sizeof(g_chunk));
        long used = g_bd.total_reads - before;
        if (used > *max_reads) { *max_reads = used; }
        if (used > spike)      { (*spikes)++; }
    }
    (void)lfs_file_close(&g_lfs, &f);
}

static void test_lookahead_cost_is_measured(void)
{
    /* UHDR geometry, 16,384 blocks (64 MiB, ~90 min of EEG at 12 kB/s) already
     * written as 4 MiB session files, then one 6,144-block (24 MiB) session:
     * longer than the 4,096-block window, so it must cross one. */
    const lfs_size_t prefill = 16384U;
    const lfs_size_t sess    = 6144U;
    const long       spike   = 1000;   /* a normal one-block write is ~a dozen */

    ASSERT(build_log(NP_LOG_PART_UHDR, 0U), "UHDR config refused");
    memset(g_chunk, 0xA5, sizeof(g_chunk));
    reboot_raw();
    ASSERT(lfs_format(&g_lfs, &g_cfg) == 0 && lfs_mount(&g_lfs, &g_cfg) == 0,
           "format/mount");
    fill(prefill, 1024U);
    (void)lfs_unmount(&g_lfs);

    /* Without priming: the first write after mount traverses. */
    long max_a = 0;
    long spikes_a = 0;
    reboot_raw();
    ASSERT(lfs_mount(&g_lfs, &g_cfg) == 0, "remount");
    session(sess, spike, &max_a, &spikes_a);
    (void)lfs_remove(&g_lfs, "session.new");
    (void)lfs_unmount(&g_lfs);

    /* Primed at mount: the traversal happens in np_lfs_log_prime_allocator(). */
    long max_b = 0;
    long spikes_b = 0;
    reboot_raw();
    ASSERT(lfs_mount(&g_lfs, &g_cfg) == 0, "remount");
    long before = g_bd.total_reads;
    ASSERT(np_lfs_log_prime_allocator(&g_lfs) == NP_HUB_OK, "prime");
    long at_mount = g_bd.total_reads - before;
    session(sess, spike, &max_b, &spikes_b);
    (void)lfs_unmount(&g_lfs);

    printf("  lookahead  UHDR 512 B = %u-block window; %u blocks in use:\n"
           "             one traversal = %ld reads (at mount, when primed)\n"
           "             unprimed %u-block session: %ld traversal(s), worst "
           "write %ld reads\n"
           "             primed   %u-block session: %ld traversal(s), worst "
           "write %ld reads\n",
           8U * NP_LFS_LOG_UHDR_LOOKAHEAD, (unsigned)prefill, at_mount,
           (unsigned)sess, spikes_a, max_a, (unsigned)sess, spikes_b, max_b);

    ASSERT(at_mount > spike,
           "priming did not traverse — np_lfs_log_prime_allocator() no longer "
           "does what its header says");
    ASSERT(spikes_a > spikes_b,
           "priming did not remove a traversal from the session");
    ASSERT(spikes_b >= 1,
           "a session longer than the lookahead window crossed no traversal — "
           "the cost this test exists to state was not observed");

    np_powerbd_sparse_free(&g_bd);
}

int main(void)
{
    printf("np_lfs_log_instance_tests — NP-SOUP-LFS-001 Rev 4 §13.1 (OI-LFS-05)\n");

    test_parameters_are_pinned();
    test_log_instances_hold_l1_l2();
    test_prog_smaller_than_xts_unit_breaks_the_contract();
    test_lookahead_cost_is_measured();

    np_sweep_release();
    np_powerbd_sparse_free(&g_bd);
    free(g_table);
    free(g_dirty);

    if (g_fail_count == 0) {
        printf("PASS\n");
    } else {
        printf("%d FAILURE(S)\n", g_fail_count);
    }
    return g_fail_count;
}
