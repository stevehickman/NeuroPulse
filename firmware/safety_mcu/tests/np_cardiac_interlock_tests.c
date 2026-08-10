/*
 * NeurOne Safety MCU — SW01-M05 Cardiac Interlock Host Tests
 * Document: NP-SW-001 §4.1 (SW01-M05), NP-FW-CVNS-001 Rev B §5,
 *           NP-FMEA-001 Rev A §SW01-M05 (FMEA-M05-02)
 *
 * Host-native tests for the STM32G071 bare-metal cervical VNS cardiac rhythm
 * interlock (np_cardiac_interlock.c) — the IEC 62304 Class C unit that owns the
 * CVNS enable bit and performs the cutoff.  Until this suite existed the only
 * "cardiac interlock" coverage in the tree was np_cvns_fai_tests, which
 * exercises the MAIN PROCESSOR module (firmware/cervical_vns/) on the other
 * side of the SPI boundary and asserts on main-processor constants.  The two
 * sides are deliberately independent implementations that cross-validate at
 * ±5 BPM; a test of one is not a test of the other.
 *
 * PURPOSE: these tests PIN CURRENT BEHAVIOUR.  Several assertions encode
 * constants (NP_CARDIAC_BASELINE_BEATS = 8, NP_RR_BUF_SIZE = 8) that are the
 * subject of open item OI-CVNS-10 (safety-MCU 8 vs main-processor 5 baseline
 * window).  That is the point: whichever way OI-CVNS-10 resolves, the change
 * must surface here as a reviewed failing assertion rather than land silently.
 *
 * These tests mock the four HAL entry points; they do NOT require ARM
 * cross-compilation, TIM2, or an R-peak signal source.
 * Build with -DNP_BUILD_TESTS=ON (see firmware/safety_mcu/CMakeLists.txt).
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "../include/np_safety_config.h"
#include "../include/np_safety_hal.h"
#include "../include/np_safety_protocol.h"

/* ── Unit under test ─────────────────────────────────────────────────────────── */
extern np_safe_status_t np_cardiac_interlock_init(void);
extern void             np_cardiac_interlock_tick(np_safety_state_t *state);
extern void             np_cardiac_interlock_reenable(np_safety_state_t *state);

/* ── Mocked HAL stubs ──────────────────────────────────────────────────────────
 * Definitions of symbols declared in np_safety_hal.h — drift from the
 * production contract is a compile error (OI-SWCI-18).                       */
/* g_capture is the free-running 1 MHz TIM2 count; g_tick_ms is the 1 kHz
 * SysTick.  They are advanced INDEPENDENTLY on purpose: a test can deliver a
 * physiologically realistic R-R interval (1 s of TIM2) while advancing SysTick
 * by only 1 ms, so the 5 s rolling-baseline refresh (NP_CARDIAC_OBS_MS) does
 * not fire unless the test asks for it.  That isolates the delta comparison
 * from the refresh path. */
static uint32_t g_tick_ms;
static uint32_t g_capture;
static bool     g_edge_pending;

uint32_t np_hal_get_tick_ms(void)      { return g_tick_ms; }
uint32_t np_hal_tim2_get_capture(void) { return g_capture; }
bool     np_hal_rpeak_edge_pending(void) { return g_edge_pending; }
void     np_hal_rpeak_edge_clear(void) { g_edge_pending = false; }

/* ── Harness ─────────────────────────────────────────────────────────────────── */
static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

/* CVNS fault slot index asserted by np_cardiac_interlock.c on cutoff. */
#define CVNS_FAULT_SLOT   10U

/* Mirrors NP_RR_BUF_SIZE, which np_cardiac_interlock.c defines privately in the
 * .c file rather than in np_safety_config.h.  Deliberately NOT hoisted into the
 * header: that would edit the Class C unit under test.  A divergence between
 * this mirror and the real ring-buffer depth changes the arm point and is caught
 * by test_baseline_arms_at_eight_intervals_not_five(). */
#define TEST_RR_BUF_SIZE  8U

