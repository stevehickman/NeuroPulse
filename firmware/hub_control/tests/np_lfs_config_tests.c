/*
 * NeurOne SW-02 — littlefs SOUP configuration + instance host tests
 * Document: NP-SOUP-LFS-001 Rev 3 §7.3 (closes OI-LFS-01)
 *
 * ── What this suite is for ───────────────────────────────────────────────────
 *
 * OI-LFS-01 is "pin a named upstream release tag and vendor it ... plus the
 * subset and configuration".  Three of those four are files, and files rot
 * quietly.  This suite is what makes each of them observable rather than
 * remembered:
 *
 *   1. THE BYTES.  firmware/vendor/littlefs/VERSION claims a per-file SHA-256
 *      for a byte-exact copy of tag v2.11.3.  Case 1 recomputes each hash from
 *      the vendored file with firmware/crypto's SHA-256 and compares.  "Byte
 *      exact" stops being a sentence somebody wrote and becomes something CI
 *      re-establishes on every run, offline, forever.  It is also the only
 *      check here that would catch the failure that matters most — a local edit
 *      to vendored SOUP, which NP-SW-CI-001 §9 forbids.
 *
 *   2. THE CONFIGURATION.  np_lfs_config.h and the VERSION record's
 *      "Configuration" section are two statements of one fact, which is the
 *      shape NP-SW-CI-001 §4.8 keeps finding defects in.  Cases 2 and 3 compare
 *      them: what the compiler actually has in force, against what the SOUP
 *      record says a reviewer may rely on.
 *
 *   3. THE INSTANCE.  EMMC-FS-01 owns five parameters (claim L-5) and NeurOne
 *      decides four more.  Case 4 pins them; case 5 checks np_lfs_config_apply()
 *      produces them; case 6 checks np_lfs_config_validate() accepts a complete
 *      config.
 *
 *   4. THE FALSIFICATION.  NP-CONV-001 §8: a check that has never been seen to
 *      fail is indistinguishable from a check that does not run.  Case 7 walks
 *      nineteen single-field perturbations of a known-good config past
 *      np_lfs_config_validate() and requires each to be REJECTED — including
 *      block_cycles = -1, which passes every "is it non-zero" test and silently
 *      disables the wear levelling claim L-6 rests on.
 *
 * ── What it does NOT prove ──────────────────────────────────────────────────
 *
 * That the vendored bytes are what upstream published.  A host test has no
 * network; two independent clones and a byte comparison did that once, at
 * vendoring time, and the hashes below are the record of it.  This suite proves
 * the tree still holds those bytes.
 *
 * That littlefs keeps any promise.  L-1…L-4 are power-loss atomicity claims and
 * a configuration comparator cannot reach them.  They are exercised by a
 * different suite — np_lfs_powerloss_tests (OI-LFS-02, NP-SOUP-LFS-001 §12) —
 * and nothing HERE may be cited as evidence for NP-FW-NVRAM-001 §4 or
 * NP-FW-HUB-001 §6.5.  Note also what that suite does not reach: it interrupts
 * the struct lfs_config contract, not the eMMC beneath it (OI-LFS-07).
 *
 * That anything mounts.  The OI-LOG-05..07 block-device glue does not exist, so
 * no lfs_mount() is reachable from this tree at all.
 *
 * Return convention: 0 = PASS, non-zero = failure count.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "np_crypto.h"        /* np_sha256 */
#include "np_lfs_config.h"    /* the configuration, as the compiler sees it */
#include "np_lfs_instance.h"  /* the EMMC-FS-01 instance + validator */

#if !defined(NP_LITTLEFS_DIR_PATH)
#error "NP_LITTLEFS_DIR_PATH must be defined by the build"
#endif

static int g_fail_count = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));  \
            g_fail_count++;                                          \
        }                                                            \
    } while (0)

/* ── file helpers ─────────────────────────────────────────────────────────── */

static unsigned char g_file[1024U * 1024U];
static char          g_version[256U * 1024U];

