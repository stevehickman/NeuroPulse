/*
 * NeurOne Hub Control Program — coordinated ramp-up (SWR-SS-05, SWR-SS-07)
 * Document: NP-SW-001 §6.3
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * Two modalities ramp up: tDCS (30 s or longer, set by the protocol, shorter when a ramp-down is reversed part way)
 * and cervical VNS (the library's fixed ramp, after its impedance and cardiac-baseline checks).  When a session
 * starts, or resumes after a pause, with either of them, the hub brings the protocol up as one:
 *
 *   - Nothing else starts until every ramp has finished, and the protocol clock stands still until then.
 *   - When both ramp, the LONGER ramp starts first and the shorter starts later, so that both end together.
 *   - Every other modality (PBM, audio, visual, BES/tACS) then starts at once.
 *
 * With neither ramping, everything starts at once.  This is a pure state machine: the runner supplies the two
 * modules' ramp reports each tick and starts what the step says to start.  It never touches a module.
 *
 * The cervical VNS ramp begins only after its checks, whose length is not known in advance.  When tDCS is the longer
 * ramp, cervical VNS is started NP_RAMP_CVNS_LEAD_ESTIMATE_MS before the tDCS ramp ends, plus its own ramp; the two end
 * together only if the checks take about that long.  If they take longer, tDCS holds at its level until cervical VNS
 * is up; if shorter, cervical VNS holds.  Nothing else starts until both are done either way.  When cervical VNS is the
 * longer ramp, the tDCS start is timed from the cervical VNS ramp itself and the ends coincide.
 */

#ifndef NP_RAMP_PLAN_H
#define NP_RAMP_PLAN_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NP_RAMP_NONE     = 0,   /* not ramping: steady, stopped, or finished */
    NP_RAMP_DOWN     = 1,   /* ramping down after a pause (stimulation still on) */
    NP_RAMP_HELD     = 2,   /* paused, nothing ramping */
    NP_RAMP_PRECHECK = 3,   /* cervical VNS impedance / cardiac baseline, before its ramp */
    NP_RAMP_UP       = 4    /* ramping up */
} np_ramp_phase_t;

#define NP_RAMP_UNKNOWN_MS   0xFFFFFFFFU

typedef struct {
    np_ramp_phase_t phase;
    uint8_t         pct;            /* 0..100 of this ramp */
    uint32_t        remaining_ms;   /* NP_RAMP_UNKNOWN_MS while it cannot be known (PRECHECK) */
} np_ramp_report_t;

/* How long the cervical VNS checks (impedance, then at least NP_CVNS_BASELINE_BEATS_MIN beats) are expected to take
 * when it is started ahead of the end of a longer tDCS ramp.  PROVISIONAL: no measurement; see
 * docs/status/unjustified-choices.md. */
#define NP_RAMP_CVNS_LEAD_ESTIMATE_MS   10000U

typedef struct {
    bool     want, started, done;
    uint32_t ramp_ms;           /* the ramp this participant will take once it starts */
} np_ramp_part_t;

typedef struct {
    np_ramp_part_t tdcs, cvns;
    bool           open;
} np_ramp_plan_t;

typedef struct {
    bool start_tdcs;            /* start tDCS now */
    bool start_cvns;            /* start cervical VNS now */
    bool release_others;        /* every ramp has finished: start everything else */
} np_ramp_step_t;

/* Open a plan.  A participant that is not wanted is never started and never waited for. */
void np_ramp_plan_begin(np_ramp_plan_t *p, bool want_tdcs, uint32_t tdcs_ramp_ms,
                        bool want_cvns, uint32_t cvns_ramp_ms);

/* The module refused the start, or it failed: stop waiting for it. */
void np_ramp_plan_refused(np_ramp_plan_t *p, bool tdcs, bool cvns);

/* One decision from the two modules' current reports.  A participant the step says to start is marked started. */
np_ramp_step_t np_ramp_plan_step(np_ramp_plan_t *p, const np_ramp_report_t *tdcs, const np_ramp_report_t *cvns);

/* The ramp that finishes last, for the progress frame: the phase, percent and time left of the gating ramp. */
np_ramp_report_t np_ramp_plan_gating(const np_ramp_plan_t *p, const np_ramp_report_t *tdcs,
                                     const np_ramp_report_t *cvns);

#endif /* NP_RAMP_PLAN_H */
