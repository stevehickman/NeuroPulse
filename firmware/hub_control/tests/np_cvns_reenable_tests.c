/*
 * NeurOne Hub Control — CVNS Re-Enable Manager Host Tests (OI-CVNS-HUB-01)
 * Document: NP-FW-HUB-001 Rev 1 §7a, NP-FW-CVNS-001 Rev 1
 *
 * Host-native tests for the three-gate CVNS re-enable state machine and the
 * session_status bit-construction helper:
 *   - Gate 1: 30s lockout from hub observation of the cardiac cutoff
 *   - Gate 2: explicit app confirmation, post-lockout only, single-use
 *   - Gate 3: fresh hub-side impedance check must pass (fail/timeout closed)
 *   - Bounded assertion window; session-boundary resets; tick-wrap safety
 *   - np_safety_session_status_bits(): the enum-cast regression (RUNNING=3
 *     must NOT assert CVNS_REENABLE; PAUSED=4 must NOT assert GEOM_REQUIRED)
 *
 * No FreeRTOS, no hardware.  The impedance HAL stubs are test-controlled.
 *
 * IEC 62304 Class B — SW-02 hub control.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/hub_control/CMakeLists.txt).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../include/np_cvns_reenable.h"
#include "../include/np_safety_spi.h"   /* np_safety_session_status_bits */

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

/* ── Impedance HAL stubs (test-controlled) ─────────────────────────────────── */

static int             g_imp_start_calls;
static np_hub_status_t g_imp_start_rc;
static bool            g_imp_complete;   /* poll() returns this            */
static bool            g_imp_passed;     /* *passed_out when complete      */

np_hub_status_t np_cvns_hal_impedance_start(void)
{
    g_imp_start_calls++;
    return g_imp_start_rc;
}

bool np_cvns_hal_impedance_poll(bool *passed_out)
{
    if (!g_imp_complete) {
        return false;
    }
    *passed_out = g_imp_passed;
    return true;
}

static void hal_reset(void)
{
    g_imp_start_calls = 0;
    g_imp_start_rc    = NP_HUB_OK;
    g_imp_complete    = false;
    g_imp_passed      = false;
}

/* ── Helpers ────────────────────────────────────────────────────────────────── */

#define ST_CARDIAC (NP_SAFETY_STATUS_CARDIAC | NP_SAFETY_STATUS_CUTOFF)
#define ST_CLEAR   (NP_SAFETY_STATUS_OK)

/* Drive the machine to AWAIT_CONFIRM: cutoff at t0, lockout elapsed. */
static uint32_t drive_to_await_confirm(uint32_t t0)
{
    np_cvns_reenable_init();
    hal_reset();
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t0);
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true,
                                        t0 + NP_CVNS_REENABLE_LOCKOUT_MS);
    return t0 + NP_CVNS_REENABLE_LOCKOUT_MS;
}

/*
 * Drive to IMPEDANCE: reach AWAIT_CONFIRM, post the confirmation, then run one
 * heartbeat so the single-writer heartbeat context consumes the intent and
 * starts gate 3.  Returns the tick at which the impedance check was started.
 */
static uint32_t drive_to_impedance(uint32_t t0)
{
    uint32_t t = drive_to_await_confirm(t0);
    (void)np_cvns_reenable_confirm();
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t + 100U);
    return t + 100U;  /* s_impedance_start_ms */
}

/* ── Tests: init and cutoff detection ───────────────────────────────────────── */

static void test_init_idle(void)
{
    np_cvns_reenable_init();
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_IDLE,
          "init: state is IDLE");
    check(!np_cvns_reenable_bit_active(),
          "init: re-enable bit inactive");
}

static void test_cutoff_rising_edge(void)
{
    np_cvns_reenable_init();
    hal_reset();

    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, 1000U);
    check(ev == NP_CVNS_RN_EV_CUTOFF_DETECTED,
          "cutoff: rising CARDIAC edge returns CUTOFF_DETECTED");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_LOCKOUT,
          "cutoff: state is LOCKOUT");
    check(!np_cvns_reenable_bit_active(),
          "cutoff: bit stays inactive");

    /* Held CARDIAC on subsequent beats is NOT a new event */
    ev = np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, 1200U);
    check(ev == NP_CVNS_RN_EV_NONE,
          "cutoff: held CARDIAC produces no repeat event");
}

/* ── Tests: gate 1 — 30s lockout ────────────────────────────────────────────── */