/* R-R intervals in µs for round BPM values (60,000,000 / BPM). */
#define RR_60_BPM    1000000U   /* 60 000 000 / 1 000 000 =  60 */
#define RR_75_BPM     800000U   /* 60 000 000 /   800 000 =  75 (delta exactly 15) */
#define RR_120_BPM    500000U   /* 60 000 000 /   500 000 = 120 */
#define RR_55_BPM    1090000U   /* 60 000 000 / 1 090 000 =  55 (delta 5, a fall) */
#define RR_30_BPM    2000000U   /* 60 000 000 / 2 000 000 =  30 (delta 30, a fall) */
#define RR_37_BPM    1621621U   /* 60 000 000 / 1 621 621 =  37 */
#define RR_IMPOSSIBLE    915U   /* 60 000 000 /       915 = 65 573 → saturates */

static void reset_all(np_safety_state_t *st, bool cvns_active, uint32_t capture0)
{
    g_tick_ms      = 0U;
    g_capture      = capture0;
    g_edge_pending = false;
    np_cardiac_interlock_init();

    memset(st, 0, sizeof(*st));
    st->fault_slot   = NP_FAULT_SLOT_NONE;
    st->granted_mask = NP_SAFETY_EN_CVNS;
    st->cvns_active  = cvns_active;
}

/* Deliver one R-peak arriving rr_us after the previous one. */
static void beat(np_safety_state_t *st, uint32_t rr_us)
{
    g_capture      += rr_us;
    g_edge_pending  = true;
    g_tick_ms      += 1U;
    np_cardiac_interlock_tick(st);
}

/* Advance SysTick with no R-peak (used for lockout expiry and refresh tests). */
static void idle_tick(np_safety_state_t *st, uint32_t dt_ms)
{
    g_tick_ms += dt_ms;
    np_cardiac_interlock_tick(st);
}

/* Establish a baseline at a steady rate.  One priming beat produces no interval
 * (the s_first_beat_seen guard), so NP_CARDIAC_BASELINE_BEATS intervals need
 * NP_CARDIAC_BASELINE_BEATS + 1 edges. */
static void establish_baseline(np_safety_state_t *st, uint32_t rr_us)
{
    beat(st, rr_us);                                        /* prime — no interval */
    for (uint8_t i = 0U; i < NP_CARDIAC_BASELINE_BEATS; i++) {
        beat(st, rr_us);
    }
}

static bool cutoff_fired(const np_safety_state_t *st)
{
    return (st->granted_mask & NP_SAFETY_EN_CVNS) == 0U;
}

/* ── Tests ───────────────────────────────────────────────────────────────────── */

/*
 * Before a baseline exists the interlock cannot fire, however violent the
 * excursion.  This is the conservative hold: np_cardiac_interlock.c returns
 * early on !s_baseline_valid.  Note the safety MCU's hold is STRICTER than
 * NP-FW-CVNS-001 §5.4.3's "fewer than 3 valid intervals" — it holds until
 * NP_CARDIAC_BASELINE_BEATS (8) intervals have accumulated.  Pinned as-is.
 */
static void test_no_cutoff_before_baseline(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);

    beat(&st, RR_60_BPM);      /* prime */
    beat(&st, RR_60_BPM);      /* interval 1 */
    beat(&st, RR_30_BPM);      /* interval 2 — wild swing */

    check(!cutoff_fired(&st), "hold: no cutoff with 2 valid intervals (< 3)");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U,
          "hold: CARDIAC status not set before baseline");

    /* Keep swinging, still short of the 8-interval arm point. */
    beat(&st, RR_120_BPM);     /* interval 3 */
    beat(&st, RR_30_BPM);      /* interval 4 */
    beat(&st, RR_120_BPM);     /* interval 5 */
    beat(&st, RR_30_BPM);      /* interval 6 */
    beat(&st, RR_120_BPM);     /* interval 7 */
    check(!cutoff_fired(&st), "hold: no cutoff with 7 intervals (baseline unarmed)");
    check(st.fault_slot == NP_FAULT_SLOT_NONE, "hold: no fault slot recorded");
}

