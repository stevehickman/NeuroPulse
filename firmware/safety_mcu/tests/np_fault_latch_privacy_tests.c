/*
 * NeurOne Safety MCU — Fault Latch Privacy-Gate Host Tests
 * Document: NP-SW-001 (SW01-M08), NP-FW-EMMC-001 Rev 2 §12,
 *           docs/status/pending-decisions.md §13.4 "Fault latch extended SPI command privacy gate"
 *
 * Host-native tests for the SHDR marshalling that any future dedicated
 * fault-latch SPI read command MUST use to surface latch fields to the hub:
 *
 *   np_fault_latch_build_report() → ONE fixed-shape record {status, slot, count}.
 *                                   tick_ms is not a member — fault event timing
 *                                   is UHDR.  No member is conditioned on WHICH
 *                                   fault is latched.
 *
 * The central test is ISC-16/17: with identical history, the reported record for
 * a cardiac fault and for a non-cardiac fault must differ ONLY in `status` — the
 * deliberately published fault-kind channel, which SHDR already carries as
 * fault_log.fault_type 'CVNS_HR_CUTOFF'.  Every other reported field must be
 * bit-identical, so no pattern of reporting discloses that the fault was cardiac.
 *
 * These build src/np_fault_latch.c with NPTEST_HOST (drops the .fault_latch
 * Mach-O section attribute) and a mocked SysTick HAL; no ARM cross-compilation.
 *
 * IEC 62304 Class C — SW-01 safety MCU.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/safety_mcu/CMakeLists.txt).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_safety_config.h"
#include "../include/np_safety_hal.h"
#include "../include/np_safety_protocol.h"

/* ── Unit under test ─────────────────────────────────────────────────────────── */
extern np_safe_status_t np_fault_latch_init(bool *prior_fault_out);
extern void             np_fault_latch_commit(const np_safety_state_t *state);
extern void             np_fault_latch_clear(void);
extern uint8_t          np_fault_latch_get_status(void);
extern uint8_t          np_fault_latch_get_slot(void);
/* np_fault_latch_build_report is declared in np_safety_protocol.h — declaring it
 * there rather than extern here means signature drift is a compile error. */

/* ── Mocked HAL: controllable SysTick ──────────────────────────────────────────
 * Defines a symbol declared in np_safety_hal.h — drift is a compile error
 * (OI-SWCI-18).                                                              */
static uint32_t g_tick_ms = 0U;
uint32_t np_hal_get_tick_ms(void) { return g_tick_ms; }

/* ── Harness ─────────────────────────────────────────────────────────────────── */
static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

static np_safety_state_t make_state(uint8_t status, uint8_t slot)
{
    np_safety_state_t s = {0};
    s.status     = status;
    s.fault_slot = slot;
    return s;
}

static np_fault_latch_report_t report(void)
{
    np_fault_latch_report_t r;
    memset(&r, 0xA5, sizeof(r));   /* poison: the builder must overwrite every member */
    np_fault_latch_build_report(&r);
    return r;
}

/*
 * Commit exactly one fault of the given kind onto a freshly cleared latch, at a
 * fixed tick, and return the reported record.  Both arms of the
 * indistinguishability test run through this identical path so the ONLY
 * difference between them is the status bit under test.
 */
static np_fault_latch_report_t single_fault_report(uint8_t status, uint8_t slot,
                                                    uint32_t tick)
{
    np_fault_latch_clear();
    g_tick_ms = tick;
    np_safety_state_t s = make_state(status, slot);
    np_fault_latch_commit(&s);
    return report();
}