/*
 * Read a whole file.  A short read is a FAILURE, never a shorter buffer: a
 * probe that silently saw a prefix would report "token absent" and hash the
 * wrong bytes, turning every case below into a quiet pass.
 */
static size_t slurp(const char *path, void *buf, size_t cap, const char *who)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        printf("FAIL [%s] cannot open %s\n", who, path);
        g_fail_count++;
        return 0U;
    }
    size_t n = fread(buf, 1U, cap - 1U, f);
    int    truncated = (feof(f) == 0);
    fclose(f);
    ((unsigned char *)buf)[n] = '\0';

    if (truncated) {
        printf("FAIL [%s] %s is larger than %u bytes — the probe would read a "
               "prefix\n", who, path, (unsigned)cap);
        g_fail_count++;
        return 0U;
    }
    return n;
}

static void path_in_vendor(char *out, size_t cap, const char *name)
{
    snprintf(out, cap, "%s/%s", NP_LITTLEFS_DIR_PATH, name);
}

static void hex_of(const unsigned char *digest, char *out /* >= 65 */)
{
    static const char hexdig[] = "0123456789abcdef";
    for (unsigned i = 0U; i < 32U; i++) {
        out[2U * i]      = hexdig[(digest[i] >> 4) & 0x0FU];
        out[2U * i + 1U] = hexdig[digest[i] & 0x0FU];
    }
    out[64] = '\0';
}

/* ── 1. The vendored bytes are the bytes the SOUP record claims ───────────── */

static void test_vendored_files_match_recorded_sha256(void)
{
    static const char *const files[] = {
        "lfs.c", "lfs.h", "lfs_util.c", "lfs_util.h", "LICENSE.md"
    };

    char vpath[1024];
    path_in_vendor(vpath, sizeof(vpath), "VERSION");
    if (slurp(vpath, g_version, sizeof(g_version), __func__) == 0U) {
        return;
    }

    for (unsigned i = 0U; i < sizeof(files) / sizeof(files[0]); i++) {
        char path[1024];
        path_in_vendor(path, sizeof(path), files[i]);

        size_t n = slurp(path, g_file, sizeof(g_file), __func__);
        if (n == 0U) {
            continue;
        }

        unsigned char digest[NP_CRYPTO_SHA256_SIZE];
        np_sha256(g_file, (uint32_t)n, digest);

        char hex[65];
        hex_of(digest, hex);

        /* The record's line is "<sha256>  <name>".  Requiring the pair to
         * appear together is what stops a hash matching some OTHER file's
         * line. */
        char expect[1024];
        snprintf(expect, sizeof(expect), "%s  %s", hex, files[i]);

        if (strstr(g_version, expect) == NULL) {
            printf("FAIL [%s] %s hashes to %s, which is not the SHA-256 the "
                   "VERSION record carries for it. Either the vendored file "
                   "was edited — NP-SW-CI-001 §9 forbids patching SOUP — or "
                   "the record was not updated with it.\n",
                   __func__, files[i], hex);
            g_fail_count++;
        }
    }
}

/* ── 2. The configuration the compiler has in force ───────────────────────── */