static void test_lockout_timing(void)
{
    np_cvns_reenable_init();
    hal_reset();
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, 1000U);

    /* 29,999ms after observation: still LOCKOUT, confirm rejected */
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, 1000U + 29999U);
    check(ev == NP_CVNS_RN_EV_NONE &&
          np_cvns_reenable_get_state() == NP_CVNS_RN_LOCKOUT,
          "gate1: at 29999ms still LOCKOUT");
    check(np_cvns_reenable_confirm() == NP_HUB_ERR_SAFETY_REJECTED,
          "gate1: confirm at 29999ms rejected");
    check(g_imp_start_calls == 0,
          "gate1: rejected confirm does not start impedance");

    /* Exactly 30,000ms: window opens */
    ev = np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, 1000U + 30000U);
    check(ev == NP_CVNS_RN_EV_CONFIRM_OPEN &&
          np_cvns_reenable_get_state() == NP_CVNS_RN_AWAIT_CONFIRM,
          "gate1: at 30000ms confirm window opens");
    check(!np_cvns_reenable_bit_active(),
          "gate1: bit still inactive in AWAIT_CONFIRM");
}

static void test_lockout_tick_wrap(void)
{
    np_cvns_reenable_init();
    hal_reset();
    uint32_t t0 = 0xFFFFFC00U;  /* 1024 ticks before wrap */
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t0);

    /* 29s later — tick has wrapped past zero */
    uint32_t t1 = t0 + 29000U;  /* wraps */
    np_cvns_reenable_event_t ev = np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t1);
    check(ev == NP_CVNS_RN_EV_NONE &&
          np_cvns_reenable_get_state() == NP_CVNS_RN_LOCKOUT,
          "wrap: 29s across tick wrap still LOCKOUT");

    uint32_t t2 = t0 + 30000U;  /* wraps */
    ev = np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t2);
    check(ev == NP_CVNS_RN_EV_CONFIRM_OPEN,
          "wrap: 30s across tick wrap opens confirm window");
}

/* ── Tests: gate 2 — explicit confirmation ──────────────────────────────────── */

static void test_confirm_in_idle_rejected(void)
{
    np_cvns_reenable_init();
    hal_reset();
    check(np_cvns_reenable_confirm() == NP_HUB_ERR_SAFETY_REJECTED,
          "gate2: confirm in IDLE rejected (no cutoff pending)");

    /* Even a spurious accept must not start impedance without a heartbeat. */
    check(g_imp_start_calls == 0,
          "gate2: no impedance started from IDLE");
}

static void test_confirm_accepted_starts_impedance(void)
{
    uint32_t t = drive_to_await_confirm(1000U);

    /* Single-writer: confirm() posts intent but does NOT act itself. */
    check(np_cvns_reenable_confirm() == NP_HUB_OK,
          "gate2: confirm in AWAIT_CONFIRM accepted (intent posted)");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_AWAIT_CONFIRM,
          "gate2: confirm() alone does not transition state");
    check(g_imp_start_calls == 0,
          "gate2: confirm() alone does not start impedance (no HAL in transport ctx)");

    /* Next heartbeat consumes the intent and starts gate 3. */
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t + 100U);
    check(ev == NP_CVNS_RN_EV_NONE,
          "gate2: heartbeat consuming confirm emits no error event");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_IMPEDANCE,
          "gate2: heartbeat moves state to IMPEDANCE");
    check(g_imp_start_calls == 1,
          "gate2: impedance check started exactly once");
    check(!np_cvns_reenable_bit_active(),
          "gate2: bit still inactive during impedance");

    /* Duplicate confirmation while measuring is rejected */
    check(np_cvns_reenable_confirm() == NP_HUB_ERR_SAFETY_REJECTED,
          "gate2: duplicate confirm during IMPEDANCE rejected");
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t + 200U);
    check(g_imp_start_calls == 1,
          "gate2: duplicate confirm does not restart impedance");
}

static void test_confirm_hal_start_failure(void)
{
    uint32_t t = drive_to_await_confirm(1000U);
    g_imp_start_rc = NP_HUB_ERR_GENERIC;

    check(np_cvns_reenable_confirm() == NP_HUB_OK,
          "gate2: confirm accepts intent regardless of later HAL result");

    /* HAL start failure now surfaces when the heartbeat tries to start it. */
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t + 100U);
    check(ev == NP_CVNS_RN_EV_IMPEDANCE_FAILED,
          "gate2: HAL start failure emits IMPEDANCE_FAILED");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_AWAIT_CONFIRM,
          "gate2: HAL start failure stays in AWAIT_CONFIRM (retryable)");
    check(!np_cvns_reenable_bit_active(),
          "gate2: bit inactive after HAL start failure");
}

