/*
 * NeurOne Hub Control Program — session stop (SWR-SS-06)
 * See np_session_stop.h.
 */

#include "np_session_stop.h"

static np_session_stop_state_fn s_state;
static np_session_stop_abort_fn s_abort;

void np_session_stop_init(np_session_stop_state_fn state, np_session_stop_abort_fn abort_fn)
{
    s_state = state;
    s_abort = abort_fn;
}

np_hub_status_t np_session_stop_on_write(const uint8_t *data, size_t len)
{
    if (data == NULL || len != 1U || data[0] != NP_SESSION_STOP_VALUE) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (s_state == NULL || s_abort == NULL) {
        return NP_HUB_ERR_NO_SESSION;
    }
    const np_session_state_t st = s_state();
    if (st != NP_SESSION_RUNNING && st != NP_SESSION_PAUSED) {
        return NP_HUB_ERR_NO_SESSION;
    }
    s_abort(NP_ABORT_USER);
    return NP_HUB_OK;
}

static np_session_request_fn s_pause;
static np_session_request_fn s_resume;

void np_session_pause_init(np_session_request_fn pause_fn, np_session_request_fn resume_fn)
{
    s_pause  = pause_fn;
    s_resume = resume_fn;
}

np_hub_status_t np_session_pause_on_write(const uint8_t *data, size_t len)
{
    if (data == NULL || len != 1U ||
        (data[0] != NP_SESSION_PAUSE_VALUE && data[0] != NP_SESSION_RESUME_VALUE)) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    const np_session_request_fn fn = (data[0] == NP_SESSION_PAUSE_VALUE) ? s_pause : s_resume;
    if (fn == NULL) {
        return NP_HUB_ERR_NO_SESSION;
    }
    return fn();
}
