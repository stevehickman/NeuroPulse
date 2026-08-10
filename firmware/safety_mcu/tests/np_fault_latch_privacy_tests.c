/*
 * NeurOne Safety MCU — Fault Latch Privacy-Gate Host Tests
 * Document: NP-SW-001 (SW01-M08), NP-FW-EMMC-001 Rev A §12,
 *           docs/status/pending-decisions.md §13.4 "Fault latch extended SPI command privacy gate"
 *
 * Host-native tests for the privacy-gated marshalling helpers that any future
 * dedicated fault-latch SPI read command MUST use to surface the `count` and
 * `tick_ms` latch fields to the hub:
 *
 *   np_fault_latch_report_count()   → count, SHDR unconditionally
 *   np_fault_latch_report_tick_ms() → tick_ms, SHDR but SUPPRESSED (0) whenever
 *                                     the latched fault carries CARDIAC
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

#include "../include/np_safety_config.h"
#include "../include/np_safety_hal.h"
#include "../include/np_safety_protocol.h"

/* ── Unit under test ─────────────────────────────────────────────────────────── */
extern np_safe_status_t np_fault_latch_init(bool *prior_fault_out);
extern void             np_fault_latch_commit(const np_safety_state_t *state);
extern void             np_fault_latch_clear(void);
extern uint8_t          np_fault_latch_get_status(void);
extern uint8_t          np_fault_latch_get_slot(void);
extern uint32_t         np_fault_latch_report_tick_ms(void);
extern uint16_t         np_fault_latch_report_count(void);

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

int main(void)
{
    bool prior = true;

    /* ── Clean-latch baseline (host s_latch starts zeroed → power-on path) ──── */
    np_fault_latch_init(&prior);
    check(prior == false,                          "init: no prior fault on clean latch");
    check(np_fault_latch_report_count() == 0U,     "init: reported count is 0");
    check(np_fault_latch_report_tick_ms() == 0U,   "init: reported tick_ms is 0");

    /* ── Non-cardiac fault: tick_ms surfaced verbatim, count == 1 ──────────── */
    g_tick_ms = 123456U;
    np_safety_state_t thermal = make_state(NP_SAFETY_STATUS_THERMAL, 2U);
    np_fault_latch_commit(&thermal);
    check(np_fault_latch_get_status() == NP_SAFETY_STATUS_THERMAL, "thermal: status latched");
    check(np_fault_latch_report_count() == 1U,        "thermal: count == 1");
    check(np_fault_latch_report_tick_ms() == 123456U, "thermal: tick_ms surfaced (not suppressed)");

    /* ── Idempotent commit does NOT increment count or move tick (change-gate) ─ */
    g_tick_ms = 999999U;
    np_fault_latch_commit(&thermal);
    check(np_fault_latch_report_count() == 1U,        "thermal repeat: count unchanged (change-gated)");
    check(np_fault_latch_report_tick_ms() == 123456U, "thermal repeat: tick_ms unchanged");

    /* ── Cardiac fault: tick_ms SUPPRESSED to 0, count still increments ─────── */
    g_tick_ms = 555000U;
    np_safety_state_t cardiac = make_state(NP_SAFETY_STATUS_CARDIAC, 0xFFU);
    np_fault_latch_commit(&cardiac);
    check(np_fault_latch_get_status() == NP_SAFETY_STATUS_CARDIAC, "cardiac: status latched");
    check(np_fault_latch_report_count() == 2U,      "cardiac: count == 2 (distinct transition)");
    check(np_fault_latch_report_tick_ms() == 0U,    "cardiac: tick_ms suppressed to 0");

    /* ── Cardiac bit set alongside other bits still suppresses ─────────────── */
    g_tick_ms = 700000U;
    np_safety_state_t cardiac_plus =
        make_state((uint8_t)(NP_SAFETY_STATUS_CARDIAC | NP_SAFETY_STATUS_CHARGE), 5U);
    np_fault_latch_commit(&cardiac_plus);
    check(np_fault_latch_report_tick_ms() == 0U,    "cardiac+charge: tick_ms suppressed to 0");
    check(np_fault_latch_report_count() == 3U,      "cardiac+charge: count == 3");

    /* ── Recovery to a non-cardiac fault surfaces tick_ms again ─────────────── */
    g_tick_ms = 800000U;
    np_safety_state_t charge = make_state(NP_SAFETY_STATUS_CHARGE, 6U);
    np_fault_latch_commit(&charge);
    check(np_fault_latch_report_tick_ms() == 800000U, "post-cardiac charge: tick_ms surfaced");
    check(np_fault_latch_report_count() == 4U,        "post-cardiac charge: count == 4");

    /* ── Clear resets both reported fields ─────────────────────────────────── */
    np_fault_latch_clear();
    check(np_fault_latch_report_count() == 0U,      "clear: count == 0");
    check(np_fault_latch_report_tick_ms() == 0U,    "clear: tick_ms == 0");
    check(np_fault_latch_get_slot() == 0xFFU,       "clear: slot == 0xFF");

    printf("\n%s (%d failures)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
           g_failures);
    return g_failures == 0 ? 0 : 1;
}