static void test_build_configuration_in_force(void)
{
#ifndef LFS_NO_MALLOC
    ASSERT(0, "LFS_NO_MALLOC is not defined. littlefs would allocate its caches "
              "from the 64 KiB FreeRTOS heap, and a missing buffer would stop "
              "being a mount failure");
#endif
#ifdef LFS_NO_ASSERT
    ASSERT(0, "LFS_NO_ASSERT is defined. Upstream reads it as 'assertions are "
              "compiled out': lfs.c guards the DEFINITION of lfs_mlist_isopen "
              "with it and keeps the CALL inside LFS_ASSERT, so the build gets "
              "an undefined symbol — and only once something pulls lfs.o in. "
              "See np_lfs_config.h decision 2");
#endif
#ifndef LFS_ASSERT
    ASSERT(0, "LFS_ASSERT is not defined. With LFS_NO_ASSERT set and no "
              "replacement, every one of littlefs's internal invariants — "
              "including lfs_init()'s configuration checks — compiles to "
              "nothing");
#endif
#ifndef LFS_NO_DEBUG
    ASSERT(0, "LFS_NO_DEBUG is not defined — firmware renders no text "
              "(CLAUDE.md §17), and this pulls <stdio.h> into the image");
#endif
#ifndef LFS_NO_WARN
    ASSERT(0, "LFS_NO_WARN is not defined (see LFS_NO_DEBUG)");
#endif
#ifndef LFS_NO_ERROR
    ASSERT(0, "LFS_NO_ERROR is not defined (see LFS_NO_DEBUG)");
#endif
#ifndef LFS_THREADSAFE
    ASSERT(0, "LFS_THREADSAFE is not defined. The Config instance has three "
              "specified writers on more than one task; without this littlefs "
              "is not serialised and struct lfs_config has no lock members");
#endif
#ifdef LFS_YES_TRACE
    ASSERT(0, "LFS_YES_TRACE is defined. The trace macros print every argument "
              "of every API call, which puts file names and offsets into a "
              "text stream this device has no destination for");
#endif
#ifdef LFS_READONLY
    ASSERT(0, "LFS_READONLY is defined — every NeurOne instance is written");
#endif
#ifdef LFS_MULTIVERSION
    ASSERT(0, "LFS_MULTIVERSION is defined. NeurOne reads no foreign media; "
              "this makes the on-disk version a run-time variable");
#endif
#ifdef LFS_NO_INTRINSICS
    ASSERT(0, "LFS_NO_INTRINSICS is defined — the GCC builtins are wanted on "
              "Cortex-M7; the fallbacks exist for debugging");
#endif
}

/* ── 3. …and the SOUP record says the same thing ──────────────────────────── */

static void test_soup_record_states_the_same_configuration(void)
{
    char vpath[1024];
    path_in_vendor(vpath, sizeof(vpath), "VERSION");
    if (slurp(vpath, g_version, sizeof(g_version), __func__) == 0U) {
        return;
    }

    static const char *const must_name[] = {
        "LFS_DEFINES=np_lfs_config.h",
        "LFS_NO_MALLOC",
        "LFS_NO_ASSERT",
        "LFS_NO_DEBUG",
        "LFS_NO_WARN",
        "LFS_NO_ERROR",
        "LFS_THREADSAFE",
        "v2.11.3",
    };

    for (unsigned i = 0U; i < sizeof(must_name) / sizeof(must_name[0]); i++) {
        if (strstr(g_version, must_name[i]) == NULL) {
            printf("FAIL [%s] the VERSION record does not mention %s, which is "
                   "in force in the build. A SOUP record a reviewer cannot read "
                   "the configuration out of is the defect this whole record "
                   "replaced.\n", __func__, must_name[i]);
            g_fail_count++;
        }
    }
}

/* ── 4. The instance parameters ───────────────────────────────────────────── */