/* Race regression: a confirmation posted for cutoff A must not attach to a
 * fresh cutoff B whose lockout has not elapsed (findings 1+2). */
static void test_confirm_does_not_carry_to_new_cutoff(void)
{
    uint32_t t = drive_to_await_confirm(1000U);

    /* App confirms cutoff A (intent posted, not yet consumed) ... */
    check(np_cvns_reenable_confirm() == NP_HUB_OK,
          "race: confirm posted for cutoff A");

    /* ... but before the heartbeat consumes it, CARDIAC clears then a NEW
     * cutoff B fires.  The stale confirmation must be discarded. */
    (void)np_cvns_reenable_on_heartbeat(ST_CLEAR,   true, t + 50U);  /* A cleared → IDLE, pending cleared */
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t + 100U);   /* B rising edge */
    check(ev == NP_CVNS_RN_EV_CUTOFF_DETECTED &&
          np_cvns_reenable_get_state() == NP_CVNS_RN_LOCKOUT,
          "race: new cutoff B restarts in LOCKOUT");

    /* Advancing time must NOT auto-start impedance — the stale confirm is gone. */
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, t + 200U);
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_LOCKOUT &&
          g_imp_start_calls == 0,
          "race: stale confirmation did not carry into cutoff B");
    check(!np_cvns_reenable_bit_active(),
          "race: bit never asserted for un-confirmed cutoff B");
}

/* ── Tests: gate 3 — impedance result ───────────────────────────────────────── */

static void test_impedance_pass_asserts(void)
{
    uint32_t ti = drive_to_impedance(1000U);
    g_imp_complete = true;
    g_imp_passed   = true;
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, ti + 200U);
    check(ev == NP_CVNS_RN_EV_IMPEDANCE_PASSED,
          "gate3: impedance pass event");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_ASSERTING,
          "gate3: state is ASSERTING after pass");
    check(np_cvns_reenable_bit_active(),
          "gate3: bit ACTIVE only after all three gates");
}

static void test_impedance_fail_closes(void)
{
    uint32_t ti = drive_to_impedance(1000U);
    g_imp_complete = true;
    g_imp_passed   = false;
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, ti + 200U);
    check(ev == NP_CVNS_RN_EV_IMPEDANCE_FAILED,
          "gate3: impedance fail event");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_AWAIT_CONFIRM,
          "gate3: fail returns to AWAIT_CONFIRM");
    check(!np_cvns_reenable_bit_active(),
          "gate3: bit inactive after fail");
}

static void test_impedance_timeout_closes(void)
{
    uint32_t ti = drive_to_impedance(1000U);

    /* Never completes; poll until past the timeout */
    g_imp_complete = false;
    np_cvns_reenable_event_t ev = np_cvns_reenable_on_heartbeat(
        ST_CARDIAC, true, ti + NP_CVNS_REENABLE_IMP_TIMEOUT_MS - 1U);
    check(ev == NP_CVNS_RN_EV_NONE &&
          np_cvns_reenable_get_state() == NP_CVNS_RN_IMPEDANCE,
          "gate3: just under timeout still measuring");

    ev = np_cvns_reenable_on_heartbeat(
        ST_CARDIAC, true, ti + NP_CVNS_REENABLE_IMP_TIMEOUT_MS);
    check(ev == NP_CVNS_RN_EV_IMPEDANCE_TIMEOUT,
          "gate3: timeout event at limit");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_AWAIT_CONFIRM,
          "gate3: timeout returns to AWAIT_CONFIRM (fail-closed)");
}

/* ── Tests: assertion completion and bounded assertion ──────────────────────── */

/* Drive fully to ASSERTING (all three gates passed).  Returns the assert-entry tick. */
static uint32_t drive_to_asserting(uint32_t t0)
{
    uint32_t ti = drive_to_impedance(t0);
    g_imp_complete = true;
    g_imp_passed   = true;
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, ti + 200U);
    return ti + 200U;
}

