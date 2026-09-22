/*
 * NeurOne Safety MCU — SW01-M09 Non-Volatile Safety State Host Tests
 * Document: NP-SW-FAULTMSG-001 P1 (OI-FAULTMSG-01)
 *
 * Drives the REAL record log (src/np_nv_state.c) over a RAM double of the two
 * NV flash pages that obeys NOR rules: erase sets all-ones, programming only
 * succeeds on an erased slot, and a failure can be injected — including a
 * TORN write, which leaves a non-erased, invalid double-word, the state a power
 * loss during programming leaves behind.
 *
 * The property under test is the one P1 exists for: no sequence of writes and
 * power losses can make a persisted cutoff read back as acknowledged, and a
 * factory-fresh unit reads as clear.  "Power cycle" here is np_nv_state_init()
 * re-reading the pages, which is exactly what boot does.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "../include/np_safety_config.h"
#include "../include/np_safety_hal.h"
#include "../include/np_nv_state.h"

/* ── NOR flash double ───────────────────────────────────────────────────────── */
static uint32_t g_flash[NP_NV_PAGE_COUNT][NP_NV_SLOTS_PER_PAGE][2];
static int      g_fail_program_next;   /* next program: return false, no write */
static int      g_tear_program_next;   /* next program: write garbage, false   */
static int      g_fail_erase_next;     /* next erase: return false, no change  */
static int      g_erase_count[NP_NV_PAGE_COUNT];

static void flash_blank(void)
{
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_fail_program_next = 0;
    g_tear_program_next = 0;
    g_fail_erase_next   = 0;
    g_erase_count[0] = 0;
    g_erase_count[1] = 0;
}

void np_hal_nv_read_dword(uint8_t page, uint16_t slot, uint32_t *lo, uint32_t *hi)
{
    *lo = g_flash[page][slot][0];
    *hi = g_flash[page][slot][1];
}

bool np_hal_nv_erase_page(uint8_t page)
{
    if (g_fail_erase_next) { g_fail_erase_next = 0; return false; }
    memset(g_flash[page], 0xFF, sizeof(g_flash[page]));
    g_erase_count[page]++;
    return true;
}

bool np_hal_nv_program_dword(uint8_t page, uint16_t slot, uint32_t lo, uint32_t hi)
{
    if ((g_flash[page][slot][0] != 0xFFFFFFFFUL) || (g_flash[page][slot][1] != 0xFFFFFFFFUL)) {
        return false;   /* PROGERR: programming a non-erased double-word */
    }
    if (g_fail_program_next) { g_fail_program_next = 0; return false; }
    if (g_tear_program_next) {
        g_tear_program_next = 0;
        g_flash[page][slot][0] = lo & 0x0F0F0F0FUL;   /* half-programmed bits */
        g_flash[page][slot][1] = hi & 0x00FF00FFUL;
        return false;
    }
    g_flash[page][slot][0] = lo;
    g_flash[page][slot][1] = hi;
    return true;
}

/* ── Harness ────────────────────────────────────────────────────────────────── */
static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

static bool power_cycle(void)
{
    (void)np_nv_state_init();
    return np_nv_cardiac_pending();
}

static uint16_t used_slots(uint8_t page)
{
    uint16_t n = 0U;
    for (uint16_t s = 0U; s < NP_NV_SLOTS_PER_PAGE; s++) {
        if ((g_flash[page][s][0] != 0xFFFFFFFFUL) || (g_flash[page][s][1] != 0xFFFFFFFFUL)) {
            n = (uint16_t)(s + 1U);
        }
    }
    return n;
}

/* ── Tests ──────────────────────────────────────────────────────────────────── */

static void test_factory_fresh_is_clear(void)
{
    flash_blank();
    check(!power_cycle(), "fresh: never-written storage reads as not pending");
}

static void test_set_survives_power_cycle(void)
{
    flash_blank();
    (void)np_nv_state_init();
    check(np_nv_cardiac_pending_set(true), "set: write succeeds");
    check(np_nv_cardiac_pending(), "set: cached value pending");
    check(power_cycle(), "set: pending after power cycle");
}

static void test_clear_after_set(void)
{
    flash_blank();
    (void)np_nv_state_init();
    (void)np_nv_cardiac_pending_set(true);
    check(np_nv_cardiac_pending_set(false), "clear: write succeeds");
    check(!power_cycle(), "clear: not pending after power cycle");
}

/* A torn newest write may have been a SET, so it must read as pending even when
 * the record before it said "acknowledged". */
static void test_torn_newest_fails_closed(void)
{
    flash_blank();
    (void)np_nv_state_init();
    (void)np_nv_cardiac_pending_set(true);
    (void)np_nv_cardiac_pending_set(false);
    g_tear_program_next = 1;
    check(!np_nv_cardiac_pending_set(true), "torn: write reports failure");
    check(power_cycle(), "torn: torn newest record reads as pending (fail closed)");
}

