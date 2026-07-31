/*
 * NeurOne Zone Module Notification — frame encoders
 * See np_zone_notify.h for the format and the static/dynamic split rationale.
 */

#include "np_zone_notify.h"
#include "np_module_map.h"
#include <string.h>

/* ── Pin the wire domains to the addressing domains they mirror ───────────────
 * The wire is 1-based over a 0-based firmware domain, so the largest 1-based id
 * equals NP_HEXMAP_MAX_SOCKETS exactly. If the socket domain is ever widened,
 * this fails the build rather than silently truncating ids into a uint8_t. */
typedef char _np_zn_socket_domain_matches[
    (NP_ZN_MAX_SOCKET_ID == NP_HEXMAP_MAX_SOCKETS) ? 1 : -1];
typedef char _np_zn_socket_fits_u8[(NP_ZN_MAX_SOCKET_ID <= 255) ? 1 : -1];

/* The map record packs lobe in 3 bits and side in 2. Both enums are duplicated
 * in np_zone_notify.h so this unit host-compiles standalone; assert they agree
 * with np_module_map.h rather than trusting two hand-kept copies. */
typedef char _np_zn_lobe_agrees[
    ((int)NP_ZN_LOBE_FRONTAL   == (int)NP_LOBE_FRONTAL   &&
     (int)NP_ZN_LOBE_TEMPORAL  == (int)NP_LOBE_TEMPORAL  &&
     (int)NP_ZN_LOBE_PARIETAL  == (int)NP_LOBE_PARIETAL  &&
     (int)NP_ZN_LOBE_OCCIPITAL == (int)NP_LOBE_OCCIPITAL &&
     (int)NP_LOBE_COUNT <= 8) ? 1 : -1];
typedef char _np_zn_side_agrees[
    ((int)NP_ZN_SIDE_MIDLINE == (int)NP_SIDE_MIDLINE &&
     (int)NP_ZN_SIDE_LEFT    == (int)NP_SIDE_LEFT    &&
     (int)NP_ZN_SIDE_RIGHT   == (int)NP_SIDE_RIGHT   &&
     (int)NP_SIDE_RIGHT <= 3) ? 1 : -1];

/* The minimum frame must admit the LARGER of the two record kinds, or a map
 * fragment could never be emitted at the floor. */
typedef char _np_zn_min_frame_admits_map[
    (NP_ZN_MIN_FRAME_BYTES >= NP_ZN_HEADER_BYTES + NP_ZN_MAP_REC_BYTES) ? 1 : -1];

/* ── Module state ─────────────────────────────────────────────────────────── */

static np_zn_tx_fn s_tx;
static void       *s_tx_ctx;
static uint8_t     s_max_frame;

/* One fragment at a time; the transmit callback consumes each before the next is
 * built, so a single buffer suffices and nothing is heap-allocated. */
static uint8_t     s_frame[255];

/* Records of the given size that fit in one fragment after the header. */
static uint8_t records_per_fragment(uint8_t rec_bytes)
{
    return (uint8_t)((s_max_frame - NP_ZN_HEADER_BYTES) / rec_bytes);
}

/* 0-based firmware id → 1-based wire id, or a range error. Out-of-domain ids are
 * REJECTED rather than masked: masking would alias socket 128 onto socket 0,
 * which addresses PBM and tES elements — a wrong-site path, not a display bug. */
static np_zn_status_t wire_socket_id(uint8_t firmware_id, uint8_t *out)
{
    if (firmware_id >= NP_ZN_MAX_SOCKET_ID) {
        return NP_ZN_ERR_SOCKET_RANGE;
    }
    *out = (uint8_t)(firmware_id + 1u);
    return NP_ZN_OK;
}

