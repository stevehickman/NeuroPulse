/*
 * Host tests — the paused session clock and the slot tracker (SWR-SS-05).
 *
 * The session runner needs the scheduler and is not host-tested; what decides how a pause behaves is here: that paused
 * time never counts (principal ruling 2026-10-13), and that a resume re-dispatches exactly the commands a pause found
 * running and nothing else.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_session_pause.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

/* @verifies SWR-SS-05 */
static void test_paused_time_does_not_count(void)
{
    np_session_clock_t c;
    np_session_clock_start(&c, 1000U);
    check(np_session_clock_elapsed(&c, 1000U) == 0U && np_session_clock_elapsed(&c, 4000U) == 3000U,
          "clock: before any pause, elapsed is real time since the start");

    check(np_session_clock_pause(&c, 4000U), "clock: a running clock pauses");
    check(np_session_clock_elapsed(&c, 4000U) == 3000U && np_session_clock_elapsed(&c, 9000U) == 3000U &&
          np_session_clock_elapsed(&c, 1000000U) == 3000U,
          "clock: while paused, elapsed stands still however long the pause lasts");

    check(np_session_clock_resume(&c, 10000U), "clock: a paused clock resumes");
    check(np_session_clock_elapsed(&c, 10000U) == 3000U && np_session_clock_elapsed(&c, 12000U) == 5000U,
          "clock: after a resume, elapsed carries on from where it stood, less the whole pause");

    np_session_clock_pause(&c, 12000U);
    np_session_clock_resume(&c, 15000U);
    check(np_session_clock_elapsed(&c, 16000U) == 6000U,
          "clock: two pauses are both left out (6 s of running in 15 s of wall time)");
}

/* @verifies SWR-SS-05 */
static void test_clock_refuses_a_double_pause_or_resume(void)
{
    np_session_clock_t c;
    np_session_clock_start(&c, 0U);
    check(!np_session_clock_resume(&c, 100U), "clock: resume without a pause changes nothing");
    check(np_session_clock_pause(&c, 100U) && !np_session_clock_pause(&c, 900U),
          "clock: a second pause does not move the instant the first began");
    check(np_session_clock_elapsed(&c, 5000U) == 100U, "clock: and elapsed is still the first pause's instant");
    np_session_clock_close(&c, 2000U);
    check(!c.paused && np_session_clock_elapsed(&c, 2000U) == 100U,
          "clock: closing an open pause (a session ending while paused) leaves the time it ran");
}

/* @verifies SWR-SS-05 */
static void test_clock_survives_tick_wrap(void)
{
    np_session_clock_t c;
    np_session_clock_start(&c, 0xFFFFFF00U);
    check(np_session_clock_elapsed(&c, 0x100U) == 0x200U, "clock: elapsed is right across a tick counter wrap");
    np_session_clock_pause(&c, 0xFFFFFFF0U);     /* 0xF0 ticks into the session, 0x100 before the wrap */
    np_session_clock_resume(&c, 0x00000010U);    /* paused for 0x20 ticks, across the wrap */
    check(np_session_clock_elapsed(&c, 0x00000110U) == 0x210U - 0x20U,
          "clock: a pause that spans the wrap is left out too");
}

/* @verifies SWR-SS-05 */
static void test_tracker_holds_exactly_what_was_running(void)
{
    np_slot_track_t t;
    uint8_t  slots[NP_HUB_SLOT_MAX];
    uint16_t cmds[NP_HUB_SLOT_MAX];
    np_slot_track_reset(&t);

    check(np_slot_track_pause(&t, slots) == 0U, "tracker: a pause with nothing running holds nothing");
    check(np_slot_track_resume(&t, cmds) == 0U, "tracker: and a resume with nothing held restores nothing");

    np_slot_track_started(&t, 7U, 3U);
    np_slot_track_started(&t, 5U, 1U);
    np_slot_track_started(&t, 9U, 4U);
    np_slot_track_stopped(&t, 9U);   /* a stop command, or the command ran its course */
    np_slot_track_started(&t, 5U, 6U);   /* a later command replaced the first on slot 5 */

    const uint8_t n = np_slot_track_pause(&t, slots);
    check(n == 2U && slots[0] == 5U && slots[1] == 7U,
          "tracker: a pause stops exactly the slots still driven, ascending, and not the one that was stopped");
    check(np_slot_track_pause(&t, slots) == 0U, "tracker: pausing again holds nothing more");

    const uint8_t m = np_slot_track_resume(&t, cmds);
    check(m == 2U && cmds[0] == 6U && cmds[1] == 3U,
          "tracker: a resume re-dispatches the command that was current on each slot, not an earlier one");
    check(np_slot_track_resume(&t, cmds) == 0U, "tracker: a second resume has nothing left to restore");
}

/* @verifies SWR-SS-05 */
static void test_tracker_ignores_a_command_stopped_during_the_pause_window_edges(void)
{
    np_slot_track_t t;
    uint8_t  slots[NP_HUB_SLOT_MAX];
    uint16_t cmds[NP_HUB_SLOT_MAX];
    np_slot_track_reset(&t);
    np_slot_track_started(&t, 2U, 0U);
    (void)np_slot_track_pause(&t, slots);
    np_slot_track_stopped(&t, 2U);      /* nothing is driven now; this must not drop the held command */
    check(np_slot_track_resume(&t, cmds) == 1U && cmds[0] == 0U,
          "tracker: what a pause holds is not lost to a stop recorded afterwards");
    np_slot_track_started(&t, 200U, 1U);   /* out of range: ignored, not a write past the table */
    np_slot_track_stopped(&t, 200U);
    check(np_slot_track_pause(&t, slots) == 0U, "tracker: an out-of-range slot is ignored");
}

int main(void)
{
    test_paused_time_does_not_count();
    test_clock_refuses_a_double_pause_or_resume();
    test_clock_survives_tick_wrap();
    test_tracker_holds_exactly_what_was_running();
    test_tracker_ignores_a_command_stopped_during_the_pause_window_edges();
    if (g_failures != 0) { printf("\n%d FAILURE(S)\n", g_failures); return 1; }
    printf("\nAll np_session_pause tests passed.\n");
    return 0;
}
