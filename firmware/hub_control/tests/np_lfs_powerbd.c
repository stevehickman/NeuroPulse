/*
 * np_lfs_powerbd.c — NeurOne power-loss-injecting test block device
 * Document: NP-SOUP-LFS-001 Rev 3 §7.2, §12 (closes OI-LFS-02)
 *
 * See np_lfs_powerbd.h for what this models and why it is NeurOne's own rather
 * than upstream's bd/lfs_emubd.*.
 */

#include "np_lfs_powerbd.h"

#include <stdlib.h>
#include <string.h>

/* The byte a half-programmed page reads back as.  NOT 0xFF: an erased-value
 * fill would make every torn program indistinguishable from a program that
 * never started, which is the one distinction the PARTIAL model exists to
 * make. */
#define NP_POWERBD_INDETERMINATE  0x5AU

static np_powerbd_t *bd_of(const struct lfs_config *c)
{
    return (np_powerbd_t *)c->context;
}

/* The bytes of `block`, allocating a sparse page on first write.  Returns NULL
 * only for a sparse block that has never been written and `for_write` is
 * false: it reads back erased. */
static uint8_t *block_ptr(np_powerbd_t *bd, lfs_block_t block, bool for_write)
{
    if (bd->media != NULL) {
        return bd->media + ((size_t)block * bd->block_size);
    }
    if (bd->sparse[block] == NULL && for_write) {
        bd->sparse[block] = malloc(bd->block_size);
        if (bd->sparse[block] != NULL) {
            memset(bd->sparse[block], 0xFF, bd->block_size);
        }
    }
    return bd->sparse[block];
}

/* Mark a block as differing from the sweep's pre-state.  Set on every prog and
 * erase that reaches the medium, torn ones included — a torn op changes the
 * block just as surely as a complete one. */
static void mark_dirty(np_powerbd_t *bd, lfs_block_t block)
{
    const size_t byte = (size_t)block / 8U;
    if (bd->dirty != NULL && byte < bd->dirty_bytes) {
        bd->dirty[byte] |= (uint8_t)(1U << (block % 8U));
    }
}

/*
 * Consume one op index and decide whether this is the one to cut at.
 * Returns true if the caller must apply its tear and stop.
 */
static bool take_op(np_powerbd_t *bd)
{
    const long idx = bd->ops_seen++;
    return (bd->cut_at != NP_POWERBD_NO_CUT) && (idx == bd->cut_at);
}

/* The processor stops here.  Nothing after the longjmp in the interrupted
 * littlefs call ever runs, which is the difference between a power loss and an
 * I/O error. */
static void stop(np_powerbd_t *bd)
{
    bd->cut_fired = true;
    bd->total_cuts++;
    longjmp(*bd->resume, 1);
}

static int np_powerbd_read(const struct lfs_config *c, lfs_block_t block,
                           lfs_off_t off, void *buffer, lfs_size_t size)
{
    np_powerbd_t *bd = bd_of(c);

    if (block >= bd->block_count || off + size > bd->block_size) {
        return LFS_ERR_IO;
    }

    /* Reads consume no op index: a read changes nothing, so cutting inside one
     * is indistinguishable from cutting just before it. */
    bd->total_reads++;

    if (bd->fail_reads > 0 && block == bd->fail_read_block) {
        /* An uncorrectable read.  The buffer is left exactly as it was — the
         * device returned nothing — which is #1205's precondition. */
        bd->fail_reads--;
        return LFS_ERR_IO;
    }

    const uint8_t *src = block_ptr(bd, block, false);
    if (src == NULL) {
        memset(buffer, 0xFF, size);
    } else {
        memcpy(buffer, src + off, size);
    }
    return 0;
}

