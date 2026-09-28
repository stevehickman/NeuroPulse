/*
 * NeurOne Hub Control — Hex-tile identity + element inventory (OI-HEXMAP-02)
 * Document: NP-HEX-ZM-001 Rev 9 §4.1
 * SW item:  SW-02 — IEC 62304 Class B
 *
 * The contract, the block layout and the fail-closed rules are in
 * np_hexmap_inventory.h. This file only implements them.
 */

#include "np_hexmap_inventory.h"
#include "np_module_map.h"
#include "np_pbm_hal.h"
#include "np_pbm_types.h"
#include <string.h>

static bool uid_all(const np_module_uid_t *u, uint8_t v)
{
    for (unsigned i = 0; i < NP_HEXMAP_UID_LEN; i++) {
        if (u->b[i] != v) {
            return false;
        }
    }
    return true;
}

np_hub_status_t np_hexmap_read_ident(uint16_t socket_id, np_hexmap_ident_t *out)
{
    if (out == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (socket_id >= NP_HEXMAP_MAX_SOCKETS) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    uint8_t hdr[NP_HEXMAP_ID_HDR_BYTES];
    if (np_pbm_hal_i2c_read((uint8_t)socket_id, (uint8_t)NP_HEXMAP_ID_REG,
                            hdr, (uint8_t)sizeof(hdr)) != NP_PBM_OK) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    if (hdr[0] != NP_HEXMAP_ID_FORMAT) {
        return NP_HUB_ERR_BAD_VERSION;
    }

    np_hexmap_ident_t id;
    memcpy(id.uid.b, &hdr[1], NP_HEXMAP_UID_LEN);
    id.elem_count = hdr[1 + NP_HEXMAP_UID_LEN];

    /* All-0x00 is the map's "empty socket"; all-0xFF is what a floating bus
     * reads. Neither may be stored as a module. */
    if (uid_all(&id.uid, 0x00u) || uid_all(&id.uid, 0xFFu) ||
        id.elem_count > NP_HEXMAP_MAX_ELEMENTS) {
        return NP_HUB_ERR_BAD_MAGIC;
    }

    *out = id;
    return NP_HUB_OK;
}

np_hub_status_t np_hexmap_inventory_read(uint16_t  socket_id,
                                         void     *ctx,
                                         uint8_t  *types_out,
                                         uint8_t   max,
                                         uint8_t  *count_out)
{
    const np_hexmap_ident_t *want = (const np_hexmap_ident_t *)ctx;
    if (want == NULL || types_out == NULL || count_out == NULL ||
        socket_id >= NP_HEXMAP_MAX_SOCKETS) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (want->elem_count > NP_HEXMAP_MAX_ELEMENTS || want->elem_count > max) {
        return NP_HUB_ERR_CMD_TOO_MANY;
    }

    const size_t body  = (size_t)NP_HEXMAP_ID_HDR_BYTES + want->elem_count;
    const size_t total = body + NP_HEXMAP_ID_CRC_BYTES;
    uint8_t      blk[NP_HEXMAP_ID_MAX_BYTES];

    if (np_pbm_hal_i2c_read((uint8_t)socket_id, (uint8_t)NP_HEXMAP_ID_REG,
                            blk, (uint8_t)total) != NP_PBM_OK) {
        return NP_HUB_ERR_NOT_PRESENT;
    }

    const uint32_t got = (uint32_t)blk[body]               |
                         ((uint32_t)blk[body + 1u] << 8)   |
                         ((uint32_t)blk[body + 2u] << 16)  |
                         ((uint32_t)blk[body + 3u] << 24);
    if (np_hexmap_crc32(blk, body) != got) {
        return NP_HUB_ERR_BAD_MAGIC;
    }
    if (blk[0] != NP_HEXMAP_ID_FORMAT) {
        return NP_HUB_ERR_BAD_VERSION;
    }

    /* Same module, same count, as the header the poll is about to store. */
    if (memcmp(&blk[1], want->uid.b, NP_HEXMAP_UID_LEN) != 0 ||
        blk[1 + NP_HEXMAP_UID_LEN] != want->elem_count) {
        return NP_HUB_ERR_NOT_PRESENT;
    }

    /* A type this image cannot name rejects the whole module. */
    const uint8_t *types = &blk[NP_HEXMAP_ID_HDR_BYTES];
    for (unsigned i = 0; i < want->elem_count; i++) {
        if (types[i] >= (uint8_t)NP_ELEM_TYPE_COUNT) {
            return NP_HUB_ERR_BAD_VERSION;
        }
    }

    memcpy(types_out, types, want->elem_count);
    *count_out = want->elem_count;
    return NP_HUB_OK;
}

np_hub_status_t np_hexmap_poll_socket(uint16_t socket_id, bool *changed_out)
{
    if (changed_out != NULL) {
        *changed_out = false;
    }
    if (socket_id >= NP_HEXMAP_MAX_SOCKETS) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    np_hexmap_ident_t id;
    uint8_t           health = 0u;
    np_hub_status_t   read_rc = np_hexmap_read_ident(socket_id, &id);
    if (read_rc == NP_HUB_OK &&
        np_pbm_hal_i2c_read((uint8_t)socket_id, (uint8_t)NP_HEXMAP_STATUS_REG,
                            &health, 1u) != NP_PBM_OK) {
        read_rc = NP_HUB_ERR_NOT_PRESENT;
    }
    if (read_rc != NP_HUB_OK) {
        /* Fail closed: an unreadable socket is applied as empty, so it never
         * keeps answering from the module that was there before. */
        memset(&id, 0, sizeof(id));
        health = 0u;
    }

    np_hub_status_t rc = np_module_map_apply_poll(socket_id, &id.uid, health,
                                                  np_hexmap_inventory_read, &id,
                                                  changed_out);
    if (rc == NP_HUB_ERR_INVALID_ARG) {
        return rc;          /* not a socket of this helmet */
    }
    return (read_rc != NP_HUB_OK) ? read_rc : rc;
}
