/*
 * NeurOne Hub Control Program — the pause time limit (SWR-SS-05)
 * See np_pause_limit.h.
 */

#include "np_pause_limit.h"

#include <string.h>

#include "np_cfg_store.h"
#include "np_gatt_server.h"
#include "np_transport.h"   /* np_transport_hal_enter/exit_critical */

#define RECORD_VERSION   0x01U
#define RECORD_LEN       5U     /* version, then the limit as a little-endian uint32 */

_Static_assert(NP_GATT_PAUSE_LIMIT_LEN == 4U, "wire length is the generated constant");

static uint32_t s_limit_s;     /* seconds; 0 = none */
static bool     s_loaded;      /* the stored record was read (or is absent) */
static bool     s_dirty;       /* a write the store has not taken yet       */

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void try_load(void)
{
    uint8_t rec[RECORD_LEN];
    const np_hub_status_t st = np_cfg_store_replicated_read(NP_CFG_FILE_PAUSE_LIMIT, rec, sizeof rec);
    uint32_t v = NP_PAUSE_LIMIT_NONE;

    if (st == NP_HUB_OK) {
        if (rec[0] != RECORD_VERSION) {
            return;   /* not a record this firmware wrote: stay unloaded, stay at the default */
        }
        v = get_u32(&rec[1]);
    } else if (st != NP_HUB_ERR_NOT_PRESENT) {
        return;       /* present but unreadable: keep trying; do not overwrite it with the default */
    }

    np_transport_hal_enter_critical();
    if (!s_dirty) {   /* a write made before the load finished wins over the stored value */
        s_limit_s = v;
    }
    s_loaded = true;
    np_transport_hal_exit_critical();
}

void np_pause_limit_init(void)
{
    np_transport_hal_enter_critical();
    s_limit_s = NP_PAUSE_LIMIT_NONE;
    s_loaded  = false;
    s_dirty   = false;
    np_transport_hal_exit_critical();
    try_load();
}

uint32_t np_pause_limit_seconds(void)
{
    np_transport_hal_enter_critical();
    const uint32_t v = s_limit_s;
    np_transport_hal_exit_critical();
    return v;
}

void np_pause_limit_poll(void)
{
    if (!s_loaded) {
        try_load();
    }

    uint8_t  rec[RECORD_LEN];
    uint32_t snap;
    np_transport_hal_enter_critical();
    if (!s_dirty) {
        np_transport_hal_exit_critical();
        return;
    }
    snap = s_limit_s;
    np_transport_hal_exit_critical();

    rec[0] = RECORD_VERSION;
    put_u32(&rec[1], snap);
    if (np_cfg_store_replicated_write(NP_CFG_FILE_PAUSE_LIMIT, rec, sizeof rec) == NP_HUB_OK) {
        np_transport_hal_enter_critical();
        if (s_limit_s == snap) {   /* nothing changed while it was written */
            s_dirty = false;
        }
        np_transport_hal_exit_critical();
    }
}

np_hub_status_t np_pause_limit_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (buf == NULL || len_out == NULL || cap < NP_GATT_PAUSE_LIMIT_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    put_u32(buf, np_pause_limit_seconds());
    *len_out = NP_GATT_PAUSE_LIMIT_LEN;
    return NP_HUB_OK;
}

np_hub_status_t np_pause_limit_write(const uint8_t *data, size_t len)
{
    if (data == NULL || len != NP_GATT_PAUSE_LIMIT_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    const uint32_t v = get_u32(data);
    np_transport_hal_enter_critical();
    const bool changed = (v != s_limit_s);
    s_limit_s = v;
    if (changed || !s_loaded) {
        s_dirty = true;
    }
    np_transport_hal_exit_critical();
    return NP_HUB_OK;
}