static int np_powerbd_prog(const struct lfs_config *c, lfs_block_t block,
                           lfs_off_t off, const void *buffer, lfs_size_t size)
{
    np_powerbd_t *bd  = bd_of(c);

    if (block >= bd->block_count || off + size > bd->block_size) {
        return LFS_ERR_IO;
    }

    uint8_t *base = block_ptr(bd, block, true);
    if (base == NULL) {
        return LFS_ERR_IO;          /* host out of memory — not a medium fault */
    }
    uint8_t *dst = base + off;

    bd->total_progs++;
    mark_dirty(bd, block);

    if (!take_op(bd)) {
        memcpy(dst, buffer, size);
        return 0;
    }

    switch (bd->tear) {
        case NP_POWERBD_TEAR_NONE:
            /* The program never reached the medium. */
            break;

        case NP_POWERBD_TEAR_PARTIAL: {
            /* A prefix landed; the page that was in flight when the rail fell
             * reads back indeterminate.  The prefix is rounded DOWN to a whole
             * prog_size page so the model stays inside the granularity
             * struct lfs_config declares — a sub-page tear is a property of the
             * medium, not of the contract, and that is OI-LFS-07's question. */
            lfs_size_t whole = (size / 2U) - ((size / 2U) % bd->prog_size);
            if (whole > 0U) {
                memcpy(dst, buffer, whole);
            }
            lfs_size_t flight = (size - whole < bd->prog_size)
                                    ? (size - whole) : bd->prog_size;
            memset(dst + whole, NP_POWERBD_INDETERMINATE, flight);

            /* An encryption layer with a data unit larger than prog_size
             * rewrote the whole enclosing unit, so the whole unit is what the
             * tear leaves indeterminate — the neighbouring bytes of the unit
             * included, whether or not littlefs had committed them. */
            if (bd->rmw_unit > bd->prog_size) {
                lfs_off_t at    = off + whole;
                lfs_off_t first = at - (at % bd->rmw_unit);
                lfs_off_t last  = first + bd->rmw_unit;
                if (last > bd->block_size) {
                    last = bd->block_size;
                }
                memset(base + first, NP_POWERBD_INDETERMINATE, last - first);
            }
            break;
        }

        case NP_POWERBD_TEAR_FULL:
        default:
            /* The program completed; power was lost immediately after. */
            memcpy(dst, buffer, size);
            break;
    }

    stop(bd);
    return 0; /* unreachable — stop() does not return */
}

static int np_powerbd_erase(const struct lfs_config *c, lfs_block_t block)
{
    np_powerbd_t *bd  = bd_of(c);

    if (block >= bd->block_count) {
        return LFS_ERR_IO;
    }

    uint8_t *dst = block_ptr(bd, block, true);
    if (dst == NULL) {
        return LFS_ERR_IO;
    }

    bd->total_erases++;
    mark_dirty(bd, block);

    if (!take_op(bd)) {
        memset(dst, 0xFF, bd->block_size);
        return 0;
    }

    switch (bd->tear) {
        case NP_POWERBD_TEAR_NONE:
            break;

        case NP_POWERBD_TEAR_PARTIAL:
            /* Half the block reached the erased state; the rest still holds
             * whatever it held.  This is the model that most often leaves a
             * block that is neither the old contents nor a usable blank. */
            memset(dst, 0xFF, bd->block_size / 2U);
            break;

        case NP_POWERBD_TEAR_FULL:
        default:
            memset(dst, 0xFF, bd->block_size);
            break;
    }

    stop(bd);
    return 0; /* unreachable */
}

static int np_powerbd_sync(const struct lfs_config *c)
{
    np_powerbd_t *bd = bd_of(c);

    bd->total_syncs++;

    /* There is no write-back cache below this device, so a sync moves no bytes.
     * It still consumes an op index: "power was lost exactly at the flush
     * boundary" is a distinct instant from "power was lost during the program
     * before it", and L-1 is a claim about that boundary. */
    if (take_op(bd)) {
        stop(bd);
    }
    return 0;
}

void np_powerbd_bind(np_powerbd_t *bd, struct lfs_config *cfg,
                     uint8_t *media, lfs_size_t block_size,
                     lfs_size_t block_count, lfs_size_t prog_size,
                     uint8_t *dirty, lfs_size_t dirty_bytes)
{
    memset(bd, 0, sizeof(*bd));
    bd->media       = media;
    bd->block_size  = block_size;
    bd->block_count = block_count;
    bd->prog_size   = prog_size;
    bd->dirty       = dirty;
    bd->dirty_bytes = dirty_bytes;
    bd->cut_at      = NP_POWERBD_NO_CUT;

    cfg->context = bd;
    cfg->read    = np_powerbd_read;
    cfg->prog    = np_powerbd_prog;
    cfg->erase   = np_powerbd_erase;
    cfg->sync    = np_powerbd_sync;
}

