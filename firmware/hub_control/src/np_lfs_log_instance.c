/*
 * np_lfs_log_instance.c — the UHDR and SHDR littlefs instances (OI-LFS-05)
 * Document: NP-SOUP-LFS-001 Rev 4 §13.1, NP-FW-EMMC-001 EMMC-FS-01
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_lfs_log_instance.h for every value's source, and for the one value
 * that departs from EMMC-FS-01 and why.
 */

#include "np_lfs_log_instance.h"

#include "np_config.h"        /* NP_UHDR_SIZE_LBA, NP_SHDR_SIZE_LBA */

/* ── Static buffers (LFS_NO_MALLOC), one set per partition ─────────────────── */
static uint8_t s_uhdr_read[NP_LFS_LOG_CACHE_SIZE];
static uint8_t s_uhdr_prog[NP_LFS_LOG_CACHE_SIZE];
static uint8_t s_uhdr_lookahead[NP_LFS_LOG_UHDR_LOOKAHEAD];
static uint8_t s_shdr_read[NP_LFS_LOG_CACHE_SIZE];
static uint8_t s_shdr_prog[NP_LFS_LOG_CACHE_SIZE];
static uint8_t s_shdr_lookahead[NP_LFS_LOG_SHDR_LOOKAHEAD];

/* littlefs's own relations (lfs_init), at compile time: a build that cannot
 * produce a mountable configuration should not produce a binary. */
_Static_assert(NP_LFS_LOG_CACHE_SIZE % NP_LFS_LOG_READ_SIZE == 0,
               "cache_size must be a multiple of read_size (lfs_init)");
_Static_assert(NP_LFS_LOG_CACHE_SIZE % NP_LFS_LOG_PROG_SIZE == 0,
               "cache_size must be a multiple of prog_size (lfs_init)");
_Static_assert(NP_LFS_LOG_BLOCK_SIZE % NP_LFS_LOG_CACHE_SIZE == 0,
               "block_size must be a multiple of cache_size (lfs_init)");
_Static_assert(NP_LFS_LOG_METADATA_MAX <= NP_LFS_LOG_BLOCK_SIZE,
               "metadata_max must not exceed block_size (lfs_init)");
_Static_assert(NP_LFS_LOG_BLOCK_CYCLES != 0 && NP_LFS_LOG_BLOCK_CYCLES != -1,
               "block_cycles 0 is refused by lfs_init; -1 disables L-6");

/* The deviation, pinned where it cannot drift: the program unit must be a
 * whole number of XTS data units (EMMC-UHDR-05: 512 B), or a torn program
 * rewrites committed bytes it does not own. */
#define NP_XTS_DATA_UNIT  512u
_Static_assert(NP_LFS_LOG_PROG_SIZE % NP_XTS_DATA_UNIT == 0,
               "prog_size must be a whole number of XTS data units "
               "(EMMC-UHDR-05) — NP-SOUP-LFS-001 §13.1.3");

/* The partition arithmetic EMMC-FS-01 prints, checked against the partition
 * map the bootloader actually uses rather than against the table's own
 * division. */
_Static_assert((uint64_t)NP_LFS_LOG_UHDR_BLOCK_COUNT * NP_LFS_LOG_BLOCK_SIZE
                   == (uint64_t)NP_UHDR_SIZE_LBA * 512u,
               "the UHDR instance must be the whole UHDR partition");
_Static_assert((uint64_t)NP_LFS_LOG_SHDR_BLOCK_COUNT * NP_LFS_LOG_BLOCK_SIZE
                   == (uint64_t)NP_SHDR_SIZE_LBA * 512u,
               "the SHDR instance must be the whole SHDR partition");

typedef struct {
    lfs_size_t block_count;
    lfs_size_t lookahead_size;
    uint8_t   *read_buffer;
    uint8_t   *prog_buffer;
    uint8_t   *lookahead_buffer;
} np_lfs_log_part_desc_t;

static const np_lfs_log_part_desc_t s_parts[2] = {
    [NP_LOG_PART_UHDR] = { NP_LFS_LOG_UHDR_BLOCK_COUNT, NP_LFS_LOG_UHDR_LOOKAHEAD,
                           s_uhdr_read, s_uhdr_prog, s_uhdr_lookahead },
    [NP_LOG_PART_SHDR] = { NP_LFS_LOG_SHDR_BLOCK_COUNT, NP_LFS_LOG_SHDR_LOOKAHEAD,
                           s_shdr_read, s_shdr_prog, s_shdr_lookahead },
};

