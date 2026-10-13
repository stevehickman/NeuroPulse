/*
 * Host tests — coordinated ramp-up (SWR-SS-05).  The plan is a pure state machine: these tests play the two modules'
 * reports into it and check what it starts and when.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "np_ramp_plan.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

static const np_ramp_report_t NONE = { NP_RAMP_NONE, 0U, 0U };
static np_ramp_report_t up(uint32_t remaining) { np_ramp_report_t r = { NP_RAMP_UP, 0U, remaining }; return r; }
static np_ramp_report_t pre(void) { np_ramp_report_t r = { NP_RAMP_PRECHECK, 0U, NP_RAMP_UNKNOWN_MS }; return r; }

/* @verifies SWR-SS-05 */
static void test_nothing_ramps_everything_starts_at_once(void)
{
    np_ramp_plan_t p;
    np_ramp_plan_begin(&p, false, 0U, false, 0U);
    np_ramp_step_t s = np_ramp_plan_step(&p, &NONE, &NONE);
    check(!s.start_tdcs && !s.start_cvns && s.release_others, "ramp plan: with no ramping modality the rest starts at once");
}

/* @verifies SWR-SS-05 */
static void test_tdcs_alone_holds_the_rest_until_its_ramp_ends(void)
{
    np_ramp_plan_t p;
    np_ramp_plan_begin(&p, true, 30000U, false, 0U);
    np_ramp_step_t s = np_ramp_plan_step(&p, &NONE, &NONE);
    check(s.start_tdcs && !s.release_others, "ramp plan: tDCS alone starts at once and the rest waits");
    np_ramp_report_t r = up(12000U);
    s = np_ramp_plan_step(&p, &r, &NONE);
    check(!s.start_tdcs && !s.release_others, "ramp plan: still waiting mid-ramp");
    s = np_ramp_plan_step(&p, &NONE, &NONE);
    check(s.release_others, "ramp plan: the rest starts when tDCS stops ramping");
}

/* @verifies SWR-SS-05 */
static void test_cvns_alone_holds_the_rest_through_its_checks_and_ramp(void)
{
    np_ramp_plan_t p;
    np_ramp_plan_begin(&p, false, 0U, true, 10000U);
    np_ramp_step_t s = np_ramp_plan_step(&p, &NONE, &NONE);
    check(s.start_cvns && !s.release_others, "ramp plan: cervical VNS alone starts at once");
    np_ramp_report_t c = pre();
    s = np_ramp_plan_step(&p, &NONE, &c);
    check(!s.release_others, "ramp plan: the rest waits through the cervical VNS checks");
    c = up(4000U);
    s = np_ramp_plan_step(&p, &NONE, &c);
    check(!s.release_others, "ramp plan: and through its ramp");
    s = np_ramp_plan_step(&p, &NONE, &NONE);
    check(s.release_others, "ramp plan: then starts");
}

/* @verifies SWR-SS-05 */
static void test_longer_tdcs_ramp_starts_first_and_cvns_is_timed_to_end_with_it(void)
{
    np_ramp_plan_t p;
    np_ramp_plan_begin(&p, true, 30000U, true, 10000U);
    np_ramp_step_t s = np_ramp_plan_step(&p, &NONE, &NONE);
    check(s.start_tdcs && !s.start_cvns, "ramp plan: the longer ramp (tDCS) starts first");

    np_ramp_report_t t = up(25000U);
    s = np_ramp_plan_step(&p, &t, &NONE);
    check(!s.start_cvns, "ramp plan: cervical VNS waits while tDCS has long to go");
    t = up(NP_RAMP_CVNS_LEAD_ESTIMATE_MS + 10000U);
    s = np_ramp_plan_step(&p, &t, &NONE);
    check(s.start_cvns && !s.release_others, "ramp plan: cervical VNS starts its checks that long before tDCS ends, plus its ramp");

    t = up(5000U); np_ramp_report_t c = up(5000U);
    s = np_ramp_plan_step(&p, &t, &c);
    check(!s.release_others, "ramp plan: the rest waits while either is still ramping");
    c = (np_ramp_report_t){ NP_RAMP_NONE, 100U, 0U };
    s = np_ramp_plan_step(&p, &t, &c);
    check(!s.release_others, "ramp plan: cervical VNS up first does not release the rest while tDCS ramps");
    s = np_ramp_plan_step(&p, &NONE, &c);
    check(s.release_others, "ramp plan: both done releases the rest");
}