static void test_assert_completes_on_cardiac_clear(void)
{
    uint32_t ta = drive_to_asserting(1000U);
    check(np_cvns_reenable_bit_active(), "assert: bit active in ASSERTING");

    /* MCU processed the re-enable: CARDIAC cleared in next reply */
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CLEAR, true, ta + 200U);
    check(ev == NP_CVNS_RN_EV_REENABLE_COMPLETE,
          "assert: CARDIAC clear completes re-enable");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_IDLE,
          "assert: back to IDLE after completion");
    check(!np_cvns_reenable_bit_active(),
          "assert: bit deasserted after completion");
}

static void test_assert_timeout(void)
{
    uint32_t ta = drive_to_asserting(1000U);
    check(np_cvns_reenable_bit_active(), "assert-timeout: bit active");

    /* MCU never clears CARDIAC */
    np_cvns_reenable_event_t ev = np_cvns_reenable_on_heartbeat(
        ST_CARDIAC, true, ta + NP_CVNS_REENABLE_ASSERT_TIMEOUT_MS);
    check(ev == NP_CVNS_RN_EV_ASSERT_TIMEOUT,
          "assert-timeout: event fires at limit");
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_AWAIT_CONFIRM,
          "assert-timeout: back to AWAIT_CONFIRM (fresh confirm required)");
    check(!np_cvns_reenable_bit_active(),
          "assert-timeout: bit deasserted");
}

/* ── Tests: resets and external clears ──────────────────────────────────────── */

static void test_session_not_running_resets(void)
{
    uint32_t ta = drive_to_asserting(1000U);
    check(np_cvns_reenable_bit_active(), "reset: bit active pre-reset");

    /* Session leaves RUNNING (abort/stop) — machine hard-resets */
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, false, ta + 200U);
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_IDLE,
          "reset: not-running resets to IDLE");
    check(!np_cvns_reenable_bit_active(),
          "reset: bit deasserted on session boundary");
}

static void test_session_reset_api(void)
{
    (void)drive_to_await_confirm(1000U);
    np_cvns_reenable_session_reset();
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_IDLE,
          "reset: session_reset() returns to IDLE");
}

static void test_cardiac_cleared_externally(void)
{
    /* In LOCKOUT */
    np_cvns_reenable_init();
    hal_reset();
    (void)np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, 1000U);
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CLEAR, true, 2000U);
    check(ev == NP_CVNS_RN_EV_CARDIAC_CLEARED &&
          np_cvns_reenable_get_state() == NP_CVNS_RN_IDLE,
          "external-clear: LOCKOUT → IDLE when CARDIAC clears");

    /* In AWAIT_CONFIRM */
    uint32_t t = drive_to_await_confirm(1000U);
    ev = np_cvns_reenable_on_heartbeat(ST_CLEAR, true, t + 200U);
    check(ev == NP_CVNS_RN_EV_CARDIAC_CLEARED &&
          np_cvns_reenable_get_state() == NP_CVNS_RN_IDLE,
          "external-clear: AWAIT_CONFIRM → IDLE when CARDIAC clears");
}

static void test_confirmation_single_use(void)
{
    /* Complete one full cycle */
    uint32_t ta = drive_to_asserting(1000U);
    (void)np_cvns_reenable_on_heartbeat(ST_CLEAR, true, ta + 200U);
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_IDLE,
          "single-use: cycle complete, IDLE");

    /* Second cutoff: prior confirmation must NOT carry over */
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, ta + 400U);
    check(ev == NP_CVNS_RN_EV_CUTOFF_DETECTED,
          "single-use: second cutoff detected");
    check(np_cvns_reenable_confirm() == NP_HUB_ERR_SAFETY_REJECTED,
          "single-use: confirm rejected during new lockout");
    check(!np_cvns_reenable_bit_active(),
          "single-use: bit inactive after second cutoff");
}

static void test_new_cutoff_during_impedance_restarts(void)
{
    /* A NEW rising edge (clear then set) mid-flow restarts the sequence.  */
    uint32_t ti = drive_to_impedance(1000U);
    check(np_cvns_reenable_get_state() == NP_CVNS_RN_IMPEDANCE,
          "restart: in IMPEDANCE before disruption");

    /* CARDIAC clears (external) then re-fires */
    (void)np_cvns_reenable_on_heartbeat(ST_CLEAR, true, ti + 100U);
    np_cvns_reenable_event_t ev =
        np_cvns_reenable_on_heartbeat(ST_CARDIAC, true, ti + 200U);
    check(ev == NP_CVNS_RN_EV_CUTOFF_DETECTED &&
          np_cvns_reenable_get_state() == NP_CVNS_RN_LOCKOUT,
          "restart: new rising edge restarts full sequence");
}

