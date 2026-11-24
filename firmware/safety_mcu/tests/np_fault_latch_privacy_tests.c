/*
 * NeurOne Safety MCU — Fault Latch Privacy-Gate Host Tests
 * Document: NP-SW-001 (SW01-M08, REQ-SW01-06), NP-FW-EMMC-001 Rev 2 §12,
 *           docs/status/pending-decisions.md §13.4 "Fault latch extended SPI command privacy gate",
 *           OI-PRIV-40 (NP-PRIV-ANALYSIS-004)
 *
 * Host-native tests for the SHDR marshalling that any future dedicated
 * fault-latch SPI read command MUST use to surface latch fields to the hub:
 *
 *   np_fault_latch_build_report() → ONE fixed-shape record {status, count}:
 *                                   the latch projected onto the device-class
 *                                   status bits.  No slot, no tick.  No member
 *                                   is conditioned on WHICH fault is latched.
 *
 * The central tests are the oracle tests: with identical device-class history,
 * a history that adds impedance transitions (the in-session check, UHDR, the
 * principal 2026-11-23) or the cardiac bit must report bit-identically to one
 * without them, so no pattern of reporting discloses either.
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
/* np_fault_latch_build_report and the host-only save/restore/raw count are
 * declared in np_safety_protocol.h — drift is a compile error. */

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

/* The record has exactly two members, and no slot: a slot member reappearing
 * fails the build here (REQ-SW01-06 (c)). */
_Static_assert(sizeof(((np_fault_latch_report_t *)0)->status) == 1U, "status is one byte");
_Static_assert(sizeof(((np_fault_latch_report_t *)0)->count) == 4U, "count is 32 bits (OI-UINT16-01)");
_Static_assert(sizeof(np_fault_latch_report_t) == 8U, "status, padding and count: nothing else");

#define IMP      NP_SAFETY_STATUS_IMPEDANCE
#define CARDIAC  NP_SAFETY_STATUS_CARDIAC
#define FAULT    NP_SAFETY_STATUS_FAULT
#define CUTOFF   NP_SAFETY_STATUS_CUTOFF
#define THERMAL  NP_SAFETY_STATUS_THERMAL
#define CHARGE   NP_SAFETY_STATUS_CHARGE
#define WATCHDOG NP_SAFETY_STATUS_WATCHDOG

static void commit(uint8_t status, uint8_t slot)
{
    np_safety_state_t s = {0};
    s.status     = status;
    s.fault_slot = slot;
    np_fault_latch_commit(&s);
}

static np_fault_latch_report_t report(void)
{
    np_fault_latch_report_t r;
    memset(&r, 0xA5, sizeof(r));   /* poison: the builder must overwrite every member */
    np_fault_latch_build_report(&r);
    return r;
}

static bool same(np_fault_latch_report_t a, np_fault_latch_report_t b)
{
    return a.status == b.status && a.count == b.count;
}

typedef struct { uint8_t status; uint8_t slot; } step_t;

/* Replay a history onto a freshly cleared latch, with the tick advancing per
 * step, and return the report. */
static np_fault_latch_report_t replay(const step_t *steps, size_t n)
{
    np_fault_latch_clear();
    for (size_t k = 0U; k < n; k++) {
        g_tick_ms = 1000U * (uint32_t)(k + 1U);
        commit(steps[k].status, steps[k].slot);
    }
    return report();
}

/* @verifies REQ-SW01-06#a,b,c */
static void test_clean_and_invalid_latch_report_nothing(void)
{
    bool prior = true;
    np_fault_latch_init(&prior);       /* host s_latch starts zeroed → power-on path */
    check(prior == false, "init: no prior fault on clean latch");
    np_fault_latch_report_t r0 = report();
    check(r0.count == 0U && r0.status == NP_SAFETY_STATUS_OK, "init: reported {OK, 0}");

    /* An invalid latch (no magic) reports the same shape as an empty one. */
    uint8_t img[NP_FAULT_LATCH_BYTES];
    memset(img, 0x5A, sizeof img);     /* garbage, magic wrong */
    np_fault_latch_host_restore(img);
    np_fault_latch_report_t ri = report();
    check(same(ri, r0), "invalid latch: reports exactly as an empty latch");
    np_fault_latch_clear();
}

/* @verifies REQ-SW01-06#a */
static void test_status_is_projected_onto_device_bits(void)
{
    np_fault_latch_clear();
    commit((uint8_t)(FAULT | WATCHDOG | CUTOFF | THERMAL | CHARGE | IMP | CARDIAC), 7U);
    check(np_fault_latch_get_status() ==
              (uint8_t)(FAULT | WATCHDOG | CUTOFF | THERMAL | CHARGE | IMP | CARDIAC),
          "projection: the latch itself still holds every bit (control path unchanged)");
    check(np_fault_latch_get_slot() == 7U, "projection: the latch itself still holds the slot");
    np_fault_latch_report_t r = report();
    check(r.status == (uint8_t)(FAULT | WATCHDOG | CUTOFF | THERMAL | CHARGE),
          "projection: every device-class bit is reported");
    check((r.status & (IMP | CARDIAC)) == 0U, "projection: impedance and cardiac bits are not");
    check(r.count == 1U, "projection: one device-class transition counted");
}

