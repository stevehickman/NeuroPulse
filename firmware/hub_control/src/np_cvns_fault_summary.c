/*
 * NeurOne Hub Control Program — Cervical VNS Offline-Fault Summary
 * Document: NP-SW-FAULTMSG-001 Rev 3 §9.6 — see np_cvns_fault_summary.h
 *
 * IEC 62304 Class B (SW-02).
 */

#include "np_cvns_fault_summary.h"
#include "np_cvns_config.h"
#include "np_spi_wire_types.h"
#include "np_transport.h"   /* np_transport_hal_enter/exit_critical */
#include <string.h>

typedef struct {
    uint32_t user_tag;         /* NP_SAFETY_USER_UNSPECIFIED = unattributed */
    uint32_t session_counter;
    uint8_t  kind;             /* np_cvns_fault_reason_t, 1–5 */
    uint8_t  side_mask;
} np_cvfs_entry_t;

/* Shared state — every access inside the critical section. */
static np_cvfs_entry_t s_entries[NP_CVFS_STORE_RECORDS];   /* oldest first */
static uint8_t         s_count;
static uint32_t        s_user;
static bool            s_dirty;

/* Owned by the poll task only (the heartbeat). */
static uint8_t s_frame[NP_CVFS_FRAME_MAX];
static size_t  s_frame_len;
static bool    s_published;

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    size_t   i;
    for (i = 0u; i < n; i++) {
        uint8_t b;
        c ^= p[i];
        for (b = 0u; b < 8u; b++) {
            c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
        }
    }
    return ~c;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool kind_valid(uint8_t kind)
{
    return (kind >= (uint8_t)NP_CVNS_FAULT_HR_CHANGE) &&
           (kind <= (uint8_t)NP_CVNS_FAULT_SAFETY_MCU);
}

/* Caller holds the critical section (or owns the snapshot). */
static size_t serialize(const np_cvfs_entry_t *entries, uint8_t count,
                        uint32_t user, uint8_t *out)
{
    size_t  o = 0u;
    uint8_t i;
    out[o++] = NP_CVFS_BLOB_VERSION;
    put_u32(&out[o], user); o += 4u;
    out[o++] = count;
    for (i = 0u; i < count; i++) {
        put_u32(&out[o], entries[i].user_tag);        o += 4u;
        put_u32(&out[o], entries[i].session_counter); o += 4u;
        out[o++] = entries[i].kind;
        out[o++] = entries[i].side_mask;
    }
    put_u32(&out[o], crc32(out, o)); o += 4u;
    return o;
}

/* ── Init ─────────────────────────────────────────────────────────────────── */

void np_cvfs_init(void)
{
    uint8_t blob[NP_CVFS_BLOB_MAX];
    size_t  len = 0u;
    uint8_t n;
    uint8_t i;

    np_transport_hal_enter_critical();
    s_count = 0u;
    s_user  = NP_SAFETY_USER_UNSPECIFIED;
    s_dirty = false;
    np_transport_hal_exit_critical();
    s_frame_len = 0u;
    s_published = false;

    if ((np_cvfs_hal_load(blob, sizeof blob, &len) != NP_HUB_OK) || (len < 10u)) {
        return;   /* nothing stored, or unreadable: start empty */
    }
    if (blob[0] != NP_CVFS_BLOB_VERSION) {
        return;
    }
    n = blob[5];
    if ((n > NP_CVFS_STORE_RECORDS) ||
        (len != (6u + ((size_t)n * NP_CVFS_BLOB_ENTRY_LEN) + 4u)) ||
        (crc32(blob, len - 4u) != get_u32(&blob[len - 4u]))) {
        return;
    }
    {
        np_cvfs_entry_t tmp[NP_CVFS_STORE_RECORDS];
        uint32_t        user = get_u32(&blob[1]);
        for (i = 0u; i < n; i++) {
            const uint8_t *e = &blob[6u + ((size_t)i * NP_CVFS_BLOB_ENTRY_LEN)];
            tmp[i].user_tag        = get_u32(&e[0]);
            tmp[i].session_counter = get_u32(&e[4]);
            tmp[i].kind            = e[8];
            tmp[i].side_mask       = e[9];
            /* A record the frame could not carry faithfully is not half-used:
             * the whole blob is discarded, as the apps discard a bad frame. */
            if (!kind_valid(tmp[i].kind) ||
                ((tmp[i].side_mask & (uint8_t)~(NP_CVFS_SIDE_LEFT | NP_CVFS_SIDE_RIGHT)) != 0u) ||
                ((tmp[i].kind != (uint8_t)NP_CVNS_FAULT_IMPEDANCE) && (tmp[i].side_mask != 0u))) {
                return;
            }
        }
        if (user == NP_SAFETY_USER_ANY) {
            return;
        }
        np_transport_hal_enter_critical();
        (void)memcpy(s_entries, tmp, (size_t)n * sizeof tmp[0]);
        s_count = n;
        s_user  = user;
        np_transport_hal_exit_critical();
    }
}

