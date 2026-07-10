/*
 * NeurOne Safety MCU — SW01-M03 Charge Monitor Host Tests
 * Document: NP-SW-001 Rev A, NP-FMEA-001 Rev B §SW01-M03
 *
 * Tests np_charge_monitor_init(), np_charge_monitor_accumulate(),
 * np_charge_monitor_tick(), np_charge_monitor_reset_session(), and
 * np_charge_monitor_set_channel_limit().
 *
 * OI-CHARGE-01 CLOSED — accumulate() is now wired in np_safety_main.c.
 * OI-CHARGE-02 OPEN   — set_channel_limit() API hook provided but the correct
 *                        per-geometry value is not yet wired from session runner.
 *
 * These tests exercise the monitor logic using a stubbed np_safety_state_t.
 * They do NOT require ARM cross-compilation or SPI hardware.
 *
 * IEC 62304 Class C — SW-01 safety MCU.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "../include/np_safety_config.h"
#include "../include/np_safety_protocol.h"

/* ── Minimal stubs for functions that np_charge_monitor.c calls ────────────── */
/* np_charge_monitor.c only includes np_safety_config.h and np_safety_protocol.h;
 * no HAL or external module calls are needed for these tests.                */

/* ── Test infrastructure ────────────────────────────────────────────────────── */

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

/* Forward-declare all public functions from np_charge_monitor.c */
extern np_safe_status_t np_charge_monitor_init(void);
extern void np_charge_monitor_accumulate(uint8_t channel, uint32_t current_ua, uint32_t dt_us);
extern void np_charge_monitor_tick(np_safety_state_t *state);
extern void np_charge_monitor_reset_session(np_safety_state_t *state);
extern void np_charge_monitor_set_channel_limit(uint8_t channel, uint32_t limit_uc);
extern void np_charge_monitor_set_channel_area_mcm2(uint8_t channel, uint16_t area_mcm2);
extern void np_charge_monitor_geom_gate(np_safety_state_t *state);

/* CLIN_STIM charge-monitor channel index = bit position of NP_SAFETY_EN_CLIN_STIM.
 * HD-tDCS accumulates on this channel (OI-CHARGE-02).                         */
#define NP_SAFETY_CH_CLIN_STIM_IDX  13U

/* ── Helper: build a clean state ────────────────────────────────────────────── */
static np_safety_state_t fresh_state(void)
{
    np_safety_state_t s;
    memset(&s, 0, sizeof(s));
    s.fault_slot    = 0xFFU;
    s.granted_mask  = 0x3FFFU;  /* all 14 channels granted */
    s.session_active = true;
    return s;
}

/* ── Test: init returns OK and no tick fires on zero accumulation ────────────── */

static void test_init_clean(void)
{
    np_safe_status_t rc = np_charge_monitor_init();
    check(rc == NP_SAFE_OK, "init returns NP_SAFE_OK");

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);

    check(s.granted_mask == 0x3FFFU,
          "tick with zero charge: granted_mask unchanged");
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "tick with zero charge: CHARGE bit not set");
    check(s.fault_slot == 0xFFU,
          "tick with zero charge: fault_slot == 0xFF");
}

/* ── Test: accumulate + tick enforces limit on target channel ────────────────── */

static void test_limit_enforced(void)
{
    np_charge_monitor_init();

    /* NP_CHARGE_LIMIT_UC = 1000 µC (default, 40 µC/cm² × 25 cm²).
     * Accumulate via 200ms ticks at 5000 µA on channel 0:
     *   charge/tick = 5000 µA × 200000 µs / 1000 = 1,000,000 nC = 1000 µC
     * One tick should be enough to hit the limit.                           */
    uint32_t current_ua  = 5000U;
    uint32_t dt_us       = (uint32_t)NP_SAFETY_HEARTBEAT_EXP_MS * 1000UL;  /* 200000 µs */

    np_charge_monitor_accumulate(0U, current_ua, dt_us);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);

    check((s.status & NP_SAFETY_STATUS_CHARGE) != 0U,
          "CHARGE status set after limit reached on ch 0");
    check((s.status & NP_SAFETY_STATUS_CUTOFF) != 0U,
          "CUTOFF status set after charge limit on ch 0");
    check((s.granted_mask & (1U << 0)) == 0U,
          "ch 0 bit cleared from granted_mask after charge limit");
    check(s.fault_slot == 0U,
          "fault_slot == 0 (channel that tripped)");

    /* Other channels must be unaffected */
    check((s.granted_mask & (1U << 1)) != 0U,
          "ch 1 still granted (was not charged)");
}