/* @verifies REQ-SW01-06#b */
static void test_impedance_only_transition_reports_as_none(void)
{
    /* A latched device fault (FAULT, which is what makes the main loop commit),
     * then the impedance check fails on channel slot 7, passes, fails on slot
     * 10: four raw transitions that are impedance only. */
    const step_t without[] = { { FAULT, 0xFFU } };
    const step_t with[] = {
        { FAULT, 0xFFU },
        { (uint8_t)(FAULT | IMP), 7U },
        { FAULT, 7U },
        { (uint8_t)(FAULT | IMP), 10U },
    };
    np_fault_latch_report_t a = replay(without, 1U);
    np_fault_latch_report_t b = replay(with, 4U);
    check(np_fault_latch_host_raw_count() == 4U, "impedance: the raw latch did count four transitions");
    check(a.status == FAULT && a.count == 1U, "impedance: positive pin, the device fault is reported once");
    check(same(a, b), "impedance: an impedance-only history reports identically to none (bit, count)");

    /* The masked-bit oracle the brief warned of: impedance alone on an
     * otherwise clean latch must not read "OK with a slot" or move the count. */
    const step_t imp_alone[] = { { IMP, 5U } };
    np_fault_latch_report_t c = replay(imp_alone, 1U);
    np_fault_latch_report_t none = replay(NULL, 0U);
    check(same(c, none), "impedance: impedance alone reports exactly as an empty latch");
}

/* @verifies REQ-SW01-06#b,c */
static void test_slot_alone_moves_nothing(void)
{
    /* The same device fault, latched under three different slots: the slot is
     * not reported and a slot change alone is not a device transition. */
    const step_t one[]   = { { (uint8_t)(THERMAL | CUTOFF | FAULT), 2U } };
    const step_t moved[] = { { (uint8_t)(THERMAL | CUTOFF | FAULT), 2U },
                             { (uint8_t)(THERMAL | CUTOFF | FAULT), 6U },
                             { (uint8_t)(THERMAL | CUTOFF | FAULT), 10U } };
    check(same(replay(one, 1U), replay(moved, 3U)), "slot: a slot change alone reports as nothing");
}

/* @verifies REQ-SW01-06#a,b */
static void test_cardiac_bit_is_not_disclosed_a_second_time(void)
{
    /* The real cardiac cutoff sets CARDIAC | CUTOFF; the same cutoff without
     * the cardiac bit must report identically.  The cutoff itself is the
     * device-class fact; the hub's SHDR fault log names the cardiac kind under
     * its own ruling. */
    const step_t cardiac[] = { { FAULT, 0xFFU }, { (uint8_t)(FAULT | CARDIAC | CUTOFF), 10U } };
    const step_t plain[]   = { { FAULT, 0xFFU }, { (uint8_t)(FAULT | CUTOFF), 3U } };
    np_fault_latch_report_t a = replay(cardiac, 2U);
    np_fault_latch_report_t b = replay(plain, 2U);
    check(a.status == (uint8_t)(FAULT | CUTOFF) && a.count == 2U, "cardiac: positive pin, the cutoff is reported");
    check(same(a, b), "cardiac: a cardiac cutoff reports identically to a plain cutoff");
}

/* @verifies REQ-SW01-06#b */
static void test_device_count_is_change_gated_and_32_bit(void)
{
    np_fault_latch_clear();
    commit(THERMAL, 2U);
    commit(THERMAL, 2U);               /* idempotent: not a transition */
    check(report().count == 1U, "count: an idempotent commit is not counted");

    np_fault_latch_clear();
    for (uint32_t i = 0U; i < 70000U; i++) {
        commit((uint8_t)((i & 1U) ? THERMAL : CHARGE), 2U);   /* device bits alternate */
    }
    check(report().count == 70000U, "count: 70000 transitions are counted as 70000, not held at 65535");

    np_fault_latch_clear();
    check(report().count == 0U && report().status == NP_SAFETY_STATUS_OK, "clear: the report is {OK, 0}");
}

/* @verifies-none harness check that the host save/restore the simulator's warm-reset model uses keeps every byte, raw slot and count included */
static void test_host_save_restore_round_trip(void)
{
    np_fault_latch_clear();
    commit((uint8_t)(FAULT | IMP), 7U);
    commit(FAULT, 7U);
    uint8_t img[NP_FAULT_LATCH_BYTES];
    np_fault_latch_host_save(img);
    np_fault_latch_report_t before = report();
    np_fault_latch_clear();
    np_fault_latch_host_restore(img);
    check(np_fault_latch_get_status() == FAULT && np_fault_latch_get_slot() == 7U &&
              np_fault_latch_host_raw_count() == 2U,
          "host save/restore: raw status, slot and count survive");
    check(same(report(), before), "host save/restore: the report survives");
    np_fault_latch_clear();
}

int main(void)
{
    test_clean_and_invalid_latch_report_nothing();
    test_status_is_projected_onto_device_bits();
    test_impedance_only_transition_reports_as_none();
    test_slot_alone_moves_nothing();
    test_cardiac_bit_is_not_disclosed_a_second_time();
    test_device_count_is_change_gated_and_32_bit();
    test_host_save_restore_round_trip();

    printf("\n%s (%d failures)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
           g_failures);
    return g_failures == 0 ? 0 : 1;
}