/* ── Tests: session_status bit construction (enum-cast regression) ──────────── */

static void test_session_status_bits(void)
{
    /* Regression: RUNNING (=3) must map to ACTIVE only — never CVNS_REENABLE */
    check(np_safety_session_status_bits(NP_SESSION_RUNNING, false, false)
              == NP_SESSION_STATUS_ACTIVE,
          "bits: RUNNING → ACTIVE only (enum-cast regression)");

    /* Regression: PAUSED (=4) must not assert GEOM_REQUIRED */
    check(np_safety_session_status_bits(NP_SESSION_PAUSED, false, false)
              == NP_SESSION_STATUS_ACTIVE,
          "bits: PAUSED → ACTIVE only (no spurious GEOM_REQUIRED)");

    check(np_safety_session_status_bits(NP_SESSION_STOPPING, false, false)
              == NP_SESSION_STATUS_ACTIVE,
          "bits: STOPPING → ACTIVE (ramp-down still in session)");

    /* Non-session states carry no ACTIVE bit */
    check(np_safety_session_status_bits(NP_SESSION_IDLE, false, false) == 0U,
          "bits: IDLE → 0");
    check(np_safety_session_status_bits(NP_SESSION_LOADING, false, false) == 0U,
          "bits: LOADING → 0 (no premature session_active)");
    check(np_safety_session_status_bits(NP_SESSION_VERIFYING, false, false) == 0U,
          "bits: VERIFYING → 0");
    check(np_safety_session_status_bits(NP_SESSION_COMPLETE, false, false) == 0U,
          "bits: COMPLETE → 0");
    check(np_safety_session_status_bits(NP_SESSION_FAULT, false, false) == 0U,
          "bits: FAULT → 0");

    /* Flag bits OR in independently */
    check(np_safety_session_status_bits(NP_SESSION_RUNNING, false, true)
              == (NP_SESSION_STATUS_ACTIVE | NP_SESSION_STATUS_CVNS_REENABLE),
          "bits: cvns flag ORs in CVNS_REENABLE");
    check(np_safety_session_status_bits(NP_SESSION_RUNNING, true, false)
              == (NP_SESSION_STATUS_ACTIVE | NP_SESSION_STATUS_GEOM_REQUIRED),
          "bits: geom flag ORs in GEOM_REQUIRED");
    check(np_safety_session_status_bits(NP_SESSION_RUNNING, true, true)
              == (NP_SESSION_STATUS_ACTIVE | NP_SESSION_STATUS_CVNS_REENABLE |
                  NP_SESSION_STATUS_GEOM_REQUIRED),
          "bits: all three bits combine");

    /* Wire-contract constants match the safety MCU side */
    check(NP_SESSION_STATUS_ACTIVE == (1U << 0),
          "bits: ACTIVE is bit 0 (wire contract)");
    check(NP_SESSION_STATUS_CVNS_REENABLE == (1U << 1),
          "bits: CVNS_REENABLE is bit 1 (wire contract)");
    check(NP_SESSION_STATUS_GEOM_REQUIRED == (1U << 2),
          "bits: GEOM_REQUIRED is bit 2 (wire contract)");
    check(NP_CVNS_REENABLE_LOCKOUT_MS == 30000U,
          "bits: hub lockout matches MCU NP_CARDIAC_LOCKOUT_MS (30000)");
}

/* ── Main ───────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("── np_cvns_reenable_tests (OI-CVNS-HUB-01) ──\n");

    test_init_idle();
    test_cutoff_rising_edge();
    test_lockout_timing();
    test_lockout_tick_wrap();
    test_confirm_in_idle_rejected();
    test_confirm_accepted_starts_impedance();
    test_confirm_hal_start_failure();
    test_confirm_does_not_carry_to_new_cutoff();
    test_impedance_pass_asserts();
    test_impedance_fail_closes();
    test_impedance_timeout_closes();
    test_assert_completes_on_cardiac_clear();
    test_assert_timeout();
    test_session_not_running_resets();
    test_session_reset_api();
    test_cardiac_cleared_externally();
    test_confirmation_single_use();
    test_new_cutoff_during_impedance_restarts();
    test_session_status_bits();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
