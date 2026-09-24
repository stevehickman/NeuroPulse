/*
 * np_session_count.c — the persisted device session count (OI-LFS-12)
 * Document: NP-SOUP-LFS-001 Rev 9 §13.13; NP-FW-EMMC-001 EMMC-SHDR-09
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_session_count.h.  Stored as 4 bytes, little-endian, so the record
 * does not depend on the host's byte order.
 */

#include "np_session_count.h"

#include <stddef.h>

#include "np_cfg_store.h"

#define NP_SESSION_COUNT_BYTES 4U

np_hub_status_t np_session_count_load(uint32_t *out)
{
    uint8_t buf[NP_SESSION_COUNT_BYTES];

    if (out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    *out = 0U;
    np_hub_status_t st = np_cfg_store_replicated_read(NP_CFG_FILE_SESSION_COUNT,
                                                      buf, sizeof(buf));
    if (st != NP_HUB_OK) {
        return st;
    }
    *out = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    return NP_HUB_OK;
}

np_hub_status_t np_session_count_commit(uint32_t count)
{
    const uint8_t buf[NP_SESSION_COUNT_BYTES] = {
        (uint8_t)(count & 0xFFU),
        (uint8_t)((count >> 8) & 0xFFU),
        (uint8_t)((count >> 16) & 0xFFU),
        (uint8_t)((count >> 24) & 0xFFU),
    };
    return np_cfg_store_replicated_write(NP_CFG_FILE_SESSION_COUNT, buf,
                                         sizeof(buf));
}
