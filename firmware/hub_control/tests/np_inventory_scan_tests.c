/*
 * NeurOne Hub Control — Module Inventory Scan Service Host Tests (OI-HEXMAP-03, OI-HWFIT-01)
 *
 * np_inventory_scan.c decides WHEN the module map's pass runs. The pass, the map and the two platform seams are doubles, so what is tested
 * is the decision: boot owes a pass, an open rim latch holds it off and owes one, a power source connecting owes one, a session defers
 * it, and a failed pass is not retried by itself. The real session lease is linked, because what a session does to a pass is the point.
 *
 * No FreeRTOS, no hardware. IEC 62304 Class B — SW-02 hub control.
 */

#include <stdbool.h>
#include <stdio.h>

#include "np_hub_types.h"
#include "np_inventory_scan.h"
#include "np_session_lease.h"

static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); } else { printf("FAIL: %s\n", name); g_failures++; }
}

/* ── Doubles ──────────────────────────────────────────────────────────────────── */

static bool g_latch_open, g_power;
bool np_hal_rim_latch_open(void)       { return g_latch_open; }
bool np_hal_power_source_present(void) { return g_power; }

static int  g_restores, g_stale, g_persists, g_passes, g_defers;
static bool g_pending;
static np_hub_status_t g_pass_rc;
static bool g_pass_changed;

np_hub_status_t np_module_map_restore(void)     { g_restores++; return NP_HUB_OK; }
void            np_module_map_mark_stale(void)  { g_stale++; }
np_hub_status_t np_module_map_persist(void)     { g_persists++; return NP_HUB_OK; }
bool            np_hexmap_scan_pending(void)    { return g_pending; }
void            np_hexmap_scan_defer(void)      { g_defers++; g_pending = true; }
np_hub_status_t np_hexmap_scan_all(bool *changed_out)
{
    g_passes++;
    g_pending = false;
    if (changed_out != NULL) { *changed_out = g_pass_changed; }
    return g_pass_rc;
}

static void fresh(void)
{
    np_lease_init(NULL, NULL);
    g_latch_open = false; g_power = true;
    g_restores = g_stale = g_persists = g_passes = g_defers = 0;
    g_pending = false; g_pass_rc = NP_HUB_OK; g_pass_changed = true;
}

/* ── Tests ────────────────────────────────────────────────────────────────────── */

/* @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21) */
static void test_boot_restores_marks_stale_and_owes_one_pass(void)
{
    fresh();
    np_inventory_boot();
    check(g_restores == 1, "boot: the cached map is restored (fail-closed: a pass confirms it)");
    check(g_stale >= 1, "boot: the inventory is stale at once, so an early upload waits and is not refused");
    check(g_passes == 0, "boot: no pass has run yet; the detect task runs it");
    check(np_inventory_service() && g_passes == 1, "service: the first call does the owed pass");
    check(g_persists == 1, "service: and persists the map once, because a socket changed");
    check(!np_inventory_service() && g_passes == 1, "service: nothing is owed after that, so nothing runs");
}

/* @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21) */
static void test_an_unchanged_inventory_is_not_persisted_again(void)
{
    fresh();
    np_inventory_boot();
    (void)np_inventory_service();
    g_pass_changed = false;
    np_inventory_request();
    check(np_inventory_service() && g_passes == 2, "persist: a second pass runs");
    check(g_persists == 1, "persist: and writes nothing, because no socket changed");
}

/* @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21) */
static void test_an_open_rim_latch_holds_the_pass_off_and_owes_one(void)
{
    fresh();
    np_inventory_boot();
    (void)np_inventory_service();
    int stale_before = g_stale;
    g_latch_open = true;
    check(!np_inventory_service() && g_passes == 1, "latch: while a rim latch is open no pass runs, the modules are being changed");
    check(g_stale > stale_before, "latch: the inventory is stale from the moment it opens");
    check(!np_inventory_service() && g_passes == 1, "latch: and stays so for as long as it is open");
    g_latch_open = false;
    check(np_inventory_service() && g_passes == 2, "latch: once all are closed the owed pass runs");
}

/* @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21) */
static void test_a_power_source_connecting_owes_a_pass_and_a_steady_one_does_not(void)
{
    fresh();
    np_inventory_boot();
    (void)np_inventory_service();
    check(!np_inventory_service() && g_passes == 1, "power: a source that stays connected owes nothing");
    g_power = false;
    check(!np_inventory_service() && g_passes == 1, "power: losing the source starts nothing by itself");
    g_power = true;
    check(np_inventory_service() && g_passes == 2, "power: connecting a source runs a pass");
}

/* @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21) */
static void test_a_session_defers_the_pass_until_it_ends(void)
{
    fresh();
    np_inventory_boot();
    (void)np_inventory_service();
    check(np_lease_claim() == NP_HUB_OK, "session: a protocol is loaded and holds the lease");
    np_inventory_request();                    // the power source was reconnected under the session (the supercap held)
    check(!np_inventory_service() && g_passes == 1 && g_defers == 1 && g_pending,
          "session: no pass runs under a session; the inventory is only marked stale and the pass is remembered");
    check(!np_inventory_service() && g_passes == 1, "session: and it is not retried while the session runs");
    np_lease_release();                        // the session ends
    check(np_inventory_service() && g_passes == 2 && !g_pending, "session end: the remembered pass runs");
}

/* @verifies-none OI-HWFIT-01 the requirement row waits on the open item's decisions (CLAUDE.md §18, §21) */
static void test_a_failed_pass_is_not_retried_until_the_next_trigger(void)
{
    fresh();
    np_inventory_boot();
    g_pass_rc = NP_HUB_ERR_BAD_MAGIC;
    check(np_inventory_service() && g_passes == 1, "failed: the pass ran");
    check(g_persists == 0, "failed: and a map from a failed pass is not persisted");
    check(!np_inventory_service() && g_passes == 1, "failed: it is not run again on its own, so uploads see the failure and are refused");
    g_pass_rc = NP_HUB_OK;
    np_inventory_request();
    check(np_inventory_service() && g_passes == 2, "failed: the next trigger starts the next pass");
}

int main(void)
{
    test_boot_restores_marks_stale_and_owes_one_pass();
    test_an_unchanged_inventory_is_not_persisted_again();
    test_an_open_rim_latch_holds_the_pass_off_and_owes_one();
    test_a_power_source_connecting_owes_a_pass_and_a_steady_one_does_not();
    test_a_session_defers_the_pass_until_it_ends();
    test_a_failed_pass_is_not_retried_until_the_next_trigger();
    printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