static void test_instance_parameters(void)
{
    /* The five EMMC-FS-01 owns (claim L-5).  Changing any of them is a change
     * to NP-FW-EMMC-001 raised as an ECR, and it RELOCATES ukmd.rec — whose
     * loss makes a user's UHDR permanently unmountable. */
    /* read/prog: 512 — EMMC-FS-01 Rev 3 (Rev 2 printed 256) — the XTS data unit
     * (EMMC-UHDR-05).  OI-LFS-10 / ECR-EMMC-002, NP-SOUP-LFS-001 §13.8. */
    ASSERT(NP_LFS_CFG_READ_SIZE   == 512U,   "read_size is the 512-byte XTS unit (OI-LFS-10)");
    ASSERT(NP_LFS_CFG_PROG_SIZE   == 512U,   "prog_size is the 512-byte XTS unit (OI-LFS-10)");
    ASSERT(NP_LFS_CFG_PROG_SIZE % 512U == 0U,
           "a program smaller than the XTS unit tears committed bytes");
    ASSERT(NP_LFS_CFG_BLOCK_SIZE  == 4096U,  "EMMC-FS-01 block_size");
    ASSERT(NP_LFS_CFG_BLOCK_COUNT == 4096U,  "EMMC-FS-01 block_count");
    ASSERT(NP_LFS_CFG_FILE_MAX    == 65536U, "EMMC-FS-01 file_max");

    /* The instance is the whole 16 MiB partition — the arithmetic behind D-21
     * and behind EMMC-CFG-02's raw-write region being void. */
    ASSERT((unsigned long long)NP_LFS_CFG_BLOCK_SIZE * NP_LFS_CFG_BLOCK_COUNT
               == 16ULL * 1024ULL * 1024ULL,
           "the Config instance must span the whole 16 MiB partition");

    /* The four NeurOne decides. */
    ASSERT(NP_LFS_CFG_CACHE_SIZE == 512U,
           "cache_size is no longer 512 — the smallest multiple of prog_size, "
           "EMMC-FS-01's own Config value, and the bound on inline_max that "
           "keeps Map 3 records (32 B) and ukmd.rec's envelope (208 B) inline");
    /* OI-LFS-10 (closed): the rest of EMMC-FS-01's Config column. */
    ASSERT(NP_LFS_CFG_LOOKAHEAD_SIZE == 64U,
           "lookahead_size is no longer EMMC-FS-01's 64 (OI-LFS-10)");
    ASSERT(NP_LFS_CFG_BLOCK_CYCLES == 200,
           "block_cycles is no longer EMMC-FS-01's 200 (OI-LFS-10). "
           "NP-SOUP-LFS-001 §7.3 requires it set EXPLICITLY: 0 is rejected by "
           "lfs_init and -1 disables wear levelling, which is claim L-6");
    ASSERT(NP_LFS_CFG_NAME_MAX == 64U,  "name_max is no longer EMMC-FS-01's 64");
    ASSERT(NP_LFS_CFG_ATTR_MAX == 256U, "attr_max is no longer EMMC-FS-01's 256");
    ASSERT(NP_LFS_CFG_METADATA_MAX == 4096U,
           "metadata_max is no longer EMMC-FS-01's 4,096");

    /* littlefs's own relations, at compile time in np_lfs_instance.c and here
     * so a reader sees them stated where the numbers are. */
    ASSERT(NP_LFS_CFG_CACHE_SIZE % NP_LFS_CFG_READ_SIZE == 0U, "cache % read");
    ASSERT(NP_LFS_CFG_CACHE_SIZE % NP_LFS_CFG_PROG_SIZE == 0U, "cache % prog");
    ASSERT(NP_LFS_CFG_BLOCK_SIZE % NP_LFS_CFG_CACHE_SIZE == 0U, "block % cache");

    ASSERT(NP_LFS_STATIC_RAM_BYTES == 1088U,
           "the instance's static RAM cost changed — NP-SW-CI-001 §4.10's "
           "FlexRAM budget is stated against it");
}

/* ── a known-good config, built the way the mount glue will build one ─────── */

static int  fake_read(const struct lfs_config *c, lfs_block_t b, lfs_off_t o,
                      void *buf, lfs_size_t sz)
{ (void)c; (void)b; (void)o; (void)buf; (void)sz; return 0; }
static int  fake_prog(const struct lfs_config *c, lfs_block_t b, lfs_off_t o,
                      const void *buf, lfs_size_t sz)
{ (void)c; (void)b; (void)o; (void)buf; (void)sz; return 0; }
static int  fake_erase(const struct lfs_config *c, lfs_block_t b)
{ (void)c; (void)b; return 0; }
static int  fake_sync(const struct lfs_config *c)  { (void)c; return 0; }
#ifdef LFS_THREADSAFE
static int  fake_lock(const struct lfs_config *c)   { (void)c; return 0; }
static int  fake_unlock(const struct lfs_config *c) { (void)c; return 0; }
#endif