np_zn_status_t np_zone_notify_encode_status(const np_zn_socket_state_t *state,
                                            uint8_t *buf)
{
    if (!state || !buf) {
        return NP_ZN_ERR_INVALID_ARG;
    }
    uint8_t wire_id;
    np_zn_status_t rc = wire_socket_id(state->socket_id, &wire_id);
    if (rc != NP_ZN_OK) {
        return rc;
    }

    /* A faulted module is never advertised as present: the app gates
     * safety-critical placement checks on presence. */
    const bool present = state->present && !state->fault;

    uint8_t flags = 0u;
    if (present)      { flags |= NP_ZN_REC_PRESENT; }
    if (state->fault) { flags |= NP_ZN_REC_FAULT;   }

    uint8_t type;
    if (state->fault) {
        type = (uint8_t)NP_ZN_MODULE_UNKNOWN;   /* seated but unidentified */
    } else if (present) {
        type = (uint8_t)state->module_type;
    } else {
        type = (uint8_t)NP_ZN_MODULE_NONE;      /* empty socket            */
    }

    /* Three bytes. No anatomy — the app already has it from the socket map, and
     * a socket's position cannot change when a module is swapped. */
    buf[0] = wire_id;
    buf[1] = type;
    buf[2] = flags;
    return NP_ZN_OK;
}

np_zn_status_t np_zone_notify_encode_map(const np_zn_socket_desc_t *desc,
                                         uint8_t *buf)
{
    if (!desc || !buf) {
        return NP_ZN_ERR_INVALID_ARG;
    }
    uint8_t wire_id;
    np_zn_status_t rc = wire_socket_id(desc->socket_id, &wire_id);
    if (rc != NP_ZN_OK) {
        return rc;
    }

    buf[0] = wire_id;
    buf[1] = (uint8_t)(((uint8_t)desc->lobe & 0x07u) |
                       (((uint8_t)desc->side & 0x03u) << 3));
    buf[2] = (uint8_t)(desc->wired ? NP_ZN_MAP_WIRED : 0u);
    /* int16 little-endian, matching every other multi-byte field on this link. */
    buf[3] = (uint8_t)((uint16_t)desc->x_mm & 0xFFu);
    buf[4] = (uint8_t)(((uint16_t)desc->x_mm >> 8) & 0xFFu);
    buf[5] = (uint8_t)((uint16_t)desc->y_mm & 0xFFu);
    buf[6] = (uint8_t)(((uint16_t)desc->y_mm >> 8) & 0xFFu);
    return NP_ZN_OK;
}

np_zn_status_t np_zone_notify_init(np_zn_tx_fn tx, void *tx_ctx,
                                   uint8_t max_frame_bytes)
{
    if (!tx) {
        return NP_ZN_ERR_INVALID_ARG;
    }
    if (max_frame_bytes < NP_ZN_MIN_FRAME_BYTES) {
        return NP_ZN_ERR_FRAME_TOO_SMALL;
    }
    s_tx        = tx;
    s_tx_ctx    = tx_ctx;
    s_max_frame = max_frame_bytes;
    return NP_ZN_OK;
}

/* Build and transmit one fragment of either kind. */
static np_zn_status_t emit_fragment(const void *records, uint8_t n,
                                    uint8_t rec_bytes, uint8_t frag_index,
                                    bool snapshot, bool last, bool is_map)
{
    uint8_t flags = 0u;
    if (snapshot) { flags |= NP_ZN_FLAG_SNAPSHOT; }
    if (last)     { flags |= NP_ZN_FLAG_LAST;     }
    if (is_map)   { flags |= NP_ZN_FLAG_MAP;      }

    s_frame[0] = NP_ZN_FORMAT_VERSION;
    s_frame[1] = flags;
    s_frame[2] = frag_index;
    s_frame[3] = n;

    for (uint8_t i = 0u; i < n; i++) {
        uint8_t *slot = &s_frame[NP_ZN_HEADER_BYTES + (i * rec_bytes)];
        np_zn_status_t rc;
        if (is_map) {
            rc = np_zone_notify_encode_map(
                &((const np_zn_socket_desc_t *)records)[i], slot);
        } else {
            rc = np_zone_notify_encode_status(
                &((const np_zn_socket_state_t *)records)[i], slot);
        }
        if (rc != NP_ZN_OK) {
            return rc;
        }
    }

    uint8_t len = (uint8_t)(NP_ZN_HEADER_BYTES + (n * rec_bytes));
    if (!s_tx(s_frame, len, is_map, s_tx_ctx)) {
        return NP_ZN_ERR_TX;
    }
    return NP_ZN_OK;
}

