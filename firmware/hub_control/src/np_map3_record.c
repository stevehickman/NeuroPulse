/*
 * np_map3_record.c — the Map 3 journal record codec and version-tolerant scan
 * Document: NP-FW-NVRAM-001 Rev 3 §3.3.1.5, §4.2(b), §7.2 (D-5, D-7, D-25)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_map3_record.h for the row and the version rule.
 */

#include "np_map3_record.h"

#include <string.h>

/* CRC-32 (IEEE 802.3, reflected 0xEDB88320), kept local as np_cfg_store.c
 * keeps its own: np_hub_control does not link np_crypto.  The host test
 * checks every row against np_crypto's np_crc32 as an independent oracle, so
 * the two cannot drift apart unnoticed. */
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

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

size_t np_map3_encode(const np_map3_rec_t *rec, uint8_t *out, size_t cap)
{
    if (rec == NULL || out == NULL || cap < NP_MAP3_V1_BYTES) {
        return 0U;
    }
    memset(out, 0, NP_MAP3_V1_BYTES);
    out[0] = (uint8_t)NP_MAP3_V1_BYTES;
    out[1] = (uint8_t)NP_MAP3_VERSION;
    memcpy(&out[2], rec->uid, NP_MAP3_UID_LEN);
    put_u32(&out[10], rec->seq);
    put_u32(&out[14], rec->ordinal);
    put_u16(&out[18], rec->session_count);
    put_u32(&out[20], rec->dose_accum);
    put_u16(&out[24], rec->throttle_count);
    out[26] = rec->fault_flags;
    out[27] = 0U;   /* reserved: written 0, never reused (§7.2 rule 1) */
    put_u32(&out[NP_MAP3_V1_BYTES - NP_MAP3_CRC_BYTES],
            crc32_le(out, NP_MAP3_V1_BYTES - NP_MAP3_CRC_BYTES));
    return NP_MAP3_V1_BYTES;
}

void np_map3_scan(const uint8_t *buf, size_t len, np_map3_row_fn cb, void *ctx,
                  np_map3_scan_t *out)
{
    np_map3_scan_t s = { 0U, 0U, NP_MAP3_END_CLEAN };
    size_t off = 0U;

    while (buf != NULL && off < len) {
        size_t left = len - off;
        const uint8_t *p = buf + off;

        if (left < NP_MAP3_V1_BYTES) {
            s.end = NP_MAP3_END_TORN;
            break;
        }
        /* Step by the row's own length — never by the length this reader
         * would have written.  A row shorter than version 1 cannot exist. */
        size_t rl = p[0];
        if (rl < NP_MAP3_V1_BYTES) {
            s.end = NP_MAP3_END_BAD_LEN;
            break;
        }
        if (rl > left) {
            /* A length that runs past the image: the tail is torn mid-row. */
            s.end = NP_MAP3_END_TORN;
            break;
        }
        if (get_u32(&p[rl - NP_MAP3_CRC_BYTES]) !=
            crc32_le(p, rl - NP_MAP3_CRC_BYTES)) {
            s.end = NP_MAP3_END_BAD_CRC;
            break;
        }
        if (p[1] == 0U) {
            s.end = NP_MAP3_END_BAD_VER;
            break;
        }

        /* Any ver >= 1 is accepted.  The version-1 fields are at fixed
         * offsets in every version; anything between them and the CRC is a
         * later version's tail, which this reader skips (D-25). */
        np_map3_row_t row;
        memset(&row, 0, sizeof(row));
        memcpy(row.rec.uid, &p[2], NP_MAP3_UID_LEN);
        row.rec.seq            = get_u32(&p[10]);
        row.rec.ordinal        = get_u32(&p[14]);
        row.rec.session_count  = get_u16(&p[18]);
        row.rec.dose_accum     = get_u32(&p[20]);
        row.rec.throttle_count = get_u16(&p[24]);
        row.rec.fault_flags    = p[26];
        row.ver      = p[1];
        row.len      = (uint8_t)rl;
        row.tail_len = (uint8_t)(rl - NP_MAP3_V1_BYTES);
        row.offset   = (uint32_t)off;

        s.rows++;
        s.bytes += (uint32_t)rl;
        off += rl;

        if (cb != NULL && !cb(&row, ctx)) {
            s.end = NP_MAP3_END_STOPPED;
            break;
        }
    }

    if (out != NULL) {
        *out = s;
    }
}
