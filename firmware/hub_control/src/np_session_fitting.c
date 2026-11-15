/*
 * NeurOne Hub Control Program — the session's hardware fitting (SWR-HV-02)
 * See np_session_fitting.h.
 */

#include "np_session_fitting.h"

#include <string.h>

#include "np_module_map.h"

size_t np_fitting_build(uint8_t hw_version, const uint8_t *rules_stamp, uint8_t *out, size_t cap)
{
    const uint16_t n = np_module_map_socket_count();
    if (out == NULL || rules_stamp == NULL || n == 0U || cap < (size_t)NP_FITTING_HDR_LEN + n) {
        return 0U;
    }
    out[0] = (uint8_t)NP_FITTING_FORMAT;
    out[NP_FITTING_IDX_HW] = hw_version;
    memcpy(&out[NP_FITTING_IDX_STAMP], rules_stamp, NP_HUB_PROTO_RULES_STAMP_LEN);
    out[NP_FITTING_IDX_COUNT] = (uint8_t)n;
    for (uint16_t i = 0U; i < n; i++) {
        np_module_uid_t uid;
        uint8_t t;
        if (np_module_map_socket_uid(i, &uid) != NP_HUB_OK) {
            t = NP_FITTING_EMPTY;
        } else {
            const np_hub_mod_type_t kind = np_module_map_pbm_kind(i);
            t = (kind == NP_MOD_NONE) ? NP_FITTING_OTHER : (uint8_t)kind;
        }
        out[NP_FITTING_HDR_LEN + i] = t;
    }
    return (size_t)NP_FITTING_HDR_LEN + n;
}
