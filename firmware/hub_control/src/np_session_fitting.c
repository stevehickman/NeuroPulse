/*
 * NeurOne Hub Control Program — the session's hardware fitting (SWR-HV-02, SWR-HV-03)
 * See np_session_fitting.h.
 */

#include "np_session_fitting.h"

#include <string.h>

#include "np_module_map.h"

size_t np_fitting_build(uint8_t hw_version, const uint8_t *rules_stamp, uint8_t *out, size_t cap)
{
    const uint16_t n = np_module_map_socket_count();
    const size_t total = (size_t)NP_FITTING_HDR_LEN + n + NP_HUB_RULES_STAMP_LEN;
    if (out == NULL || n == 0U || cap < total) {
        return 0U;
    }
    out[0] = (uint8_t)NP_FITTING_FORMAT;
    out[1] = hw_version;
    out[2] = (uint8_t)n;
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
    /* Copied, never read: the hub holds no rules and no register, so it states what the descriptor stated. */
    if (rules_stamp != NULL) {
        memcpy(&out[NP_FITTING_HDR_LEN + n], rules_stamp, NP_HUB_RULES_STAMP_LEN);
    } else {
        memset(&out[NP_FITTING_HDR_LEN + n], 0, NP_HUB_RULES_STAMP_LEN);
    }
    return total;
}