/* ── Mutators ─────────────────────────────────────────────────────────────── */

np_hub_status_t np_cvfs_record_fault(uint32_t               session_counter,
                                     np_cvns_fault_reason_t kind,
                                     uint8_t                side_mask)
{
    np_cvfs_entry_t e;

    if (!kind_valid((uint8_t)kind)) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    e.session_counter = session_counter;
    e.kind            = (uint8_t)kind;
    e.side_mask       = (kind == NP_CVNS_FAULT_IMPEDANCE)
                        ? (uint8_t)(side_mask & (NP_CVFS_SIDE_LEFT | NP_CVFS_SIDE_RIGHT))
                        : 0u;

    np_transport_hal_enter_critical();
    e.user_tag = s_user;
    if (s_count == NP_CVFS_STORE_RECORDS) {
        (void)memmove(&s_entries[0], &s_entries[1],
                      (NP_CVFS_STORE_RECORDS - 1u) * sizeof s_entries[0]);
        s_count--;
    }
    s_entries[s_count++] = e;
    s_dirty = true;
    np_transport_hal_exit_critical();
    return NP_HUB_OK;
}

uint8_t np_cvfs_pad_side_mask(float left_kohm, float right_kohm)
{
    uint8_t m = 0u;
    /* `!(x > 0 && x <= max)` is true for NaN as well as out-of-window values. */
    if (!((left_kohm > 0.0f) && (left_kohm <= NP_CVNS_IMPEDANCE_MAX_KOHM))) {
        m |= NP_CVFS_SIDE_LEFT;
    }
    if (!((right_kohm > 0.0f) && (right_kohm <= NP_CVNS_IMPEDANCE_MAX_KOHM))) {
        m |= NP_CVFS_SIDE_RIGHT;
    }
    return (m == 0u) ? (uint8_t)(NP_CVFS_SIDE_LEFT | NP_CVFS_SIDE_RIGHT) : m;
}

uint32_t np_cvfs_current_user(void)
{
    uint32_t u;
    np_transport_hal_enter_critical();
    u = s_user;
    np_transport_hal_exit_critical();
    return u;
}

/* ── Frame ────────────────────────────────────────────────────────────────── */

np_hub_status_t np_cvfs_build_frame(uint8_t  reenable_state,
                                    bool     nv_valid,
                                    uint8_t  nv_flags,
                                    uint8_t *buf,
                                    size_t   cap,
                                    size_t  *len_out)
{
    np_cvfs_entry_t pick[NP_CVFS_MAX_WIRE_RECORDS];
    uint8_t         npick = 0u;
    int             i;
    uint8_t         k;
    size_t          o;

    if ((buf == NULL) || (len_out == NULL) || (cap < NP_CVFS_FRAME_MAX)) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    /* Newest first, the active user's and the unattributed, up to four. */
    np_transport_hal_enter_critical();
    for (i = (int)s_count - 1; (i >= 0) && (npick < NP_CVFS_MAX_WIRE_RECORDS); i--) {
        uint32_t tag = s_entries[i].user_tag;
        if ((tag == s_user) || (tag == NP_SAFETY_USER_UNSPECIFIED)) {
            pick[npick++] = s_entries[i];
        }
    }
    np_transport_hal_exit_critical();

    buf[0] = NP_CVFS_WIRE_VERSION;
    buf[1] = (reenable_state <= 4u) ? reenable_state : 0u;
    buf[2] = npick;
    buf[3] = nv_valid ? (uint8_t)(nv_flags & NP_CVFS_FLAGS_MASK) : 0u;
    o = NP_CVFS_HEADER_LEN;
    for (k = npick; k > 0u; k--) {             /* oldest first on the wire */
        const np_cvfs_entry_t *e = &pick[k - 1u];
        put_u32(&buf[o], e->session_counter);
        buf[o + 4u] = e->kind;
        buf[o + 5u] = e->side_mask;
        buf[o + 6u] = 0u;
        buf[o + 7u] = 0u;
        o += NP_CVFS_RECORD_LEN;
    }
    *len_out = o;
    return NP_HUB_OK;
}