/*
 * The priming beat must NOT produce an interval.  s_last_capture is 0 at init,
 * so an unguarded implementation would record a phantom interval equal to the
 * boot-time TIM2 count.  Starting the capture at 5 s makes that phantom huge
 * (6 s → 10 BPM), which would drag the baseline far below the true rate and
 * fire a spurious cutoff as the phantom rolls out of the ring buffer.
 *
 * Guarded   : buffer = 8 × 1 s → baseline 60 BPM, current 60 BPM, no cutoff.
 * Unguarded : buffer = [6 s, 7 × 1 s] → baseline (6e6+7e6)/8 = 1.625 s = 36 BPM,
 *             then current climbs to 60 → delta 24 > 15 → cutoff.
 * So "no cutoff" here is a genuine falsification of the unguarded variant.
 */
static void test_priming_beat_produces_no_interval(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 5000000U);    /* TIM2 already 5 s into its count */

    establish_baseline(&st, RR_60_BPM);
    beat(&st, RR_60_BPM);              /* one more steady beat */

    check(!cutoff_fired(&st),
          "prime guard: no phantom first interval (no spurious cutoff)");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U,
          "prime guard: CARDIAC status clear at steady 60 BPM");
}

/*
 * Pins NP_CARDIAC_BASELINE_BEATS = 8 by arithmetic that a 5-beat implementation
 * cannot pass (OI-CVNS-10 — the main processor uses 5).
 *
 * Sequence: prime, 5 × 1 s, then 1/2 s beats.  Ring buffer means (µs) and the
 * BPM each yields:
 *   after interval 8   : 812 500 →  73     (8-beat impl arms HERE, baseline 73)
 *   after interval 9   : 750 000 →  80
 *   after interval 10  : 687 500 →  87
 *   after interval 11  : 625 000 →  96
 * A 5-beat impl arms at interval 5 with baseline 60, so it fires at interval 9
 * (|80−60| = 20 > 15).  The 8-beat impl has baseline 73 and does not reach
 * delta > 15 until interval 11 (|96−73| = 23).
 */
static void test_baseline_arms_at_eight_intervals_not_five(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);

    beat(&st, RR_60_BPM);                                  /* prime */
    for (uint8_t i = 0U; i < 5U; i++) { beat(&st, RR_60_BPM); }   /* intervals 1-5 */
    for (uint8_t i = 0U; i < 3U; i++) { beat(&st, RR_120_BPM); }  /* intervals 6-8 */

    check(!cutoff_fired(&st), "arm point: no cutoff at interval 8 (baseline just set)");

    beat(&st, RR_120_BPM);   /* interval 9  — a 5-beat baseline would fire here */
    check(!cutoff_fired(&st),
          "arm point: no cutoff at interval 9 (falsifies a 5-beat baseline)");

    beat(&st, RR_120_BPM);   /* interval 10 */
    check(!cutoff_fired(&st), "arm point: no cutoff at interval 10");

    beat(&st, RR_120_BPM);   /* interval 11 — 96 vs baseline 73 → delta 23 */
    check(cutoff_fired(&st), "arm point: cutoff at interval 11 (8-beat baseline)");
}

/* A rise of more than NP_CARDIAC_HR_DELTA_BPM clears the CVNS enable and
 * records the fault. */
static void test_rise_above_threshold_cuts_off(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);
    check(!cutoff_fired(&st), "rise: armed at 60 BPM with no cutoff");

    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, RR_120_BPM); }

    check(cutoff_fired(&st), "rise: CVNS dropped from granted_mask");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) != 0U, "rise: CARDIAC status set");
    check((st.status & NP_SAFETY_STATUS_CUTOFF) != 0U,  "rise: CUTOFF status set");
    check(st.fault_slot == CVNS_FAULT_SLOT, "rise: fault_slot = 10 (CVNS)");
}