static const np_lfs_log_part_desc_t *desc_of(np_log_part_t part)
{
    if (part != NP_LOG_PART_UHDR && part != NP_LOG_PART_SHDR) {
        return NULL;
    }
    return &s_parts[part];
}

lfs_size_t np_lfs_log_block_count(np_log_part_t part)
{
    const np_lfs_log_part_desc_t *d = desc_of(part);
    return (d != NULL) ? d->block_count : 0U;
}

np_hub_status_t np_lfs_log_config_apply(np_log_part_t part,
                                        struct lfs_config *cfg)
{
    const np_lfs_log_part_desc_t *d = desc_of(part);
    if (cfg == NULL || d == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    cfg->read_size      = NP_LFS_LOG_READ_SIZE;
    cfg->prog_size      = NP_LFS_LOG_PROG_SIZE;
    cfg->block_size     = NP_LFS_LOG_BLOCK_SIZE;
    cfg->block_count    = d->block_count;
    cfg->block_cycles   = NP_LFS_LOG_BLOCK_CYCLES;
    cfg->cache_size     = NP_LFS_LOG_CACHE_SIZE;
    cfg->lookahead_size = d->lookahead_size;
    cfg->file_max       = NP_LFS_LOG_FILE_MAX;
    cfg->name_max       = NP_LFS_LOG_NAME_MAX;
    cfg->attr_max       = NP_LFS_LOG_ATTR_MAX;
    cfg->metadata_max   = NP_LFS_LOG_METADATA_MAX;

    cfg->read_buffer      = d->read_buffer;
    cfg->prog_buffer      = d->prog_buffer;
    cfg->lookahead_buffer = d->lookahead_buffer;
    return NP_HUB_OK;
}

np_hub_status_t np_lfs_log_config_validate(np_log_part_t part,
                                           const struct lfs_config *cfg)
{
    const np_lfs_log_part_desc_t *d = desc_of(part);
    if (cfg == NULL || d == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* Geometry and the program unit — L-5, and ECR-EMMC-002. */
    if (cfg->read_size   != NP_LFS_LOG_READ_SIZE  ||
        cfg->prog_size   != NP_LFS_LOG_PROG_SIZE  ||
        cfg->block_size  != NP_LFS_LOG_BLOCK_SIZE ||
        cfg->block_count != d->block_count) {
        return NP_HUB_ERR_BAD_VERSION;
    }

    /* Caches, allocator and wear levelling.  block_cycles by VALUE: -1 passes
     * every not-zero test and silently disables L-6. */
    if (cfg->cache_size     != NP_LFS_LOG_CACHE_SIZE   ||
        cfg->lookahead_size != d->lookahead_size       ||
        cfg->block_cycles   != NP_LFS_LOG_BLOCK_CYCLES) {
        return NP_HUB_ERR_BAD_VERSION;
    }

    /* The four limits EMMC-FS-01 states for the logs.  littlefs writes
     * name_max / file_max / attr_max into the superblock and refuses a mount
     * whose config is SMALLER than the disk's — so a drift here is a latent
     * mount failure, not a harmless default. */
    if (cfg->file_max     != NP_LFS_LOG_FILE_MAX ||
        cfg->name_max     != NP_LFS_LOG_NAME_MAX ||
        cfg->attr_max     != NP_LFS_LOG_ATTR_MAX ||
        cfg->metadata_max != NP_LFS_LOG_METADATA_MAX) {
        return NP_HUB_ERR_BAD_VERSION;
    }

    if (cfg->read_buffer == NULL || cfg->prog_buffer == NULL ||
        cfg->lookahead_buffer == NULL) {
        return NP_HUB_ERR_BAD_VERSION;
    }
    if (cfg->read == NULL || cfg->prog == NULL ||
        cfg->erase == NULL || cfg->sync == NULL) {
        return NP_HUB_ERR_BAD_VERSION;
    }
#ifdef LFS_THREADSAFE
    if (cfg->lock == NULL || cfg->unlock == NULL) {
        return NP_HUB_ERR_BAD_VERSION;
    }
#endif
    return NP_HUB_OK;
}

np_hub_status_t np_lfs_log_prime_allocator(lfs_t *lfs)
{
    if (lfs == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    return (lfs_fs_gc(lfs) == 0) ? NP_HUB_OK : NP_HUB_ERR_STORE_IO;
}
