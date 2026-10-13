/*
 * NeurOne Hub Control Program — pause bookkeeping (SWR-SS-05)
 * See np_session_pause.h.
 */

#include "np_session_pause.h"

void np_session_clock_start(np_session_clock_t *c, uint32_t now)
{
    c->start        = now;
    c->paused_total = 0U;
    c->pause_start  = 0U;
    c->paused       = false;
}

bool np_session_clock_pause(np_session_clock_t *c, uint32_t now)
{
    if (c->paused) {
        return false;
    }
    c->paused      = true;
    c->pause_start = now;
    return true;
}

bool np_session_clock_resume(np_session_clock_t *c, uint32_t now)
{
    if (!c->paused) {
        return false;
    }
    c->paused_total += now - c->pause_start;
    c->paused = false;
    return true;
}

uint32_t np_session_clock_elapsed(const np_session_clock_t *c, uint32_t now)
{
    const uint32_t at = c->paused ? c->pause_start : now;
    return at - c->start - c->paused_total;
}

void np_session_clock_close(np_session_clock_t *c, uint32_t now)
{
    (void)np_session_clock_resume(c, now);
}

void np_slot_track_reset(np_slot_track_t *t)
{
    for (uint8_t i = 0U; i < NP_HUB_SLOT_MAX; i++) {
        t->current[i] = NP_SLOT_NONE;
        t->held[i]    = NP_SLOT_NONE;
    }
}

void np_slot_track_started(np_slot_track_t *t, uint8_t slot, uint16_t cmd_index)
{
    if (slot < NP_HUB_SLOT_MAX) {
        t->current[slot] = (int16_t)cmd_index;
    }
}

void np_slot_track_stopped(np_slot_track_t *t, uint8_t slot)
{
    if (slot < NP_HUB_SLOT_MAX) {
        t->current[slot] = NP_SLOT_NONE;
    }
}

uint8_t np_slot_track_pause(np_slot_track_t *t, uint8_t slots_out[NP_HUB_SLOT_MAX])
{
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < NP_HUB_SLOT_MAX; i++) {
        if (t->current[i] != NP_SLOT_NONE) {
            t->held[i] = t->current[i];
            t->current[i] = NP_SLOT_NONE;
            slots_out[n++] = i;
        }
    }
    return n;
}

uint8_t np_slot_track_resume(np_slot_track_t *t, uint16_t cmds_out[NP_HUB_SLOT_MAX])
{
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < NP_HUB_SLOT_MAX; i++) {
        if (t->held[i] != NP_SLOT_NONE) {
            cmds_out[n++] = (uint16_t)t->held[i];
            t->held[i] = NP_SLOT_NONE;
        }
    }
    return n;
}
