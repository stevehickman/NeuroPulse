/*
 * NeurOne Hub Control — Commanded-versus-Delivered Cross-Check Host Tests
 * OI-FMEA-09 / NP-FMEA-001 §3.3 FMEA-M03-02; NP-FW-HUB-001 Rev 9 §8.3.1
 *
 * The safety MCU's charge monitor integrates COMMANDED current, so it cannot
 * see a driver delivering more than it was told to.  np_stim_xcheck.c is the
 * control FMEA-M03-02's residual score depends on.  These tests pin its two
 * obligations in both directions: it flags a sustained excess, once, and it
 * does NOT flag what the driver is supposed to do, such as a tDCS ramp-down,
 * a within-tolerance reading, or one noisy sample.
 *
 * The thresholds are UNVALIDATED placeholders (np_hub_config.h).  The tests
 * are written against the macros, not their values, so replacing the numbers
 * with derived ones leaves these tests valid.
 *
 * IEC 62304 Class B — SW-02 hub control.
 */

#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_stim_xcheck.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

static np_telem_record_t rec(np_hub_mod_type_t type, float delivered_ua)
{
    np_telem_record_t r;
    memset(&r, 0, sizeof r);
    r.mod_type = type;
    r.data.stim.current_ua = delivered_ua;
    return r;
}

/* Observe the same reading n times, 1 s apart from t0; count flags raised. */
static int observe_n(np_hub_mod_type_t type, float delivered_ua,
                     uint32_t t0, unsigned n)
{
    np_telem_record_t r = rec(type, delivered_ua);
    int raised = 0;
    for (unsigned i = 0U; i < n; i++) {
        if (np_stim_xcheck_observe(&r, t0 + i * 1000U)) {
            raised++;
        }
    }
    return raised;
}

/* The smallest reading over the bound for a commanded level. */
static float just_over(uint16_t commanded_ua)
{
    float margin = (float)commanded_ua * (float)NP_STIM_XCHECK_TOL_PCT / 100.0f;
    if (margin < (float)NP_STIM_XCHECK_FLOOR_UA) {
        margin = (float)NP_STIM_XCHECK_FLOOR_UA;
    }
    return (float)commanded_ua + margin + 1.0f;
}

static float just_under(uint16_t commanded_ua)
{
    return just_over(commanded_ua) - 2.0f;
}

static void test_within_bound_is_not_flagged(void)
{
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1500U, 0U);
    check(observe_n(NP_MOD_TDCS, just_under(1500U), 0U, 10U) == 0,
          "within bound: delivered at the edge of tolerance is never flagged");
    check(observe_n(NP_MOD_TDCS, 1500.0f, 20000U, 10U) == 0,
          "within bound: delivered == commanded is never flagged");
    check(observe_n(NP_MOD_TDCS, 200.0f, 40000U, 10U) == 0,
          "within bound: under-delivery is not the FMEA-M03-02 hazard");
}

static void test_sustained_excess_is_flagged_once(void)
{
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_BES_TACS, 800U, 0U);

    check(observe_n(NP_MOD_BES_TACS, just_over(800U), 0U,
                    NP_STIM_XCHECK_CONSECUTIVE - 1U) == 0,
          "debounce: fewer than CONSECUTIVE excess snapshots raise nothing");
    check(observe_n(NP_MOD_BES_TACS, just_over(800U), 10000U, 1U) == 1,
          "debounce: the CONSECUTIVE-th excess snapshot raises the flag");
    check(np_stim_xcheck_latched(NP_SAFETY_CH_BES_TACS),
          "latch: the channel reports latched after the flag");
    check(observe_n(NP_MOD_BES_TACS, just_over(800U) * 2.0f, 20000U, 20U) == 0,
          "latch: a continuing excess raises no second flag this session");
}

static void test_one_noisy_sample_resets_the_run(void)
{
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1000U, 0U);

    int raised = 0;
    for (unsigned i = 0U; i < 5U; i++) {
        raised += observe_n(NP_MOD_TDCS, just_over(1000U), i * 10000U,
                            NP_STIM_XCHECK_CONSECUTIVE - 1U);
        raised += observe_n(NP_MOD_TDCS, 1000.0f, i * 10000U + 5000U, 1U);
    }
    check(raised == 0 && !np_stim_xcheck_latched(NP_SAFETY_CH_TDCS),
          "debounce: excesses separated by an in-bound snapshot never "
          "accumulate into a flag");
}

static void test_floor_applies_at_low_current(void)
{
    /* At low commanded current the percentage is smaller than the floor; the
     * floor is what stops read-back noise at 0-200 µA from flagging. */
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 100U, 0U);
    check(observe_n(NP_MOD_TDCS, 100.0f + (float)NP_STIM_XCHECK_FLOOR_UA - 1.0f,
                    0U, 10U) == 0,
          "floor: an excess below the absolute floor is not flagged");
    check(observe_n(NP_MOD_TDCS, 100.0f + (float)NP_STIM_XCHECK_FLOOR_UA + 1.0f,
                    20000U, NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "floor: an excess above the absolute floor is flagged");
}

