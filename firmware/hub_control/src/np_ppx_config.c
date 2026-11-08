/*
 * NeurOne Hub Control Program — the detector's clinician settings (REQ-FWHUB-71)
 * See np_ppx_config.h.
 */
#include "np_ppx_config.h"

#include <stdbool.h>

#include "np_cfg_store.h"
#include "np_gatt_server.h"
#include "np_ppx_detect.h"
#include "np_ppx_policy.h"
#include "np_transport.h"   /* np_transport_hal_enter/exit_critical */

#define PPX_RECORD_VERSION  0x01U
#define PPX_RECORD_LEN      2U      /* version, then the flags */

_Static_assert(NP_GATT_PPX_CONFIG_LEN == 2U, "read length is the generated constant");
_Static_assert(NP_GATT_PPX_CONFIG_WRITE_LEN == 1U, "write length is the generated constant");

static uint8_t s_flags = NP_PPX_CFG_DEFAULT;
static bool    s_loaded;
static bool    s_dirty;

static void apply(uint8_t flags)
{
    const np_ppx_config_t cfg = {
        .stop_on_occipital_only = (flags & NP_PPX_CFG_STOP_OCCIPITAL) != 0U,
        .manual_override        = (flags & NP_PPX_CFG_OVERRIDE) != 0U,
    };
    np_ppx_policy_set_config(cfg);
}

static void try_load(void)
{
    uint8_t rec[PPX_RECORD_LEN];
    const np_hub_status_t st = np_cfg_store_replicated_read(NP_CFG_FILE_PPX_CONFIG, rec, sizeof rec);
    uint8_t flags = NP_PPX_CFG_DEFAULT;

    if (st == NP_HUB_OK) {
        if (rec[0] != PPX_RECORD_VERSION || (rec[1] & (uint8_t)~NP_PPX_CFG_KNOWN_BITS) != 0U) {
            return;   /* not a record this firmware wrote: stay unloaded, stay at the protective defaults */
        }
        flags = rec[1];
        if (np_ppx_policy_evidence() == NP_PPX_EVIDENCE_NONE) {
            flags &= (uint8_t)~NP_PPX_CFG_OVERRIDE;   /* in force only in RAM: a stored override waits for its evidence, it is not woken by it */
        }
    } else if (st != NP_HUB_ERR_NOT_PRESENT) {
        return;       /* present but unreadable: keep trying, and do not overwrite it with the defaults */
    }

    np_transport_hal_enter_critical();
    if (!s_dirty) {   /* a write made before the load finished wins over the stored value */
        s_flags = flags;
        apply(flags);
    }
    s_loaded = true;
    np_transport_hal_exit_critical();
}

void np_ppx_config_init(void)
{
    np_transport_hal_enter_critical();
    s_flags  = NP_PPX_CFG_DEFAULT;
    s_loaded = false;
    s_dirty  = false;
    apply(s_flags);
    np_transport_hal_exit_critical();
    try_load();
}

uint8_t np_ppx_config_flags(void)
{
    np_transport_hal_enter_critical();
    const uint8_t v = s_flags;
    np_transport_hal_exit_critical();
    return v;
}

void np_ppx_config_poll(void)
{
    if (!s_loaded) {
        try_load();
    }

    uint8_t rec[PPX_RECORD_LEN];
    uint8_t snap;
    np_transport_hal_enter_critical();
    if (!s_dirty) {
        np_transport_hal_exit_critical();
        return;
    }
    snap = s_flags;
    np_transport_hal_exit_critical();

    rec[0] = PPX_RECORD_VERSION;
    rec[1] = snap;
    if (np_cfg_store_replicated_write(NP_CFG_FILE_PPX_CONFIG, rec, sizeof rec) == NP_HUB_OK) {
        np_transport_hal_enter_critical();
        if (s_flags == snap) {
            s_dirty = false;
        }
        np_transport_hal_exit_critical();
    }
}

np_hub_status_t np_ppx_config_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (buf == NULL || len_out == NULL || cap < NP_GATT_PPX_CONFIG_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    buf[0] = np_ppx_config_flags();
    buf[1] = (uint8_t)np_ppx_policy_evidence();
    *len_out = NP_GATT_PPX_CONFIG_LEN;
    return NP_HUB_OK;
}

np_hub_status_t np_ppx_config_write(const uint8_t *data, size_t len)
{
    if (data == NULL || len != NP_GATT_PPX_CONFIG_WRITE_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    const uint8_t v = data[0];
    if ((v & (uint8_t)~NP_PPX_CFG_KNOWN_BITS) != 0U) {
        return NP_HUB_ERR_INVALID_ARG;      /* a bit this firmware does not know is not half-understood */
    }
    if ((v & NP_PPX_CFG_OVERRIDE) != 0U && np_ppx_policy_evidence() == NP_PPX_EVIDENCE_NONE) {
        return NP_HUB_ERR_INVALID_ARG;      /* an override with nothing to override is refused, so it cannot be pre-set and wake up
                                             * later when the evidence level is raised: a clinician sets it when it means something */
    }
    np_transport_hal_enter_critical();
    const bool changed = (v != s_flags);
    s_flags = v;
    if (changed || !s_loaded) { s_dirty = true; }
    const np_ppx_config_t next = {
        .stop_on_occipital_only = (v & NP_PPX_CFG_STOP_OCCIPITAL) != 0U,
        .manual_override        = (v & NP_PPX_CFG_OVERRIDE) != 0U,
    };
    np_ppx_policy_set_config(next);
    np_transport_hal_exit_critical();
    /* Protecting more takes effect at once, in a session already running. Protecting less waits for the next session:
     * np_ppx_init() reads the policy's occipital switch afresh, and a detector already watching is not told to look less. */
    if (next.stop_on_occipital_only) {
        np_ppx_set_stop_on_occipital_only(true);
    }
    return NP_HUB_OK;
}
