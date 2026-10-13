/*
 * NeurOne Hub Control Program — session progress (SWR-SS-07)
 * See np_session_progress.h.
 */

#include "np_session_progress.h"

#include <string.h>

#include "np_gatt_server.h"

_Static_assert(NP_PROGRESS_LEN == NP_GATT_SESSION_PROGRESS_LEN, "wire length is the generated constant");

static np_progress_source_fn s_source;

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

void np_progress_build(const np_progress_in_t *in, uint8_t out[NP_PROGRESS_LEN])
{
    uint16_t mask = 0U;
    uint8_t  part = NP_PROGRESS_NO_PART;
    uint32_t elapsed;

    memset(out, 0, NP_PROGRESS_LEN);
    if (in == NULL) {
        out[1] = NP_PROGRESS_NO_PART;
        return;
    }
    elapsed = (in->total_ms != 0U && in->elapsed_ms > in->total_ms) ? in->total_ms : in->elapsed_ms;

    /* A command runs from its start until start + duration; a duration of 0 is "until the session ends". */
    if (in->cmds != NULL) {
        for (uint8_t i = 0U; i < in->cmd_count; i++) {
            const np_session_cmd_t *c = &in->cmds[i];
            const uint64_t end = (c->duration_ms == 0U) ? UINT64_MAX
                                                        : (uint64_t)c->start_ms + (uint64_t)c->duration_ms;
            const int running = (in->state == NP_SESSION_RUNNING || in->state == NP_SESSION_PAUSED) &&
                                c->start_ms <= elapsed && (uint64_t)elapsed < end;
            if (running) {
                part = i;
                if ((unsigned)c->mod_type < 16U) {
                    mask |= (uint16_t)(1U << (unsigned)c->mod_type);
                }
            }
        }
    }

    out[0] = (uint8_t)in->state;
    out[1] = part;
    out[2] = (uint8_t)(mask & 0xFFU);
    out[3] = (uint8_t)(mask >> 8);
    put_u32(&out[4], elapsed);
    put_u32(&out[8], (in->total_ms > elapsed) ? (in->total_ms - elapsed) : 0U);
    put_u32(&out[12], in->total_ms);
}

void np_progress_set_source(np_progress_source_fn source)
{
    s_source = source;
}

np_hub_status_t np_progress_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (buf == NULL || len_out == NULL || cap < NP_PROGRESS_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if (s_source == NULL) {
        return NP_HUB_ERR_NO_SESSION;
    }
    s_source(buf);
    *len_out = NP_PROGRESS_LEN;
    return NP_HUB_OK;
}

void np_progress_notify(void)
{
    uint8_t frame[NP_PROGRESS_LEN];
    if (s_source == NULL) {
        return;
    }
    s_source(frame);
    (void)np_gatt_notify(NP_GATT_SESSION_PROGRESS_ID, frame, sizeof(frame));
}
