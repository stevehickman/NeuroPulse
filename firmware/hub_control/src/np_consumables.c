/*
 * NeurOne Hub Control Program — Consumable Session Counts (OI-ACC-08, GitHub #381)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_consumables.h.
 */

#include "np_consumables.h"

#include <string.h>

#include "np_cfg_store.h"
#include "np_gatt_server.h"
#include "np_transport.h"   /* np_transport_hal_enter/exit_critical */

/* Kind index → the modality whose drive exposes that part (np_consumables.h (i)).
 * The order is the wire order both apps' ConsumableKind raw values use. */
static const uint8_t k_kind_mod[NP_CONS_KIND_COUNT] = {
    (uint8_t)NP_MOD_INTRANASAL,
    (uint8_t)NP_MOD_EEG,
    (uint8_t)NP_MOD_VNS_HRV,
    (uint8_t)NP_MOD_AUDIO,
};

static uint16_t s_count[NP_CONS_KIND_COUNT];
static uint16_t s_held[NP_CONS_KIND_COUNT];   /* increments while unloaded */
static bool     s_loaded;
static bool     s_dirty;
static bool     s_busy;        /* a store write is in flight */
static uint32_t s_seq;         /* bumps on every change to s_count */

static uint8_t  s_published[NP_CONS_WIRE_LEN];
static bool     s_have_published;

static uint16_t sat_add(uint16_t a, uint16_t b)
{
    uint32_t s = (uint32_t)a + (uint32_t)b;
    return (s > NP_CONS_COUNT_MAX) ? (uint16_t)NP_CONS_COUNT_MAX : (uint16_t)s;
}

static void encode(const uint16_t c[NP_CONS_KIND_COUNT], uint8_t *out)
{
    for (unsigned k = 0U; k < NP_CONS_KIND_COUNT; k++) {
        out[2U * k]      = (uint8_t)(c[k] & 0xFFU);
        out[2U * k + 1U] = (uint8_t)(c[k] >> 8);
    }
}

/* Try to load the record.  Caller must not hold the critical section. */
static void try_load(void)
{
    uint8_t rec[NP_CONS_RECORD_LEN];
    uint16_t loaded[NP_CONS_KIND_COUNT] = { 0U, 0U, 0U, 0U };

    np_hub_status_t st = np_cfg_store_replicated_read(NP_CFG_FILE_CONSUMABLES,
                                                      rec, sizeof(rec));
    if (st == NP_HUB_OK) {
        if (rec[0] != NP_CONS_RECORD_VERSION) {
            return;   /* not a record this firmware wrote: stay unloaded */
        }
        for (unsigned k = 0U; k < NP_CONS_KIND_COUNT; k++) {
            loaded[k] = (uint16_t)rec[1U + 2U * k] |
                        (uint16_t)((uint16_t)rec[2U + 2U * k] << 8);
        }
    } else if (st != NP_HUB_ERR_NOT_PRESENT) {
        return;       /* present but unreadable: NOT zero */
    }

    np_transport_hal_enter_critical();
    bool held = false;
    for (unsigned k = 0U; k < NP_CONS_KIND_COUNT; k++) {
        s_count[k] = sat_add(loaded[k], s_held[k]);
        held = held || (s_held[k] != 0U);
        s_held[k] = 0U;
    }
    s_loaded = true;
    if (held) {
        s_dirty = true;
        s_seq++;
    }
    np_transport_hal_exit_critical();
}

/* Write the current counts if they changed.  Serialised by s_busy: the writer
 * snapshots under the critical section, and clears s_dirty afterwards only if
 * nothing changed while it wrote. */
static void persist(void)
{
    uint8_t  rec[NP_CONS_RECORD_LEN];
    uint16_t snap[NP_CONS_KIND_COUNT];
    uint32_t seq;

    np_transport_hal_enter_critical();
    if (!s_loaded || !s_dirty || s_busy) {
        np_transport_hal_exit_critical();
        return;
    }
    s_busy = true;
    memcpy(snap, s_count, sizeof(snap));
    seq = s_seq;
    np_transport_hal_exit_critical();

    rec[0] = NP_CONS_RECORD_VERSION;
    encode(snap, &rec[1]);
    np_hub_status_t st = np_cfg_store_replicated_write(NP_CFG_FILE_CONSUMABLES,
                                                       rec, sizeof(rec));

    np_transport_hal_enter_critical();
    s_busy = false;
    if (st == NP_HUB_OK && seq == s_seq) {
        s_dirty = false;
    }
    np_transport_hal_exit_critical();
}

void np_cons_init(void)
{
    try_load();
}

void np_cons_on_session_end(uint32_t mods_active_mask)
{
    np_transport_hal_enter_critical();
    bool changed = false;
    for (unsigned k = 0U; k < NP_CONS_KIND_COUNT; k++) {
        if ((mods_active_mask & (1UL << k_kind_mod[k])) == 0UL) {
            continue;
        }
        if (s_loaded) {
            s_count[k] = sat_add(s_count[k], 1U);
            changed = true;
        } else {
            s_held[k] = sat_add(s_held[k], 1U);
        }
    }
    if (changed) {
        s_dirty = true;
        s_seq++;
    }
    np_transport_hal_exit_critical();

    persist();
}

void np_cons_poll(void)
{
    if (!s_loaded) {
        try_load();
    }
    persist();

    uint8_t frame[NP_CONS_WIRE_LEN];
    np_transport_hal_enter_critical();
    bool loaded = s_loaded;
    encode(s_count, frame);
    np_transport_hal_exit_critical();

    if (!loaded) {
        return;
    }
    if (s_have_published && memcmp(frame, s_published, sizeof(frame)) == 0) {
        return;
    }
    if (np_gatt_notify(NP_GATT_ID_CONSUMABLE_STATUS, frame, sizeof(frame)) == NP_HUB_OK) {
        memcpy(s_published, frame, sizeof(frame));
        s_have_published = true;
    }
}

np_hub_status_t np_cons_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (buf == NULL || len_out == NULL || cap < NP_CONS_WIRE_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    np_transport_hal_enter_critical();
    bool loaded = s_loaded;
    encode(s_count, buf);
    np_transport_hal_exit_critical();

    if (!loaded) {
        memset(buf, 0, NP_CONS_WIRE_LEN);
        return NP_HUB_ERR_NOT_PRESENT;
    }
    *len_out = NP_CONS_WIRE_LEN;
    return NP_HUB_OK;
}

np_hub_status_t np_cons_on_reset_write(const uint8_t *data, size_t len)
{
    if (data == NULL || len != NP_CONS_RESET_LEN || data[0] >= NP_CONS_KIND_COUNT) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    np_hub_status_t st = NP_HUB_OK;
    np_transport_hal_enter_critical();
    if (!s_loaded) {
        st = NP_HUB_ERR_NOT_PRESENT;
    } else if (s_count[data[0]] != 0U) {
        s_count[data[0]] = 0U;
        s_dirty = true;
        s_seq++;
    }
    np_transport_hal_exit_critical();
    return st;
}

#ifdef NPTEST_HOST
void np_cons_test_reset(void)
{
    memset(s_count, 0, sizeof(s_count));
    memset(s_held, 0, sizeof(s_held));
    s_loaded = false;
    s_dirty = false;
    s_busy = false;
    s_seq = 0U;
    memset(s_published, 0, sizeof(s_published));
    s_have_published = false;
}
#endif
