/*
 * NeurOne Hub Control Program — why a session is held. See np_session_hold.h.
 */
#include "np_session_hold.h"

#include "np_gatt_server.h"

_Static_assert(NP_HOLD_LEN == NP_GATT_SESSION_HOLD_LEN, "wire length is the generated constant");

static np_hold_source_fn s_source;

void np_hold_set_source(np_hold_source_fn source)
{
    s_source = source;
}

np_hub_status_t np_hold_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (buf == NULL || len_out == NULL || cap < NP_HOLD_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    buf[0] = (s_source == NULL) ? (uint8_t)NP_HOLD_NONE : (uint8_t)s_source();
    *len_out = NP_HOLD_LEN;
    return NP_HUB_OK;
}

void np_hold_notify(void)
{
    if (s_source == NULL) {
        return;
    }
    const uint8_t v = (uint8_t)s_source();
    (void)np_gatt_notify(NP_GATT_SESSION_HOLD_ID, &v, sizeof v);
}