/* ── Test: only the charged channel is cut; others remain ───────────────────── */

static void test_only_tripped_channel_cut(void)
{
    np_charge_monitor_init();

    /* Accumulate on channel 5 only */
    uint32_t dt_us = (uint32_t)NP_SAFETY_HEARTBEAT_EXP_MS * 1000UL;
    np_charge_monitor_accumulate(5U, 5000U, dt_us);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);

    check((s.granted_mask & (1U << 5)) == 0U, "ch 5 cut after limit");
    check((s.granted_mask & (1U << 4)) != 0U, "ch 4 unaffected");
    check((s.granted_mask & (1U << 6)) != 0U, "ch 6 unaffected");
    check(s.fault_slot == 5U,                 "fault_slot == 5");
}

/* ── Test: reset_session clears accumulators ─────────────────────────────────── */

static void test_reset_clears_accumulators(void)
{
    np_charge_monitor_init();

    /* Push channel 3 to limit */
    uint32_t dt_us = (uint32_t)NP_SAFETY_HEARTBEAT_EXP_MS * 1000UL;
    np_charge_monitor_accumulate(3U, 5000U, dt_us);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) != 0U, "ch 3 tripped before reset");

    /* Reset on the SAME persistent state (as the real main loop does — s_state is
     * a static that survives session boundaries).  reset_session must clear the
     * latched CHARGE bit in place, otherwise np_spi_watchdog_tick would keep
     * forcing granted_mask=0 for the entire power cycle. */
    np_charge_monitor_reset_session(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "CHARGE cleared in persistent state after reset_session");

    /* Accumulators and limit_reached flags must also be cleared: a fresh tick on
     * the re-armed monitor grants all channels again. */
    s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U, "CHARGE stays clear after reset_session");
    check(s.granted_mask == 0x3FFFU,                  "granted_mask full after reset_session");
}

/* ── Test: set_channel_limit overrides default ───────────────────────────────── */

static void test_set_channel_limit(void)
{
    np_charge_monitor_init();

    /* Override channel 1 limit to 4 µC (T2 HD-tDCS 3.5mm electrode geometry,
     * OI-CHARGE-02 placeholder value — actual value TBD by session runner).  */
    np_charge_monitor_set_channel_limit(1U, 4U);

    /* Accumulate 5 µC on channel 1 (over the 4 µC limit):
     *   current = 25 µA, dt = 200s (200000000 µs): 25 × 200000000 / 1000 = 5000000 nC = 5000 µC
     * That's too large.  Use: current = 1 µA, dt = 5000000 µs → 5 µC */
    np_charge_monitor_accumulate(1U, 1U, 5000000UL);   /* 5 µC > 4 µC limit */

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);

    check((s.status & NP_SAFETY_STATUS_CHARGE) != 0U,
          "CHARGE set after custom 4 µC limit exceeded on ch 1");
    check((s.granted_mask & (1U << 1)) == 0U,
          "ch 1 cut after custom limit");

    /* Channel with default limit: 1 µC accumulated should NOT trigger */
    np_charge_monitor_init();
    np_charge_monitor_accumulate(2U, 1U, 1000000UL);   /* 1 µC < 1000 µC default */
    s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "CHARGE not set: 1 µC < 1000 µC default limit on ch 2");
}

/* ── Test: out-of-range channel is silently ignored ─────────────────────────── */

static void test_out_of_range_channel_ignored(void)
{
    np_charge_monitor_init();
    np_charge_monitor_accumulate(14U, 99999U, 999999UL);   /* channel 14 = out of range */
    np_charge_monitor_accumulate(255U, 99999U, 999999UL);  /* definitely out of range */
    np_charge_monitor_set_channel_limit(14U, 1U);          /* also out of range */

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);

    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "out-of-range channel does not trigger charge fault");
    check(s.granted_mask == 0x3FFFU,
          "granted_mask unaffected by out-of-range accumulate");
}

