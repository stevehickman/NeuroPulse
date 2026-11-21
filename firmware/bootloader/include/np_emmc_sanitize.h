/*
 * NeurOne Bootloader — eMMC SANITIZE sequence (OI-PRIV-27, NP-PRIV-ANALYSIS-004 H7)
 * Document: NP-FW-EMMC-002 §B.3 (R-5..R-7a), §B.4
 *
 * ── What SANITIZE is, and what it is not ────────────────────────────────────
 *
 * eMMC SANITIZE is started by CMD6 SWITCH (write-byte access) writing 1 to
 * EXT_CSD byte 165, SANITIZE_START.  It is NOT a CMD38 argument: CMD38 with
 * argument 0 is a plain ERASE, which is what this bootloader issued, alone,
 * until OI-PRIV-27.  The device advertises SANITIZE in EXT_CSD byte 231,
 * SEC_FEATURE_SUPPORT, bit 6 (SEC_SANITIZE).
 *
 *   Sources: Linux v6.12 include/linux/mmc/mmc.h (EXT_CSD_SANITIZE_START 165,
 *   EXT_CSD_SEC_FEATURE_SUPPORT 231, EXT_CSD_SEC_SANITIZE BIT(6), the R1 bits
 *   below) and drivers/mmc/core/mmc_ops.c mmc_sanitize(), which calls
 *   __mmc_switch(card, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_SANITIZE_START, 1, ...).
 *   The JEDEC JESD84-B51 wording and a vendor datasheet for the selected part
 *   are still to be read against these figures (OI-PRIV-27, NP-PRIV-ANALYSIS-004).
 *
 * SANITIZE purges the device's UNMAPPED user address space, the whole device
 * and not an LBA range.  So the caller first unmaps what it wants purged
 * (the ERASE of each data partition), and runs SANITIZE after the last of
 * them.  It also purges the stale copies an overwrite left behind in
 * wear-levelled blocks, which is how Config is purged rather than only zeroed.
 *
 * ── Why this is a header of pure functions ──────────────────────────────────
 *
 * np_emmc.c drives USDHC2 registers and cannot run on a host.  The decisions a
 * silent error would hide in are pure, and live here: the CMD6 argument, the
 * support bit, the R1 status decode, and the order and failure handling of the
 * sequence.  np_emmc_sanitize() in np_emmc.c binds the sequence to the real
 * CMD8 / CMD6 / CMD13 transport; the host tests bind it to a recorder.
 *
 * A SANITIZE that is unsupported, refused, reports an error, or does not finish
 * within its bound returns an error.  The caller then leaves the reset in
 * progress, so the next boot runs it again (NP-FW-EMMC-002 §B.4).  There is no
 * fallback to the plain erase: that would report a purge that did not happen.
 */
#ifndef NP_EMMC_SANITIZE_H
#define NP_EMMC_SANITIZE_H

#include <stdbool.h>
#include <stdint.h>
#include "np_types.h"

/* ── EXT_CSD fields ──────────────────────────────────────────────────────── */
#define NP_EMMC_EXT_CSD_SIZE                512U
#define NP_EMMC_EXTCSD_SANITIZE_START       165U    /* W: write 1 to start   */
#define NP_EMMC_EXTCSD_SEC_FEATURE_SUPPORT  231U    /* RO                    */
#define NP_EMMC_SEC_SANITIZE                (1U << 6) /* in byte 231         */

/* ── R1 card status (CMD13) ──────────────────────────────────────────────── */
#define NP_EMMC_R1_READY_FOR_DATA   (1UL << 8)
#define NP_EMMC_R1_SWITCH_ERROR     (1UL << 7)
#define NP_EMMC_R1_STATE_SHIFT      9U
#define NP_EMMC_R1_STATE_MASK       0xFUL
#define NP_EMMC_R1_STATE_TRAN       4UL
#define NP_EMMC_R1_STATE_PRG        7UL
/* Linux R1_STATUS() error bits (31..19, 16, 15, 13) and SWITCH_ERROR. */
#define NP_EMMC_R1_ERROR_MASK       (0xFFF9A000UL | NP_EMMC_R1_SWITCH_ERROR)

/* ── Bound on the wait (UC-201, PROVISIONAL) ─────────────────────────────── */
/* The time SANITIZE may take is the selected part's figure, and no part is
 * selected (OI-EMMC2-03, FAI-RESET-01).  240 s is the Linux default when the
 * caller gives none (mmc_ops.c MMC_SANITIZE_TIMEOUT_MS), not this part's.  The
 * poll count is derived from it and moves with it. */
