/*
 * The hub's partitions, stored in files (NP-SIM-FW-001 §33, OI-SIMFW-51, REQ-SIMFW-25).  HOST ONLY; see np_sim_storage.h.
 *
 * Three block devices, each over an unlinked, owner-only, sparse file; the log partitions' littlefs instances and the file layer the
 * session logger's backend (np_log_backend.c) calls through the np_log_hal_part_* seams.  This file is on the littlefs callers' list of
 * scripts/check-lfs-caller-rules.ts with its reason: it is the host-only glue that gate's banner says must be added when the log file I/O
 * (OI-LOG-05..07) exists.
 */
#include "np_sim_storage.h"

#include "np_lfs_instance.h"
#include "np_lfs_log_instance.h"
#include "np_sim_bus.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    int      fd;
    uint64_t bytes;
} sim_dev_t;

static sim_dev_t g_cfg_dev = { -1, 0U };

/* ── the medium: a file nobody can name ──────────────────────────────────────────────────────────────────────────────────────── */

/* The system temporary directory (the host's own choice of it), a random name, mode 0600, and unlinked at once: the descriptor is the only
 * way to it (REQ-SIMFW-21 (b), (d)).  Sparse, so a partition of gigabytes costs the blocks written. */
static int open_backing(uint64_t bytes)
{
    const char *dir = getenv("TMPDIR");
    if (dir == NULL || dir[0] == '\0') { dir = "/tmp"; }
    char path[512];
    if (snprintf(path, sizeof path, "%s/np-sim-XXXXXX", dir) >= (int)sizeof path) { return -1; }
    const int fd = mkstemp(path);
    if (fd < 0) { return -1; }
    (void)unlink(path);
    if (fchmod(fd, S_IRUSR | S_IWUSR) != 0 || ftruncate(fd, (off_t)bytes) != 0) { (void)close(fd); return -1; }
    return fd;
}

static int dev_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buf, lfs_size_t size)
{
    const sim_dev_t *d = c->context;
    const ssize_t r = pread(d->fd, buf, size, (off_t)block * (off_t)c->block_size + (off_t)off);
    return (r == (ssize_t)size) ? 0 : LFS_ERR_IO;
}

static int dev_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buf, lfs_size_t size)
{
    const sim_dev_t *d = c->context;
    const ssize_t r = pwrite(d->fd, buf, size, (off_t)block * (off_t)c->block_size + (off_t)off);
    return (r == (ssize_t)size) ? 0 : LFS_ERR_IO;
}

/* An erase moves nothing: a block never written reads as zeros (the file is sparse), and littlefs does not depend on an erased block's
 * content.  The eMMC's erase is not modelled. */
static int dev_erase(const struct lfs_config *c, lfs_block_t block) { (void)c; (void)block; return 0; }

/* A sync flushes nothing to the host's disk (no durability is claimed). */
static int dev_sync(const struct lfs_config *c) { (void)c; return 0; }
static int dev_lock(const struct lfs_config *c) { (void)c; return 0; }
static int dev_unlock(const struct lfs_config *c) { (void)c; return 0; }

static void dev_bind(struct lfs_config *cfg, sim_dev_t *d)
{
    cfg->context = d;
    cfg->read    = dev_read;
    cfg->prog    = dev_prog;
    cfg->erase   = dev_erase;
    cfg->sync    = dev_sync;
    cfg->lock    = dev_lock;
    cfg->unlock  = dev_unlock;
}

np_hub_status_t np_sim_storage_config_device(struct lfs_config *cfg, uint64_t bytes)
{
    if (cfg == NULL) { return NP_HUB_ERR_INVALID_ARG; }
    if (g_cfg_dev.fd >= 0) { (void)close(g_cfg_dev.fd); g_cfg_dev.fd = -1; }
    g_cfg_dev.fd = open_backing(bytes);
    if (g_cfg_dev.fd < 0) { return NP_HUB_ERR_STORE_IO; }
    g_cfg_dev.bytes = bytes;
    dev_bind(cfg, &g_cfg_dev);
    return NP_HUB_OK;
}

static bool fd_info(int fd, np_sim_store_info_t *out)
{
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) { return false; }
    out->apparent_bytes  = (uint64_t)st.st_size;
    out->allocated_bytes = (uint64_t)st.st_blocks * 512U;
    out->nlink           = (unsigned)st.st_nlink;
    out->mode            = (unsigned)(st.st_mode & 0777U);
    return true;
}

/* ── the log partitions ──────────────────────────────────────────────────────────────────────────────────────────────────────── */

typedef struct {
    bool                  up;
    sim_dev_t             dev;
    struct lfs_config     cfg;
    lfs_t                 lfs;
    bool                  file_open;
    lfs_file_t            file;
    uint8_t               fbuf[NP_LFS_LOG_FILE_BUFFER_SIZE];
} sim_log_t;