int main(void)
{
    bool prior = true;

    /* ── Clean-latch baseline (host s_latch starts zeroed → power-on path) ──── */
    np_fault_latch_init(&prior);
    check(prior == false,                     "init: no prior fault on clean latch");
    np_fault_latch_report_t r0 = report();
    check(r0.count  == 0U,                    "init: reported count is 0");
    check(r0.slot   == 0xFFU,                 "init: reported slot is 0xFF");
    check(r0.status == NP_SAFETY_STATUS_OK,   "init: reported status is OK");

    /* ── Non-cardiac fault: count == 1, status + slot surfaced ─────────────── */
    g_tick_ms = 123456U;
    np_safety_state_t thermal = make_state(NP_SAFETY_STATUS_THERMAL, 2U);
    np_fault_latch_commit(&thermal);
    check(np_fault_latch_get_status() == NP_SAFETY_STATUS_THERMAL, "thermal: status latched");
    np_fault_latch_report_t r1 = report();
    check(r1.count  == 1U,                        "thermal: count == 1");
    check(r1.status == NP_SAFETY_STATUS_THERMAL,  "thermal: status reported");
    check(r1.slot   == 2U,                        "thermal: slot reported");

    /* ── Idempotent commit does NOT increment count (change-gate) ──────────── */
    g_tick_ms = 999999U;
    np_fault_latch_commit(&thermal);
    check(report().count == 1U,               "thermal repeat: count unchanged (change-gated)");

    /* ── Cardiac fault: count still increments, nothing is suppressed ───────── */
    g_tick_ms = 555000U;
    np_safety_state_t cardiac = make_state(NP_SAFETY_STATUS_CARDIAC, 0xFFU);
    np_fault_latch_commit(&cardiac);
    check(np_fault_latch_get_status() == NP_SAFETY_STATUS_CARDIAC, "cardiac: status latched");
    np_fault_latch_report_t r2 = report();
    check(r2.count  == 2U,                        "cardiac: count == 2 (distinct transition)");
    check(r2.status == NP_SAFETY_STATUS_CARDIAC,  "cardiac: status reported, not suppressed");

    /* ── Cardiac bit alongside other bits: still nothing suppressed ─────────── */
    g_tick_ms = 700000U;
    np_safety_state_t cardiac_plus =
        make_state((uint8_t)(NP_SAFETY_STATUS_CARDIAC | NP_SAFETY_STATUS_CHARGE), 5U);
    np_fault_latch_commit(&cardiac_plus);
    np_fault_latch_report_t r3 = report();
    check(r3.count == 3U,                     "cardiac+charge: count == 3");
    check(r3.slot  == 5U,                     "cardiac+charge: slot reported");

    /* ── Recovery to a non-cardiac fault ───────────────────────────────────── */
    g_tick_ms = 800000U;
    np_safety_state_t charge = make_state(NP_SAFETY_STATUS_CHARGE, 6U);
    np_fault_latch_commit(&charge);
    check(report().count == 4U,               "post-cardiac charge: count == 4");

    /* ── Clear resets the reported record ──────────────────────────────────── */
    np_fault_latch_clear();
    np_fault_latch_report_t rc = report();
    check(rc.count == 0U,                     "clear: count == 0");
    check(rc.slot  == 0xFFU,                  "clear: slot == 0xFF");

    /* ══ ISC-16/17 — THE ORACLE TEST ═══════════════════════════════════════════
     * Two arms, identical in every respect except the latched fault kind, at the
     * SAME tick.  The reported records must differ ONLY in `status`.
     *
     * This is the regression guard for the defect fixed 2026-08-12: the retired
     * design reported `count` unconditionally while zeroing `tick_ms` only for
     * cardiac faults, so (count > 0, tick_ms == 0) identified a cardiac fault
     * with certainty.  Any reintroduction of a status-conditioned reported field
     * makes the masked comparison below differ, and this test FAILS.
     *
     * The `count`/`slot` equalities are pinned to their POSITIVE expected values
     * (1 and 2), not merely to each other: an implementation that reported
     * nothing at all would make the two arms equal but would fail those pins.  A
     * "the bad thing is absent" assertion alone is satisfied by a dead builder.
     */
    const uint32_t T = 424242U;
    np_fault_latch_report_t non_cardiac =
        single_fault_report(NP_SAFETY_STATUS_THERMAL, 2U, T);
    np_fault_latch_report_t is_cardiac =
        single_fault_report(NP_SAFETY_STATUS_CARDIAC, 2U, T);

    /* Positive pins — kill the dead-implementation mutant. */
    check(non_cardiac.count == 1U,  "oracle: non-cardiac arm positively reports count == 1");
    check(is_cardiac.count  == 1U,  "oracle: cardiac arm positively reports count == 1");
    check(non_cardiac.slot  == 2U,  "oracle: non-cardiac arm positively reports slot == 2");
    check(is_cardiac.slot   == 2U,  "oracle: cardiac arm positively reports slot == 2");

    /* The deliberate channel: status DOES distinguish, and is meant to. */
    check(non_cardiac.status != is_cardiac.status,
          "oracle: status is the one deliberate fault-kind channel");

    /* The guard: masking that one channel makes the records bit-identical. */
    np_fault_latch_report_t masked_nc = non_cardiac;
    np_fault_latch_report_t masked_c  = is_cardiac;
    masked_nc.status = 0U;
    masked_c.status  = 0U;
    check(memcmp(&masked_nc, &masked_c, sizeof(masked_nc)) == 0,
          "oracle: cardiac and non-cardiac reports differ ONLY in status (no oracle)");

    printf("\n%s (%d failures)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
           g_failures);
    return g_failures == 0 ? 0 : 1;
}
