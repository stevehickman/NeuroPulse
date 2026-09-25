/*
 * np_cfg_store.c — the Config-partition file store over littlefs
 * Document: NP-SOUP-LFS-001 Rev 4 §13 (OI-LFS-06, OI-LFS-08, OI-LFS-09)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_cfg_store.h for the rules this module is, and why each one exists.
 * The short form: littlefs v2.11.3's three applicable anomalies are stopped by
 * caller policy, and this is the caller.
 */

#include "np_cfg_store.h"

#include <string.h>

#include "np_lfs_instance.h"

/* ── The file table ────────────────────────────────────────────────────────────
 * The ONLY place a Config file is named.  scripts/check-lfs-caller-rules.ts
 * parses it: the set of TAIL_ADDITIVE rows is pinned there (REQ-LFS-01), so a
 * second journal — or a rebuild-cache file quietly moved to tail-additive — is
 * a decision that fails CI until it is made on the record.
 *
 * Paths.  The replicated record's two copies live in two DIRECTORIES, not two
 * names in one directory: each littlefs directory is its own chain of metadata
 * pairs, and upstream #1210 loses a NAME tag while compacting ONE pair.  Two
 * names in the root would share the pair the defect acts on.  Nothing is ever
 * created in or removed from either directory after its first write, so
 * neither pair ever sees the create/delete traffic #1210 is triggered by.
 */
typedef struct {
    np_cfg_policy_t policy;
    const char     *path[2];   /* [1] is used by REPLICATED only */
} np_cfg_file_desc_t;

#define NP_CFG_REPLICA_DIR_A  "ra"
#define NP_CFG_REPLICA_DIR_B  "rb"

static const np_cfg_file_desc_t s_files[NP_CFG_FILE_COUNT] = {
    [NP_CFG_FILE_NPMP] = { NP_CFG_POLICY_REBUILD,       { "npmp.bin", NULL } },
    [NP_CFG_FILE_MAP3] = { NP_CFG_POLICY_TAIL_ADDITIVE, { "map3.jrn", NULL } },
    [NP_CFG_FILE_UKMD] = { NP_CFG_POLICY_REPLICATED,
                           { NP_CFG_REPLICA_DIR_A "/ukmd.rec",
                             NP_CFG_REPLICA_DIR_B "/ukmd.rec" } },
    /* OI-LFS-12: the count must never go backwards, so it is REPLICATED —
     * the envelope's generation picks the newer copy after a torn write, and
     * one lost entry does not lose it.  It bounds nothing (REQ-LFS-01). */
    [NP_CFG_FILE_SESSION_COUNT] = { NP_CFG_POLICY_REPLICATED,
                           { NP_CFG_REPLICA_DIR_A "/sesscnt.rec",
                             NP_CFG_REPLICA_DIR_B "/sesscnt.rec" } },
    /* OI-WA-03: the 256-bit warranty token (NP-FW-EMMC-002 §A.2).  Nothing
     * re-answers it — a lost token cannot be regenerated as the same value —
     * so it is REPLICATED, like ukmd.rec.  Written once per device life (and
     * once per factory reset); it bounds nothing (REQ-LFS-01). */
    [NP_CFG_FILE_WARRANTY_TOKEN] = { NP_CFG_POLICY_REPLICATED,
                           { NP_CFG_REPLICA_DIR_A "/wtoken.rec",
                             NP_CFG_REPLICA_DIR_B "/wtoken.rec" } },
    /* OI-ACC-08: per-consumable session counts since replacement.  A count
     * that went backwards after a torn write would skip a prompt (the
     * intranasal sleeve's is one session), so REPLICATED.  Rewritten once per
     * session end and per replacement; it bounds no emission (REQ-LFS-01). */
    [NP_CFG_FILE_CONSUMABLES] = { NP_CFG_POLICY_REPLICATED,
                           { NP_CFG_REPLICA_DIR_A "/consum.rec",
                             NP_CFG_REPLICA_DIR_B "/consum.rec" } },
};

