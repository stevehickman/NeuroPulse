/*
 * NeurOne Bootloader — eMMC Replay Protected Memory Block (RPMB) frame protocol
 * OI-PRIV-32 (NP-PRIV-ANALYSIS-004); requirement REQ-SW02-16 (NP-SW-001 §5.2.2)
 *
 * ── Sources, and what was and was not checked ───────────────────────────────
 *
 * The frame layout, the message types and the result codes are JEDEC
 * JESD84-B51 (eMMC 5.1) §6.6.22, "Replay Protected Memory Block": Table 17
 * (data frame fields), Table 18 (request/response message types) and Table 20
 * (operation results). THOSE CLAUSE AND TABLE NUMBERS ARE TAKEN FROM SECONDARY
 * SOURCES, NOT FROM THE STANDARD, which was not available to this change:
 *   - the armored-witness-os `rpmb` package (v0.4.3), whose declarations carry
 *     the captions "Table 17 — Data Frame Files for RPMB", "Table 18 — RPMB
 *     Request/Response Message Types" and "Table 20 — RPMB Operation Results",
 *     each "JESD84-B51";
 *   - the virtio-rpmb specification draft, which cites JESD84-B51 §6.6.22 for
 *     the frame definition (seen in a search result; the page was not reachable).
 * The field sizes, offsets, codes and the MAC span below were checked against
 * three independent implementations that agree with each other:
 *   - Linux v6.12 include/linux/mmc/mmc.h: EXT_CSD_PART_CONFIG_ACC_RPMB 0x3,
 *     EXT_CSD_RPMB_MULT 168, EXT_CSD_REL_WR_SEC_C 222;
 *   - U-Boot v2024.10 drivers/mmc/rpmb.c (written against JESD84-A441): the
 *     frame struct, the request and response codes, the result codes, the MAC
 *     as HMAC-SHA256 over the 284 bytes from `data` to the end of the frame,
 *     CMD23 with bit 31 set (reliable write) before an authenticated write, and
 *     CMD25 / CMD18 for one 512-byte frame;
 *   - armored-witness-os `rpmb`, as above.
 * Checking every figure here against JESD84-B51 itself is OI-SW02-02 (NP-SW-001).
 *
 * ── Why this is a module apart from the register driver ─────────────────────
 *
 * np_emmc.c drives USDHC2 registers and cannot run on a host (the SANITIZE
 * precedent, np_emmc_sanitize.h). Every decision a forged or replayed frame
 * would hide in — the MAC, the nonce, the counter, the result, the response
 * type, the address — is made here, against a two-function transport
 * (np_rpmb_ops_t). np_emmc_rpmb_ops() binds it to CMD23 / CMD25 / CMD18 on the
 * real device; the host tests bind it to a fake RPMB device.
 *
 * ── What a caller may rely on ───────────────────────────────────────────────
 *
 * Each function returns NP_OK only when the device's response frame:
 *   - is the response type that answers the request;
 *   - carries result 0x0000 (any other value, including the write-counter-
 *     expired bit 0x0080 and 0x0007 "authentication key not yet programmed",
 *     is an error);
 *   - carries a MAC, computed under `key`, that matches (compared in constant
 *     time);
 *   - echoes the request's nonce (counter read, data read), so a response
 *     recorded earlier and replayed is refused, and the request's address;
 *   - for an authenticated write, reports the write counter one above the
 *     counter the request carried, so a write the device did not commit is not
 *     reported as done (the CVE-2020-13799 check).
 * A failed MAC primitive, a failed nonce source or a transport error is an
 * error. No function invents a value on any error.
 */
#ifndef NP_EMMC_RPMB_H
#define NP_EMMC_RPMB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "np_types.h"

/* ── EXT_CSD and partition access (Linux v6.12 mmc.h) ────────────────────── */
#define NP_EMMC_RPMB_ACCESS         3U      /* PARTITION_CONFIG[2:0] = RPMB   */

/* ── Data frame (JESD84-B51 Table 17, via the sources above) ─────────────── */
/* 512 bytes; multi-byte fields are big-endian.                               */
#define NP_RPMB_FRAME_SIZE          512U
#define NP_RPMB_OFF_STUFF           0U      /* 196 bytes                      */
#define NP_RPMB_OFF_MAC             196U    /* 32: key (programming) or MAC   */
#define NP_RPMB_OFF_DATA            228U    /* 256                            */
#define NP_RPMB_OFF_NONCE           484U    /* 16                             */
#define NP_RPMB_OFF_COUNTER         500U    /* 4                              */
#define NP_RPMB_OFF_ADDRESS         504U    /* 2: half-sector (256 B) address */
#define NP_RPMB_OFF_BLOCK_COUNT     506U    /* 2                              */
#define NP_RPMB_OFF_RESULT          508U    /* 2                              */
#define NP_RPMB_OFF_REQ_RESP        510U    /* 2                              */
#define NP_RPMB_KEY_SIZE            32U
#define NP_RPMB_MAC_SIZE            32U
#define NP_RPMB_DATA_SIZE           256U
#define NP_RPMB_NONCE_SIZE          16U
/* The MAC covers the frame from the data field to its end: 512 - 228 = 284. */
#define NP_RPMB_MAC_SPAN            (NP_RPMB_FRAME_SIZE - NP_RPMB_OFF_DATA)