/*
 * Emit an ordered fragment run over a record array. Validates the ENTIRE set
 * first: a snapshot (of either kind) is a complete-inventory claim, and emitting
 * half of one would leave the app accumulating a sequence that never completes.
 */
/*
 * `rec_bytes` is the WIRE size of a record; `elem_size` is the C array stride.
 * They are deliberately separate — a struct is padded and larger than its packed
 * wire form, so walking the array by rec_bytes would read misaligned garbage
 * after the first element.
 */
static np_zn_status_t emit_run(const void *records, uint16_t count,
                               uint8_t rec_bytes, size_t elem_size, bool is_map)
{
    if (!s_tx) {
        return NP_ZN_ERR_INVALID_ARG;
    }
    if (count > 0u && !records) {
        return NP_ZN_ERR_INVALID_ARG;
    }

    for (uint16_t i = 0u; i < count; i++) {
        uint8_t probe[NP_ZN_MAP_REC_BYTES];
        np_zn_status_t rc;
        if (is_map) {
            rc = np_zone_notify_encode_map(
                &((const np_zn_socket_desc_t *)records)[i], probe);
        } else {
            rc = np_zone_notify_encode_status(
                &((const np_zn_socket_state_t *)records)[i], probe);
        }
        if (rc != NP_ZN_OK) {
            return rc;
        }
    }

    const uint8_t per_frag = records_per_fragment(rec_bytes);

    /* Empty inventory: one header-only fragment. "No modules" / "no sockets" is
     * a real answer and must be distinguishable from silence. */
    if (count == 0u) {
        return emit_fragment(NULL, 0u, rec_bytes, 0u, true, true, is_map);
    }

    uint16_t sent = 0u;
    uint8_t  frag = 0u;
    while (sent < count) {
        uint16_t remaining = (uint16_t)(count - sent);
        uint8_t  n = (remaining > per_frag) ? per_frag : (uint8_t)remaining;
        bool     last = ((uint16_t)(sent + n) >= count);

        const uint8_t *base = (const uint8_t *)records;
        np_zn_status_t rc = emit_fragment(base + ((size_t)sent * elem_size),
                                          n, rec_bytes, frag, true, last, is_map);
        if (rc != NP_ZN_OK) {
            return rc;
        }
        sent = (uint16_t)(sent + n);
        frag++;
    }
    return NP_ZN_OK;
}

np_zn_status_t np_zone_notify_socket_map(const np_zn_socket_desc_t *descs,
                                         uint16_t count)
{
    return emit_run(descs, count, NP_ZN_MAP_REC_BYTES,
                    sizeof(np_zn_socket_desc_t), /*is_map=*/true);
}

np_zn_status_t np_zone_notify_snapshot(const np_zn_socket_state_t *states,
                                       uint16_t count)
{
    return emit_run(states, count, NP_ZN_STATUS_REC_BYTES,
                    sizeof(np_zn_socket_state_t), /*is_map=*/false);
}

np_zn_status_t np_zone_notify_event(const np_zn_socket_state_t *state)
{
    if (!s_tx) {
        return NP_ZN_ERR_INVALID_ARG;
    }
    if (!state) {
        return NP_ZN_ERR_INVALID_ARG;
    }
    /* Validate before emitting so a bad id produces no frame at all, rather than
     * a header the app must then decide how to un-see. */
    uint8_t probe[NP_ZN_STATUS_REC_BYTES];
    np_zn_status_t rc = np_zone_notify_encode_status(state, probe);
    if (rc != NP_ZN_OK) {
        return rc;
    }
    return emit_fragment(state, 1u, NP_ZN_STATUS_REC_BYTES, 0u,
                         /*snapshot=*/false, /*last=*/true, /*is_map=*/false);
}
