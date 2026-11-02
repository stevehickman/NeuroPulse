/*
 * NeurOne Hub Control Program — Socket Layouts by Hardware Version
 * Document: NP-FW-HUB-001 Rev 33, REQ-FWHUB-64, OI-FWHUB-27.  See np_socket_layouts.h.
 * The table is in np_socket_layouts.generated.c; this file only looks things up in it.
 */
#include "np_socket_layouts.h"

#include <stddef.h>

const np_socket_layout_t *np_socket_layout_find(uint8_t hardware_version)
{
    for (uint8_t i = 0U; i < NP_SOCKET_LAYOUT_COUNT; i++) {
        if (NP_SOCKET_LAYOUTS[i].hardware_version == hardware_version) {
            return &NP_SOCKET_LAYOUTS[i];
        }
    }
    return NULL;
}

bool np_socket_layout_valid_address(const np_socket_layout_t *layout, uint16_t socket_index)
{
    if (layout == NULL || socket_index >= layout->table_len) {
        return false;
    }
    return layout->geom[socket_index].present_in_helmet;
}