/* A torn record that a later valid record supersedes no longer matters. */
static void test_torn_then_superseded(void)
{
    flash_blank();
    (void)np_nv_state_init();
    (void)np_nv_cardiac_pending_set(true);
    g_tear_program_next = 1;
    (void)np_nv_cardiac_pending_set(false);
    check(np_nv_cardiac_pending_set(false), "superseded: retry succeeds after torn slot");
    check(!power_cycle(), "superseded: valid record after torn slot decides");
}

static void test_program_failure_reported(void)
{
    flash_blank();
    (void)np_nv_state_init();
    g_fail_program_next = 1;
    check(!np_nv_cardiac_pending_set(true), "program fail: reported as failure");
    check(np_nv_cardiac_pending_set(true), "program fail: retry succeeds");
    check(power_cycle(), "program fail: pending after retry");
}

/* Filling a page forces a rotation: the new record goes to the other page
 * first, and the full page is erased only afterwards. */
static void test_rotation(void)
{
    flash_blank();
    (void)np_nv_state_init();
    for (uint16_t i = 0U; i < NP_NV_SLOTS_PER_PAGE; i++) {
        (void)np_nv_cardiac_pending_set((i % 2U) == 0U);
    }
    check(used_slots(0U) == NP_NV_SLOTS_PER_PAGE, "rotation: page 0 full");
    check(!power_cycle(), "rotation: last record in full page is CLR");

    check(np_nv_cardiac_pending_set(true), "rotation: write into other page succeeds");
    check(used_slots(1U) == 1U, "rotation: record at page 1 slot 0");
    check(used_slots(0U) == 0U, "rotation: full page erased after new record");
    check(power_cycle(), "rotation: SET survives power cycle after rotation");
}

/* Power loss between writing the new page and erasing the old one: both pages
 * hold records, and the higher sequence number must win. */
static void test_rotation_interrupted_before_old_erase(void)
{
    flash_blank();
    (void)np_nv_state_init();
    for (uint16_t i = 0U; i < NP_NV_SLOTS_PER_PAGE; i++) {
        (void)np_nv_cardiac_pending_set(false);
    }
    /* The erase of the full page fails — the new record is already down. */
    {
        /* First erase in the write is step 1 (other page empty → skipped), so
         * the only erase attempted is of the old page. */
        g_fail_erase_next = 1;
        (void)np_nv_cardiac_pending_set(true);
    }
    check(used_slots(0U) == NP_NV_SLOTS_PER_PAGE && used_slots(1U) == 1U,
          "interrupted: both pages hold records");
    check(power_cycle(), "interrupted: newer SET wins over the old full page");

    check(np_nv_cardiac_pending_set(false), "interrupted: next write succeeds");
    check(used_slots(0U) == 0U, "interrupted: residue page erased by the next write");
    check(!power_cycle(), "interrupted: CLR after cleanup");
}

/* Power loss while writing slot 0 of the new page during rotation: the new
 * page holds only a torn record.  It may have been a SET, so fail closed; the
 * next write erases it and recovers. */
static void test_rotation_torn_new_page(void)
{
    flash_blank();
    (void)np_nv_state_init();
    for (uint16_t i = 0U; i < NP_NV_SLOTS_PER_PAGE; i++) {
        (void)np_nv_cardiac_pending_set(false);
    }
    g_tear_program_next = 1;
    check(!np_nv_cardiac_pending_set(true), "torn rotation: write reports failure");
    check(used_slots(0U) == NP_NV_SLOTS_PER_PAGE, "torn rotation: old page NOT erased");
    check(power_cycle(), "torn rotation: reads as pending (fail closed)");

    check(np_nv_cardiac_pending_set(true), "torn rotation: retry succeeds");
    check(power_cycle(), "torn rotation: pending after retry");
    check(np_nv_cardiac_pending_set(false), "torn rotation: later CLR succeeds");
    check(!power_cycle(), "torn rotation: recovers to CLR");
}

/* A page of garbage with no valid record anywhere (e.g. a torn very first
 * write) fails closed, and a write recovers from it. */
static void test_garbage_only_fails_closed(void)
{
    flash_blank();
    g_flash[0][0][0] = 0x12345678UL;
    g_flash[0][0][1] = 0x9ABCDEF0UL;
    check(power_cycle(), "garbage: undecodable storage reads as pending");
    check(np_nv_cardiac_pending_set(false), "garbage: write succeeds");
    check(!power_cycle(), "garbage: recovers to CLR");
}

int main(void)
{
    test_factory_fresh_is_clear();
    test_set_survives_power_cycle();
    test_clear_after_set();
    test_torn_newest_fails_closed();
    test_torn_then_superseded();
    test_program_failure_reported();
    test_rotation();
    test_rotation_interrupted_before_old_erase();
    test_rotation_torn_new_page();
    test_garbage_only_fails_closed();

    if (g_failures == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
