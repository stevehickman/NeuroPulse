/*
 * NeurOne Hub Control Program — Hexagonal Zone-Module Addressing + NVRAM Map
 * Implements np_module_map.h (design study: hexagonal zone-module redesign).
 *
 * File-static singleton (house style, matching np_module_registry /
 * np_cvns_reenable). Pure C, no FreeRTOS, no heap. NVRAM I/O behind an injected
 * HAL. IEC 62304 Class B (SW-02).
 */

#include <string.h>

#include "np_module_map.h"

/* ── Compile-time invariants ──────────────────────────────────────────────────── */

typedef char _np_hexmap_elem_domain[(NP_ELEM_TYPE_COUNT <= 64) ? 1 : -1];
typedef char _np_hexmap_socket_fits[(NP_HEXMAP_MAX_SOCKETS <=
                                     (1 << NP_HEXMAP_SOCKET_BITS)) ? 1 : -1];
typedef char _np_hexmap_elem_fits[(NP_HEXMAP_MAX_ELEMENTS <=
                                   (1 << NP_HEXMAP_ELEM_BITS)) ? 1 : -1];

/* ── Per-socket inventory record ──────────────────────────────────────────────── */

typedef struct {
    np_module_uid_t uid;           /* zero = empty socket                        */
    bool            module_present;/* module plugged AND inventoried cleanly     */
    uint8_t         health;
    uint8_t         elem_count;     /* valid entries in elem_type[]              */
    uint8_t         elem_type[NP_HEXMAP_MAX_ELEMENTS];  /* np_elem_type_t values */
} np_socket_record_t;

/* ── Module state ─────────────────────────────────────────────────────────────── */

static struct {
    const np_socket_geom_t *geom;      /* borrowed const helmet table            */
    uint16_t                n_sockets;
    bool                    initialized;
    np_socket_record_t      rec[NP_HEXMAP_MAX_SOCKETS];
} s_map;

/* ── NVRAM serialization format ───────────────────────────────────────────────── */

#define NP_HEXMAP_NVRAM_MAGIC    0x4E504D50u   /* "NPMP" */
#define NP_HEXMAP_NVRAM_VERSION  0x0001u

/* Serialized layout constants (NP_HEXMAP_HDR_BYTES / REC_BYTES / CRC_BYTES /
 * NVRAM_MAX_BYTES) are public in np_module_map.h so integrators can size the
 * Config-partition region.
 *
 * NVRAM serialize/restore scratch. File-static (not on-stack): the serialized
 * blob is ~9 KB, too large for a FreeRTOS task stack. persist()/restore() are
 * called only from the single boot / module-registry context, so no lock is
 * needed. */
static uint8_t s_nvram_scratch[NP_HEXMAP_NVRAM_MAX_BYTES];

/* ── UID helpers ──────────────────────────────────────────────────────────────── */

bool np_module_uid_equal(const np_module_uid_t *a, const np_module_uid_t *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    return memcmp(a->b, b->b, NP_HEXMAP_UID_LEN) == 0;
}

bool np_module_uid_is_zero(const np_module_uid_t *a)
{
    if (a == NULL) {
        return true;
    }
    for (unsigned i = 0; i < NP_HEXMAP_UID_LEN; i++) {
        if (a->b[i] != 0u) {
            return false;
        }
    }
    return true;
}

/* ── Little-endian store/load helpers ─────────────────────────────────────────── */

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ── CRC-32 (IEEE 802.3, reflected) — self-contained NVRAM integrity check ─────── */