static void test_stuck_on_driver_is_flagged(void)
{
    /* Commanded zero (never driven, or stopped with no ramp): any delivered
     * current beyond the floor is a driver that is on when it should be off. */
    np_stim_xcheck_clear();
    check(observe_n(NP_MOD_BES_TACS, just_over(0U), 0U,
                    NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "stuck on: current on a channel commanded to zero is flagged");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_BES_TACS, 900U, 0U);
    np_stim_xcheck_commanded(NP_SAFETY_CH_BES_TACS, 0U, 0U);   /* BES steps */
    check(observe_n(NP_MOD_BES_TACS, 900.0f, 0U,
                    NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "stuck on: BES still delivering after a stop is flagged at once "
          "(no ramp to hold through)");
}

static void test_ramp_down_is_held(void)
{
    const uint32_t ramp_ms = 60000U;

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 2000U, 0U);
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 500U, ramp_ms);

    /* The hold is anchored at the FIRST observation after the command (t=5 s
     * here), so it lasts until 65 s, never less. */
    check(observe_n(NP_MOD_TDCS, 1900.0f, 5000U, 60U) == 0,
          "ramp: delivery above the new target during the ramp is not flagged");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 2000U, 0U);
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 500U, ramp_ms);
    check(observe_n(NP_MOD_TDCS, just_over(2000U), 0U,
                    NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "ramp: the hold bounds at the PREVIOUS level, it does not suspend "
          "the check; exceeding that level is flagged mid-ramp");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 2000U, 0U);
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 500U, ramp_ms);
    (void)observe_n(NP_MOD_TDCS, 1000.0f, 0U, 1U);        /* anchors the hold */
    check(observe_n(NP_MOD_TDCS, 1900.0f, ramp_ms + 1000U,
                    NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "ramp: after the ramp window the bound drops to the new target");
}

static void test_increase_is_not_held(void)
{
    /* An increase takes effect as the bound at once: a hold only ever widens
     * the bound for a decrease the driver is still ramping through. */
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 500U, 0U);
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1500U, 30000U);
    check(observe_n(NP_MOD_TDCS, 1500.0f, 0U, 10U) == 0,
          "increase: the new, higher command is the bound immediately");
}

static void test_readings_that_cannot_be_trusted(void)
{
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1000U, 0U);
    check(observe_n(NP_MOD_TDCS, -just_over(1000U), 0U,
                    NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "magnitude: reverse-polarity excess is flagged like forward");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1000U, 0U);
    check(observe_n(NP_MOD_TDCS, NAN, 0U, NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "non-finite: a NaN read-back counts against the channel, since it "
          "cannot show delivery is in bounds");
}

static void test_scope(void)
{
    np_stim_xcheck_clear();
    check(observe_n(NP_MOD_CVNS, 1.0e6f, 0U, 10U) == 0,
          "scope: cervical VNS reports COMMANDED current in current_ua, so "
          "it is not cross-checked (it would compare a number with itself)");
    check(observe_n(NP_MOD_PBM_BASE, 1.0e6f, 0U, 10U) == 0,
          "scope: a non-electrical record is ignored");
    check(!np_stim_xcheck_observe(NULL, 0U), "scope: NULL record is ignored");

    np_stim_xcheck_commanded(NP_SAFETY_MAX_CHANNELS, 1U, 0U);   /* no-op */
    check(!np_stim_xcheck_latched(NP_SAFETY_MAX_CHANNELS),
          "scope: an out-of-range channel is ignored, not written");
}

static void test_channels_and_sessions_are_independent(void)
{
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_BES_TACS, 800U, 0U);
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 2000U, 0U);

    (void)observe_n(NP_MOD_BES_TACS, just_over(800U), 0U,
                    NP_STIM_XCHECK_CONSECUTIVE);
    check(np_stim_xcheck_latched(NP_SAFETY_CH_BES_TACS) &&
          !np_stim_xcheck_latched(NP_SAFETY_CH_TDCS),
          "independence: a BES flag does not latch tDCS");
    check(observe_n(NP_MOD_TDCS, 2000.0f, 0U, 10U) == 0,
          "independence: tDCS keeps its own, higher bound");

    np_stim_xcheck_reset();
    check(!np_stim_xcheck_latched(NP_SAFETY_CH_BES_TACS),
          "session: reset clears the latch, so the next session can flag");
    check(observe_n(NP_MOD_BES_TACS, just_over(800U), 0U,
                    NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "session: after reset the same fault raises its own flag again");
    check(observe_n(NP_MOD_TDCS, 2000.0f, 0U, 10U) == 0,
          "session: reset keeps the commanded level; it is the driver's "
          "state, not the session's");
}

static void test_ramp_down_crosses_a_session_boundary(void)
{
    /* The runner waits 5 s at shutdown; the tDCS stop ramp takes 30 s.  A
     * session started in between must not flag the previous one's ramp. */
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 2000U, 0U);
    (void)observe_n(NP_MOD_TDCS, 2000.0f, 100000U, 3U);   /* old session clock */
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 0U, 30000U);
    (void)observe_n(NP_MOD_TDCS, 1800.0f, 104000U, 1U);   /* anchors the hold */

    np_stim_xcheck_reset();                                /* new session, clock 0 */
    check(observe_n(NP_MOD_TDCS, 1500.0f, 0U, 20U) == 0,
          "boundary: the previous session's ramp-down is not flagged in the "
          "next session");
    check(observe_n(NP_MOD_TDCS, 1500.0f, 31000U, NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "boundary: the re-anchored hold still ends; a driver still on "
          "after it is flagged");
}

int main(void)
{
    test_within_bound_is_not_flagged();
    test_sustained_excess_is_flagged_once();
    test_one_noisy_sample_resets_the_run();
    test_floor_applies_at_low_current();
    test_stuck_on_driver_is_flagged();
    test_ramp_down_is_held();
    test_increase_is_not_held();
    test_readings_that_cannot_be_trusted();
    test_scope();
    test_channels_and_sessions_are_independent();
    test_ramp_down_crosses_a_session_boundary();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