static sim_log_t g_log[2] = { { .dev = { -1, 0U } }, { .dev = { -1, 0U } } };

/* The file configuration littlefs keeps a pointer to for as long as a file is open: static storage, one per partition (the one-handle rule). */
static struct lfs_file_config g_fcfg[2];

static const char *const UHDR_DIR  = "/uhdr";
static const char *const UHDR_SESS = "/uhdr/sessions";
static const char *const SHDR_FILE = "/shdr.log";

static bool log_mount(sim_log_t *l)
{
    if (lfs_mount(&l->lfs, &l->cfg) != 0) { return false; }
    return np_lfs_log_prime_allocator(&l->lfs) == NP_HUB_OK;
}

/* A new part: the partition's file made, the instance formatted in the device's own geometry, mounted, the allocator primed once as the glue
 * must (np_lfs_log_instance.h), and for UHDR the directories the session files live in.  The device mounts UHDR only after the user's key
 * unlocks it (np_uhdr_key_unlock); the twin has no key (OI-SIMFW-52) and mounts it on the first session file. */
static np_hub_status_t log_up(np_log_part_t part)
{
    sim_log_t *l = &g_log[part];
    if (l->up) { return NP_HUB_OK; }
    const uint64_t bytes = (uint64_t)np_lfs_log_block_count(part) * NP_LFS_LOG_BLOCK_SIZE;
    memset(&l->cfg, 0, sizeof l->cfg);
    memset(&l->lfs, 0, sizeof l->lfs);
    if (np_lfs_log_config_apply(part, &l->cfg) != NP_HUB_OK) { return NP_HUB_ERR_STORE_IO; }
    l->dev.fd = open_backing(bytes);
    if (l->dev.fd < 0) { return NP_HUB_ERR_STORE_IO; }
    l->dev.bytes = bytes;
    dev_bind(&l->cfg, &l->dev);
    if (np_lfs_log_config_validate(part, &l->cfg) != NP_HUB_OK) { return NP_HUB_ERR_STORE_IO; }
    if (lfs_format(&l->lfs, &l->cfg) != 0 || !log_mount(l)) { return NP_HUB_ERR_STORE_IO; }
    if (part == NP_LOG_PART_UHDR) {
        if (lfs_mkdir(&l->lfs, UHDR_DIR) != 0 || lfs_mkdir(&l->lfs, UHDR_SESS) != 0) { return NP_HUB_ERR_STORE_IO; }
    }
    l->up = true;
    return NP_HUB_OK;
}

np_hub_status_t np_log_hal_part_open(np_log_part_t part, uint64_t segment)
{
    if (part != NP_LOG_PART_UHDR && part != NP_LOG_PART_SHDR) { return NP_HUB_ERR_INVALID_ARG; }
    np_hub_status_t rc = log_up(part);
    if (rc != NP_HUB_OK) { return rc; }
    sim_log_t *l = &g_log[part];
    if (l->file_open) {                       /* the contract is one open file per partition: a second open closes the first */
        (void)lfs_file_close(&l->lfs, &l->file);
        l->file_open = false;
    }
    memset(&g_fcfg[part], 0, sizeof g_fcfg[part]);
    g_fcfg[part].buffer = l->fbuf;
    int flags;
    char path[96];
    if (part == NP_LOG_PART_UHDR) {           /* an exclusive create of one file per session; an existing one is never reopened */
        (void)snprintf(path, sizeof path, "%s/%020llu", UHDR_SESS, (unsigned long long)segment);
        flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_EXCL;
    } else {                                  /* the partition's one append-mode file; the segment is ignored */
        (void)snprintf(path, sizeof path, "%s", SHDR_FILE);
        flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND;
    }
    const int r = lfs_file_opencfg(&l->lfs, &l->file, path, flags, &g_fcfg[part]);
    if (r == LFS_ERR_EXIST) { return NP_HUB_ERR_LOG_EXISTS; }
    if (r != 0) { return NP_HUB_ERR_STORE_IO; }
    l->file_open = true;
    return NP_HUB_OK;
}

np_hub_status_t np_log_hal_part_append(np_log_part_t part, const uint8_t *buf, size_t len)
{
    if (part != NP_LOG_PART_UHDR && part != NP_LOG_PART_SHDR) { return NP_HUB_ERR_INVALID_ARG; }
    sim_log_t *l = &g_log[part];
    if (!l->file_open) { return NP_HUB_ERR_GENERIC; }          /* a write with no file open is a bug */
    return (lfs_file_write(&l->lfs, &l->file, buf, (lfs_size_t)len) == (lfs_ssize_t)len) ? NP_HUB_OK : NP_HUB_ERR_STORE_IO;
}