/* ── RAM state — all of it lost at a reboot, and np_cfg_store_bind() is one ── */

static lfs_t                   *s_lfs;
static const struct lfs_config *s_cfg;
static np_cfg_lock_fn           s_lock;
static np_cfg_lock_fn           s_unlock;
static bool                     s_mounted;   /* a caller asked for it   */
static bool                     s_live;      /* lfs_t actually mounted  */

/* OI-LFS-09: set by any block-device read error, cleared only by a remount.
 * While set, NO operation may reach littlefs through the old lfs_t — its read
 * cache may hold bytes the device never returned (#1205). */
static bool                     s_remount_pending;

/* OI-LFS-06: the open-handle registry.  One slot per (file, copy). */
static bool                     s_open[NP_CFG_FILE_COUNT][2];

/* One per-file cache.  LFS_NO_MALLOC makes it caller-owned; the registry makes
 * it safe to share, because at most one handle is open at a time — the store
 * lock is held from open to close. */
static uint8_t                  s_file_cache[NP_LFS_FILE_BUFFER_SIZE];

static np_cfg_store_stats_t     s_stats;

/* ── Replica envelope ──────────────────────────────────────────────────────────
 *   magic "NPRR" (4) | generation (4, LE) | length (4, LE) | payload | CRC-32 (4)
 * The CRC covers everything before it.  Kept local rather than shared with
 * np_module_map's so the two stored formats cannot drift together.
 */
#define NP_CFG_REPLICA_HDR     12U
#define NP_CFG_REPLICA_CRC     4U
#define NP_CFG_REPLICA_MAX     (NP_CFG_REPLICA_HDR + NP_CFG_REPLICA_MAX_PAYLOAD \
                                + NP_CFG_REPLICA_CRC)

static const uint8_t k_replica_magic[4] = { 'N', 'P', 'R', 'R' };