#define NP_EMMC_SANITIZE_TIMEOUT_MS 240000UL    /* PROVISIONAL: UC-201 */
#define NP_EMMC_SANITIZE_POLL_MS    10UL
static inline uint32_t np_emmc_sanitize_max_polls(uint32_t timeout_ms, uint32_t poll_ms)
{
    return (poll_ms == 0U) ? 0U : (timeout_ms + poll_ms - 1U) / poll_ms;  /* round up */
}

/* CMD6 SWITCH argument: [25:24]=3 (write byte), [23:16]=index, [15:8]=value. */
static inline uint32_t np_emmc_switch_arg(uint8_t index, uint8_t value)
{
    return (3UL << 24U) | ((uint32_t)index << 16U) | ((uint32_t)value << 8U);
}

static inline bool np_emmc_sanitize_supported(const uint8_t ext_csd[NP_EMMC_EXT_CSD_SIZE])
{
    return (ext_csd[NP_EMMC_EXTCSD_SEC_FEATURE_SUPPORT] & NP_EMMC_SEC_SANITIZE) != 0U;
}

static inline uint32_t np_emmc_r1_state(uint32_t r1)
{
    return (r1 >> NP_EMMC_R1_STATE_SHIFT) & NP_EMMC_R1_STATE_MASK;
}

/* ── The sequence ────────────────────────────────────────────────────────── */
typedef struct {
    /* CMD8 SEND_EXT_CSD: one 512-byte block. */
    np_status_t (*read_ext_csd)(void *ctx, uint8_t ext_csd[NP_EMMC_EXT_CSD_SIZE]);
    /* CMD6 SWITCH with this argument; returns when the command completes. */
    np_status_t (*switch_cmd)(void *ctx, uint32_t cmd6_arg);
    /* CMD13 SEND_STATUS; the R1 word in *r1. */
    np_status_t (*send_status)(void *ctx, uint32_t *r1);
    /* Wait one poll interval. */
    void        (*wait_poll)(void *ctx);
    void        *ctx;
} np_emmc_sanitize_ops_t;

/* Run SANITIZE to completion.  NP_OK only when the device advertised it,
 * accepted the switch, and returned to TRAN, ready for data, with no error
 * bit, within max_polls status reads.  A status read that fails on the bus is
 * counted and polled again (the card may be busy on DAT0); any other state
 * than PRG or TRAN is an error. */
static inline np_status_t np_emmc_sanitize_run(const np_emmc_sanitize_ops_t *ops,
                                               uint32_t max_polls)
{
    uint8_t ext_csd[NP_EMMC_EXT_CSD_SIZE];
    np_status_t ret;

    if (ops == 0 || ops->read_ext_csd == 0 || ops->switch_cmd == 0 ||
        ops->send_status == 0 || ops->wait_poll == 0 || max_polls == 0U) {
        return NP_ERR_INVALID_ARG;
    }

    ret = ops->read_ext_csd(ops->ctx, ext_csd);
    if (ret != NP_OK) {
        return ret;
    }
    if (!np_emmc_sanitize_supported(ext_csd)) {
        return NP_ERR_EMMC_SANITIZE_UNSUPPORTED;
    }

    ret = ops->switch_cmd(ops->ctx,
                          np_emmc_switch_arg((uint8_t)NP_EMMC_EXTCSD_SANITIZE_START, 1U));
    if (ret != NP_OK) {
        return ret;
    }

    for (uint32_t i = 0U; i < max_polls; i++) {
        uint32_t r1 = 0U;
        ops->wait_poll(ops->ctx);
        if (ops->send_status(ops->ctx, &r1) != NP_OK) {
            continue;
        }
        if ((r1 & NP_EMMC_R1_ERROR_MASK) != 0U) {
            return NP_ERR_EMMC_WRITE;
        }
        uint32_t state = np_emmc_r1_state(r1);
        if (state == NP_EMMC_R1_STATE_TRAN && (r1 & NP_EMMC_R1_READY_FOR_DATA) != 0U) {
            return NP_OK;
        }
        if (state != NP_EMMC_R1_STATE_PRG && state != NP_EMMC_R1_STATE_TRAN) {
            return NP_ERR_EMMC_WRITE;
        }
    }
    return NP_ERR_TIMEOUT;
}

#endif /* NP_EMMC_SANITIZE_H */