/* ── Test: tick is idempotent after limit reached ────────────────────────────── */

static void test_tick_idempotent_after_trip(void)
{
    np_charge_monitor_init();

    uint32_t dt_us = (uint32_t)NP_SAFETY_HEARTBEAT_EXP_MS * 1000UL;
    np_charge_monitor_accumulate(0U, 5000U, dt_us);   /* trips limit */

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);  /* first tick: trips */

    uint16_t mask_after_first = s.granted_mask;
    uint8_t  status_after_first = s.status;

    /* Continue accumulating — limit_reached flag prevents double-trip */
    np_charge_monitor_accumulate(0U, 5000U, dt_us);
    np_charge_monitor_tick(&s);  /* second tick: already tripped */

    check(s.granted_mask == mask_after_first,
          "granted_mask stable after second tick (idempotent)");
    check(s.status == status_after_first,
          "status stable after second tick (idempotent)");
}

/* ── Test: charge calculation precision ──────────────────────────────────────── */

static void test_charge_accumulation_math(void)
{
    np_charge_monitor_init();

    /* 200 µA for 200ms = 200 µA × 200000 µs / 1000 = 40000 nC = 40 µC.
     * 40 µC is 4% of the 1000 µC default limit — should NOT trip.           */
    np_charge_monitor_accumulate(6U, 200U, 200000UL);

    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);

    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "40 µC << 1000 µC limit: CHARGE not set");

    /* 25 ticks of 200 µA × 200ms = 25 × 40 µC = 1000 µC → at limit
     * Total after 24 more: 25 × 40 µC = 1000 µC.  The >= comparison trips. */
    uint8_t tick;
    for (tick = 0U; tick < 24U; tick++) {
        np_charge_monitor_accumulate(6U, 200U, 200000UL);
    }
    np_charge_monitor_tick(&s);

    check((s.status & NP_SAFETY_STATUS_CHARGE) != 0U,
          "25 × 40 µC = 1000 µC == limit: CHARGE set");
    check((s.granted_mask & (1U << 6)) == 0U,
          "ch 6 cut at 1000 µC limit");
}

/* ── Test: set_channel_area_mcm2 — HD-tDCS geometry (OI-CHARGE-02) ──────────── */

static void test_set_channel_area_hd_tdcs(void)
{
    np_charge_monitor_init();

    /* HD-tDCS 3.5mm electrode: area 96 milli-cm² → limit 40 × 96 = 3840 nC
     * = 3.84 µC on CLIN_STIM channel (index 13).                            */
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);

    /* Accumulate 3.80 µC (3800 nC) — just UNDER the 3.84 µC limit: no trip.
     * current = 19 µA, dt = 200000 µs → 19 × 200000 / 1000 = 3800 nC.        */
    np_charge_monitor_accumulate(NP_SAFETY_CH_CLIN_STIM_IDX, 19U, 200000UL);
    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "HD-tDCS: 3.80 µC < 3.84 µC limit → no trip");
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "HD-tDCS: CLIN_STIM still granted below limit");

    /* One more 60 nC push crosses 3.84 µC (3840 nC): 3800 + 60 = 3860 ≥ 3840. */
    np_charge_monitor_accumulate(NP_SAFETY_CH_CLIN_STIM_IDX, 60U, 1000UL);
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) != 0U,
          "HD-tDCS: crossing 3.84 µC → CHARGE set");
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) == 0U,
          "HD-tDCS: CLIN_STIM cut at 3.84 µC geometry limit");
}

/* ── Test: set_channel_area_mcm2 reproduces the 1000 µC default from 25cm² ──── */

static void test_set_channel_area_default_pad(void)
{
    np_charge_monitor_init();

    /* Standard 25 cm² pad: area 25000 milli-cm² → 40 × 25000 = 1,000,000 nC
     * = 1000 µC (identical to the reset default).                           */
    np_charge_monitor_set_channel_area_mcm2(3U, 25000U);

    /* 999 µC accumulated should NOT trip (below 1000 µC).                    */
    np_charge_monitor_accumulate(3U, 999U, 1000000UL);   /* 999 µC */
    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "25cm² area path: 999 µC < 1000 µC → no trip");

    /* Area 0 = keep current limit (no override): channel 4 stays at default. */
    np_charge_monitor_set_channel_area_mcm2(4U, 0U);
    np_charge_monitor_accumulate(4U, 1U, 1000000UL);     /* 1 µC << 1000 µC */
    s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "area 0 leaves default limit intact (1 µC << 1000 µC)");
}