static uint32_t crc32_le(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0U; i < len; i++) {
        crc ^= data[i];
        for (unsigned b = 0U; b < 8U; b++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ── Internals ─────────────────────────────────────────────────────────────── */

static bool file_ok(np_cfg_file_t file)
{
    return (unsigned)file < (unsigned)NP_CFG_FILE_COUNT;
}

/* Record a read failure: count it, and poison the instance until remounted. */
static void note_read_error(void)
{
    s_stats.read_errors++;
    s_remount_pending = true;
}

/*
 * OI-LFS-09: the remount that discards littlefs's read cache.  lfs_mount()
 * runs lfs_init(), which zeroes rcache and pcache — the only public-API way to
 * drop a cache that may hold bytes the device never returned.  Patching
 * lfs_bd_read() is not available (vendored SOUP is byte-exact, NP-SW-CI-001
 * §9), and reaching into lfs_t to clear rcache.block would couple NeurOne to a
 * private layout.  Safe here because the registry guarantees no handle is open
 * across a store call.
 */
static np_hub_status_t ensure_usable(void)
{
    if (!s_mounted) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    if (!s_remount_pending) {
        return NP_HUB_OK;
    }
    if (s_live) {
        (void)lfs_unmount(s_lfs);
        s_live = false;
    }
    memset(s_lfs, 0, sizeof(*s_lfs));
    if (lfs_mount(s_lfs, s_cfg) != 0) {
        /* The remount itself met the fault.  Stay pending and try again on
         * the next call: a transient error must not leave the instance
         * unmounted for the rest of the power cycle.  Found by this module's
         * own test (NP-SOUP-LFS-001 §13.4). */
        return NP_HUB_ERR_STORE_IO;
    }
    s_live            = true;
    s_remount_pending = false;
    s_stats.remounts++;
    return NP_HUB_OK;
}

/*
 * OI-LFS-06: the single opener.  Every handle on the instance comes from here,
 * and the gate forbids lfs_file_open*() anywhere else in firmware/.  Called
 * with the store lock held.
 */
static np_hub_status_t cfg_open(np_cfg_file_t file, unsigned copy,
                                lfs_file_t *handle, int flags)
{
    if (s_open[file][copy]) {
        s_stats.busy_refusals++;
        return NP_HUB_ERR_STORE_BUSY;
    }

    struct lfs_file_config fcfg;
    memset(&fcfg, 0, sizeof(fcfg));
    fcfg.buffer = s_file_cache;

    int err = lfs_file_opencfg(s_lfs, handle, s_files[file].path[copy], flags,
                               &fcfg);
    if (err == LFS_ERR_NOENT) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    if (err != 0) {
        /* A failed open fetched metadata; if a read failed underneath it the
         * cache is suspect.  Treat every non-NOENT failure as one. */
        note_read_error();
        return NP_HUB_ERR_STORE_IO;
    }
    s_open[file][copy] = true;
    return NP_HUB_OK;
}

static int cfg_close(np_cfg_file_t file, unsigned copy, lfs_file_t *handle)
{
    int err = lfs_file_close(s_lfs, handle);
    s_open[file][copy] = false;
    return err;
}

/*
 * Read the whole of (file, copy) into buf.  No retry on failure, by rule: a
 * failed read is an absence and poisons the instance until the next remount.
 */
static np_hub_status_t read_whole(np_cfg_file_t file, unsigned copy,
                                  uint8_t *buf, size_t cap, size_t *out_len)
{
    lfs_file_t handle;
    np_hub_status_t st = cfg_open(file, copy, &handle, LFS_O_RDONLY);
    if (st != NP_HUB_OK) {
        return st;
    }

    lfs_soff_t size = lfs_file_size(s_lfs, &handle);
    if (size < 0) {
        (void)cfg_close(file, copy, &handle);
        note_read_error();
        return NP_HUB_ERR_STORE_IO;
    }
    if ((size_t)size > cap) {
        (void)cfg_close(file, copy, &handle);
        s_stats.integrity_fails++;
        return NP_HUB_ERR_STORE_INTEGRITY;
    }

    lfs_ssize_t got = lfs_file_read(s_lfs, &handle, buf, (lfs_size_t)size);
    int cerr = cfg_close(file, copy, &handle);
    if (got < 0 || cerr != 0) {
        note_read_error();
        return NP_HUB_ERR_STORE_IO;
    }
    if (got != size) {
        /* Short read with no error: littlefs disagrees with itself about the
         * file's length.  Not a device fault, but not usable either. */
        s_stats.integrity_fails++;
        return NP_HUB_ERR_STORE_INTEGRITY;
    }
    *out_len = (size_t)got;
    return NP_HUB_OK;
}

/*
 * The atomic in-place replacement (OI-LFS-08).  O_CREAT only brings the file
 * into existence the first time; O_TRUNC is lazy in littlefs and is committed
 * together with the new contents at close, so the old file survives a power
 * loss at any point before that commit.  No second name exists at any moment,
 * and nothing is removed.
 */
static np_hub_status_t write_whole(np_cfg_file_t file, unsigned copy,
                                   const uint8_t *buf, size_t len)
{
    lfs_file_t handle;
    np_hub_status_t st = cfg_open(file, copy, &handle, LFS_O_RDONLY);
    bool existed = (st == NP_HUB_OK);
    if (existed) {
        (void)cfg_close(file, copy, &handle);
    } else if (st != NP_HUB_ERR_NOT_PRESENT) {
        return st;
    }

    st = cfg_open(file, copy, &handle, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (st != NP_HUB_OK) {
        return (st == NP_HUB_ERR_NOT_PRESENT) ? NP_HUB_ERR_STORE_IO : st;
    }
    lfs_ssize_t put = lfs_file_write(s_lfs, &handle, buf, (lfs_size_t)len);
    int cerr = cfg_close(file, copy, &handle);
    if (put != (lfs_ssize_t)len || cerr != 0) {
        /* A failed write may have read metadata on the way; same poisoning
         * rule as a failed read. */
        note_read_error();
        return NP_HUB_ERR_STORE_IO;
    }
    if (!existed) {
        s_stats.creates++;
    }
    return NP_HUB_OK;
}

/* Run `fn` with the store lock held and the instance usable. */
#define WITH_STORE(expr)                                  \
    do {                                                  \
        s_lock();                                         \
        np_hub_status_t _st = ensure_usable();            \
        if (_st == NP_HUB_OK) {                           \
            _st = (expr);                                 \
        }                                                 \
        s_unlock();                                       \
        return _st;                                       \
    } while (0)

/* ── Lifecycle ─────────────────────────────────────────────────────────────── */

static void noop_lock(void) { }

np_hub_status_t np_cfg_store_bind(lfs_t *lfs, const struct lfs_config *cfg,
                                  np_cfg_lock_fn lock, np_cfg_lock_fn unlock)
{
    /* A reboot: every piece of RAM state goes, whatever it was. */
    s_lfs             = NULL;
    s_cfg             = NULL;
    s_lock            = noop_lock;
    s_unlock          = noop_lock;
    s_mounted         = false;
    s_live            = false;
    s_remount_pending = false;
    memset(s_open, 0, sizeof(s_open));
    memset(&s_stats, 0, sizeof(s_stats));

    if (lfs == NULL || cfg == NULL || lock == NULL || unlock == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    s_lfs    = lfs;
    s_cfg    = cfg;
    s_lock   = lock;
    s_unlock = unlock;
    return NP_HUB_OK;
}

static np_hub_status_t ensure_dir(const char *path)
{
    int err = lfs_mkdir(s_lfs, path);
    if (err == 0) {
        s_stats.creates++;
        return NP_HUB_OK;
    }
    return (err == LFS_ERR_EXIST) ? NP_HUB_OK : NP_HUB_ERR_STORE_IO;
}

static np_hub_status_t mount_locked(void)
{
    if (s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    np_hub_status_t st = np_lfs_config_validate(s_cfg);
    if (st != NP_HUB_OK) {
        return st;
    }
    memset(s_lfs, 0, sizeof(*s_lfs));
    if (lfs_mount(s_lfs, s_cfg) != 0) {
        return NP_HUB_ERR_STORE_IO;
    }
    s_mounted         = true;
    s_live            = true;
    s_remount_pending = false;

    st = ensure_dir(NP_CFG_REPLICA_DIR_A);
    if (st == NP_HUB_OK) {
        st = ensure_dir(NP_CFG_REPLICA_DIR_B);
    }
    return st;
}

np_hub_status_t np_cfg_store_format(void)
{
    if (s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    s_lock();
    np_hub_status_t st = np_lfs_config_validate(s_cfg);
    if (st == NP_HUB_OK) {
        memset(s_lfs, 0, sizeof(*s_lfs));
        st = (lfs_format(s_lfs, s_cfg) == 0) ? NP_HUB_OK : NP_HUB_ERR_STORE_IO;
    }
    if (st == NP_HUB_OK) {
        st = mount_locked();
    }
    if (st == NP_HUB_OK) {
        (void)lfs_unmount(s_lfs);
        s_mounted = false;
        s_live    = false;
    }
    s_unlock();
    return st;
}

np_hub_status_t np_cfg_store_mount(void)
{
    if (s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    s_lock();
    np_hub_status_t st = mount_locked();
    s_unlock();
    return st;
}

void np_cfg_store_unmount(void)
{
    if (s_lfs == NULL) {
        return;
    }
    s_lock();
    if (s_live) {
        (void)lfs_unmount(s_lfs);
    }
    s_mounted = false;
    s_live    = false;
    s_unlock();
}

np_cfg_policy_t np_cfg_store_policy(np_cfg_file_t file)
{
    return file_ok(file) ? s_files[file].policy : NP_CFG_POLICY_INVALID;
}

/* ── REBUILD ───────────────────────────────────────────────────────────────── */

static np_hub_status_t read_locked(np_cfg_file_t file, uint8_t *buf, size_t cap,
                                   size_t *out_len, np_cfg_verify_fn verify,
                                   void *ctx)
{
    size_t len = 0U;
    np_hub_status_t st = read_whole(file, 0U, buf, cap, &len);
    if (st != NP_HUB_OK) {
        return st;
    }
    /* After the read, never before it and never instead of it. */
    if (!verify(buf, len, ctx)) {
        s_stats.integrity_fails++;
        return NP_HUB_ERR_STORE_INTEGRITY;
    }
    *out_len = len;
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_read(np_cfg_file_t file, uint8_t *buf, size_t cap,
                                  size_t *out_len, np_cfg_verify_fn verify,
                                  void *ctx)
{
    if (!file_ok(file) || s_files[file].policy != NP_CFG_POLICY_REBUILD ||
        buf == NULL || out_len == NULL || verify == NULL || s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    *out_len = 0U;
    WITH_STORE(read_locked(file, buf, cap, out_len, verify, ctx));
}

np_hub_status_t np_cfg_store_replace(np_cfg_file_t file, const uint8_t *buf,
                                     size_t len)
{
    if (!file_ok(file) || s_files[file].policy != NP_CFG_POLICY_REBUILD ||
        buf == NULL || s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    WITH_STORE(write_whole(file, 0U, buf, len));
}

/* ── TAIL_ADDITIVE ─────────────────────────────────────────────────────────── */

static np_hub_status_t append_locked(np_cfg_file_t file, const uint8_t *rec,
                                     size_t rec_len)
{
    lfs_file_t handle;
    np_hub_status_t st = cfg_open(file, 0U, &handle, LFS_O_RDONLY);
    bool existed = (st == NP_HUB_OK);
    if (existed) {
        (void)cfg_close(file, 0U, &handle);
    } else if (st != NP_HUB_ERR_NOT_PRESENT) {
        return st;
    }

    /* O_APPEND and never O_TRUNC: D-5, and REQ-LFS-01's premise. */
    st = cfg_open(file, 0U, &handle, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND);
    if (st != NP_HUB_OK) {
        return (st == NP_HUB_ERR_NOT_PRESENT) ? NP_HUB_ERR_STORE_IO : st;
    }
    lfs_ssize_t put = lfs_file_write(s_lfs, &handle, rec, (lfs_size_t)rec_len);
    int cerr = cfg_close(file, 0U, &handle);
    if (put != (lfs_ssize_t)rec_len || cerr != 0) {
        note_read_error();
        return NP_HUB_ERR_STORE_IO;
    }
    if (!existed) {
        s_stats.creates++;
    }
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_journal_append(np_cfg_file_t file,
                                            const uint8_t *rec, size_t rec_len)
{
    if (!file_ok(file) || s_files[file].policy != NP_CFG_POLICY_TAIL_ADDITIVE ||
        rec == NULL || rec_len == 0U || s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    WITH_STORE(append_locked(file, rec, rec_len));
}

static np_hub_status_t journal_read_locked(np_cfg_file_t file, uint8_t *buf,
                                           size_t cap, size_t rec_len,
                                           uint32_t *out_count,
                                           np_cfg_record_verify_fn verify,
                                           void *ctx)
{
    size_t len = 0U;
    np_hub_status_t st = read_whole(file, 0U, buf, cap, &len);
    if (st != NP_HUB_OK) {
        return st;
    }
    uint32_t n = 0U;
    for (size_t off = 0U; off + rec_len <= len; off += rec_len) {
        if (!verify(buf + off, rec_len, n, ctx)) {
            /* The valid prefix ends here.  A torn tail is L-4's expected
             * outcome, not a fault — but it is counted, so a journal that is
             * torn more often than power is lost becomes visible. */
            s_stats.integrity_fails++;
            break;
        }
        n++;
    }
    *out_count = n;
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_journal_read(np_cfg_file_t file, uint8_t *buf,
                                          size_t cap, size_t rec_len,
                                          uint32_t *out_count,
                                          np_cfg_record_verify_fn verify,
                                          void *ctx)
{
    if (!file_ok(file) || s_files[file].policy != NP_CFG_POLICY_TAIL_ADDITIVE ||
        buf == NULL || out_count == NULL || verify == NULL || rec_len == 0U ||
        s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    *out_count = 0U;
    WITH_STORE(journal_read_locked(file, buf, cap, rec_len, out_count, verify,
                                   ctx));
}

/* ── REPLICATED ────────────────────────────────────────────────────────────── */

static uint8_t s_env[2][NP_CFG_REPLICA_MAX];

/* Returns true and the generation if copy `env` is a valid envelope carrying
 * exactly `len` payload bytes. */
static bool envelope_valid(const uint8_t *env, size_t env_len, size_t len,
                           uint32_t *gen)
{
    if (env_len != NP_CFG_REPLICA_HDR + len + NP_CFG_REPLICA_CRC) {
        return false;
    }
    if (memcmp(env, k_replica_magic, 4U) != 0) {
        return false;
    }
    if (get_u32(env + 8) != (uint32_t)len) {
        return false;
    }
    size_t body = NP_CFG_REPLICA_HDR + len;
    if (crc32_le(env, body) != get_u32(env + body)) {
        return false;
    }
    *gen = get_u32(env + 4);
    return true;
}

static size_t envelope_build(uint8_t *env, uint32_t gen, const uint8_t *payload,
                             size_t len)
{
    memcpy(env, k_replica_magic, 4U);
    put_u32(env + 4, gen);
    put_u32(env + 8, (uint32_t)len);
    memcpy(env + NP_CFG_REPLICA_HDR, payload, len);
    size_t body = NP_CFG_REPLICA_HDR + len;
    put_u32(env + body, crc32_le(env, body));
    return body + NP_CFG_REPLICA_CRC;
}

/* Read both copies.  valid[c] says whether copy c verified. */
/*
 * Read both copies.  `why[c]` says what became of copy c when it is not valid:
 * NP_HUB_ERR_NOT_PRESENT (the entry does not exist), NP_HUB_ERR_STORE_IO (a
 * read failed, or the remount it forced did) or NP_HUB_ERR_STORE_INTEGRITY
 * (the bytes were read and refused).  NP_HUB_OK when valid.
 */
static void read_replicas(np_cfg_file_t file, size_t len, bool valid[2],
                          uint32_t gen[2], np_hub_status_t why[2])
{
    for (unsigned c = 0U; c < 2U; c++) {
        valid[c] = false;
        gen[c]   = 0U;
        why[c]   = NP_HUB_ERR_STORE_IO;
    }
    for (unsigned c = 0U; c < 2U; c++) {
        size_t got = 0U;
        if (s_remount_pending) {
            /* The first copy's read failed: the cache is suspect, and the
             * second copy must not be read through it (#1205).  Remount now,
             * inside this operation, rather than giving up on the twin. */
            if (ensure_usable() != NP_HUB_OK) {
                return;
            }
        }
        np_hub_status_t st = read_whole(file, c, s_env[c], sizeof(s_env[c]), &got);
        if (st != NP_HUB_OK) {
            why[c] = st;
            continue;
        }
        if (envelope_valid(s_env[c], got, len, &gen[c])) {
            valid[c] = true;
            why[c]   = NP_HUB_OK;
        } else {
            s_stats.integrity_fails++;
            why[c] = NP_HUB_ERR_STORE_INTEGRITY;
        }
    }
}

static np_hub_status_t replicated_read_locked(np_cfg_file_t file,
                                              uint8_t *payload, size_t len)
{
    bool            valid[2];
    uint32_t        gen[2];
    np_hub_status_t why[2];
    read_replicas(file, len, valid, gen, why);

    if (!valid[0] && !valid[1]) {
        /* Absent only if BOTH entries are absent.  A copy that could not be
         * read, or was read and refused, is not an absence, and a caller that
         * creates a record on absence (np_warranty_token) must not be told it
         * is one: it would overwrite a record it merely failed to read. */
        if (why[0] == NP_HUB_ERR_NOT_PRESENT && why[1] == NP_HUB_ERR_NOT_PRESENT) {
            return NP_HUB_ERR_NOT_PRESENT;
        }
        return (why[0] == NP_HUB_ERR_STORE_IO || why[1] == NP_HUB_ERR_STORE_IO)
                   ? NP_HUB_ERR_STORE_IO : NP_HUB_ERR_STORE_INTEGRITY;
    }
    unsigned good = (valid[0] && (!valid[1] || gen[0] >= gen[1])) ? 0U : 1U;
    unsigned other = 1U - good;
    memcpy(payload, s_env[good] + NP_CFG_REPLICA_HDR, len);

    /* Repair the twin the first time the record is used.  Rewritten with the
     * SAME generation: it is a copy of the good record, not a new one. */
    if (!valid[other] || gen[other] != gen[good]) {
        if (ensure_usable() == NP_HUB_OK &&
            write_whole(file, other, s_env[good],
                        NP_CFG_REPLICA_HDR + len + NP_CFG_REPLICA_CRC) == NP_HUB_OK) {
            s_stats.repairs++;
        }
    }
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_read(np_cfg_file_t file,
                                             uint8_t *payload, size_t len)
{
    if (!file_ok(file) || s_files[file].policy != NP_CFG_POLICY_REPLICATED ||
        payload == NULL || len == 0U || len > NP_CFG_REPLICA_MAX_PAYLOAD ||
        s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    WITH_STORE(replicated_read_locked(file, payload, len));
}

static np_hub_status_t replicated_write_locked(np_cfg_file_t file,
                                               const uint8_t *payload,
                                               size_t len)
{
    bool            valid[2];
    uint32_t        gen[2];
    np_hub_status_t why[2];
    read_replicas(file, len, valid, gen, why);

    uint32_t newest = 0U;
    for (unsigned c = 0U; c < 2U; c++) {
        if (valid[c] && gen[c] > newest) {
            newest = gen[c];
        }
    }

    uint8_t env[NP_CFG_REPLICA_MAX];
    size_t  env_len = envelope_build(env, newest + 1U, payload, len);

    /* A first, then B.  Each is an atomic in-place replacement, so at every
     * instant at least one copy is a complete valid envelope — the old record
     * or the new one — provided at least one was valid to begin with. */
    for (unsigned c = 0U; c < 2U; c++) {
        np_hub_status_t st = ensure_usable();
        if (st == NP_HUB_OK) {
            st = write_whole(file, c, env, env_len);
        }
        if (st != NP_HUB_OK) {
            return st;
        }
    }
    return NP_HUB_OK;
}

np_hub_status_t np_cfg_store_replicated_write(np_cfg_file_t file,
                                              const uint8_t *payload,
                                              size_t len)
{
    if (!file_ok(file) || s_files[file].policy != NP_CFG_POLICY_REPLICATED ||
        payload == NULL || len == 0U || len > NP_CFG_REPLICA_MAX_PAYLOAD ||
        s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    WITH_STORE(replicated_write_locked(file, payload, len));
}

/* ── Observability ─────────────────────────────────────────────────────────── */

void np_cfg_store_stats(np_cfg_store_stats_t *out)
{
    if (out != NULL) {
        s_lock();
        *out = s_stats;
        s_unlock();
    }
}

#ifdef NPTEST_HOST
np_hub_status_t np_cfg_store_test_double_open(np_cfg_file_t file)
{
    if (!file_ok(file) || s_lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    s_lock();
    np_hub_status_t st = ensure_usable();
    lfs_file_t first;
    lfs_file_t second;
    if (st == NP_HUB_OK) {
        st = cfg_open(file, 0U, &first, LFS_O_RDONLY);
    }
    if (st == NP_HUB_OK) {
        st = cfg_open(file, 0U, &second, LFS_O_RDONLY);
        if (st == NP_HUB_OK) {
            (void)cfg_close(file, 0U, &second);   /* the registry failed */
        }
        (void)cfg_close(file, 0U, &first);
    }
    s_unlock();
    return st;
}
#endif