/*
 * FMEA-M05-02: the delta comparison must use signed arithmetic so that a FALL
 * below baseline does not underflow.  Two assertions together pin the semantics:
 *
 *   (a) a fall of 30 BPM (60 → 30) MUST fire;
 *   (b) a fall of 5 BPM (60 → 55) MUST NOT fire.
 *
 * (b) is the falsifying one.  Under an unsigned `cur - baseline`, 55 − 60 wraps
 * to 65 531, which is > 15, so an unsigned implementation fires a spurious
 * cutoff on ANY fall of ≥ 1 BPM and fails (b).  Assertion (a) alone would NOT
 * distinguish the two — an unsigned implementation passes it for the wrong
 * reason.
 */
static void test_fall_below_threshold_cuts_off(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);

    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, RR_30_BPM); }

    check(cutoff_fired(&st), "fall: 60 → 30 BPM fires the cutoff (signed delta)");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) != 0U, "fall: CARDIAC status set");
}

static void test_small_fall_does_not_cut_off(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);

    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, RR_55_BPM); }

    check(!cutoff_fired(&st),
          "underflow guard: 60 → 55 BPM does NOT fire (falsifies unsigned delta)");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U,
          "underflow guard: CARDIAC status clear on a 5 BPM fall");
}

/* The comparison is strict `>`: a delta of exactly NP_CARDIAC_HR_DELTA_BPM
 * holds.  Pins the boundary so a `>=` edit surfaces here. */
static void test_delta_exactly_at_threshold_holds(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);

    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, RR_75_BPM); }

    check(!cutoff_fired(&st), "boundary: delta of exactly 15 BPM does not fire");
}

/* No cutoff while the hub has not declared CVNS active for the session. */
static void test_no_cutoff_when_cvns_inactive(void)
{
    np_safety_state_t st;
    reset_all(&st, false, 0U);          /* cvns_active = false */
    establish_baseline(&st, RR_60_BPM);

    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, RR_120_BPM); }

    check(!cutoff_fired(&st), "gate: no cutoff while cvns_active is false");
    check(st.fault_slot == NP_FAULT_SLOT_NONE, "gate: no fault slot while inactive");
}

/*
 * rr_to_bpm() saturates at INT16_MAX rather than truncating.  The clamp is not
 * reachable during normal operation (getting there means passing through the
 * cutoff first), so the buffer is filled while cvns_active is false — which
 * suppresses the cutoff path but still updates the ring buffer — and CVNS is
 * then activated for a single tick.
 *
 * Baseline is 37 BPM.  The buffer then holds 8 × 915 µs → 60e6/915 = 65 573.
 *   saturating : current = INT16_MAX = 32 767 → delta 32 730 → cutoff fires.
 *   truncating : (int16_t)65 573 = 37       → delta 0        → no cutoff.
 * 65 573 − 65 536 = 37 is exactly the baseline, so the truncating variant is
 * silent.  Asserting the cutoff fires therefore falsifies truncation.
 */
static void test_impossible_rr_saturates_rather_than_wrapping(void)
{
    np_safety_state_t st;
    reset_all(&st, false, 0U);          /* inactive: buffer fills, cutoff suppressed */

    establish_baseline(&st, RR_37_BPM);                     /* baseline = 37 BPM */
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, RR_IMPOSSIBLE); }

    check(!cutoff_fired(&st), "saturation: no cutoff accrued while inactive");

    st.cvns_active = true;
    idle_tick(&st, 1U);

    check(cutoff_fired(&st),
          "saturation: impossible R-R saturates to INT16_MAX (falsifies truncation)");
}

/* Re-enable is refused until the 30 s lockout has elapsed, and the elapsed
 * check is driven by tick(), not by the re-enable call itself. */