static void make_good(struct lfs_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    (void)np_lfs_config_apply(cfg);
    cfg->read  = fake_read;
    cfg->prog  = fake_prog;
    cfg->erase = fake_erase;
    cfg->sync  = fake_sync;
#ifdef LFS_THREADSAFE
    cfg->lock   = fake_lock;
    cfg->unlock = fake_unlock;
#endif
}

/* ── 5. np_lfs_config_apply fills what it says it fills ───────────────────── */

static void test_apply(void)
{
    struct lfs_config cfg;
    memset(&cfg, 0xA5, sizeof(cfg));

    ASSERT(np_lfs_config_apply(NULL) == NP_HUB_ERR_INVALID_ARG,
           "np_lfs_config_apply(NULL) must be rejected");

    ASSERT(np_lfs_config_apply(&cfg) == NP_HUB_OK, "apply must succeed");

    ASSERT(cfg.read_size      == NP_LFS_CFG_READ_SIZE,       "read_size");
    ASSERT(cfg.prog_size      == NP_LFS_CFG_PROG_SIZE,       "prog_size");
    ASSERT(cfg.block_size     == NP_LFS_CFG_BLOCK_SIZE,      "block_size");
    ASSERT(cfg.block_count    == NP_LFS_CFG_BLOCK_COUNT,     "block_count");
    ASSERT(cfg.file_max       == NP_LFS_CFG_FILE_MAX,        "file_max");
    ASSERT(cfg.cache_size     == NP_LFS_CFG_CACHE_SIZE,      "cache_size");
    ASSERT(cfg.lookahead_size == NP_LFS_CFG_LOOKAHEAD_SIZE,  "lookahead_size");
    ASSERT(cfg.block_cycles   == NP_LFS_CFG_BLOCK_CYCLES,    "block_cycles");
    ASSERT(cfg.name_max       == NP_LFS_CFG_NAME_MAX,        "name_max");
    ASSERT(cfg.attr_max       == NP_LFS_CFG_ATTR_MAX,        "attr_max");
    ASSERT(cfg.metadata_max   == NP_LFS_CFG_METADATA_MAX,    "metadata_max");

    ASSERT(cfg.read_buffer      != NULL, "read_buffer must be static, not heap");
    ASSERT(cfg.prog_buffer      != NULL, "prog_buffer must be static, not heap");
    ASSERT(cfg.lookahead_buffer != NULL, "lookahead_buffer must be static");

    /* apply() must NOT invent the glue.  If it ever did, a mount would proceed
     * against callbacks nobody bound.  Checked from a ZEROED config: the 0xA5
     * fill above proves apply() writes every field it claims to, and this
     * proves it writes no field it does not. */
    struct lfs_config zeroed;
    memset(&zeroed, 0, sizeof(zeroed));
    (void)np_lfs_config_apply(&zeroed);

    ASSERT(zeroed.read  == NULL && zeroed.prog == NULL &&
           zeroed.erase == NULL && zeroed.sync == NULL,
           "np_lfs_config_apply must leave the block-device glue to its caller");
#ifdef LFS_THREADSAFE
    ASSERT(zeroed.lock == NULL && zeroed.unlock == NULL,
           "np_lfs_config_apply must leave the mutex to its caller");
#endif
}

/* ── 6. A complete config is accepted ─────────────────────────────────────── */

static void test_validate_accepts_a_complete_config(void)
{
    struct lfs_config cfg;
    make_good(&cfg);

    ASSERT(np_lfs_config_validate(&cfg) == NP_HUB_OK,
           "a config built by np_lfs_config_apply plus bound callbacks must "
           "validate — if this fails, every rejection below is vacuous");

    ASSERT(np_lfs_config_validate(NULL) == NP_HUB_ERR_INVALID_ARG,
           "np_lfs_config_validate(NULL) must be rejected");
}