/* @verifies SWR-SS-05 */
static void test_longer_cvns_ramp_starts_first_and_tdcs_ends_with_it(void)
{
    np_ramp_plan_t p;
    np_ramp_plan_begin(&p, true, 6000U, true, 10000U);   /* a reversed ramp-down left tDCS 6 s to go */
    np_ramp_step_t s = np_ramp_plan_step(&p, &NONE, &NONE);
    check(!s.start_tdcs && s.start_cvns, "ramp plan: the longer ramp (cervical VNS) starts first");

    np_ramp_report_t c = pre();
    s = np_ramp_plan_step(&p, &NONE, &c);
    check(!s.start_tdcs, "ramp plan: tDCS waits through the cervical VNS checks");
    c = up(8000U);
    s = np_ramp_plan_step(&p, &NONE, &c);
    check(!s.start_tdcs, "ramp plan: and while cervical VNS has more than tDCS's ramp to go");
    c = up(6000U);
    s = np_ramp_plan_step(&p, &NONE, &c);
    check(s.start_tdcs, "ramp plan: tDCS starts when cervical VNS has exactly its ramp left, so both end together");
}

/* @verifies SWR-SS-05 */
static void test_a_refused_or_failed_participant_does_not_hold_the_rest_forever(void)
{
    np_ramp_plan_t p;
    np_ramp_plan_begin(&p, true, 30000U, true, 10000U);
    (void)np_ramp_plan_step(&p, &NONE, &NONE);
    np_ramp_plan_refused(&p, true, false);
    np_ramp_step_t s = np_ramp_plan_step(&p, &NONE, &NONE);
    check(s.start_cvns, "ramp plan: a refused tDCS lets cervical VNS start at once");
    np_ramp_report_t c = pre();
    s = np_ramp_plan_step(&p, &NONE, &c);
    np_ramp_report_t gone = { NP_RAMP_NONE, 0U, 0U };   /* cervical VNS faulted in its checks */
    s = np_ramp_plan_step(&p, &NONE, &gone);
    check(s.release_others, "ramp plan: a participant that ends in a fault releases the rest");
}

/* @verifies SWR-SS-07 */
static void test_gating_ramp_is_the_one_that_finishes_last(void)
{
    np_ramp_plan_t p;
    np_ramp_plan_begin(&p, true, 30000U, true, 10000U);
    (void)np_ramp_plan_step(&p, &NONE, &NONE);
    np_ramp_report_t t = up(28000U);
    np_ramp_report_t g = np_ramp_plan_gating(&p, &t, &NONE);
    check(g.phase == NP_RAMP_UP && g.remaining_ms >= 28000U, "ramp plan: while cervical VNS has not started, the gating ramp is tDCS");
    t = up(20000U);
    (void)np_ramp_plan_step(&p, &t, &NONE);
    np_ramp_report_t c = pre();
    t = up(19000U);
    g = np_ramp_plan_gating(&p, &t, &c);
    check(g.phase == NP_RAMP_PRECHECK, "ramp plan: an unknown time (checks running) is the later");
}

int main(void)
{
    test_nothing_ramps_everything_starts_at_once();
    test_tdcs_alone_holds_the_rest_until_its_ramp_ends();
    test_cvns_alone_holds_the_rest_through_its_checks_and_ramp();
    test_longer_tdcs_ramp_starts_first_and_cvns_is_timed_to_end_with_it();
    test_longer_cvns_ramp_starts_first_and_tdcs_ends_with_it();
    test_a_refused_or_failed_participant_does_not_hold_the_rest_forever();
    test_gating_ramp_is_the_one_that_finishes_last();
    if (g_failures != 0) { printf("\n%d FAILURE(S)\n", g_failures); return 1; }
    printf("\nAll np_ramp_plan tests passed.\n");
    return 0;
}
