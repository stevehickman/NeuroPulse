/*
 * Host tests — SESSION_HOLD, why a session is held (SWR-SS-09). The cause itself is the runner's (np_runner_run_tests); what is
 * here is the characteristic: a read gives the runner's cause, nothing bound gives "not held", a short buffer is refused, and the
 * ranking of causes is the one the header states.
 */
#include <stdint.h>
#include <stdio.h>

#include "np_gatt_server.h"
#include "np_session_hold.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

static int      g_notifies;
static uint16_t g_notified_id;
static uint8_t  g_notified_val;
np_hub_status_t np_gatt_notify(uint16_t id, const uint8_t *data, size_t len)
{
    g_notifies++; g_notified_id = id; g_notified_val = (len == 1U) ? data[0] : 0xEEU;
    return NP_HUB_OK;
}

static np_hold_cause_t g_cause = NP_HOLD_NONE;
static np_hold_cause_t src(void) { return g_cause; }

/* @verifies SWR-SS-09 */
static void test_a_read_gives_the_runners_cause_and_nothing_bound_gives_not_held(void)
{
    uint8_t b[1]; size_t n = 0;
    np_hold_set_source(NULL);
    check(np_hold_read(b, 1U, &n) == NP_HUB_OK && n == 1U && b[0] == 0U, "hold: with no source bound the answer is not held");
    np_hold_set_source(src);
    for (int c = NP_HOLD_NONE; c <= NP_HOLD_DETECTOR_BLIND; c++) {
        g_cause = (np_hold_cause_t)c;
        check(np_hold_read(b, 1U, &n) == NP_HUB_OK && b[0] == (uint8_t)c, "hold: a read gives the cause as one byte");
    }
    uint8_t two[2] = { 0, 0 };
    check(np_hold_read(two, 0U, &n) == NP_HUB_ERR_INVALID_ARG && np_hold_read(NULL, 1U, &n) == NP_HUB_ERR_INVALID_ARG &&
          np_hold_read(b, 1U, NULL) == NP_HUB_ERR_INVALID_ARG, "hold: a buffer too small, or none, is refused");
}

/* @verifies SWR-SS-09 */
static void test_a_notification_carries_the_cause_on_its_own_characteristic(void)
{
    np_hold_set_source(src);
    g_cause = NP_HOLD_DETECTOR_BLIND; g_notifies = 0;
    np_hold_notify();
    check(g_notifies == 1 && g_notified_id == NP_GATT_SESSION_HOLD_ID && g_notified_val == (uint8_t)NP_HOLD_DETECTOR_BLIND,
          "hold: the notification is the cause, on SESSION_HOLD and not on SESSION_PROGRESS");
    np_hold_set_source(NULL);
    g_notifies = 0;
    np_hold_notify();
    check(g_notifies == 0, "hold: nothing bound, nothing is sent");
}

/* @verifies SWR-SS-09 */
static void test_a_detector_cause_outranks_a_persons(void)
{
    check(np_hold_outranks(NP_HOLD_USER_PANIC, NP_HOLD_USER_PAUSE), "hold: a panic outranks a pause");
    check(np_hold_outranks(NP_HOLD_DETECTOR_DISCHARGE, NP_HOLD_USER_PANIC) && np_hold_outranks(NP_HOLD_DETECTOR_BLIND, NP_HOLD_USER_PANIC),
          "hold: a detector cause outranks a person's panic");
    check(!np_hold_outranks(NP_HOLD_USER_PAUSE, NP_HOLD_DETECTOR_DISCHARGE) && !np_hold_outranks(NP_HOLD_NONE, NP_HOLD_USER_PAUSE),
          "hold: and nothing lower raises a higher one");
}

int main(void)
{
    test_a_read_gives_the_runners_cause_and_nothing_bound_gives_not_held();
    test_a_notification_carries_the_cause_on_its_own_characteristic();
    test_a_detector_cause_outranks_a_persons();
    if (g_failures == 0) { printf("ALL PASS (0 failures)\n"); return 0; }
    printf("FAILED (%d failure%s)\n", g_failures, g_failures == 1 ? "" : "s");
    return 1;
}
