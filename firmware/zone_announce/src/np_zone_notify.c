/*
 * NeurOne Zone Module Notification — frame encoder
 * See np_zone_notify.h for the format and the rationale.
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

/* The record packs lobe in 3 bits and side in 2. Both enums are duplicated in
 * np_zone_notify.h so this unit host-compiles standalone; assert they agree with
 * np_module_map.h rather than trusting two hand-kept copies. */
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

/* ── Module state ─────────────────────────────────────────────────────────── */

static np_zn_tx_fn s_tx;
static void       *s_tx_ctx;
static uint8_t     s_max_frame;

/* One fragment at a time; the transmit callback consumes each before the next is
 * built, so a single buffer suffices and nothing is heap-allocated. */
static uint8_t     s_frame[255];

/* Records that fit in one fragment after the header. */
static uint8_t records_per_fragment(void)
{
    return (uint8_t)((s_max_frame - NP_ZN_HEADER_BYTES) / NP_ZN_RECORD_BYTES);
}

np_zn_status_t np_zone_notify_encode_record(const np_zn_socket_state_t *state,
                                            uint8_t *buf)
{
    if (!state || !buf) {
        return NP_ZN_ERR_INVALID_ARG;
    }
    /* state->socket_id is 0-based; the wire is 1-based. An id at the top of the
     * 0-based domain maps to NP_ZN_MAX_SOCKET_ID, so anything at or above the
     * domain size is out of range — never masked into a plausible neighbour.
     * (Wrong-socket is a wrong-site dosing path, per np_module_map.h.) */
    if (state->socket_id >= NP_ZN_MAX_SOCKET_ID) {
        return NP_ZN_ERR_SOCKET_RANGE;
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

    /* Anatomy is emitted for empty sockets too — the app lists the sockets it is
     * still waiting on by name, not only the ones already filled. */
    buf[0] = (uint8_t)(state->socket_id + 1u);
    buf[1] = type;
    buf[2] = (uint8_t)(((uint8_t)state->lobe & 0x07u) |
                       (((uint8_t)state->side & 0x03u) << 3));
    buf[3] = flags;
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

/* Build and transmit one fragment. */
static np_zn_status_t emit_fragment(const np_zn_socket_state_t *states,
                                    uint8_t n, uint8_t frag_index,
                                    bool snapshot, bool last)
{
    uint8_t flags = 0u;
    if (snapshot) { flags |= NP_ZN_FLAG_SNAPSHOT; }
    if (last)     { flags |= NP_ZN_FLAG_LAST;     }

    s_frame[0] = NP_ZN_FORMAT_VERSION;
    s_frame[1] = flags;
    s_frame[2] = frag_index;
    s_frame[3] = n;

    for (uint8_t i = 0u; i < n; i++) {
        np_zn_status_t rc = np_zone_notify_encode_record(
            &states[i], &s_frame[NP_ZN_HEADER_BYTES + (i * NP_ZN_RECORD_BYTES)]);
        if (rc != NP_ZN_OK) {
            return rc;
        }
    }

    uint8_t len = (uint8_t)(NP_ZN_HEADER_BYTES + (n * NP_ZN_RECORD_BYTES));
    if (!s_tx(s_frame, len, s_tx_ctx)) {
        return NP_ZN_ERR_TX;
    }
    return NP_ZN_OK;
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
    uint8_t probe[NP_ZN_RECORD_BYTES];
    np_zn_status_t rc = np_zone_notify_encode_record(state, probe);
    if (rc != NP_ZN_OK) {
        return rc;
    }
    return emit_fragment(state, 1u, 0u, /*snapshot=*/false, /*last=*/true);
}

np_zn_status_t np_zone_notify_snapshot(const np_zn_socket_state_t *states,
                                       uint16_t count)
{
    if (!s_tx) {
        return NP_ZN_ERR_INVALID_ARG;
    }
    if (count > 0u && !states) {
        return NP_ZN_ERR_INVALID_ARG;
    }

    /* Validate the whole set first. A snapshot is an inventory claim; emitting
     * half of one and then failing would leave the app accumulating a sequence
     * that never completes. */
    for (uint16_t i = 0u; i < count; i++) {
        uint8_t probe[NP_ZN_RECORD_BYTES];
        np_zn_status_t rc = np_zone_notify_encode_record(&states[i], probe);
        if (rc != NP_ZN_OK) {
            return rc;
        }
    }

    const uint8_t per_frag = records_per_fragment();

    /* Empty inventory: one header-only fragment. "No modules" is a real answer
     * and must be distinguishable from silence. */
    if (count == 0u) {
        return emit_fragment(NULL, 0u, 0u, /*snapshot=*/true, /*last=*/true);
    }

    uint16_t sent  = 0u;
    uint8_t  frag  = 0u;
    while (sent < count) {
        uint16_t remaining = (uint16_t)(count - sent);
        uint8_t  n = (remaining > per_frag) ? per_frag : (uint8_t)remaining;
        bool     last = ((uint16_t)(sent + n) >= count);

        np_zn_status_t rc = emit_fragment(&states[sent], n, frag, true, last);
        if (rc != NP_ZN_OK) {
            return rc;
        }
        sent = (uint16_t)(sent + n);
        frag++;
    }
    return NP_ZN_OK;
}
