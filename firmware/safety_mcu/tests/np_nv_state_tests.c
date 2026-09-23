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
 * The properties under test:
 *   - no sequence of writes and power losses can make a persisted cutoff read
 *     back as acknowledged, and a factory-fresh unit reads as clear (P1);
 *   - a cutoff is held for the user who triggered it and nobody else, and the
 *     active user persists until the app names another (per-user scope,
 *     principal 2026-09-22);
 *   - anything that cannot be attributed to one person is withheld from all.
 * "Power cycle" here is np_nv_state_init() re-reading the pages, which is
 * exactly what boot does.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "../include/np_safety_config.h"
#include "../include/np_safety_hal.h"
#include "../include/np_nv_state.h"
#include "../include/np_safety_protocol.h"

/* ── NOR flash double ───────────────────────────────────────────────────────── */
static uint32_t g_flash[NP_NV_PAGE_COUNT][NP_NV_SLOTS_PER_PAGE][2];
static int      g_fail_program_next;   /* next program: return false, no write */
static int      g_tear_program_next;   /* next program: write garbage, false   */
static int      g_fail_erase_next;     /* next erase: return false, no change  */
static int      g_tear_program_at;     /* tear the Nth program from now (1-based), 0 = off */
static int      g_stop_program_at;     /* power lost before the Nth program: it and all later fail */
static int      g_erase_count[NP_NV_PAGE_COUNT];