/* ── 7. Falsification — every perturbation is rejected (NP-CONV-001 §8) ───── */

static void reject_case(const char *what, void (*perturb)(struct lfs_config *))
{
    struct lfs_config cfg;
    make_good(&cfg);
    perturb(&cfg);

    if (np_lfs_config_validate(&cfg) == NP_HUB_OK) {
        printf("FAIL [falsification] np_lfs_config_validate ACCEPTED a config "
               "with %s. The check does not run.\n", what);
        g_fail_count++;
    }
}

static void p_read_size(struct lfs_config *c)   { c->read_size = 256U; }
static void p_prog_size(struct lfs_config *c)   { c->prog_size = 256U; }
static void p_block_size(struct lfs_config *c)  { c->block_size = 512U; }
static void p_block_count(struct lfs_config *c) { c->block_count = 32768U; }
static void p_file_max(struct lfs_config *c)    { c->file_max = 0U; }
static void p_cache_size(struct lfs_config *c)  { c->cache_size = 1024U; }
static void p_lookahead(struct lfs_config *c)   { c->lookahead_size = 16U; }
static void p_cycles_zero(struct lfs_config *c) { c->block_cycles = 0; }
static void p_cycles_off(struct lfs_config *c)  { c->block_cycles = -1; }
static void p_cycles_other(struct lfs_config *c){ c->block_cycles = 500; }
static void p_name_max(struct lfs_config *c)    { c->name_max = 0U; }
static void p_attr_max(struct lfs_config *c)    { c->attr_max = 1022U; }
static void p_metadata_max(struct lfs_config *c){ c->metadata_max = 0U; }
static void p_lookahead_old(struct lfs_config *c){ c->lookahead_size = 512U; }
static void p_no_read_buf(struct lfs_config *c) { c->read_buffer = NULL; }
static void p_no_prog_buf(struct lfs_config *c) { c->prog_buffer = NULL; }
static void p_no_look_buf(struct lfs_config *c) { c->lookahead_buffer = NULL; }
static void p_no_read(struct lfs_config *c)     { c->read = NULL; }
static void p_no_prog(struct lfs_config *c)     { c->prog = NULL; }
static void p_no_erase(struct lfs_config *c)    { c->erase = NULL; }
static void p_no_sync(struct lfs_config *c)     { c->sync = NULL; }
#ifdef LFS_THREADSAFE
static void p_no_lock(struct lfs_config *c)     { c->lock = NULL; }
static void p_no_unlock(struct lfs_config *c)   { c->unlock = NULL; }
#endif

static void test_validate_rejects_every_perturbation(void)
{
    reject_case("read_size 256 (EMMC-FS-01 Rev 2 — below "
                "the XTS unit, OI-LFS-10)",                   p_read_size);
    reject_case("prog_size 256 (EMMC-FS-01 Rev 2 — below "
                "the XTS unit, OI-LFS-10)",                   p_prog_size);
    reject_case("block_size 512 (EMMC-FS-01 says 4,096)",     p_block_size);
    reject_case("block_count 32,768 (EMMC-FS-01 says 4,096)", p_block_count);
    reject_case("file_max 0 — littlefs's 'use the default', "
                "which is not EMMC-FS-01's 65,536",           p_file_max);
    reject_case("cache_size 1024",                            p_cache_size);
    reject_case("lookahead_size 16 (upstream's example — "
                "256 blocks of 4,096 per pass)",              p_lookahead);
    reject_case("block_cycles 0 (lfs_init rejects it, but "
                "only via an assertion)",                     p_cycles_zero);
    reject_case("block_cycles -1 — WEAR LEVELLING DISABLED, "
                "and it passes every non-zero test",          p_cycles_off);
    reject_case("block_cycles 500 — the pre-OI-LFS-10 value, in "
                "upstream's range and still not EMMC-FS-01's", p_cycles_other);
    reject_case("lookahead_size 512 — the pre-OI-LFS-10 value", p_lookahead_old);
    reject_case("name_max 0 — littlefs's default 255, not "
                "EMMC-FS-01's 64",                            p_name_max);
    reject_case("attr_max 1,022 — littlefs's default, not "
                "EMMC-FS-01's 256",                           p_attr_max);
    reject_case("metadata_max 0 — the default, not stated",   p_metadata_max);
    reject_case("no read_buffer (LFS_NO_MALLOC)",             p_no_read_buf);
    reject_case("no prog_buffer (LFS_NO_MALLOC)",             p_no_prog_buf);
    reject_case("no lookahead_buffer (LFS_NO_MALLOC)",        p_no_look_buf);
    reject_case("no read callback (OI-LOG-05..07 unbound)",   p_no_read);
    reject_case("no prog callback",                           p_no_prog);
    reject_case("no erase callback",                          p_no_erase);
    reject_case("no sync callback — claim L-1 is about this " "one",  p_no_sync);
#ifdef LFS_THREADSAFE
    reject_case("no lock (LFS_THREADSAFE calls it on the "
                "first API call — a hard fault, not an error)", p_no_lock);
    reject_case("no unlock",                                   p_no_unlock);
#endif
}