/* ── Message types (JESD84-B51 Table 18) ─────────────────────────────────── */
#define NP_RPMB_REQ_KEY_PROGRAM     0x0001U
#define NP_RPMB_REQ_COUNTER_READ    0x0002U
#define NP_RPMB_REQ_DATA_WRITE      0x0003U
#define NP_RPMB_REQ_DATA_READ       0x0004U
#define NP_RPMB_REQ_RESULT_READ     0x0005U
/* A response type is its request type shifted into the high byte.           */
#define NP_RPMB_RESP_KEY_PROGRAM    0x0100U
#define NP_RPMB_RESP_COUNTER_READ   0x0200U
#define NP_RPMB_RESP_DATA_WRITE     0x0300U
#define NP_RPMB_RESP_DATA_READ      0x0400U

/* ── Operation results (JESD84-B51 Table 20) ─────────────────────────────── */
#define NP_RPMB_RESULT_OK               0x0000U
#define NP_RPMB_RESULT_GENERAL_FAILURE  0x0001U
#define NP_RPMB_RESULT_AUTH_FAILURE     0x0002U
#define NP_RPMB_RESULT_COUNTER_FAILURE  0x0003U
#define NP_RPMB_RESULT_ADDRESS_FAILURE  0x0004U
#define NP_RPMB_RESULT_WRITE_FAILURE    0x0005U
#define NP_RPMB_RESULT_READ_FAILURE     0x0006U
#define NP_RPMB_RESULT_KEY_NOT_PROGRAMMED 0x0007U
#define NP_RPMB_RESULT_COUNTER_EXPIRED  0x0080U  /* a flag, ORed with the above */

/* ── Big-endian field access ─────────────────────────────────────────────── */
static inline uint16_t np_rpmb_get16(const uint8_t *f, uint32_t off)
{
    return (uint16_t)(((uint16_t)f[off] << 8U) | f[off + 1U]);
}
static inline void np_rpmb_put16(uint8_t *f, uint32_t off, uint16_t v)
{
    f[off] = (uint8_t)(v >> 8U);
    f[off + 1U] = (uint8_t)v;
}
static inline uint32_t np_rpmb_get32(const uint8_t *f, uint32_t off)
{
    return ((uint32_t)f[off] << 24U) | ((uint32_t)f[off + 1U] << 16U) |
           ((uint32_t)f[off + 2U] << 8U) | (uint32_t)f[off + 3U];
}
static inline void np_rpmb_put32(uint8_t *f, uint32_t off, uint32_t v)
{
    f[off] = (uint8_t)(v >> 24U);
    f[off + 1U] = (uint8_t)(v >> 16U);
    f[off + 2U] = (uint8_t)(v >> 8U);
    f[off + 3U] = (uint8_t)v;
}

/* ── Transport ───────────────────────────────────────────────────────────── */
/* Called with the RPMB partition selected. Each moves exactly one frame.     */
typedef struct {
    /* CMD23 SET_BLOCK_COUNT(1, bit 31 = `reliable`), then CMD25 with one
     * 512-byte frame. `reliable` is true for an authenticated write.         */
    np_status_t (*write_frame)(void *ctx, const uint8_t frame[NP_RPMB_FRAME_SIZE],
                               bool reliable);
    /* CMD23 SET_BLOCK_COUNT(1), then CMD18 for one 512-byte frame.          */
    np_status_t (*read_frame)(void *ctx, uint8_t frame[NP_RPMB_FRAME_SIZE]);
    void *ctx;
} np_rpmb_ops_t;

/* Authenticated read of the write counter (request 0x0002).                  */
np_status_t np_rpmb_read_counter(const np_rpmb_ops_t *ops,
                                 const uint8_t key[NP_RPMB_KEY_SIZE],
                                 uint32_t *counter);

/* Authenticated data read of one 256-byte half-sector (request 0x0004).      */
np_status_t np_rpmb_read_block(const np_rpmb_ops_t *ops,
                               const uint8_t key[NP_RPMB_KEY_SIZE],
                               uint16_t address,
                               uint8_t data[NP_RPMB_DATA_SIZE]);

/* Authenticated data write of one 256-byte half-sector (request 0x0003):
 * reads the counter, writes with it under a reliable write, then reads the
 * result (request 0x0005) and checks it as described above.                  */
np_status_t np_rpmb_write_block(const np_rpmb_ops_t *ops,
                                const uint8_t key[NP_RPMB_KEY_SIZE],
                                uint16_t address,
                                const uint8_t data[NP_RPMB_DATA_SIZE]);

/* Constant-time comparison: true when the n bytes are equal.                */
bool np_rpmb_ct_equal(const uint8_t *a, const uint8_t *b, size_t n);

/* Overwrite n bytes in a way the compiler may not elide (key copies).       */
void np_rpmb_wipe(void *p, size_t n);

/* The real device's transport (np_emmc.c, USDHC2). Not built for host tests. */
const np_rpmb_ops_t *np_emmc_rpmb_ops(void);

#endif /* NP_EMMC_RPMB_H */
