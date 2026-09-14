/*
 * np_lfs_instance.c — EMMC-FS-01 Config-partition littlefs instance
 * Document: NP-SOUP-LFS-001 Rev 2 §7.3, NP-FW-EMMC-001 EMMC-FS-01
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_lfs_instance.h for why this file exists: littlefs checks that a
 * configuration is self-consistent, and nothing checked that it was NeurOne's.
 */

#include "np_lfs_instance.h"

#if defined(NPTEST_HOST)
#include <stdio.h>
#include <stdlib.h>
#else
#include "FreeRTOS.h"
#include "task.h"
#endif

/* ── Static buffers (LFS_NO_MALLOC) ───────────────────────────────────────────
 * Not in the FreeRTOS heap and not on any task stack: these live for as long as
 * the instance is mounted, which is the life of the program.
 */
static uint8_t s_read_buffer[NP_LFS_CFG_CACHE_SIZE];
static uint8_t s_prog_buffer[NP_LFS_CFG_CACHE_SIZE];
static uint8_t s_lookahead_buffer[NP_LFS_CFG_LOOKAHEAD_SIZE];

/* The three sizes must satisfy littlefs's own relations.  Asserted at COMPILE
 * time as well as at mount, because a build that cannot produce a mountable
 * configuration should not produce a binary. */
_Static_assert(NP_LFS_CFG_CACHE_SIZE % NP_LFS_CFG_READ_SIZE == 0,
               "cache_size must be a multiple of read_size (lfs_init)");
_Static_assert(NP_LFS_CFG_CACHE_SIZE % NP_LFS_CFG_PROG_SIZE == 0,
               "cache_size must be a multiple of prog_size (lfs_init)");
_Static_assert(NP_LFS_CFG_BLOCK_SIZE % NP_LFS_CFG_CACHE_SIZE == 0,
               "block_size must be a multiple of cache_size (lfs_init)");
_Static_assert(NP_LFS_CFG_BLOCK_SIZE >= 128,
               "block_size must fit all ctz pointers (lfs_init)");
_Static_assert(NP_LFS_CFG_BLOCK_CYCLES != 0,
               "block_cycles = 0 is rejected by lfs_init");
_Static_assert(NP_LFS_CFG_BLOCK_CYCLES != -1,
               "block_cycles = -1 disables wear levelling — claim L-6");
_Static_assert(8u * NP_LFS_CFG_LOOKAHEAD_SIZE >= NP_LFS_CFG_BLOCK_COUNT,
               "lookahead must cover the whole partition in one pass");
_Static_assert((uint64_t)NP_LFS_CFG_BLOCK_SIZE * NP_LFS_CFG_BLOCK_COUNT
                   == 16u * 1024u * 1024u,
               "EMMC-FS-01: the instance is the whole 16 MiB Config partition");

/* ── LFS_ASSERT target (np_lfs_config.h) ──────────────────────────────────────
 * Same fault philosophy as np_freertos_assert_failed(): halting the main
 * processor stops the SPI heartbeat, and the STM32G071 safety MCU cuts all
 * stimulation within <50 ms with hardware this processor does not own.
 */
void np_lfs_assert_failed(const char *file, unsigned long line)
{
#if defined(NPTEST_HOST)
    fprintf(stderr, "littlefs LFS_ASSERT failed: %s:%lu\n",
            (file != NULL) ? file : "?", line);
    fflush(stderr);
    abort();
#else
    (void)file;
    (void)line;
    taskDISABLE_INTERRUPTS();
    for (;;) {
        /* Halt.  Safety MCU watchdog cuts all stimulation on heartbeat loss. */
    }
#endif
}

np_hub_status_t np_lfs_config_apply(struct lfs_config *cfg)
{
    if (cfg == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    cfg->read_size      = NP_LFS_CFG_READ_SIZE;
    cfg->prog_size      = NP_LFS_CFG_PROG_SIZE;
    cfg->block_size     = NP_LFS_CFG_BLOCK_SIZE;
    cfg->block_count    = NP_LFS_CFG_BLOCK_COUNT;
    cfg->block_cycles   = NP_LFS_CFG_BLOCK_CYCLES;
    cfg->cache_size     = NP_LFS_CFG_CACHE_SIZE;
    cfg->lookahead_size = NP_LFS_CFG_LOOKAHEAD_SIZE;
    cfg->file_max       = NP_LFS_CFG_FILE_MAX;

    cfg->read_buffer      = s_read_buffer;
    cfg->prog_buffer      = s_prog_buffer;
    cfg->lookahead_buffer = s_lookahead_buffer;

    return NP_HUB_OK;
}

np_hub_status_t np_lfs_config_validate(const struct lfs_config *cfg)
{
    if (cfg == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* 1. The five EMMC-FS-01 owns — claim L-5. */
    if (cfg->read_size   != NP_LFS_CFG_READ_SIZE   ||
        cfg->prog_size   != NP_LFS_CFG_PROG_SIZE   ||
        cfg->block_size  != NP_LFS_CFG_BLOCK_SIZE  ||
        cfg->block_count != NP_LFS_CFG_BLOCK_COUNT ||
        cfg->file_max    != NP_LFS_CFG_FILE_MAX) {
        return NP_HUB_ERR_BAD_VERSION;
    }

    /* 2. The four NeurOne decides.  block_cycles is checked by VALUE, not for
     *    "not zero": -1 passes a not-zero test and silently disables wear
     *    levelling, which is the failure claim L-6 would not survive. */
    if (cfg->cache_size     != NP_LFS_CFG_CACHE_SIZE     ||
        cfg->lookahead_size != NP_LFS_CFG_LOOKAHEAD_SIZE ||
        cfg->block_cycles   != NP_LFS_CFG_BLOCK_CYCLES) {
        return NP_HUB_ERR_BAD_VERSION;
    }

    /* 3. LFS_NO_MALLOC: every buffer must be supplied.  Without this the mount
     *    fails later with LFS_ERR_NOMEM, which reads like a resource problem
     *    rather than a configuration one. */
    if (cfg->read_buffer      == NULL ||
        cfg->prog_buffer      == NULL ||
        cfg->lookahead_buffer == NULL) {
        return NP_HUB_ERR_BAD_VERSION;
    }

    /* 4. The block-device glue (OI-LOG-05..07) is bound. */
    if (cfg->read  == NULL || cfg->prog == NULL ||
        cfg->erase == NULL || cfg->sync == NULL) {
        return NP_HUB_ERR_BAD_VERSION;
    }

#ifdef LFS_THREADSAFE
    /* 5. With LFS_THREADSAFE a NULL lock is not an error path — lfs_mount()
     *    calls it immediately.  This check is the difference between a refused
     *    mount and a hard fault. */
    if (cfg->lock == NULL || cfg->unlock == NULL) {
        return NP_HUB_ERR_BAD_VERSION;
    }
#endif

    return NP_HUB_OK;
}