np_hub_status_t np_log_hal_part_sync(np_log_part_t part)
{
    if (part != NP_LOG_PART_UHDR && part != NP_LOG_PART_SHDR) { return NP_HUB_ERR_INVALID_ARG; }
    sim_log_t *l = &g_log[part];
    if (!l->file_open) { return NP_HUB_ERR_GENERIC; }
    return (lfs_file_sync(&l->lfs, &l->file) == 0) ? NP_HUB_OK : NP_HUB_ERR_STORE_IO;
}

np_hub_status_t np_log_hal_part_close(np_log_part_t part)
{
    if (part != NP_LOG_PART_UHDR && part != NP_LOG_PART_SHDR) { return NP_HUB_ERR_INVALID_ARG; }
    sim_log_t *l = &g_log[part];
    if (!l->file_open) { return NP_HUB_OK; }
    l->file_open = false;
    return (lfs_file_close(&l->lfs, &l->file) == 0) ? NP_HUB_OK : NP_HUB_ERR_STORE_IO;
}

/* ── inspection ──────────────────────────────────────────────────────────────────────────────────────────────────────────────── */

bool np_sim_storage_info(np_sim_store_t which, np_sim_store_info_t *out)
{
    if (out == NULL || (unsigned)which >= NP_SIM_STORE_COUNT) { return false; }
    memset(out, 0, sizeof *out);
    if (which == NP_SIM_STORE_CONFIG) {
        out->up = (g_cfg_dev.fd >= 0);
        return fd_info(g_cfg_dev.fd, out);
    }
    const sim_log_t *l = &g_log[which == NP_SIM_STORE_UHDR ? NP_LOG_PART_UHDR : NP_LOG_PART_SHDR];
    out->up = l->up;
    return fd_info(l->dev.fd, out);
}

unsigned np_sim_storage_uhdr_sessions(uint64_t *counters, uint32_t *sizes, unsigned max)
{
    sim_log_t *l = &g_log[NP_LOG_PART_UHDR];
    if (!l->up) { return 0U; }
    lfs_dir_t dir;
    if (lfs_dir_open(&l->lfs, &dir, UHDR_SESS) != 0) { return 0U; }
    unsigned n = 0U;
    struct lfs_info info;
    while (lfs_dir_read(&l->lfs, &dir, &info) > 0) {
        if (info.type != LFS_TYPE_REG) { continue; }
        if (n < max) {
            if (counters != NULL) { counters[n] = strtoull(info.name, NULL, 10); }
            if (sizes != NULL)    { sizes[n] = (uint32_t)info.size; }
        }
        n++;
    }
    (void)lfs_dir_close(&l->lfs, &dir);
    return n;
}

bool np_sim_storage_shdr_size(uint32_t *size)
{
    sim_log_t *l = &g_log[NP_LOG_PART_SHDR];
    struct lfs_info info;
    if (!l->up || size == NULL || lfs_stat(&l->lfs, SHDR_FILE, &info) != 0) { return false; }
    *size = (uint32_t)info.size;
    return true;
}

bool np_sim_storage_uhdr_read(uint64_t counter, uint32_t off, void *buf, uint32_t len, uint32_t *got)
{
    sim_log_t *l = &g_log[NP_LOG_PART_UHDR];
    if (!l->up || buf == NULL || got == NULL) { return false; }
    char path[96];
    (void)snprintf(path, sizeof path, "%s/%020llu", UHDR_SESS, (unsigned long long)counter);
    static uint8_t rbuf[NP_LFS_LOG_FILE_BUFFER_SIZE];
    static struct lfs_file_config rcfg;
    memset(&rcfg, 0, sizeof rcfg);
    rcfg.buffer = rbuf;
    lfs_file_t f;
    if (lfs_file_opencfg(&l->lfs, &f, path, LFS_O_RDONLY, &rcfg) != 0) { return false; }
    bool ok = (lfs_file_seek(&l->lfs, &f, (lfs_soff_t)off, LFS_SEEK_SET) >= 0);
    lfs_ssize_t r = ok ? lfs_file_read(&l->lfs, &f, buf, len) : -1;
    (void)lfs_file_close(&l->lfs, &f);
    if (r < 0) { return false; }
    *got = (uint32_t)r;
    return true;
}

bool np_sim_storage_remount_log(np_sim_store_t which)
{
    if (which != NP_SIM_STORE_UHDR && which != NP_SIM_STORE_SHDR) { return false; }
    const np_log_part_t part = (which == NP_SIM_STORE_UHDR) ? NP_LOG_PART_UHDR : NP_LOG_PART_SHDR;
    sim_log_t *l = &g_log[part];
    if (!l->up || l->file_open) { return false; }
    if (lfs_unmount(&l->lfs) != 0) { return false; }
    memset(&l->lfs, 0, sizeof l->lfs);            /* littlefs's RAM state is gone; the file is what is left */
    return log_mount(l);
}