/* ── Test: area override on one channel does not weaken others (ISC-12) ─────── */

static void test_area_override_isolated(void)
{
    np_charge_monitor_init();

    /* Tighten CLIN_STIM (ch 13) to 3.84 µC via area. */
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);

    /* Channel 0 (untouched) must retain its 1000 µC default: 500 µC → no trip. */
    np_charge_monitor_accumulate(0U, 500U, 1000000UL);   /* 500 µC */
    np_safety_state_t s = fresh_state();
    np_charge_monitor_tick(&s);
    check((s.granted_mask & (1U << 0)) != 0U,
          "untouched ch 0 keeps 1000 µC default after ch 13 area override");
    check((s.status & NP_SAFETY_STATUS_CHARGE) == 0U,
          "500 µC on default-limit ch 0 does not trip");
}

/* ── Test: OI-CHARGE-03 geometry gate blocks CLIN_STIM until area applied ───── */

static void test_geom_gate_blocks_until_applied(void)
{
    np_charge_monitor_reset_session(NULL);   /* geom NOT applied yet */

    /* Session requires geometry override, none applied → CLIN_STIM blocked. */
    np_safety_state_t s = fresh_state();
    s.geom_required = true;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) == 0U,
          "geom gate: CLIN_STIM blocked when required and not applied (fail-closed)");
    /* Other channels untouched by the gate. */
    check((s.granted_mask & (1U << 6)) != 0U,
          "geom gate: non-CLIN_STIM channels unaffected");

    /* Apply HD-tDCS electrode area to CLIN_STIM → gate opens. */
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);
    np_safety_state_t s2 = fresh_state();
    s2.geom_required = true;
    np_charge_monitor_geom_gate(&s2);
    check((s2.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "geom gate: CLIN_STIM granted after area applied");
}

/* ── Test: geometry gate does NOT block clinical tACS (no override declared) ── */

static void test_geom_gate_not_required_passes(void)
{
    np_charge_monitor_reset_session(NULL);   /* geom NOT applied */

    /* Clinical tACS: shares CLIN_STIM but the hub does not set geom_required. */
    np_safety_state_t s = fresh_state();
    s.geom_required = false;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) != 0U,
          "geom gate: CLIN_STIM NOT blocked when geom_required is false (tACS)");
}

/* ── Test: geometry gate re-arms each session ───────────────────────────────── */

static void test_geom_gate_rearms_each_session(void)
{
    /* Apply area (gate opens), then a new session must re-arm the gate. */
    np_charge_monitor_reset_session(NULL);
    np_charge_monitor_set_channel_area_mcm2(NP_SAFETY_CH_CLIN_STIM_IDX, 96U);

    np_charge_monitor_reset_session(NULL);   /* new session — applied flag cleared */
    np_safety_state_t s = fresh_state();
    s.geom_required = true;
    np_charge_monitor_geom_gate(&s);
    check((s.granted_mask & (1U << NP_SAFETY_CH_CLIN_STIM_IDX)) == 0U,
          "geom gate: re-armed after reset_session (blocks again until re-applied)");
}

/* ── Main ───────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("=== np_charge_monitor_tests ===\n");

    test_init_clean();
    test_limit_enforced();
    test_only_tripped_channel_cut();
    test_reset_clears_accumulators();
    test_set_channel_limit();
    test_out_of_range_channel_ignored();
    test_tick_idempotent_after_trip();
    test_charge_accumulation_math();
    test_set_channel_area_hd_tdcs();
    test_set_channel_area_default_pad();
    test_area_override_isolated();
    test_geom_gate_blocks_until_applied();
    test_geom_gate_not_required_passes();
    test_geom_gate_rearms_each_session();

    printf("\n%s: %d failure(s)\n",
           (g_failures == 0) ? "PASS" : "FAIL", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
