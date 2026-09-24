/*
 * np_lfs_sweep.c — power-cut sweep harness for the Rev 4 littlefs suites
 * Document: NP-SOUP-LFS-001 Rev 4 §13.  See np_lfs_sweep.h.
 */

#include "np_lfs_sweep.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

static np_powerbd_t *g_bd;
static void        (*g_reboot)(void);

/* The pre-state: one saved copy per block that held data at snapshot time.
 * A NULL entry on a sparse medium means "never written" and is restored by
 * dropping the page, not by writing 0xFF into a new one. */
static uint8_t **g_snap;
static lfs_size_t g_snap_count;   /* the medium can change between sweeps */

static jmp_buf  g_resume;
static void   (*g_body)(void);

void np_sweep_bind(np_powerbd_t *bd, void (*reboot)(void))
{
    g_bd     = bd;
    g_reboot = reboot;
}

void np_sweep_release(void)
{
    if (g_snap == NULL) {
        return;
    }
    for (lfs_size_t b = 0U; b < g_snap_count; b++) {
        free(g_snap[b]);
    }
    free(g_snap);
    g_snap       = NULL;
    g_snap_count = 0U;
}

static void snapshot_take(void)
{
    np_sweep_release();
    g_snap = calloc(g_bd->block_count, sizeof(*g_snap));
    g_snap_count = (g_snap != NULL) ? g_bd->block_count : 0U;
    for (lfs_size_t b = 0U; g_snap != NULL && b < g_bd->block_count; b++) {
        const uint8_t *src = np_powerbd_block_data(g_bd, b);
        if (src != NULL) {
            g_snap[b] = malloc(g_bd->block_size);
            if (g_snap[b] != NULL) {
                memcpy(g_snap[b], src, g_bd->block_size);
            }
        }
    }
    np_powerbd_dirty_clear(g_bd);
}

static void snapshot_restore(void)
{
    for (lfs_size_t b = 0U; b < g_bd->block_count; b++) {
        if (!np_powerbd_block_dirty(g_bd, b)) {
            continue;
        }
        if (g_snap[b] == NULL) {
            np_powerbd_sparse_drop(g_bd, b);
            continue;
        }
        uint8_t *dst = np_powerbd_block_data(g_bd, b);
        if (dst == NULL) {
            /* A sparse block that existed at the snapshot and was since
             * dropped cannot happen — nothing drops blocks mid-attempt. */
            continue;
        }
        memcpy(dst, g_snap[b], g_bd->block_size);
    }
    np_powerbd_dirty_clear(g_bd);
}

/* Run the mutation once from the snapshot, cutting at `cut_at`.
 * Returns true if the processor "stopped" inside it. */
static bool attempt(long cut_at, np_powerbd_tear_t tear)
{
    snapshot_restore();
    g_reboot();
    np_powerbd_power_cycle(g_bd);
    np_powerbd_arm(g_bd, cut_at, tear, &g_resume);

    if (setjmp(g_resume) == 0) {
        g_body();
        np_powerbd_power_cycle(g_bd);
        return false;
    }
    np_powerbd_power_cycle(g_bd);
    return true;
}

np_sweep_result_t np_sweep_run(void (*baseline)(void), void (*mutate)(void),
                               const char *what, np_sweep_verify_fn verify)
{
    np_sweep_result_t r;
    memset(&r, 0, sizeof(r));

    np_powerbd_wipe(g_bd);
    np_powerbd_power_cycle(g_bd);
    g_reboot();
    baseline();
    snapshot_take();
    if (g_snap == NULL) {
        r.violations = 1;         /* host out of memory: report, never pass */
        return r;
    }

    /* The uncut reference run: it establishes how many ops there are to cut. */
    g_body = mutate;
    g_reboot();
    np_powerbd_power_cycle(g_bd);
    np_powerbd_arm(g_bd, NP_POWERBD_NO_CUT, NP_POWERBD_TEAR_NONE, &g_resume);
    if (setjmp(g_resume) == 0) {
        mutate();
    }
    r.ops = np_powerbd_ops_seen(g_bd);

    for (int t = 0; t < (int)NP_POWERBD_TEAR_MODEL_COUNT; t++) {
        for (long cut = 0; cut < r.ops; cut++) {
            bool fired = attempt(cut, (np_powerbd_tear_t)t);
            r.attempts++;
            if (fired) {
                r.cuts++;
            } else {
                r.missed_cuts++;
            }
            g_reboot();
            r.violations += verify(what, cut, (np_powerbd_tear_t)t);
        }
    }
    return r;
}
