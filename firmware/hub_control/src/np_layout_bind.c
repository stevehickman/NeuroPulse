/*
 * NeurOne Hub Control Program — Binding the Helmet's Socket Layout
 * Document: NP-FW-HUB-001 Rev 34, REQ-FWHUB-64, OI-FWHUB-27.  See np_layout_bind.h.
 */
#include "np_layout_bind.h"

#include <stddef.h>

#include "np_module_map.h"

static const np_socket_layout_t *s_bound;

np_hub_status_t np_layout_bind(uint8_t hardware_version)
{
    const np_socket_layout_t *row = np_socket_layout_find(hardware_version);
    s_bound = NULL;
    if (row == NULL) {
        return NP_HUB_ERR_NOT_PRESENT;
    }
    np_hub_status_t rc = np_module_map_init(row->geom, row->table_len);
    if (rc != NP_HUB_OK) {
        return rc;
    }
    s_bound = row;
    return NP_HUB_OK;
}

const np_socket_layout_t *np_layout_bound(void)
{
    return s_bound;
}

np_hub_status_t np_layout_admit(uint8_t device_hw_version)
{
    if (s_bound == NULL || s_bound->hardware_version != device_hw_version) {
        return NP_HUB_ERR_HW_VERSION_MISMATCH;
    }
    return NP_HUB_OK;
}
