/*
 * NeurOne Hub Control — PBM calibration bridge (OI-HUB-C06)
 * Implements np_pbm_cal_bridge.h. See that header for why the coupling is a
 * bridge and why the key is a module UID rather than a socket id.
 */

#include <string.h>

#include "np_pbm_cal_bridge.h"
#include "np_module_map.h"
#include "np_pbm1064_dose.h"
#include "np_pbm1064_session.h"

/*
 * The two UID representations are separate types by necessity (the libraries
 * cannot include each other) but identical by construction. Pin that here, in
 * the one translation unit that can see both, so a change to either side is a
 * build error rather than a silent truncation of a module identifier.
 */
typedef char _np_cal_bridge_uid_len_match[
    (NP_PBM1064_MODULE_UID_LEN == NP_HEXMAP_UID_LEN) ? 1 : -1];
typedef char _np_cal_bridge_uid_size_match[
    (sizeof(np_pbm1064_module_uid_t) == sizeof(np_module_uid_t)) ? 1 : -1];

/*
 * The calibration payload is a flat float[9] on the map side and an array of
 * np_pbm1064_cal_t on the PBM side. Ordering is 3*wl + coeff with coeff in
 * {K_PD1, K_PD2, K_ratio_nom} — stated in np_module_map.h and mirrored here.
 */
typedef char _np_cal_bridge_payload_shape[
    (NP_HEXMAP_CAL_FLOATS == (NP_PBM1064_WL_COUNT * 3u)) ? 1 : -1];

static bool bridge_cal_provider(const np_pbm1064_module_uid_t *uid,
                                np_pbm1064_cal_t cal_out[NP_PBM1064_WL_COUNT],
                                void *ctx)
{
    (void)ctx;
    if (uid == NULL || cal_out == NULL) {
        return false;
    }

    np_module_uid_t map_uid;
    memcpy(map_uid.b, uid->b, NP_HEXMAP_UID_LEN);

    float flat[NP_HEXMAP_CAL_FLOATS];
    if (np_module_map_get_cal(&map_uid, flat) != NP_HUB_OK) {
        /* No record for THIS module. Report the miss and write nothing; the
         * PBM side answers with firmware defaults and NP_CAL_DEFAULT. It must
         * never fall through to whatever another module has. */
        return false;
    }

    for (uint8_t w = 0; w < NP_PBM1064_WL_COUNT; w++) {
        cal_out[w].K_PD1       = flat[(w * 3u) + 0u];
        cal_out[w].K_PD2       = flat[(w * 3u) + 1u];
        cal_out[w].K_ratio_nom = flat[(w * 3u) + 2u];
        cal_out[w].valid       = true;   /* factory-sourced */
    }
    return true;
}

static bool bridge_socket_uid(uint8_t socket_id,
                              np_pbm1064_module_uid_t *uid_out,
                              void *ctx)
{
    (void)ctx;
    if (uid_out == NULL) {
        return false;
    }

    np_module_uid_t map_uid;
    if (np_module_map_socket_uid((uint16_t)socket_id, &map_uid) != NP_HUB_OK) {
        /* Empty, un-inventoried or out-of-domain socket. Fail closed: the
         * session loads defaults for this socket rather than guessing an
         * occupant. */
        return false;
    }
    memcpy(uid_out->b, map_uid.b, NP_PBM1064_MODULE_UID_LEN);
    return true;
}

void np_hub_pbm_cal_bridge_install(void)
{
    np_pbm1064_dose_set_cal_provider(bridge_cal_provider, NULL);
    np_pbm1064_session_set_uid_resolver(bridge_socket_uid, NULL);
}

void np_hub_pbm_cal_bridge_remove(void)
{
    np_pbm1064_dose_set_cal_provider(NULL, NULL);
    np_pbm1064_session_set_uid_resolver(NULL, NULL);
}
