/* np_accel_shdr_tests.c — host tests for NP-FW-EMMC-002 §G / §H
 *
 * Every suppression assertion in this file is paired with a POSITIVE control
 * that runs the same input through the open-window configuration.  Without the
 * pair, "no extended record was emitted" is satisfied just as well by a module
 * that emits nothing at all — the failure mode recorded in
 * feedback_negative_assertions_pass_on_inaction.  The pairing is what makes
 * each negative mean something: the control proves the code path exists and
 * works, so the negative proves the gate closed it and not that it was never
 * there.
 */

#include "np_accel_shdr.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        g_checks++;                                                            \
        if (!(cond)) {                                                         \
            printf("  FAIL  %s (%s:%d)\n", (msg), __FILE__, __LINE__);         \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

/* ── Platform stubs (NPTEST_HOST) ───────────────────────────────────────────
 * The evaluator is pure, so most tests call it directly.  These exist so the
 * module links and so process_session_gap() can be exercised end to end. */

static float  g_imu_samples[64];
static size_t g_imu_n = 0;

static np_shdr_accel_record_t      g_written_std;
static int                         g_written_std_count  = 0;
static np_shdr_accel_char_record_t g_written_char;
static int                         g_written_char_count = 0;

static np_accel_char_enrolment_t   g_stored_enr;
static bool                        g_enr_readable = false;
static np_accel_shdr_state_t       g_stored_state;

np_accel_status_t np_imu_read_gap_resultants(float *dst, size_t cap, size_t *out_n)
{
    size_t n = (g_imu_n < cap) ? g_imu_n : cap;
    for (size_t i = 0; i < n; i++) { dst[i] = g_imu_samples[i]; }
    *out_n = n;
    return NP_ACCEL_OK;
}

np_accel_status_t np_shdr_write_accel_record(const np_shdr_accel_record_t *rec)
{
    g_written_std = *rec;
    g_written_std_count++;
    return NP_ACCEL_OK;
}

np_accel_status_t np_shdr_write_accel_char_record(const np_shdr_accel_char_record_t *rec)
{
    g_written_char = *rec;
    g_written_char_count++;
    return NP_ACCEL_OK;
}

np_accel_status_t np_config_read_char_enrolment(np_accel_char_enrolment_t *enr)
{
    if (!g_enr_readable) { return NP_ACCEL_ERR_WRITE; }
    *enr = g_stored_enr;
    return NP_ACCEL_OK;
}

np_accel_status_t np_config_write_char_enrolment(const np_accel_char_enrolment_t *enr)
{
    g_stored_enr = *enr;
    return NP_ACCEL_OK;
}

np_accel_status_t np_config_read_accel_state(np_accel_shdr_state_t *state)
{
    *state = g_stored_state;
    return NP_ACCEL_OK;
}

np_accel_status_t np_config_write_accel_state(const np_accel_shdr_state_t *state)
{
    g_stored_state = *state;
    return NP_ACCEL_OK;
}

/* ── Helpers ────────────────────────────────────────────────────────────── */

static np_accel_char_enrolment_t valid_enrolment(void)
{
    np_accel_char_enrolment_t e = {0};
    e.enrolled        = true;
    e.programme_id    = NP_ACCEL_CHAR_PROGRAMME_ID;
    e.consent_epoch   = 1u;
    e.records_emitted = 0u;
    e.crc32           = np_accel_char_enrolment_crc(&e);
    return e;
}

/* One 4 g bump, one 18 g impact — 18 g is above the 15.0 g placeholder, so this
 * gap contains a drop and two events. */
static const float kTwoEvents[] = { 0.9f, 4.0f, 0.9f, 0.9f, 18.0f, 17.0f, 0.9f };
#define kTwoEventsN (sizeof kTwoEvents / sizeof kTwoEvents[0])

static void run_gap(const float *s, size_t n,
                    np_accel_shdr_state_t *state,
                    np_accel_char_enrolment_t *enr,
                    np_shdr_accel_record_t *std_out,
                    np_shdr_accel_char_record_t *char_out,
                    bool *char_valid)
{
    np_accel_status_t st = np_accel_shdr_evaluate(s, n, state, enr,
                                                  std_out, char_out, char_valid);
    CHECK(st == NP_ACCEL_OK, "evaluate returned OK");
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

/* §G.2 — the standing record is produced identically whether or not the device
 * is enrolled.  This is the non-coercion invariant (CHAR-4 / §H.6) expressed as
 * a property of the code: a non-participant's maintenance signal is bit-for-bit
 * what a participant's is. */
static void test_standard_record_identical_regardless_of_enrolment(void)
{
    printf("standard record is enrolment-independent\n");

    np_accel_shdr_state_t s1 = {0}, s2 = {0};
    np_shdr_accel_record_t r1, r2;
    np_shdr_accel_char_record_t c;
    bool cv;

    np_accel_char_build_gate = true;
    np_accel_char_enrolment_t enr = valid_enrolment();

    run_gap(kTwoEvents, kTwoEventsN, &s1, NULL, &r1, &c, &cv);   /* not enrolled */
    run_gap(kTwoEvents, kTwoEventsN, &s2, &enr, &r2, &c, &cv);   /* enrolled     */

    CHECK(r1.drop_detected == r2.drop_detected,         "drop_detected identical");
    CHECK(r1.maintenance_alert == r2.maintenance_alert, "maintenance_alert identical");
    CHECK(r1.drop_detected == true,                     "18 g scored as a drop");
    CHECK(s1.drop_flags == s2.drop_flags,               "rolling state identical");
}

/* The positive control for every suppression test below. */
static void test_extended_emitted_on_the_happy_path(void)
{
    printf("extended record IS emitted when every condition holds\n");

    np_accel_char_build_gate = true;
    np_accel_shdr_state_t st = {0};
    np_accel_char_enrolment_t enr = valid_enrolment();
    np_shdr_accel_record_t r;
    np_shdr_accel_char_record_t c;
    bool cv = false;

    run_gap(kTwoEvents, kTwoEventsN, &st, &enr, &r, &c, &cv);

    CHECK(cv == true,                                  "extended record valid");
    CHECK(c.programme_id == NP_ACCEL_CHAR_PROGRAMME_ID, "programme id stamped");
    CHECK(c.consent_epoch == 1u,                        "consent epoch carried");
    CHECK(c.record_seq == 1u,                           "record_seq is 1-based");
    CHECK(c.impact_event_count == 2u,                   "two events counted");
    CHECK(enr.records_emitted == 1u,                    "budget consumed by exactly one");

    /* Coarsening: 4.0 g lands in bin 1 [3.0,4.5); 18.0 g in bin 5 [14.0,21.0). */
    CHECK(c.g_bin[1] == 1u, "4 g binned at [3.0,4.5)");
    CHECK(c.g_bin[5] == 1u, "18 g binned at [14.0,21.0)");
    unsigned total = 0;
    for (unsigned i = 0; i < NP_ACCEL_CHAR_G_BIN_COUNT; i++) { total += c.g_bin[i]; }
    CHECK(total == c.impact_event_count, "histogram sums to event count");
}

/* CHAR-2 — the build-time gate. */
static void test_window_disabled_build_emits_standard_only(void)
{
    printf("window-disabled build emits standard only\n");

    np_accel_char_build_gate = false;
    np_accel_shdr_state_t st = {0};
    np_accel_char_enrolment_t enr = valid_enrolment();   /* fully valid consent */
    np_shdr_accel_record_t r;
    np_shdr_accel_char_record_t c;
    bool cv = true;                                       /* poisoned */

    run_gap(kTwoEvents, kTwoEventsN, &st, &enr, &r, &c, &cv);

    CHECK(cv == false,                "no extended record with the build gate off");
    CHECK(enr.records_emitted == 0u,  "budget untouched");
    CHECK(r.drop_detected == true,    "standard record still produced");

    np_accel_char_build_gate = true;
}

/* §H.3 — affirmative opt-in. */
static void test_not_consented_emits_standard_only(void)
{
    printf("non-consented device emits standard only\n");

    np_accel_char_build_gate = true;
    np_accel_shdr_state_t st = {0};
    np_accel_char_enrolment_t enr = valid_enrolment();
    enr.enrolled = false;
    enr.crc32    = np_accel_char_enrolment_crc(&enr);     /* CRC valid, consent absent */

    np_shdr_accel_record_t r;
    np_shdr_accel_char_record_t c;
    bool cv = true;

    run_gap(kTwoEvents, kTwoEventsN, &st, &enr, &r, &c, &cv);

    CHECK(cv == false,               "no extended record without opt-in");
    CHECK(enr.records_emitted == 0u, "budget untouched");
    CHECK(r.drop_detected == true,   "standard record still produced");
}

/* Fail-closed on a corrupt record: a flipped bit must read as NOT enrolled,
 * never as enrolled. */
static void test_corrupt_enrolment_is_not_enrolled(void)
{
    printf("corrupt enrolment record fails closed\n");

    np_accel_char_build_gate = true;
    np_accel_char_enrolment_t enr = valid_enrolment();
    CHECK(np_accel_char_window_open(&enr) == true, "control: intact record opens");

    enr.crc32 ^= 0x00000001u;
    CHECK(np_accel_char_window_open(&enr) == false, "bad CRC closes the window");

    np_accel_char_enrolment_t zeroed = {0};
    CHECK(np_accel_char_window_open(&zeroed) == false, "all-zero record closes");
    CHECK(np_accel_char_window_open(NULL) == false,    "NULL record closes");
}

/* A device consented to programme N must not contribute to programme N+1. */
static void test_programme_id_mismatch_closes_window(void)
{
    printf("programme id mismatch fails closed\n");

    np_accel_char_build_gate = true;
    np_accel_char_enrolment_t enr = valid_enrolment();
    enr.programme_id = NP_ACCEL_CHAR_PROGRAMME_ID + 1u;
    enr.crc32        = np_accel_char_enrolment_crc(&enr);

    CHECK(np_accel_char_window_open(&enr) == false, "other programme closes window");
}

/* The expiry itself.  Record BUDGET is emitted; record BUDGET+1 is not. */
static void test_budget_boundary_is_the_expiry(void)
{
    printf("budget boundary closes the window\n");

    np_accel_char_build_gate = true;
    np_accel_shdr_state_t st = {0};
    np_shdr_accel_record_t r;
    np_shdr_accel_char_record_t c;
    bool cv;

    np_accel_char_enrolment_t enr = valid_enrolment();
    enr.records_emitted = NP_ACCEL_CHAR_RECORD_BUDGET - 1u;
    enr.crc32           = np_accel_char_enrolment_crc(&enr);

    run_gap(kTwoEvents, kTwoEventsN, &st, &enr, &r, &c, &cv);
    CHECK(cv == true, "the last record inside the budget IS emitted");
    CHECK(c.record_seq == NP_ACCEL_CHAR_RECORD_BUDGET, "seq reaches the budget");
    CHECK(enr.records_emitted == NP_ACCEL_CHAR_RECORD_BUDGET, "budget now exhausted");

    cv = true;
    run_gap(kTwoEvents, kTwoEventsN, &st, &enr, &r, &c, &cv);
    CHECK(cv == false, "the next record is NOT emitted");
    CHECK(enr.records_emitted == NP_ACCEL_CHAR_RECORD_BUDGET,
          "budget does not advance past the ceiling");
    CHECK(r.drop_detected == true, "standard record continues after expiry");
}

/* The counter must be consumed ONLY by emission — that is what makes it a
 * monotone the window can be denominated in. */
static void test_budget_consumed_only_by_emission(void)
{
    printf("budget advances only when a record is emitted\n");

    np_accel_char_build_gate = true;
    np_accel_shdr_state_t st = {0};
    np_shdr_accel_record_t r;
    np_shdr_accel_char_record_t c;
    bool cv;

    np_accel_char_enrolment_t enr = valid_enrolment();

    /* A quiet gap: no samples above the event floor.  A record is still emitted
     * (an empty histogram is evidence too), so the budget advances by one. */
    const float quiet[] = { 0.9f, 1.1f, 0.8f };
    run_gap(quiet, 3, &st, &enr, &r, &c, &cv);
    CHECK(cv == true,                  "quiet gap still emits an extended record");
    CHECK(c.impact_event_count == 0u,  "quiet gap has zero events");
    CHECK(enr.records_emitted == 1u,   "budget advanced by exactly one");

    /* Suppressed gap: budget must not move. */
    np_accel_char_build_gate = false;
    run_gap(quiet, 3, &st, &enr, &r, &c, &cv);
    CHECK(enr.records_emitted == 1u,   "suppressed gap does not advance the budget");
    np_accel_char_build_gate = true;
}

/* §G.2 rolling alert, counted in GAPS (see NP_ACCEL_MAINT_WINDOW_GAPS). */
static void test_maintenance_alert_rolling_window(void)
{
    printf("maintenance alert over the rolling gap window\n");

    np_accel_char_build_gate = false;
    np_accel_shdr_state_t st = {0};
    np_shdr_accel_record_t r;
    np_shdr_accel_char_record_t c;
    bool cv;

    const float quiet[] = { 0.9f };
    const float drop[]  = { 0.9f, 20.0f, 0.9f };

    run_gap(drop, 3, &st, NULL, &r, &c, &cv);
    CHECK(r.maintenance_alert == false, "one drop does not alert");
    run_gap(quiet, 1, &st, NULL, &r, &c, &cv);
    run_gap(drop, 3, &st, NULL, &r, &c, &cv);
    CHECK(r.maintenance_alert == false, "two drops do not alert");
    run_gap(drop, 3, &st, NULL, &r, &c, &cv);
    CHECK(r.maintenance_alert == true,  "three drops in the window alert");

    /* And it must decay: seven quiet gaps push all three out of the window. */
    for (int i = 0; i < 7; i++) {
        run_gap(quiet, 1, &st, NULL, &r, &c, &cv);
    }
    CHECK(r.maintenance_alert == false, "alert clears once the window rolls past");
    np_accel_char_build_gate = true;
}

/* §H.2 coarsening — every bin edge maps as specified, and a value below the
 * event floor produces no event at all. */
static void test_histogram_binning(void)
{
    printf("histogram binning\n");

    np_accel_char_build_gate = true;

    const float probes[NP_ACCEL_CHAR_G_BIN_COUNT] =
        { 2.0f, 3.0f, 4.5f, 6.5f, 9.5f, 14.0f, 21.0f, 31.0f };

    for (unsigned i = 0; i < NP_ACCEL_CHAR_G_BIN_COUNT; i++) {
        np_accel_shdr_state_t st = {0};
        np_accel_char_enrolment_t enr = valid_enrolment();
        np_shdr_accel_record_t r;
        np_shdr_accel_char_record_t c;
        bool cv;
        const float one[] = { 0.5f, probes[i], 0.5f };
        run_gap(one, 3, &st, &enr, &r, &c, &cv);
        CHECK(cv && c.g_bin[i] == 1u, "bin edge lands in its own bin");
    }

    /* Below the floor: no event, no bin, but still a record. */
    np_accel_shdr_state_t st = {0};
    np_accel_char_enrolment_t enr = valid_enrolment();
    np_shdr_accel_record_t r;
    np_shdr_accel_char_record_t c;
    bool cv;
    const float below[] = { 1.9f, 1.5f };
    run_gap(below, 2, &st, &enr, &r, &c, &cv);
    CHECK(c.impact_event_count == 0u, "sub-floor motion is not an event");
}

/* End-to-end through the I/O path, including the unreadable-enrolment case. */
static void test_process_session_gap_end_to_end(void)
{
    printf("process_session_gap end to end\n");

    np_accel_char_build_gate = true;
    memcpy(g_imu_samples, kTwoEvents, sizeof kTwoEvents);
    g_imu_n = kTwoEventsN;
    memset(&g_stored_state, 0, sizeof g_stored_state);
    g_written_std_count = g_written_char_count = 0;

    g_stored_enr   = valid_enrolment();
    g_enr_readable = true;
    CHECK(np_accel_shdr_process_session_gap() == NP_ACCEL_OK, "gap processed");
    CHECK(g_written_std_count == 1,  "one standard record written");
    CHECK(g_written_char_count == 1, "one extended record written");
    CHECK(g_stored_enr.records_emitted == 1u, "budget persisted");

    /* Config read failure ⇒ not enrolled ⇒ standard only.  The control above is
     * what makes this meaningful: the extended path demonstrably works. */
    g_enr_readable = false;
    g_written_std_count = g_written_char_count = 0;
    CHECK(np_accel_shdr_process_session_gap() == NP_ACCEL_OK, "gap processed");
    CHECK(g_written_std_count == 1,  "standard record still written");
    CHECK(g_written_char_count == 0, "unreadable enrolment emits no extended record");
}

/* Anti-criterion, asserted on the type itself: the extended record has no field
 * that could carry an orientation, an axis, a per-event g value or a clock.
 * A future field of any of those kinds changes sizeof and trips this. */
static void test_extended_record_carries_no_prohibited_field(void)
{
    printf("extended record shape\n");

    const size_t expect = sizeof(uint16_t)                                  /* programme_id */
                        + sizeof(uint32_t)                                  /* consent_epoch */
                        + sizeof(uint32_t)                                  /* record_seq */
                        + sizeof(uint16_t) * NP_ACCEL_CHAR_G_BIN_COUNT      /* g_bin */
                        + sizeof(uint16_t);                                 /* impact_event_count */
    CHECK(sizeof(np_shdr_accel_char_record_t) >= expect,   "no field is missing");
    CHECK(sizeof(np_shdr_accel_char_record_t) <= expect + 4u,
          "no room for an orientation vector, a raw peak or a timestamp");
    CHECK(sizeof(np_shdr_accel_record_t) == 8u, "§G.2 standard record is 8 bytes");
}

int main(void)
{
    printf("NP-FW-EMMC-002 §G/§H — np_accel_shdr host tests\n\n");

    test_standard_record_identical_regardless_of_enrolment();
    test_extended_emitted_on_the_happy_path();
    test_window_disabled_build_emits_standard_only();
    test_not_consented_emits_standard_only();
    test_corrupt_enrolment_is_not_enrolled();
    test_programme_id_mismatch_closes_window();
    test_budget_boundary_is_the_expiry();
    test_budget_consumed_only_by_emission();
    test_maintenance_alert_rolling_window();
    test_histogram_binning();
    test_process_session_gap_end_to_end();
    test_extended_record_carries_no_prohibited_field();

    printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
