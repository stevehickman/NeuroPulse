/*
 * NeurOne Bootloader — where the rollback floor lives in the RPMB, and its record
 * OI-PRIV-32 (NP-PRIV-ANALYSIS-004); REQ-SW02-16 (NP-SW-001 §5.2.2)
 *
 * ── Where ───────────────────────────────────────────────────────────────────
 *
 * RPMB half-sector (256-byte unit) address 0, the record at byte 0 of its data.
 * Why address 0: it is the one address every RPMB has. The RPMB's size is
 * 128 KiB × RPMB_SIZE_MULT (EXT_CSD[168]), and a device with an RPMB reports
 * at least 1, so addresses 0 to 511 always exist; any other fixed address would
 * rest on a figure of a part that is not selected (OI-EMMC2-03). The floor is
 * the only thing the design keeps in the RPMB, so nothing else claims it. If a
 * second RPMB record is ever added it takes another address and this one stays.
 * Why byte 0: the record is 12 bytes and the frame carries the whole 256-byte
 * half-sector authenticated, so its position inside it buys nothing either way.
 * (The RPMB_SIZE_MULT minimum is part of OI-SW02-02's check of JESD84-B51.)
 *
 * ── The record (big-endian, the rest of the 256 bytes zero) ─────────────────
 *
 *   [0..3]   magic  0x4E505246 "NPRF"
 *   [4..5]   format 1
 *   [6..7]   0
 *   [8..11]  floor: the signed firmware version (np_image_header_t.version)
 *   [12..255] 0
 *
 * A half-sector that is not exactly such a record — never written (an RPMB
 * whose key is programmed but which holds no floor), or written by something
 * else — is an error, never floor 0, so it refuses every image like a blank or
 * wrong-key RPMB. The bootloader therefore never writes the first record: it
 * raises an existing floor only. The first record is written once, at
 * manufacture, after the key is programmed, by the factory step that programs
 * the key (encoded with np_rollback_rpmb_encode()).
 */
#ifndef NP_ROLLBACK_RPMB_H
#define NP_ROLLBACK_RPMB_H

#include <stdbool.h>
#include <stdint.h>
#include "np_emmc_rpmb.h"

#define NP_ROLLBACK_RPMB_ADDRESS    0U
#define NP_ROLLBACK_RPMB_MAGIC      0x4E505246UL   /* "NPRF" */
#define NP_ROLLBACK_RPMB_FORMAT     1U
#define NP_ROLLBACK_RPMB_OFF_MAGIC  0U
#define NP_ROLLBACK_RPMB_OFF_FORMAT 4U
#define NP_ROLLBACK_RPMB_OFF_FLOOR  8U
#define NP_ROLLBACK_RPMB_RECORD_LEN 12U

static inline void np_rollback_rpmb_encode(uint32_t floor, uint8_t data[NP_RPMB_DATA_SIZE])
{
    for (uint32_t i = 0U; i < NP_RPMB_DATA_SIZE; i++) {
        data[i] = 0U;
    }
    np_rpmb_put32(data, NP_ROLLBACK_RPMB_OFF_MAGIC, (uint32_t)NP_ROLLBACK_RPMB_MAGIC);
    np_rpmb_put16(data, NP_ROLLBACK_RPMB_OFF_FORMAT, (uint16_t)NP_ROLLBACK_RPMB_FORMAT);
    np_rpmb_put32(data, NP_ROLLBACK_RPMB_OFF_FLOOR, floor);
}

/* True, with *floor set, only for an exact record: magic, format, and every
 * byte that is not the floor zero. */
static inline bool np_rollback_rpmb_decode(const uint8_t data[NP_RPMB_DATA_SIZE], uint32_t *floor)
{
    if (np_rpmb_get32(data, NP_ROLLBACK_RPMB_OFF_MAGIC) != (uint32_t)NP_ROLLBACK_RPMB_MAGIC ||
        np_rpmb_get16(data, NP_ROLLBACK_RPMB_OFF_FORMAT) != (uint16_t)NP_ROLLBACK_RPMB_FORMAT ||
        data[6] != 0U || data[7] != 0U) {
        return false;
    }
    for (uint32_t i = NP_ROLLBACK_RPMB_RECORD_LEN; i < NP_RPMB_DATA_SIZE; i++) {
        if (data[i] != 0U) {
            return false;
        }
    }
    *floor = np_rpmb_get32(data, NP_ROLLBACK_RPMB_OFF_FLOOR);
    return true;
}

#endif /* NP_ROLLBACK_RPMB_H */