/* ── 8. The library compiles and links under this configuration ───────────── */

static void test_library_links(void)
{
    /* Referencing a real littlefs symbol is what makes this suite fail if
     * np_littlefs stops building under NeurOne's configuration — the one thing
     * a header-only test could not catch.  lfs_migrate is absent under
     * LFS_READONLY/LFS_NO_MALLOC combinations, so lfs_crc is used: it lives in
     * lfs_util.c, which is the vendored file whose presence is otherwise only
     * argued for in prose. */
    const unsigned char probe[4] = { 'N', 'P', 'M', 'P' };
    uint32_t crc = lfs_crc(0xFFFFFFFFU, probe, sizeof(probe));

    ASSERT(crc == lfs_crc(0xFFFFFFFFU, probe, sizeof(probe)),
           "lfs_crc must be deterministic");
    ASSERT(crc != 0xFFFFFFFFU,
           "lfs_crc returned its seed unchanged — lfs_util.c is not the one "
           "linked, or LFS_CRC has been redirected");

    /* lfs_crc lives in lfs_util.c, so the case above does NOT pull lfs.o into
     * the link — which is how the LFS_NO_ASSERT defect (np_lfs_config.h
     * decision 2) produced a clean build the first time.  Taking the address of
     * a symbol defined in lfs.c makes the linker resolve that object, so an
     * unresolvable configuration fails here rather than at whichever later
     * phase first calls the filesystem.  The functions are never invoked: there
     * is no block device on a host. */
    ASSERT((void *)&lfs_mount  != NULL, "lfs.o must resolve at link");
    ASSERT((void *)&lfs_format != NULL, "lfs.o must resolve at link");
    ASSERT((void *)&lfs_file_opencfg != NULL,
           "lfs_file_opencfg must resolve — it is the only open() available "
           "under LFS_NO_MALLOC, and it is the function whose assertion the "
           "LFS_NO_ASSERT defect removed");
    ASSERT((void *)&lfs_file_sync != NULL,
           "lfs_file_sync must resolve — claim L-1 is about this function");
}

int main(void)
{
    printf("── littlefs SOUP configuration + instance tests "
           "(NP-SOUP-LFS-001 §7.3) ──\n");

    test_vendored_files_match_recorded_sha256();
    test_build_configuration_in_force();
    test_soup_record_states_the_same_configuration();
    test_instance_parameters();
    test_apply();
    test_validate_accepts_a_complete_config();
    test_validate_rejects_every_perturbation();
    test_library_links();

    if (g_fail_count == 0) {
        printf("PASS — all checks\n");
    } else {
        printf("FAILED — %d check(s)\n", g_fail_count);
    }
    return g_fail_count;
}