void np_powerbd_bind_sparse(np_powerbd_t *bd, struct lfs_config *cfg,
                            uint8_t **table, lfs_size_t block_size,
                            lfs_size_t block_count, lfs_size_t prog_size,
                            uint8_t *dirty, lfs_size_t dirty_bytes)
{
    np_powerbd_bind(bd, cfg, NULL, block_size, block_count, prog_size,
                    dirty, dirty_bytes);
    bd->sparse = table;
}

uint8_t *np_powerbd_block_data(const np_powerbd_t *bd, lfs_size_t block)
{
    if (bd->media != NULL) {
        return bd->media + ((size_t)block * bd->block_size);
    }
    return bd->sparse[block];
}

void np_powerbd_sparse_drop(np_powerbd_t *bd, lfs_size_t block)
{
    if (bd->media == NULL && bd->sparse[block] != NULL) {
        free(bd->sparse[block]);
        bd->sparse[block] = NULL;
    }
}

void np_powerbd_sparse_free(np_powerbd_t *bd)
{
    if (bd->media != NULL || bd->sparse == NULL) {
        return;
    }
    for (lfs_size_t b = 0U; b < bd->block_count; b++) {
        np_powerbd_sparse_drop(bd, b);
    }
}

void np_powerbd_fail_reads(np_powerbd_t *bd, lfs_block_t block, long count)
{
    bd->fail_read_block = block;
    bd->fail_reads      = count;
}

void np_powerbd_wipe(np_powerbd_t *bd)
{
    if (bd->media == NULL) {
        np_powerbd_sparse_free(bd);          /* never-written reads back erased */
        if (bd->dirty != NULL) {
            memset(bd->dirty, 0xFF, bd->dirty_bytes);
        }
        return;
    }
    memset(bd->media, 0xFF, (size_t)bd->block_size * bd->block_count);
    if (bd->dirty != NULL) {
        memset(bd->dirty, 0xFF, bd->dirty_bytes);   /* every block differs now */
    }
}

bool np_powerbd_block_dirty(const np_powerbd_t *bd, lfs_size_t block)
{
    const size_t byte = (size_t)block / 8U;
    if (bd->dirty == NULL || byte >= bd->dirty_bytes) {
        return true;    /* no map: assume the worst, restore everything */
    }
    return (bd->dirty[byte] & (uint8_t)(1U << (block % 8U))) != 0U;
}

void np_powerbd_dirty_clear(np_powerbd_t *bd)
{
    if (bd->dirty != NULL) {
        memset(bd->dirty, 0, bd->dirty_bytes);
    }
}

void np_powerbd_power_cycle(np_powerbd_t *bd)
{
    bd->ops_seen  = 0;
    bd->cut_at    = NP_POWERBD_NO_CUT;
    bd->cut_fired = false;
    bd->resume    = NULL;
}

void np_powerbd_arm(np_powerbd_t *bd, long cut_at, np_powerbd_tear_t tear,
                    jmp_buf *resume)
{
    bd->ops_seen  = 0;
    bd->cut_at    = cut_at;
    bd->tear      = tear;
    bd->cut_fired = false;
    bd->resume    = resume;
}

long np_powerbd_ops_seen(const np_powerbd_t *bd)
{
    return bd->ops_seen;
}

bool np_powerbd_cut_fired(const np_powerbd_t *bd)
{
    return bd->cut_fired;
}

const char *np_powerbd_tear_name(np_powerbd_tear_t tear)
{
    switch (tear) {
        case NP_POWERBD_TEAR_NONE:    return "NONE (op never reached the medium)";
        case NP_POWERBD_TEAR_PARTIAL: return "PARTIAL (prefix landed, page in flight indeterminate)";
        case NP_POWERBD_TEAR_FULL:    return "FULL (op completed, power lost after)";
        default:                      return "?";
    }
}