static void test_reenable_refused_during_lockout(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);
    /* Stop at the exact tick the cutoff fires: the lockout window is measured
     * from there, so beating on past it would make cutoff_ms too late and the
     * "1 ms early" probe would land after the lockout had already expired. */
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE && !cutoff_fired(&st); i++) {
        beat(&st, RR_120_BPM);
    }
    check(cutoff_fired(&st), "lockout: cutoff fired to arm the lockout");

    uint32_t cutoff_ms = g_tick_ms;

    /* 1 ms short of the lockout — still refused. */
    g_tick_ms = cutoff_ms + (NP_CARDIAC_LOCKOUT_MS - 1U);
    np_cardiac_interlock_tick(&st);
    np_cardiac_interlock_reenable(&st);
    check((st.status & NP_SAFETY_STATUS_CARDIAC) != 0U,
          "lockout: re-enable refused 1 ms before 30 s");

    /* Exactly at the lockout — accepted. */
    g_tick_ms = cutoff_ms + NP_CARDIAC_LOCKOUT_MS;
    np_cardiac_interlock_tick(&st);
    np_cardiac_interlock_reenable(&st);
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U,
          "lockout: re-enable accepted at exactly 30 s");
    check((st.status & NP_SAFETY_STATUS_CUTOFF) == 0U,
          "lockout: CUTOFF status cleared on re-enable");
    check((st.granted_mask & NP_SAFETY_EN_CVNS) == 0U,
          "lockout: re-enable does not itself restore the enable bit");
}

/*
 * After re-enable the baseline is invalidated, so the interlock is disarmed
 * until NP_CARDIAC_BASELINE_BEATS fresh intervals accumulate.  Without that
 * reset the still-elevated ring buffer would re-fire immediately.
 */
static void test_reenable_forces_fresh_baseline(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, RR_120_BPM); }
    check(cutoff_fired(&st), "fresh baseline: cutoff fired");

    g_tick_ms += NP_CARDIAC_LOCKOUT_MS;
    np_cardiac_interlock_tick(&st);
    np_cardiac_interlock_reenable(&st);
    st.granted_mask |= NP_SAFETY_EN_CVNS;     /* hub re-grants after confirm */

    /* Two beats: an interval count far below the arm point.  Even a violent
     * swing cannot fire while the baseline is invalid. */
    beat(&st, RR_120_BPM);   /* prime again (s_first_beat_seen was cleared) */
    beat(&st, RR_120_BPM);
    beat(&st, RR_30_BPM);
    check(!cutoff_fired(&st),
          "fresh baseline: disarmed until 8 new intervals accumulate");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U,
          "fresh baseline: CARDIAC status stays clear while disarmed");
}

/*
 * Pins the rolling-baseline refresh (NP_CARDIAC_OBS_MS = 5 s): once the
 * observation window elapses with no cutoff active, the baseline adopts the
 * current rate, so a slow drift does not accumulate into a cutoff.
 */
static void test_rolling_baseline_absorbs_slow_drift(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);

    /* Drift to 75 BPM (delta exactly 15, below the fire threshold). */
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, RR_75_BPM); }
    check(!cutoff_fired(&st), "refresh: 15 BPM drift holds");

    /* Let the observation window elapse — baseline should become 75. */
    idle_tick(&st, NP_CARDIAC_OBS_MS);
    check(!cutoff_fired(&st), "refresh: no cutoff on the refresh tick");

    /* A further 12 BPM rise (75 → 87) is under threshold against the REFRESHED
     * baseline but would be 27 against the original 60 — so no cutoff here
     * proves the refresh happened. */
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE; i++) { beat(&st, 690000U); } /* 86 BPM */
    check(!cutoff_fired(&st),
          "refresh: baseline advanced to 75 (86 BPM is within 15 of it)");
}

int main(void)
{
    test_no_cutoff_before_baseline();
    test_priming_beat_produces_no_interval();
    test_baseline_arms_at_eight_intervals_not_five();
    test_rise_above_threshold_cuts_off();
    test_fall_below_threshold_cuts_off();
    test_small_fall_does_not_cut_off();
    test_delta_exactly_at_threshold_holds();
    test_no_cutoff_when_cvns_inactive();
    test_impossible_rr_saturates_rather_than_wrapping();
    test_reenable_refused_during_lockout();
    test_reenable_forces_fresh_baseline();
    test_rolling_baseline_absorbs_slow_drift();

    if (g_failures == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
