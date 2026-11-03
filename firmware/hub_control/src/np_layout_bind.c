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
        np_module_map_unbind();         /* no stale geometry of another version may go on answering */
        return NP_HUB_ERR_NOT_PRESENT;
    }
    np_hub_status_t rc = np_module_map_init(row->geom, row->table_len);
    if (rc != NP_HUB_OK) {
        np_module_map_unbind();
        return rc;
    }
    s_bound = row;
    return NP_HUB_OK;
}

const np_socket_layout_t *np_layout_bound(void)
{
    return s_bound;
}

np_hub_status_t np_layout_follow_report(uint8_t reported_version, bool report_received, bool *rebound_out)
{
    if (rebound_out != NULL) {
        *rebound_out = false;
    }
    if (!report_received) {
        return NP_HUB_OK;                       /* the default binding stands until a report says otherwise */
    }
    if (s_bound != NULL && s_bound->hardware_version == reported_version) {
        return NP_HUB_OK;                       /* already the helmet's layout: leave the module map alone */
    }
    if (rebound_out != NULL) {
        *rebound_out = true;
    }
    return np_layout_bind(reported_version);
}

np_hub_status_t np_layout_check_protocol_addresses(const np_session_desc_t *desc)
{
    if (desc == NULL) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    for (uint8_t i = 0U; i < desc->cmd_count && i < NP_HUB_PROTO_CMD_MAX; i++) {
        const np_session_cmd_t *c = &desc->cmds[i];
        if (c->target_kind != (uint8_t)NP_PROTO_TARGET_SOCKET_MASK) {
            continue;
        }
        for (unsigned s = 0U; s < (unsigned)NP_HUB_SOCKET_MASK_BYTES * 8U; s++) {
            if (((c->socket_mask[s / 8U] >> (s % 8U)) & 1U) != 0U &&
                !np_module_map_address_valid((uint16_t)s)) {
                return NP_HUB_ERR_INVALID_ARG;
            }
        }
    }
    return NP_HUB_OK;
}

np_hub_status_t np_layout_admit(uint8_t device_hw_version)
{
    if (s_bound == NULL || s_bound->hardware_version != device_hw_version) {
        return NP_HUB_ERR_NO_LAYOUT;
    }
    return NP_HUB_OK;
}