void np_cvfs_poll(uint8_t reenable_state, bool nv_valid, uint8_t nv_flags)
{
    uint8_t frame[NP_CVFS_FRAME_MAX];
    size_t  len = 0u;
    bool    dirty;

    np_transport_hal_enter_critical();
    dirty = s_dirty;
    np_transport_hal_exit_critical();

    if (dirty) {
        uint8_t         blob[NP_CVFS_BLOB_MAX];
        np_cvfs_entry_t snap[NP_CVFS_STORE_RECORDS];
        uint8_t         n;
        uint32_t        user;
        size_t          blen;

        np_transport_hal_enter_critical();
        n    = s_count;
        user = s_user;
        (void)memcpy(snap, s_entries, (size_t)n * sizeof snap[0]);
        s_dirty = false;   /* cleared now; a change during the save re-sets it */
        np_transport_hal_exit_critical();

        blen = serialize(snap, n, user, blob);
        if (np_cvfs_hal_save(blob, blen) != NP_HUB_OK) {
            np_transport_hal_enter_critical();
            s_dirty = true;   /* retry next poll */
            np_transport_hal_exit_critical();
        }
    }

    if (np_cvfs_build_frame(reenable_state, nv_valid, nv_flags,
                            frame, sizeof frame, &len) != NP_HUB_OK) {
        return;
    }
    if (!s_published || (len != s_frame_len) || (memcmp(frame, s_frame, len) != 0)) {
        (void)memcpy(s_frame, frame, len);
        s_frame_len = len;
        s_published = true;
        np_cvfs_hal_notify(s_frame, s_frame_len);
    }
}

np_hub_status_t np_cvfs_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if ((buf == NULL) || (len_out == NULL) || (cap < NP_CVFS_FRAME_MAX)) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (!s_published) {
        buf[0] = NP_CVFS_WIRE_VERSION;
        buf[1] = 0u;
        buf[2] = 0u;
        buf[3] = 0u;
        *len_out = NP_CVFS_HEADER_LEN;
        return NP_HUB_OK;
    }
    (void)memcpy(buf, s_frame, s_frame_len);
    *len_out = s_frame_len;
    return NP_HUB_OK;
}

/* ── ATT write handlers ───────────────────────────────────────────────────── */

np_hub_status_t np_cvfs_on_active_user_write(const uint8_t *data, size_t len)
{
    uint32_t        tag;
    np_hub_status_t rc;

    if ((data == NULL) || (len != 4u)) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    tag = get_u32(data);
    if ((tag == NP_SAFETY_USER_UNSPECIFIED) || (tag == NP_SAFETY_USER_ANY)) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    /* Attribution changes only when the heartbeat forwards the tag to the
     * safety MCU (np_cvfs_set_user), which it does between sessions — so the
     * hub and the MCU name the same person for any session's fault. */
    rc = np_hub_set_active_user(tag);
    return rc;
}

void np_cvfs_set_user(uint32_t tag)
{
    if ((tag == NP_SAFETY_USER_UNSPECIFIED) || (tag == NP_SAFETY_USER_ANY)) {
        return;
    }
    np_transport_hal_enter_critical();
    if (s_user != tag) {
        s_user  = tag;
        s_dirty = true;
    }
    np_transport_hal_exit_critical();
}

np_hub_status_t np_cvfs_on_reenable_confirm_write(const uint8_t *data, size_t len)
{
    if ((data == NULL) || (len != 1u) || (data[0] != 0x01u)) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    return np_hub_cvns_reenable_confirm();
}
