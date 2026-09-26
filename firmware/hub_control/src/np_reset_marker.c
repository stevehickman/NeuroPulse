/*
 * np_reset_marker.c — the durable factory-reset marker in the Config partition
 * Document: NP-FW-NVRAM-001 Rev 3 §3.4.1 (option A; OI-NVRAM-05)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * See np_reset_marker.h.
 */

#include "np_reset_marker.h"

#include <string.h>

#include "np_cfg_store.h"
#include "np_factory_reset.h"

np_fr_marker_state_t np_reset_marker_classify(np_hub_status_t mount_st,
                                              np_hub_status_t read_st)
{
    if (mount_st == NP_HUB_ERR_STORE_INTEGRITY) {
        return NP_FR_MARKER_NO_STORE;
    }
    if (mount_st != NP_HUB_OK) {
        return NP_FR_MARKER_UNKNOWN;
    }
    switch (read_st) {
    case NP_HUB_OK:                  return NP_FR_MARKER_PRESENT;
    case NP_HUB_ERR_NOT_PRESENT:     return NP_FR_MARKER_ABSENT;
    case NP_HUB_ERR_STORE_INTEGRITY: return NP_FR_MARKER_PRESENT;
    default:                         return NP_FR_MARKER_UNKNOWN;
    }
}

np_reset_status_t np_factory_reset_hal_marker_write(void)
{
    const uint8_t *p = (const uint8_t *)NP_RESET_MARKER_PAYLOAD;
    return (np_cfg_store_replicated_write(NP_CFG_FILE_RESET_MARKER, p,
                                          NP_RESET_MARKER_PAYLOAD_LEN) == NP_HUB_OK)
               ? NP_RESET_OK
               : NP_RESET_ERR_MARKER;
}

np_fr_marker_state_t np_factory_reset_hal_marker_state(void)
{
    np_hub_status_t mount_st = np_cfg_store_mount();
    np_hub_status_t read_st  = NP_HUB_ERR_GENERIC;

    if (mount_st == NP_HUB_OK) {
        uint8_t payload[NP_RESET_MARKER_PAYLOAD_LEN];
        read_st = np_cfg_store_replicated_read(NP_CFG_FILE_RESET_MARKER,
                                               payload, sizeof(payload));
        /* The envelope's CRC has already decided the copy is intact; a
         * payload that is not ours is a store fault, and a fault near a
         * reset marker is read as a marker (see the classify table). */
        if (read_st == NP_HUB_OK &&
            memcmp(payload, NP_RESET_MARKER_PAYLOAD,
                   NP_RESET_MARKER_PAYLOAD_LEN) != 0) {
            read_st = NP_HUB_ERR_STORE_INTEGRITY;
        }
    }
    return np_reset_marker_classify(mount_st, read_st);
}
