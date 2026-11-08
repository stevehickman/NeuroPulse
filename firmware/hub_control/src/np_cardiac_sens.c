/*
 * NeurOne Hub Control Program — the cervical VNS cardiac cutoff's sensitivity. See np_cardiac_sens.h.
 */
#include "np_cardiac_sens.h"

#include <stdbool.h>

#include "np_cfg_store.h"
#include "np_gatt_server.h"
#include "np_spi_wire_types.h"
#include "np_transport.h"   /* np_transport_hal_enter/exit_critical */

#define CS_RECORD_VERSION  0x01U
#define CS_RECORD_LEN      2U      /* version, then the level */

_Static_assert(NP_GATT_CARDIAC_SENSITIVITY_LEN == 2U, "read length is the generated constant");
_Static_assert(NP_GATT_CARDIAC_SENSITIVITY_WRITE_LEN == 1U, "write length is the generated constant");

static uint8_t s_level;     /* 0 is the validated cutoff: the default and the floor */
static bool    s_loaded;
static bool    s_dirty;

static void try_load(void)
{
    uint8_t rec[CS_RECORD_LEN];
    const np_hub_status_t st = np_cfg_store_replicated_read(NP_CFG_FILE_CARDIAC_SENS, rec, sizeof rec);
    uint8_t level = 0U;

    if (st == NP_HUB_OK) {
        if (rec[0] != CS_RECORD_VERSION || rec[1] > NP_SAFETY_CARDIAC_SENS_MAX) {
            return;   /* not a record this firmware wrote, or a level it does not offer: stay unloaded, stay at the validated cutoff */
        }
        level = rec[1];
    } else if (st != NP_HUB_ERR_NOT_PRESENT) {
        return;       /* present but unreadable: keep trying, and do not overwrite it with the default */
    }

    np_transport_hal_enter_critical();
    if (!s_dirty) {   /* a write made before the load finished wins over the stored value */
        s_level = level;
    }
    s_loaded = true;
    np_transport_hal_exit_critical();
}

void np_cardiac_sens_init(void)
{
    np_transport_hal_enter_critical();
    s_level  = 0U;
    s_loaded = false;
    s_dirty  = false;
    np_transport_hal_exit_critical();
    try_load();
}

uint8_t np_cardiac_sens_level(void)
{
    np_transport_hal_enter_critical();
    const uint8_t v = s_level;
    np_transport_hal_exit_critical();
    return v;
}

void np_cardiac_sens_poll(void)
{
    if (!s_loaded) {
        try_load();
    }

    uint8_t rec[CS_RECORD_LEN];
    uint8_t snap;
    np_transport_hal_enter_critical();
    if (!s_dirty) {
        np_transport_hal_exit_critical();
        return;
    }
    snap = s_level;
    np_transport_hal_exit_critical();

    rec[0] = CS_RECORD_VERSION;
    rec[1] = snap;
    if (np_cfg_store_replicated_write(NP_CFG_FILE_CARDIAC_SENS, rec, sizeof rec) == NP_HUB_OK) {
        np_transport_hal_enter_critical();
        if (s_level == snap) { s_dirty = false; }
        np_transport_hal_exit_critical();
    }
}

np_hub_status_t np_cardiac_sens_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (buf == NULL || len_out == NULL || cap < NP_GATT_CARDIAC_SENSITIVITY_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    buf[0] = np_cardiac_sens_level();
    buf[1] = (uint8_t)NP_SAFETY_CARDIAC_SENS_MAX;
    *len_out = NP_GATT_CARDIAC_SENSITIVITY_LEN;
    return NP_HUB_OK;
}

np_hub_status_t np_cardiac_sens_write(const uint8_t *data, size_t len)
{
    if (data == NULL || len != NP_GATT_CARDIAC_SENSITIVITY_WRITE_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    const uint8_t v = data[0];
    if (v > NP_SAFETY_CARDIAC_SENS_MAX) {
        return NP_HUB_ERR_INVALID_ARG;      /* no level the helmet does not offer, and none below 0 */
    }
    np_transport_hal_enter_critical();
    const bool changed = (v != s_level);
    s_level = v;
    if (changed || !s_loaded) { s_dirty = true; }
    np_transport_hal_exit_critical();
    return NP_HUB_OK;
}