static void flash_blank(void)
{
    memset(g_flash, 0xFF, sizeof(g_flash));
    g_fail_program_next = 0;
    g_tear_program_next = 0;
    g_fail_erase_next   = 0;
    g_tear_program_at   = 0;
    g_stop_program_at   = 0;
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
    if (g_stop_program_at > 0) {
        if (g_stop_program_at == 1) { return false; }   /* dead from here on */
        g_stop_program_at--;
    }
    if (g_tear_program_at > 0) {
        g_tear_program_at--;
        if (g_tear_program_at == 0) { g_tear_program_next = 1; }
    }
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

#define ALICE  0x1111AAAAUL
#define BOB    0x2222BBBBUL
#define UNSPEC NP_SAFETY_USER_UNSPECIFIED

static void boot(void) { (void)np_nv_state_init(); }

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

/* Fill the live page with alternating user changes until it is full. */
static void fill_page(void)
{
    while ((used_slots(0U) < NP_NV_SLOTS_PER_PAGE) && (used_slots(1U) == 0U)) {
        (void)np_nv_set_current_user((used_slots(0U) % 2U) ? ALICE : BOB);
    }
}

/* ── Single-user behaviour (no user ever named) ─────────────────────────────── */

static void test_factory_fresh_is_clear(void)
{
    flash_blank(); boot();
    check(np_nv_current_user() == UNSPEC, "fresh: no user named");
    check(!np_nv_cardiac_outstanding(), "fresh: nothing outstanding");
    check(!np_nv_cardiac_blocked(UNSPEC), "fresh: not blocked");
}

static void test_single_user_set_survives_and_clears(void)
{
    flash_blank(); boot();
    check(np_nv_cardiac_set(UNSPEC, true), "single: SET succeeds");
    boot();
    check(np_nv_cardiac_blocked(UNSPEC), "single: blocked after power cycle");
    check(np_nv_cardiac_set(UNSPEC, false), "single: CLR succeeds");
    boot();
    check(!np_nv_cardiac_blocked(UNSPEC) && !np_nv_cardiac_outstanding(), "single: clear after power cycle");
}

/* ── Per-user scope ─────────────────────────────────────────────────────────── */

static void test_cutoff_held_for_its_user_only(void)
{
    flash_blank(); boot();
    (void)np_nv_set_current_user(ALICE);
    check(np_nv_cardiac_set(ALICE, true), "per-user: Alice's cutoff recorded");
    boot();
    check(np_nv_current_user() == ALICE, "per-user: active user persists across power cycle");
    check(np_nv_cardiac_blocked(ALICE), "per-user: Alice blocked");
    check(!np_nv_cardiac_blocked(BOB), "per-user: Bob NOT blocked");
    check(np_nv_cardiac_outstanding(), "per-user: outstanding flag set (blanket warning)");

    (void)np_nv_set_current_user(BOB);
    boot();
    check(np_nv_current_user() == BOB, "per-user: switch to Bob persists");
    check(np_nv_cardiac_blocked(ALICE) && !np_nv_cardiac_blocked(BOB),
          "per-user: switching users does not clear Alice's cutoff");
    check(np_nv_cardiac_blocked(UNSPEC), "per-user: an unnamed user cannot be ruled out");

    check(np_nv_cardiac_set(BOB, false), "per-user: Bob acknowledging writes");
    check(np_nv_cardiac_blocked(ALICE), "per-user: Bob cannot acknowledge Alice's cutoff");
    check(np_nv_cardiac_set(ALICE, false), "per-user: Alice acknowledges");
    boot();
    check(!np_nv_cardiac_outstanding(), "per-user: nothing outstanding after Alice acknowledges");
}

/* A cutoff recorded before any user was named cannot be attributed: it is
 * withheld from everyone, and whoever runs the confirmation clears it. */
static void test_unattributed_cutoff_blocks_everyone(void)
{
    flash_blank(); boot();
    (void)np_nv_cardiac_set(UNSPEC, true);
    (void)np_nv_set_current_user(ALICE);
    boot();
    check(np_nv_cardiac_blocked(ALICE) && np_nv_cardiac_blocked(BOB),
          "unattributed: withheld from every user");
    (void)np_nv_cardiac_set(ALICE, false);
    boot();
    check(!np_nv_cardiac_blocked(BOB) && !np_nv_cardiac_outstanding(),
          "unattributed: cleared by the person who acknowledged it");
}

/* More users than the table holds: the extra cutoff is not dropped. */
static void test_full_table_fails_closed(void)
{
    flash_blank(); boot();
    for (uint32_t u = 1U; u <= NP_NV_MAX_PENDING; u++) { (void)np_nv_cardiac_set(0x100U + u, true); }
    check(!np_nv_cardiac_blocked(BOB), "full: a user without a cutoff is not blocked");
    (void)np_nv_cardiac_set(BOB, true);
    boot();
    check(np_nv_cardiac_blocked(BOB) && np_nv_cardiac_blocked(ALICE),
          "full: overflow recorded as ANY — withheld from everyone, never dropped");
}

/* ── Power loss ─────────────────────────────────────────────────────────────── */

static void test_torn_record_fails_closed(void)
{
    flash_blank(); boot();
    (void)np_nv_set_current_user(ALICE);
    g_tear_program_next = 1;
    check(!np_nv_cardiac_set(ALICE, true), "torn: write reports failure");
    boot();
    check(np_nv_cardiac_blocked(BOB), "torn: an undecodable record is withheld from everyone");
    (void)np_nv_set_current_user(BOB);   /* a later record does not hide it */
    boot();
    check(np_nv_cardiac_blocked(BOB), "torn: still withheld after later records");
    check(np_nv_cardiac_set(BOB, false), "torn: acknowledgement writes");
    boot();
    check(!np_nv_cardiac_outstanding(), "torn: cleared by acknowledgement");
}

static void test_compaction_preserves_state(void)
{
    flash_blank(); boot();
    (void)np_nv_set_current_user(ALICE);
    (void)np_nv_cardiac_set(ALICE, true);
    fill_page();
    check(used_slots(0U) == NP_NV_SLOTS_PER_PAGE, "compact: page 0 full");
    uint32_t cur = np_nv_current_user();
    check(np_nv_cardiac_set(BOB, true), "compact: write triggers compaction");
    check(used_slots(0U) == 0U && used_slots(1U) > 0U, "compact: snapshot on page 1, page 0 erased");
    boot();
    check(np_nv_current_user() == cur, "compact: active user survives");
    check(np_nv_cardiac_blocked(ALICE) && np_nv_cardiac_blocked(BOB), "compact: both cutoffs survive");
    (void)np_nv_cardiac_set(ALICE, false);
    boot();
    check(!np_nv_cardiac_blocked(ALICE) && np_nv_cardiac_blocked(BOB), "compact: log continues after compaction");
}

/* Power lost before the full page was erased: both pages hold records. */
static void test_compaction_interrupted_before_erase(void)
{
    flash_blank(); boot();
    (void)np_nv_set_current_user(ALICE);
    (void)np_nv_cardiac_set(ALICE, true);
    fill_page();
    g_fail_erase_next = 1;
    (void)np_nv_cardiac_set(BOB, true);
    check(used_slots(0U) > 0U && used_slots(1U) > 0U, "interrupted erase: both pages hold records");
    boot();
    check(np_nv_cardiac_blocked(ALICE) && np_nv_cardiac_blocked(BOB), "interrupted erase: committed snapshot wins");
    check(np_nv_cardiac_set(ALICE, false), "interrupted erase: next write recovers");
    check(used_slots(0U) == 0U || used_slots(1U) == 0U, "interrupted erase: one page again");
    boot();
    check(!np_nv_cardiac_blocked(ALICE) && np_nv_cardiac_blocked(BOB), "interrupted erase: state exact");
}

/* Power lost part-way through writing the snapshot: it has no COMMIT and must
 * not replace the state — and whatever it was carrying may be missing. */
static void test_compaction_torn_snapshot(void)
{
    flash_blank(); boot();
    (void)np_nv_set_current_user(ALICE);
    (void)np_nv_cardiac_set(ALICE, true);
    fill_page();
    g_tear_program_next = 1;             /* the RESET itself tears */
    check(!np_nv_cardiac_set(BOB, true), "torn snapshot: write reports failure");
    check(used_slots(0U) == NP_NV_SLOTS_PER_PAGE, "torn snapshot: full page NOT erased");
    boot();
    check(np_nv_cardiac_blocked(ALICE), "torn snapshot: Alice still blocked");
    check(np_nv_cardiac_blocked(BOB), "torn snapshot: Bob's lost write fails closed");
    check(np_nv_cardiac_set(BOB, true), "torn snapshot: retry succeeds");
    check(np_nv_cardiac_set(BOB, false), "torn snapshot: acknowledgement succeeds");
    boot();
    check(np_nv_cardiac_blocked(ALICE) && !np_nv_cardiac_blocked(BOB), "torn snapshot: recovers exactly");
}

static void test_garbage_only_fails_closed(void)
{
    flash_blank();
    g_flash[0][0][0] = 0x12345678UL;
    g_flash[0][0][1] = 0x9ABCDEF0UL;
    boot();
    check(np_nv_cardiac_blocked(ALICE), "garbage: undecodable storage is withheld from everyone");
    check(np_nv_cardiac_set(ALICE, false), "garbage: acknowledgement writes");
    boot();
    check(!np_nv_cardiac_outstanding(), "garbage: recovers");
}


/* The snapshot's RESET and first records land but its COMMIT does not.  It
 * must NOT replace the state (that would drop whatever it had not yet written),
 * and what it was carrying may be missing — so fail closed. */
static void test_uncommitted_snapshot_never_replaces_state(void)
{
    /* Snapshot = RESET, USER, SET(Alice), SET(Bob), COMMIT: tear the COMMIT. */
    flash_blank(); boot();
    (void)np_nv_set_current_user(ALICE);
    (void)np_nv_cardiac_set(ALICE, true);
    fill_page();
    g_tear_program_at = 5;
    check(!np_nv_cardiac_set(BOB, true), "no commit: write reports failure");
    boot();
    check(np_nv_cardiac_blocked(ALICE) && np_nv_cardiac_blocked(BOB),
          "no commit: nobody unblocked by an unfinished snapshot");

    /* Tear the SET(Alice) inside the snapshot: the group holds only RESET +
     * USER.  Applied as a reset it would silently clear Alice. */
    flash_blank(); boot();
    (void)np_nv_set_current_user(ALICE);
    (void)np_nv_cardiac_set(ALICE, true);
    fill_page();
    g_tear_program_at = 3;
    (void)np_nv_cardiac_set(BOB, true);
    boot();
    check(np_nv_cardiac_blocked(ALICE), "partial snapshot: Alice's cutoff not dropped");
    check(np_nv_cardiac_set(ALICE, false) && np_nv_cardiac_set(BOB, false), "partial snapshot: recovers");
    boot();
    check(!np_nv_cardiac_outstanding(), "partial snapshot: clean after acknowledgements");
}


/* Power lost cleanly part-way through a snapshot — no torn record at all.  The
 * snapshot copied Alice but not yet Carol; were it trusted, Carol's cutoff
 * would vanish. */
#define CAROL 0x3333CCCCUL
static void test_cut_off_snapshot_keeps_uncopied_users(void)
{
    flash_blank(); boot();
    (void)np_nv_set_current_user(ALICE);
    (void)np_nv_cardiac_set(ALICE, true);
    (void)np_nv_cardiac_set(CAROL, true);
    fill_page();
    /* Snapshot order: RESET, USER, SET(Alice), SET(Carol), SET(Bob), COMMIT.
     * Power is lost before the 4th program — SET(Carol). */
    g_stop_program_at = 4;
    (void)np_nv_cardiac_set(BOB, true);
    g_stop_program_at = 0;
    boot();
    check(np_nv_cardiac_blocked(CAROL), "cut-off snapshot: Carol's cutoff survives");
    check(np_nv_cardiac_blocked(ALICE), "cut-off snapshot: Alice's cutoff survives");
    check(np_nv_cardiac_blocked(BOB), "cut-off snapshot: Bob's unwritten cutoff fails closed");
}

int main(void)
{
    test_factory_fresh_is_clear();
    test_single_user_set_survives_and_clears();
    test_cutoff_held_for_its_user_only();
    test_unattributed_cutoff_blocks_everyone();
    test_full_table_fails_closed();
    test_torn_record_fails_closed();
    test_compaction_preserves_state();
    test_compaction_interrupted_before_erase();
    test_compaction_torn_snapshot();
    test_uncommitted_snapshot_never_replaces_state();
    test_cut_off_snapshot_keeps_uncopied_users();
    test_garbage_only_fails_closed();

    if (g_failures == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