static uint32_t crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (unsigned k = 0; k < 8; k++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

/* ── Init ─────────────────────────────────────────────────────────────────────── */

np_hub_status_t np_module_map_init(const np_socket_geom_t *geom, uint16_t n_sockets)
{
    if (geom == NULL || n_sockets == 0u || n_sockets > NP_HEXMAP_MAX_SOCKETS) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    memset(&s_map, 0, sizeof(s_map));
    s_map.geom        = geom;
    s_map.n_sockets   = n_sockets;
    s_map.initialized = true;
    return NP_HUB_OK;
}

/* ── Poll application ─────────────────────────────────────────────────────────── */

static void clear_record(np_socket_record_t *r)
{
    memset(r, 0, sizeof(*r));  /* uid zeroed, module_present false, count 0 */
}

np_hub_status_t np_module_map_apply_poll(uint16_t                 socket_id,
                                         const np_module_uid_t   *reported_uid,
                                         uint8_t                  health,
                                         np_hexmap_inventory_fn   inv_cb,
                                         void                    *inv_ctx,
                                         bool                    *changed_out)
{
    if (changed_out != NULL) {
        *changed_out = false;
    }
    if (!s_map.initialized || reported_uid == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (socket_id >= s_map.n_sockets ||
        !s_map.geom[socket_id].present_in_helmet) {
        return NP_HUB_ERR_INVALID_ARG;
    }

    np_socket_record_t *r = &s_map.rec[socket_id];
    r->health = health;

    /* Empty socket. */
    if (np_module_uid_is_zero(reported_uid)) {
        if (r->module_present || !np_module_uid_is_zero(&r->uid)) {
            clear_record(r);
            r->health = health;
            if (changed_out != NULL) {
                *changed_out = true;
            }
        }
        return NP_HUB_OK;
    }

    /* Same module as last time in this socket — refresh health only, no re-inventory. */
    if (r->module_present && np_module_uid_equal(&r->uid, reported_uid)) {
        return NP_HUB_OK;
    }

    /* New module in this socket — must inventory it. Fail closed on any error. */
    if (inv_cb == NULL) {
        clear_record(r);
        r->health = health;
        if (changed_out != NULL) {
            *changed_out = true;   /* prior module (if any) is gone */
        }
        return NP_HUB_ERR_NOT_PRESENT;
    }

    uint8_t         types[NP_HEXMAP_MAX_ELEMENTS];
    uint8_t         count = 0;
    np_hub_status_t rc = inv_cb(socket_id, inv_ctx, types,
                                (uint8_t)NP_HEXMAP_MAX_ELEMENTS, &count);
    if (rc != NP_HUB_OK || count > NP_HEXMAP_MAX_ELEMENTS) {
        clear_record(r);
        r->health = health;
        if (changed_out != NULL) {
            *changed_out = true;
        }
        return (rc != NP_HUB_OK) ? rc : NP_HUB_ERR_CMD_TOO_MANY;
    }

    clear_record(r);
    r->uid            = *reported_uid;
    r->health         = health;
    r->elem_count     = count;
    r->module_present = true;
    memcpy(r->elem_type, types, count);
    if (changed_out != NULL) {
        *changed_out = true;
    }
    return NP_HUB_OK;
}

/* ── Address resolution ───────────────────────────────────────────────────────── */

np_hub_status_t np_module_map_resolve(np_hex_addr_t addr, np_physical_loc_t *out)
{
    if (out == NULL || !s_map.initialized) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (addr.socket_id >= s_map.n_sockets ||
        !s_map.geom[addr.socket_id].present_in_helmet) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    const np_socket_record_t *r = &s_map.rec[addr.socket_id];
    if (!r->module_present || addr.element_id >= r->elem_count) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    const np_socket_geom_t *g = &s_map.geom[addr.socket_id];
    out->lobe      = g->lobe;
    out->side      = g->side;
    out->x_mm      = g->x_mm;
    out->y_mm      = g->y_mm;
    out->elem_type = (np_elem_type_t)r->elem_type[addr.element_id];
    return NP_HUB_OK;
}

/* ── Group resolution ─────────────────────────────────────────────────────────── */

static bool type_passes(uint64_t mask, bool exclude, np_elem_type_t t)
{
    if (t == NP_ELEM_NONE) {
        return false;                  /* unpopulated slots never selected */
    }
    if (mask == 0u) {
        return true;                   /* mask 0 = no filter → all types (both modes) */
    }
    bool listed = (mask & NP_ELEM_BIT(t)) != 0u;
    return exclude ? !listed : listed;
}

/* Emit every passing element of one socket. Returns false on overflow. */
static bool emit_socket(uint16_t socket_id, uint64_t mask, bool exclude,
                        np_hex_addr_t *out, uint16_t max, uint16_t *count)
{
    if (socket_id >= s_map.n_sockets ||
        !s_map.geom[socket_id].present_in_helmet) {
        return true;                   /* skip missing sockets, not an overflow */
    }
    const np_socket_record_t *r = &s_map.rec[socket_id];
    if (!r->module_present) {
        return true;
    }
    for (uint8_t e = 0; e < r->elem_count; e++) {
        if (!type_passes(mask, exclude, (np_elem_type_t)r->elem_type[e])) {
            continue;
        }
        if (*count >= max) {
            return false;
        }
        out[*count].socket_id  = (uint8_t)socket_id;
        out[*count].element_id = e;
        (*count)++;
    }
    return true;
}

np_hub_status_t np_module_map_resolve_group(const np_group_query_t *q,
                                            np_hex_addr_t          *out,
                                            uint16_t                max,
                                            uint16_t               *count_out)
{
    if (q == NULL || out == NULL || count_out == NULL || !s_map.initialized) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    *count_out = 0;
    bool ok = true;

    switch (q->kind) {
    case NP_GROUP_KIND_LOBE:
        for (uint16_t s = 0; s < s_map.n_sockets && ok; s++) {
            const np_socket_geom_t *g = &s_map.geom[s];
            if (!g->present_in_helmet || g->lobe != q->lobe) {
                continue;
            }
            /* MIDLINE query side = any hemisphere. */
            if (q->side != NP_SIDE_MIDLINE && g->side != q->side) {
                continue;
            }
            ok = emit_socket(s, q->type_mask, q->type_exclude, out, max, count_out);
        }
        break;

    case NP_GROUP_KIND_SOCKET_SET:
        if (q->sockets == NULL && q->socket_count != 0u) {
            return NP_HUB_ERR_INVALID_ARG;
        }
        for (uint16_t i = 0; i < q->socket_count && ok; i++) {
            ok = emit_socket(q->sockets[i], q->type_mask, q->type_exclude,
                             out, max, count_out);
        }
        break;

    case NP_GROUP_KIND_ADDR_SET:
        if (q->addrs == NULL && q->addr_count != 0u) {
            return NP_HUB_ERR_INVALID_ARG;
        }
        for (uint16_t i = 0; i < q->addr_count && ok; i++) {
            np_physical_loc_t loc;
            np_hex_addr_t     a = q->addrs[i];
            if (np_module_map_resolve(a, &loc) != NP_HUB_OK) {
                continue;              /* silently drop unresolvable addresses */
            }
            if (!type_passes(q->type_mask, q->type_exclude, loc.elem_type)) {
                continue;
            }
            if (*count_out >= max) {
                ok = false;
                break;
            }
            out[*count_out] = a;
            (*count_out)++;
        }
        break;

    default:
        return NP_HUB_ERR_INVALID_ARG;
    }

    return ok ? NP_HUB_OK : NP_HUB_ERR_CMD_TOO_MANY;
}

np_hub_status_t np_module_map_predefined(np_pgroup_t     g,
                                         uint64_t        type_mask,
                                         bool            type_exclude,
                                         np_hex_addr_t  *out,
                                         uint16_t        max,
                                         uint16_t       *count_out)
{
    static const struct { np_lobe_t lobe; np_side_t side; } tbl[NP_PGROUP_COUNT] = {
        { NP_LOBE_FRONTAL,   NP_SIDE_LEFT  }, { NP_LOBE_FRONTAL,   NP_SIDE_RIGHT },
        { NP_LOBE_TEMPORAL,  NP_SIDE_LEFT  }, { NP_LOBE_TEMPORAL,  NP_SIDE_RIGHT },
        { NP_LOBE_PARIETAL,  NP_SIDE_LEFT  }, { NP_LOBE_PARIETAL,  NP_SIDE_RIGHT },
        { NP_LOBE_OCCIPITAL, NP_SIDE_LEFT  }, { NP_LOBE_OCCIPITAL, NP_SIDE_RIGHT },
    };
    if (g >= NP_PGROUP_COUNT) {   /* enum is unsigned; a negative int cast wraps high */
        return NP_HUB_ERR_INVALID_ARG;
    }
    np_group_query_t q;
    memset(&q, 0, sizeof(q));
    q.kind         = NP_GROUP_KIND_LOBE;
    q.lobe         = tbl[g].lobe;
    q.side         = tbl[g].side;
    q.type_mask    = type_mask;
    q.type_exclude = type_exclude;
    return np_module_map_resolve_group(&q, out, max, count_out);
}

/* ── Placement validation ─────────────────────────────────────────────────────── */

static bool socket_has_type(uint16_t socket_id, uint64_t type_mask)
{
    if (socket_id >= s_map.n_sockets ||
        !s_map.geom[socket_id].present_in_helmet) {
        return false;
    }
    const np_socket_record_t *r = &s_map.rec[socket_id];
    if (!r->module_present) {
        return false;
    }
    for (uint8_t e = 0; e < r->elem_count; e++) {
        np_elem_type_t t = (np_elem_type_t)r->elem_type[e];
        if (t != NP_ELEM_NONE && (type_mask & NP_ELEM_BIT(t)) != 0u) {
            return true;
        }
    }
    return false;
}

np_hub_status_t np_module_map_check_placement(const np_placement_req_t *reqs,
                                              uint16_t                  n,
                                              uint8_t                  *failed_sockets,
                                              uint16_t                  max,
                                              uint16_t                 *fail_count)
{
    if (!s_map.initialized || fail_count == NULL || (reqs == NULL && n != 0u)) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    *fail_count = 0;
    for (uint16_t i = 0; i < n; i++) {
        if (!socket_has_type(reqs[i].socket_id, reqs[i].type_mask)) {
            if (failed_sockets != NULL && *fail_count < max) {
                failed_sockets[*fail_count] = reqs[i].socket_id;
            }
            (*fail_count)++;   /* count ALL failures even if the list is full */
        }
    }
    return (*fail_count == 0u) ? NP_HUB_OK : NP_HUB_ERR_NOT_PRESENT;
}

/* ── NVRAM persistence ────────────────────────────────────────────────────────── */

size_t np_module_map_serialized_size(void)
{
    if (!s_map.initialized) {
        return 0;
    }
    return (size_t)NP_HEXMAP_HDR_BYTES +
           (size_t)s_map.n_sockets * NP_HEXMAP_REC_BYTES +
           (size_t)NP_HEXMAP_CRC_BYTES;
}

int np_module_map_serialize(uint8_t *buf, size_t buf_len)
{
    if (buf == NULL || !s_map.initialized) {
        return -1;
    }
    size_t total = np_module_map_serialized_size();
    if (buf_len < total) {
        return -1;
    }
    size_t off = 0;
    put_u32(&buf[off], NP_HEXMAP_NVRAM_MAGIC);   off += 4;
    put_u16(&buf[off], NP_HEXMAP_NVRAM_VERSION); off += 2;
    put_u16(&buf[off], s_map.n_sockets);         off += 2;

    for (uint16_t s = 0; s < s_map.n_sockets; s++) {
        const np_socket_record_t *r = &s_map.rec[s];
        memcpy(&buf[off], r->uid.b, NP_HEXMAP_UID_LEN); off += NP_HEXMAP_UID_LEN;
        buf[off++] = r->module_present ? 1u : 0u;
        buf[off++] = r->health;
        buf[off++] = r->elem_count;
        memcpy(&buf[off], r->elem_type, NP_HEXMAP_MAX_ELEMENTS);
        off += NP_HEXMAP_MAX_ELEMENTS;
    }

    uint32_t crc = crc32(buf, off);
    put_u32(&buf[off], crc); off += 4;
    return (int)off;
}

np_hub_status_t np_module_map_load(const uint8_t *buf, size_t buf_len)
{
    if (buf == NULL || !s_map.initialized) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (buf_len < NP_HEXMAP_HDR_BYTES) {
        return NP_HUB_ERR_BAD_MAGIC;
    }
    if (get_u32(&buf[0]) != NP_HEXMAP_NVRAM_MAGIC) {
        return NP_HUB_ERR_BAD_MAGIC;
    }
    if (get_u16(&buf[4]) != NP_HEXMAP_NVRAM_VERSION) {
        return NP_HUB_ERR_BAD_VERSION;
    }
    uint16_t n = get_u16(&buf[6]);
    if (n != s_map.n_sockets) {
        return NP_HUB_ERR_INVALID_ARG;   /* geometry mismatch */
    }
    size_t total = np_module_map_serialized_size();
    if (buf_len < total) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    uint32_t want = get_u32(&buf[total - NP_HEXMAP_CRC_BYTES]);
    uint32_t got  = crc32(buf, total - NP_HEXMAP_CRC_BYTES);
    if (want != got) {
        return NP_HUB_ERR_BAD_SIGNATURE; /* integrity failure */
    }

    size_t off = NP_HEXMAP_HDR_BYTES;
    for (uint16_t s = 0; s < n; s++) {
        np_socket_record_t *r = &s_map.rec[s];
        clear_record(r);
        memcpy(r->uid.b, &buf[off], NP_HEXMAP_UID_LEN); off += NP_HEXMAP_UID_LEN;
        r->module_present = (buf[off++] != 0u);
        r->health         = buf[off++];
        uint8_t count     = buf[off++];
        if (count > NP_HEXMAP_MAX_ELEMENTS) {
            count = NP_HEXMAP_MAX_ELEMENTS;  /* defensive clamp */
        }
        r->elem_count = count;
        memcpy(r->elem_type, &buf[off], NP_HEXMAP_MAX_ELEMENTS);
        off += NP_HEXMAP_MAX_ELEMENTS;
        /* A record marked present with a zero UID is inconsistent — drop it. */
        if (r->module_present && np_module_uid_is_zero(&r->uid)) {
            clear_record(r);
        }
    }
    return NP_HUB_OK;
}

np_hub_status_t np_module_map_persist(void)
{
    if (!s_map.initialized) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    int n = np_module_map_serialize(s_nvram_scratch, sizeof(s_nvram_scratch));
    if (n < 0) {
        return NP_HUB_ERR_GENERIC;
    }
    return np_hexmap_nvram_write(s_nvram_scratch, (size_t)n);
}

np_hub_status_t np_module_map_restore(void)
{
    if (!s_map.initialized) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    size_t          read_len = 0;
    np_hub_status_t rc = np_hexmap_nvram_read(s_nvram_scratch,
                                              sizeof(s_nvram_scratch), &read_len);
    if (rc != NP_HUB_OK) {
        return rc;
    }
    return np_module_map_load(s_nvram_scratch, read_len);
}
