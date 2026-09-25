/*
 * NeurOne Hub Control Program — Commanded-versus-Delivered Stimulation Cross-Check
 * Document: NP-FW-HUB-001 Rev 9 §8.3.1; NP-FMEA-001 §3.3 FMEA-M03-02 (OI-FMEA-09)
 *
 * See np_stim_xcheck.h.  IEC 62304 Class B — SW-02 hub control.
 */

#include "np_stim_xcheck.h"

#include <math.h>
#include <string.h>

typedef struct {
    uint16_t level_ua;      /* the current commanded now                          */
    uint16_t held_ua;       /* a higher previous level, still ramping down        */
    uint32_t hold_ms;       /* ramp-down duration still to start (pending)         */
    uint32_t hold_until_ms; /* held_ua is the bound until this session time        */
    bool     hold_pending;  /* hold_ms has not been anchored to an observation yet */
    uint8_t  excess_run;    /* consecutive observations over the bound             */
    bool     latched;       /* flag already raised this session                    */
} np_xcheck_chan_t;

static np_xcheck_chan_t s_chan[NP_SAFETY_MAX_CHANNELS];

/* The modalities whose telemetry carries a DELIVERED current read, and the
 * safety-MCU channel each one's commanded current is published on.  Every
 * other modality returns NP_SAFETY_MAX_CHANNELS and is not cross-checked. */
static uint8_t channel_for(np_hub_mod_type_t type)
{
    switch (type) {
        case NP_MOD_BES_TACS: return NP_SAFETY_CH_BES_TACS;
        case NP_MOD_TDCS:     return NP_SAFETY_CH_TDCS;
        default:              return NP_SAFETY_MAX_CHANNELS;
    }
}

void np_stim_xcheck_reset(void)
{
    /* The latch and the run are per session.  The envelope is not: it is the
     * driver's state, and a tDCS ramp-down (30 s) outlives the runner's 5 s
     * shutdown wait, so a session started soon after one ends may begin with
     * the previous ramp still running.  Zeroing the envelope would flag that
     * ramp.  The session clock restarts, so a live hold is re-anchored at this
     * session's first observation, for its full duration again.  That can
     * lengthen the hold and never shortens it. */
    for (uint8_t ch = 0U; ch < NP_SAFETY_MAX_CHANNELS; ch++) {
        np_xcheck_chan_t *c = &s_chan[ch];
        c->excess_run = 0U;
        c->latched    = false;
        if (c->held_ua > c->level_ua) {
            c->hold_pending = true;
        } else {
            c->held_ua      = 0U;
            c->hold_pending = false;
        }
    }
}

void np_stim_xcheck_clear(void)
{
    memset(s_chan, 0, sizeof(s_chan));
}

void np_stim_xcheck_commanded(uint8_t safety_ch, uint16_t commanded_ua,
                              uint32_t ramp_down_ms)
{
    if (safety_ch >= NP_SAFETY_MAX_CHANNELS) {
        return;
    }
    np_xcheck_chan_t *c = &s_chan[safety_ch];

    if (commanded_ua < c->level_ua && ramp_down_ms > 0U) {
        /* A decrease the driver ramps through: the old level stays the bound
         * while the ramp runs.  If a hold is already running and is higher, the
         * higher of the two is kept.  The ramp starts from wherever delivery
         * is now, which is never above either level. */
        uint16_t prev = c->level_ua;
        if (c->held_ua > prev) {
            prev = c->held_ua;
        }
        c->held_ua      = prev;
        c->hold_ms      = ramp_down_ms;
        c->hold_pending = true;
    }
    c->level_ua = commanded_ua;
}

/* The highest current the channel may be delivering at now_ms. */
static uint16_t bound_at(np_xcheck_chan_t *c, uint32_t now_ms)
{
    if (c->hold_pending) {
        c->hold_until_ms = now_ms + c->hold_ms;
        c->hold_pending  = false;
    }
    if (c->held_ua > c->level_ua && now_ms < c->hold_until_ms) {
        return c->held_ua;
    }
    c->held_ua = 0U;
    return c->level_ua;
}

bool np_stim_xcheck_observe(const np_telem_record_t *rec, uint32_t now_ms)
{
    if (rec == NULL) {
        return false;
    }
    uint8_t ch = channel_for(rec->mod_type);
    if (ch >= NP_SAFETY_MAX_CHANNELS) {
        return false;
    }
    np_xcheck_chan_t *c = &s_chan[ch];

    const float bound = (float)bound_at(c, now_ms);
    float margin = bound * (float)NP_STIM_XCHECK_TOL_PCT / 100.0f;
    if (margin < (float)NP_STIM_XCHECK_FLOOR_UA) {
        margin = (float)NP_STIM_XCHECK_FLOOR_UA;
    }

    /* Magnitude: DC polarity may read back negative.  A non-finite reading
     * cannot show delivery is within bounds, so it counts against it. */
    const float delivered = rec->data.stim.current_ua;
    const bool  excess    = !isfinite(delivered) ||
                            fabsf(delivered) > bound + margin;

    if (!excess) {
        c->excess_run = 0U;
        return false;
    }
    if (c->excess_run < NP_STIM_XCHECK_CONSECUTIVE) {
        c->excess_run++;
    }
    if (c->excess_run >= NP_STIM_XCHECK_CONSECUTIVE && !c->latched) {
        c->latched = true;
        return true;
    }
    return false;
}

bool np_stim_xcheck_latched(uint8_t safety_ch)
{
    return (safety_ch < NP_SAFETY_MAX_CHANNELS) && s_chan[safety_ch].latched;
}
