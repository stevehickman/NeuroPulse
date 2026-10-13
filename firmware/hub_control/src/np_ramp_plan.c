/*
 * NeurOne Hub Control Program — coordinated ramp-up (SWR-SS-05, SWR-SS-07)
 * See np_ramp_plan.h.
 */

#include "np_ramp_plan.h"

void np_ramp_plan_begin(np_ramp_plan_t *p, bool want_tdcs, uint32_t tdcs_ramp_ms,
                        bool want_cvns, uint32_t cvns_ramp_ms)
{
    p->tdcs.want = want_tdcs; p->tdcs.started = false; p->tdcs.done = !want_tdcs; p->tdcs.ramp_ms = tdcs_ramp_ms;
    p->cvns.want = want_cvns; p->cvns.started = false; p->cvns.done = !want_cvns; p->cvns.ramp_ms = cvns_ramp_ms;
    p->open = true;
}

void np_ramp_plan_refused(np_ramp_plan_t *p, bool tdcs, bool cvns)
{
    if (tdcs) { p->tdcs.started = true; p->tdcs.done = true; }
    if (cvns) { p->cvns.started = true; p->cvns.done = true; }
}

static bool ramping_up(const np_ramp_report_t *r)
{
    return r->phase == NP_RAMP_UP;
}

np_ramp_step_t np_ramp_plan_step(np_ramp_plan_t *p, const np_ramp_report_t *tdcs, const np_ramp_report_t *cvns)
{
    np_ramp_step_t out = { false, false, false };

    /* A started participant is done when its module stops reporting a ramp: steady, or ended (fault, stop). */
    if (p->tdcs.started && !p->tdcs.done && tdcs->phase == NP_RAMP_NONE) { p->tdcs.done = true; }
    if (p->cvns.started && !p->cvns.done && cvns->phase == NP_RAMP_NONE) { p->cvns.done = true; }

    const bool both     = p->tdcs.want && p->cvns.want;
    const bool tdcs_long = p->tdcs.ramp_ms > p->cvns.ramp_ms;

    /* Evaluate against what was true at the start of the step, so a participant started now is not also judged now. */
    const bool t_was_started = p->tdcs.started;
    const bool c_was_started = p->cvns.started;

    if (p->tdcs.want && !p->tdcs.started) {
        bool go = true;
        if (both && !tdcs_long) {
            /* cervical VNS is the longer ramp: start tDCS so it ends with it */
            go = p->cvns.done ||
                 (c_was_started && ramping_up(cvns) && cvns->remaining_ms <= p->tdcs.ramp_ms);
        }
        if (go) { p->tdcs.started = true; out.start_tdcs = true; }
    }
    if (p->cvns.want && !p->cvns.started) {
        bool go = true;
        if (both && tdcs_long) {
            /* tDCS is the longer ramp: start cervical VNS so its checks and ramp end with it */
            go = p->tdcs.done ||
                 (t_was_started && ramping_up(tdcs) &&
                  tdcs->remaining_ms <= p->cvns.ramp_ms + NP_RAMP_CVNS_LEAD_ESTIMATE_MS);
        }
        if (go) { p->cvns.started = true; out.start_cvns = true; }
    }

    out.release_others = p->tdcs.done && p->cvns.done;
    if (out.release_others) { p->open = false; }
    return out;
}

np_ramp_report_t np_ramp_plan_gating(const np_ramp_plan_t *p, const np_ramp_report_t *tdcs,
                                     const np_ramp_report_t *cvns)
{
    np_ramp_report_t g = { NP_RAMP_NONE, 0U, 0U };
    const bool t_live = p->tdcs.want && !p->tdcs.done;
    const bool c_live = p->cvns.want && !p->cvns.done;

    if (t_live && c_live) {
        /* The one that finishes last.  An unknown time (checks still running) is the later. */
        const bool c_later = !(p->cvns.started && cvns->remaining_ms != NP_RAMP_UNKNOWN_MS) ||
                             !(p->tdcs.started && tdcs->remaining_ms != NP_RAMP_UNKNOWN_MS) ||
                             cvns->remaining_ms >= tdcs->remaining_ms;
        if (p->cvns.started && p->tdcs.started) { return c_later ? *cvns : *tdcs; }
        /* one has not started: it is the later by definition; it is waiting */
        g.phase = NP_RAMP_UP; g.pct = 0U;
        g.remaining_ms = (p->tdcs.started ? p->cvns.ramp_ms : p->tdcs.ramp_ms);
        if (p->tdcs.started && tdcs->remaining_ms != NP_RAMP_UNKNOWN_MS && tdcs->remaining_ms > g.remaining_ms) {
            g.remaining_ms = tdcs->remaining_ms;
        }
        return g;
    }
    if (t_live) { return p->tdcs.started ? *tdcs : (np_ramp_report_t){ NP_RAMP_UP, 0U, p->tdcs.ramp_ms }; }
    if (c_live) { return p->cvns.started ? *cvns : (np_ramp_report_t){ NP_RAMP_UP, 0U, p->cvns.ramp_ms }; }
    return g;
}
