/*
 * NeurOne Safety MCU — SW01-M05 Cardiac Interlock Host Tests
 * Document: NP-SW-001 §4.1 (SW01-M05), NP-FW-CVNS-001 Rev 2 §5,
 *           NP-FMEA-001 Rev 1 §SW01-M05 (FMEA-M05-02)
 *
 * Host-native tests for the STM32G071 bare-metal cervical VNS cardiac rhythm
 * interlock (np_cardiac_interlock.c) — the IEC 62304 Class C unit that owns the
 * CVNS enable bit and performs the cutoff.  Until this suite existed the only
 * "cardiac interlock" coverage in the tree was np_cvns_fai_tests, which
 * exercises the MAIN PROCESSOR module (firmware/cervical_vns/) on the other
 * side of the SPI boundary and asserts on main-processor constants.  The two
 * sides are independent implementations; a test of one is not a test of the
 * other.  (They do NOT cross-validate: the ±5 BPM check was never built, and
 * the safety MCU never receives the main processor's baseline — NP-FW-CVNS-001
 * Rev 9, OI-CVNS-10 closed.)
 *
 * PURPOSE: these tests PIN CURRENT BEHAVIOUR.  Several assertions encode
 * constants (NP_CARDIAC_BASELINE_BEATS = 8, NP_RR_BUF_SIZE = 8,
 * NP_CARDIAC_HR_HIST_LEN = 18).  The OI-CVNS-12 group runs beats in real time
 * and pins the lagged comparison that replaced the 5 s baseline refresh
 * (NP-FW-CVNS-001 Rev 10).  Any change must surface here as a reviewed failing
 * assertion, not land silently.
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
extern void             np_cardiac_interlock_restore(bool cutoff_pending);
extern bool             np_cardiac_interlock_nv_request(bool *pending_out);
extern void             np_cardiac_interlock_nv_done(bool written);
extern void             np_cardiac_interlock_arm_reset(void);
extern void             np_cardiac_interlock_user_changed(np_safety_state_t *state,
                                                          bool new_user_blocked);
/* The grant computation, linked in so the scope of a cardiac cutoff is tested
 * against the real code rather than a mirror of its mask. */
extern np_safe_status_t np_spi_watchdog_init(void);
extern void             np_spi_watchdog_tick(np_safety_state_t              *state,
                                             const np_safety_rx_ext_frame_t *rx,
                                             np_safety_tx_frame_t           *tx);

/* ── Mocked HAL stubs ──────────────────────────────────────────────────────────
 * Definitions of symbols declared in np_safety_hal.h — drift from the
 * production contract is a compile error (OI-SWCI-18).                       */
/* g_capture is the free-running 1 MHz TIM2 count; g_tick_ms is the 1 kHz
 * SysTick.  They are advanced INDEPENDENTLY on purpose: a test can deliver a
 * physiologically realistic R-R interval (1 s of TIM2) while advancing SysTick
 * by only 1 ms, so no heart-rate snapshot (NP_CARDIAC_HR_SNAP_MS) is taken
 * unless the test asks for it: the history holds only the arming mean.  That
 * isolates the delta comparison from the history.  beat_realtime() advances
 * both together, which is what OI-CVNS-12 was found with. */
static uint32_t g_tick_ms;
static uint32_t g_capture;
static bool     g_edge_pending;

uint32_t np_hal_get_tick_ms(void)      { return g_tick_ms; }
uint32_t np_hal_tim2_get_capture(void) { return g_capture; }
/* Linked for np_spi_watchdog.c's tick-liveness check, which no test here
 * calls; the SysTick-vs-TIM2 comparison is np_gpio_mgr_tests §7. */
uint32_t np_hal_tim2_now_us(void)      { return g_capture; }
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

/* Model the heartbeat grant between ticks: np_spi_watchdog_tick() re-derives
 * granted_mask from the hub's request on every valid frame, withholding CVNS
 * only under CARDIAC.  Before OI-RISK2-05 the interlock never withheld CVNS
 * except by cutting it, so the harness could leave the mask alone; the pre-arm
 * hold now clears the bit on every unarmed tick, and without this re-grant a
 * cleared bit would stick and read as a cutoff. */
static void regrant(np_safety_state_t *st)
{
    if (st->cvns_active && (st->status & NP_SAFETY_STATUS_CARDIAC) == 0U) {
        st->granted_mask |= NP_SAFETY_EN_CVNS;
    }
}

/* Deliver one R-peak arriving rr_us after the previous one. */
static void beat(np_safety_state_t *st, uint32_t rr_us)
{
    g_capture      += rr_us;
    g_edge_pending  = true;
    g_tick_ms      += 1U;
    regrant(st);
    np_cardiac_interlock_tick(st);
}

/* Advance SysTick with no R-peak (used for lockout expiry and refresh tests). */
static void idle_tick(np_safety_state_t *st, uint32_t dt_ms)
{
    g_tick_ms += dt_ms;
    regrant(st);
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

/* A cutoff is the CARDIAC status (with CVNS withheld).  Before OI-RISK2-05 a
 * withheld CVNS bit could only mean a cutoff; the pre-arm hold now withholds it
 * silently too, so "withheld" alone no longer distinguishes the two. */
static bool cutoff_fired(const np_safety_state_t *st)
{
    return (st->status & NP_SAFETY_STATUS_CARDIAC) != 0U &&
           (st->granted_mask & NP_SAFETY_EN_CVNS) == 0U;
}

static bool cvns_granted(const np_safety_state_t *st)
{
    return (st->granted_mask & NP_SAFETY_EN_CVNS) != 0U;
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
 * cannot pass (the main processor uses 5; OI-CVNS-10 closed, the length is
 * now OI-CVNS-12's to choose).
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

/* ── OI-CVNS-12: lagged comparison, beats in real time (NP-FW-CVNS-001 Rev 10) ──
 * Until Rev 10 the baseline was refreshed to the current mean every 5 s,
 * unconditionally, and adopted a step part-way through the 8-interval window:
 * a sustained 70 -> 50 BPM fall was never cut.  The cutoff now compares the
 * current mean with every 1 s snapshot of it from the last 18 s (horizon
 * NP_CARDIAC_HR_HIST_LEN x NP_CARDIAC_HR_SNAP_MS: 1 s of snapshot phase + the
 * 5 s spread CLAUDE.md §4.2 allows + 8 intervals at the 40 BPM floor).  These tests advance SysTick with TIM2, so snapshots
 * land as they would on the bench. */
static void beat_realtime(np_safety_state_t *st, uint32_t rr_us)
{
    /* The main loop runs between beats, so a snapshot lands on its 1 s
     * cadence rather than only on a beat. */
    for (uint32_t ms = 1U; ms < rr_us / 1000U; ms++) { idle_tick(st, 1U); }
    beat(st, rr_us);                     /* beat() adds the last 1 ms */
}

#define RR_70_BPM   857143U   /* 60 000 000 / 857 143 = 69.99 → 69 (BPM truncates) */
#define RR_50_BPM  1200000U   /* 60 000 000 / 1 200 000 = 50 (a 20 BPM fall) */
#define RR_54_BPM  1111112U   /* 60 000 000 / 1 111 112 = 53.99 → 53 (16 below 69) */
#define RR_56_BPM  1071428U   /* 60 000 000 / 1 071 428 = 56.00 → 56 */
#define RR_52_BPM  1153846U   /* 60 000 000 / 1 153 846 = 52.00 → 52 */
#define RR_40_BPM  1500000U   /* 60 000 000 / 1 500 000 = 40 (the horizon floor) */
#ifndef PRE_FLOOR_RAMP
#define PRE_FLOOR_RAMP 44U
#endif
#define RR_90_BPM   666667U   /* 60 000 000 /   666 667 = 89.99 → 89 */

/* Arm at rr0 in real time and run `pre` beats after the priming edge, then
 * step to rr1 for n beats.  Returns the number of post-step beats before the
 * cutoff, or 0 if none. */
static uint32_t realtime_step(np_safety_state_t *st, uint32_t rr0, uint32_t pre,
                              uint32_t rr1, uint32_t n)
{
    reset_all(st, true, 0U);
    beat_realtime(st, rr0);                                          /* prime */
    for (uint32_t i = 0U; i < pre; i++) { beat_realtime(st, rr0); }
    if (!cvns_granted(st) || cutoff_fired(st)) { return 0xFFFFFFFFU; }
    for (uint32_t i = 1U; i <= n; i++) {
        beat_realtime(st, rr1);
        if (cutoff_fired(st)) { return i; }
    }
    return 0U;
}

/* The case OI-CVNS-12 was raised on, which the 5 s refresh never cut.  The
 * RISK-25 hazard direction.  Cut on the 6th post-step interval, the first at
 * which the 8-interval mean has moved more than 15 BPM (7.2 s). */
static void test_sustained_fall_cut_in_real_time(void)
{
    np_safety_state_t st;
    uint32_t k = realtime_step(&st, RR_70_BPM, 30U, RR_50_BPM, 40U);
    check(k == 6U, "OI-CVNS-12: a sustained 70 -> 50 BPM fall is cut, on interval 6");
}

/* FAI-CV02's own step, cut by the as-built rule in about 30 % of trials. */
static void test_fai_cv02_rise_cut_in_real_time(void)
{
    np_safety_state_t st;
    uint32_t k = realtime_step(&st, RR_70_BPM, 30U, RR_90_BPM, 40U);
    check(k > 0U && k <= TEST_RR_BUF_SIZE, "OI-CVNS-12: FAI-CV02's 70 -> 90 BPM rise is cut");
}

/* A step only just over the threshold needs all 8 post-step intervals.  A
 * band-gated refresh (candidate A) would still have absorbed part of it. */
static void test_marginal_fall_cut_in_real_time(void)
{
    np_safety_state_t st;
    uint32_t k = realtime_step(&st, RR_70_BPM, 30U, RR_54_BPM, 40U);
    check(k == TEST_RR_BUF_SIZE, "OI-CVNS-12: a 70 -> 54 BPM fall is cut once the window has turned over");
}

/* The horizon floor, instantaneous: a 56 -> 40 BPM fall needs all 8 intervals
 * at 1.5 s, 12 s of transit, and the pre-step snapshot must still be in the
 * history.  The worst case is the last pre-step snapshot landing just over
 * 0.5 s after the last pre-step beat.  29 beats after arming puts it 509 ms
 * after (armed on edge 9, 1071 ms per beat, snapshots every 1000 ms from
 * arming), so it must outlive +11.5 s: a horizon of 11 fails this (mutation-
 * checked).  An instantaneous step needs only 12 s; the 5 s spread is what
 * takes the horizon to 18, and the next test pins that. */
static void test_floor_fall_to_40_cut_in_real_time(void)
{
    np_safety_state_t st;
    uint32_t k = realtime_step(&st, RR_56_BPM, 29U, RR_40_BPM, 40U);
    check(k == TEST_RR_BUF_SIZE, "OI-CVNS-12: a 56 -> 40 BPM fall (the floor) is cut");
}

/* A change spread over the requirement's whole 5 s, in its worst shape: part
 * of it at onset (rr0 -> rr_mid), the rest by mid_ms later (rr_mid -> rr1, from
 * the last interval that STARTS within mid_ms).  The mid rate is within 15 BPM
 * of both ends, so only the snapshot taken before onset can see the change,
 * and it must outlive the spread plus all 8 intervals at rr1.  rr0 for `pre`
 * beats after the priming edge.  Returns the number of post-onset beats before
 * the cutoff, or 0 if none. */
static uint32_t realtime_two_stage(np_safety_state_t *st, uint32_t rr0, uint32_t pre,
                                   uint32_t rr_mid, uint32_t mid_ms, uint32_t rr1, uint32_t n)
{
    reset_all(st, true, 0U);
    beat_realtime(st, rr0);                                          /* prime */
    for (uint32_t i = 0U; i < pre; i++) { beat_realtime(st, rr0); }
    if (!cvns_granted(st) || cutoff_fired(st)) { return 0xFFFFFFFFU; }
    uint32_t t_ms = 0U;
    for (uint32_t i = 1U; i <= n; i++) {
        uint32_t rr = ((t_ms + rr_mid / 1000U) <= mid_ms) ? rr_mid : rr1;
        beat_realtime(st, rr);
        t_ms += rr / 1000U;
        if (cutoff_fired(st)) { return i; }
    }
    return 0U;
}

/* The horizon floor with the 5 s spread: 56 -> 52 BPM, then -> 40 BPM from
 * the interval starting 4.6 s later.  52 keeps the 7-of-8 mean at 41, so the
 * cut needs all 8 intervals at 40: 16.6 s after the last 56 beat.  The 56
 * snapshot can be up to 1 s older than the first 52 interval's end.
 * PRE_FLOOR_RAMP puts it at its worst phase here, so NP_CARDIAC_HR_HIST_LEN =
 * 16 fails this and 17 and 18 pass (mutation-checked).  The 18th second is the
 * analytic allowance for a change whose final rate starts at exactly +5 s
 * (NP-FW-CVNS-001 §5.4), which a realisable beat train here does not reach.
 * A linear 56 -> 40 ramp over 5 s needs only 14 s. */
static void test_floor_two_stage_to_40_cut_in_real_time(void)
{
    np_safety_state_t st;
    uint32_t k = realtime_two_stage(&st, RR_56_BPM, PRE_FLOOR_RAMP,
                                    RR_52_BPM, 5000U, RR_40_BPM, 40U);
    check(k > 0U && k < 0xFFFFFFFFU,
          "OI-CVNS-12: 56 -> 52 -> 40 BPM within 5 s (the floor) is cut");
}

/* Slow drift is still not a cutoff: 60 -> 90 BPM at 1 BPM per 2 s.  No 18 s
 * span holds more than about 9 BPM of it, so the history keeps up. */
static void test_slow_drift_not_cut(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    beat_realtime(&st, RR_60_BPM);
    for (uint8_t i = 0U; i < 30U; i++) { beat_realtime(&st, RR_60_BPM); }
    double t_s = 0.0;
    while (t_s < 60.0) {
        double hr = 60.0 + t_s / 2.0;
        uint32_t rr = (uint32_t)(60000000.0 / hr);
        beat_realtime(&st, rr);
        t_s += (double)rr / 1e6;
    }
    check(!cutoff_fired(&st) && cvns_granted(&st), "OI-CVNS-12: a 30 BPM drift over 60 s is not cut");
}

/* After a lockout expires, the history is reseeded: the pre-event snapshots
 * must not re-trip a still-elevated rate and restart the lockout, which would
 * make re-enable unreachable. */
static void test_lockout_expiry_reseeds_history(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE && !cutoff_fired(&st); i++) { beat(&st, RR_120_BPM); }
    check(cutoff_fired(&st), "reseed: cutoff fired");
    g_tick_ms += NP_CARDIAC_LOCKOUT_MS;
    beat(&st, RR_120_BPM);                 /* lockout expires; still at 120 */
    beat(&st, RR_120_BPM);
    np_cardiac_interlock_reenable(&st);
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U,
          "reseed: re-enable reachable at a still-elevated rate after lockout");
}

/* ── Power-cycle persistence (NP-SW-FAULTMSG-001 P1, OI-FAULTMSG-01) ──────────
 * The interlock does not touch flash; it posts write requests and, at boot,
 * re-asserts a persisted cutoff when a session first requests CVNS. */

/* A live cutoff posts "pending"; the request survives until that value is
 * reported written, and a stale completion cannot drop a newer request. */
static void test_cutoff_posts_pending_write(void)
{
    np_safety_state_t st;
    bool v = false;
    reset_all(&st, true, 0U);
    check(!np_cardiac_interlock_nv_request(&v), "nv: no request before any cutoff");

    establish_baseline(&st, RR_60_BPM);
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE && !cutoff_fired(&st); i++) {
        beat(&st, RR_120_BPM);
    }
    check(np_cardiac_interlock_nv_request(&v) && v, "nv: cutoff requests persist(pending)");

    np_cardiac_interlock_nv_done(false);   /* stale completion for another value */
    check(np_cardiac_interlock_nv_request(&v) && v, "nv: stale completion does not drop it");
    np_cardiac_interlock_nv_done(true);
    check(!np_cardiac_interlock_nv_request(&v), "nv: request cleared once written");
}

/* A completed re-enable posts "acknowledged". */
static void test_reenable_posts_clear_write(void)
{
    np_safety_state_t st;
    bool v = true;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE && !cutoff_fired(&st); i++) {
        beat(&st, RR_120_BPM);
    }
    np_cardiac_interlock_nv_done(true);
    g_tick_ms += NP_CARDIAC_LOCKOUT_MS;
    np_cardiac_interlock_tick(&st);
    np_cardiac_interlock_reenable(&st);
    check(np_cardiac_interlock_nv_request(&v) && !v, "nv: re-enable requests persist(acknowledged)");
}

/* A restored cutoff is LATENT: a session that does not request CVNS is
 * untouched, so one cardiac cutoff cannot lock the wearer out of every other
 * modality. */
static void test_restored_cutoff_latent_without_cvns(void)
{
    np_safety_state_t st;
    reset_all(&st, false, 0U);
    st.granted_mask = NP_SAFETY_EN_CVNS | (uint16_t)(NP_SAFETY_EN_CVNS >> 1);
    np_cardiac_interlock_restore(true);
    idle_tick(&st, 10U);
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U,
          "restore: no CARDIAC while CVNS is not requested");
    check(st.granted_mask == (uint16_t)(NP_SAFETY_EN_CVNS | (NP_SAFETY_EN_CVNS >> 1)),
          "restore: grants untouched while CVNS is not requested");
}

/* The first CVNS request after a restore meets exactly the state a live cutoff
 * leaves: CVNS dropped before it reaches the GPIO, CARDIAC + CUTOFF, fault slot
 * 10, and a 30 s lockout counted from now — then the ordinary re-enable path. */
static void test_restored_cutoff_asserts_on_cvns_request(void)
{
    np_safety_state_t st;
    bool v = false;
    reset_all(&st, true, 0U);
    g_tick_ms = 5000U;
    np_cardiac_interlock_restore(true);
    np_cardiac_interlock_tick(&st);
    check(cutoff_fired(&st), "restore: CVNS dropped on first CVNS request");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) != 0U, "restore: CARDIAC set");
    check((st.status & NP_SAFETY_STATUS_CUTOFF) != 0U, "restore: CUTOFF set");
    check(st.fault_slot == CVNS_FAULT_SLOT, "restore: fault_slot = 10 (CVNS)");
    check(!np_cardiac_interlock_nv_request(&v), "restore: no write — already persisted");

    g_tick_ms = 5000U + NP_CARDIAC_LOCKOUT_MS - 1U;
    np_cardiac_interlock_tick(&st);
    np_cardiac_interlock_reenable(&st);
    check((st.status & NP_SAFETY_STATUS_CARDIAC) != 0U,
          "restore: re-enable refused inside the restarted 30 s lockout");

    g_tick_ms = 5000U + NP_CARDIAC_LOCKOUT_MS;
    np_cardiac_interlock_tick(&st);
    np_cardiac_interlock_reenable(&st);
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U, "restore: re-enable accepted after lockout");
    check(np_cardiac_interlock_nv_request(&v) && !v, "restore: acknowledgement persisted");
}

static void test_restore_false_is_inert(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    np_cardiac_interlock_restore(false);
    np_cardiac_interlock_tick(&st);
    check(!cutoff_fired(&st), "restore(false): CVNS stays granted");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U, "restore(false): no CARDIAC");
}


/* A cardiac cutoff withholds cervical VNS and nothing else (principal,
 * 2026-09-22): every other channel that is otherwise safe keeps its grant, and
 * CUTOFF stays set while the cervical channel is cut. */
static void test_cardiac_blocks_only_cvns(void)
{
    np_safety_state_t st;
    const uint16_t other = (uint16_t)(NP_SAFETY_EN_ALL_MASK & ~NP_SAFETY_EN_CVNS);
    reset_all(&st, true, 0U);
    (void)np_spi_watchdog_init();
    st.requested_mask = NP_SAFETY_EN_ALL_MASK;
    st.status         = NP_SAFETY_STATUS_CARDIAC | NP_SAFETY_STATUS_CUTOFF;

    np_spi_watchdog_tick(&st, NULL, NULL);
    check((st.granted_mask & NP_SAFETY_EN_CVNS) == 0U, "scope: CVNS withheld under CARDIAC");
    check((st.granted_mask & other) == other,          "scope: every other channel still granted");
    check((st.status & NP_SAFETY_STATUS_CUTOFF) != 0U, "scope: CUTOFF stays set while CVNS is cut");

    st.status = NP_SAFETY_STATUS_CARDIAC | NP_SAFETY_STATUS_THERMAL;
    np_spi_watchdog_tick(&st, NULL, NULL);
    check(st.granted_mask == 0U, "scope: an all-channel fault still blocks everything");
}


/* Per-user scope (principal, 2026-09-22): a cutoff is held for the user who
 * triggered it.  Switching to a user without one releases cervical VNS for
 * them; switching back re-arms the first user's cutoff as latent. */
static void test_user_change_scopes_the_cutoff(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    (void)np_spi_watchdog_init();
    establish_baseline(&st, RR_60_BPM);
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE && !cutoff_fired(&st); i++) {
        beat(&st, RR_120_BPM);
    }
    check((st.status & NP_SAFETY_STATUS_CARDIAC) != 0U, "user: Alice's live cutoff");

    np_cardiac_interlock_user_changed(&st, false);          /* Bob: not blocked */
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U, "user: Bob does not inherit CARDIAC");
    st.requested_mask = NP_SAFETY_EN_CVNS;
    np_spi_watchdog_tick(&st, NULL, NULL);
    np_cardiac_interlock_tick(&st);
    /* Bob has no baseline yet (user_changed clears it), so the pre-arm hold
     * withholds CVNS until his own beats arm the interlock (OI-RISK2-05). */
    check(!cvns_granted(&st) && !cutoff_fired(&st),
          "user: Bob is held, not cut, until his baseline arms");
    establish_baseline(&st, RR_60_BPM);
    check(cvns_granted(&st), "user: Bob is granted cervical VNS once armed");

    np_cardiac_interlock_user_changed(&st, true);           /* back to Alice */
    np_spi_watchdog_tick(&st, NULL, NULL);
    np_cardiac_interlock_tick(&st);
    check(cutoff_fired(&st) && (st.status & NP_SAFETY_STATUS_CARDIAC) != 0U,
          "user: Alice's cutoff re-asserted on her next CVNS request");
}

/* ── Class C self-sufficiency (NP-RISK-002 OI-RISK2-05) ───────────────────────── */

/* PRE-ARM HOLD: CVNS is withheld until the baseline arms — silently (no CARDIAC,
 * no lockout, no NV write) — and granted from the tick that arms it. */
static void test_prearm_hold_withholds_then_grants(void)
{
    np_safety_state_t st;
    bool nv = false;
    reset_all(&st, true, 0U);

    beat(&st, RR_60_BPM);                                    /* prime */
    check(!cvns_granted(&st), "pre-arm: CVNS withheld before any interval");
    for (uint8_t i = 0U; i + 1U < NP_CARDIAC_BASELINE_BEATS; i++) { beat(&st, RR_60_BPM); }
    check(!cvns_granted(&st), "pre-arm: CVNS withheld at 7 intervals");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U &&
          !np_cardiac_interlock_nv_request(&nv),
          "pre-arm: the hold is silent (no CARDIAC, no persisted cutoff)");
    beat(&st, RR_60_BPM);                                    /* 8th interval arms */
    check(cvns_granted(&st), "pre-arm: CVNS granted on the tick that arms");
}

/* No R-peaks ever: CVNS is never granted, and nothing false-trips a lockout. */
static void test_no_rpeaks_never_granted(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    for (uint8_t i = 0U; i < 60U; i++) { idle_tick(&st, 1000U); }
    check(!cvns_granted(&st), "no R-peaks: CVNS never granted over 60 s");
    check((st.status & NP_SAFETY_STATUS_CARDIAC) == 0U,
          "no R-peaks: no cardiac lockout without an armed baseline");
}

/* STALENESS: once armed and granted, no edge for NP_CARDIAC_RPEAK_STALE_MS cuts
 * exactly like a cardiac event — lockout and a persisted cutoff. */
static void test_stale_rpeak_cuts_off(void)
{
    np_safety_state_t st;
    bool nv = false;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);
    check(cvns_granted(&st), "stale: armed and granted");

    idle_tick(&st, NP_CARDIAC_RPEAK_STALE_MS - 1U);
    check(!cutoff_fired(&st), "stale: no cutoff 1 ms before the stale limit");
    idle_tick(&st, 1U);
    check(cutoff_fired(&st), "stale: cutoff at exactly the stale limit");
    check(st.fault_slot == 10U, "stale: CVNS fault slot recorded");
    check(np_cardiac_interlock_nv_request(&nv) && nv, "stale: cutoff persisted like a cardiac event");

    np_cardiac_interlock_reenable(&st);
    check((st.status & NP_SAFETY_STATUS_CARDIAC) != 0U, "stale: lockout refuses re-enable");
}

/* A live rhythm never trips staleness: beats 1 s apart, 60 s of them. */
static void test_live_rhythm_never_stale(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);
    for (uint8_t i = 0U; i < 60U; i++) {
        g_tick_ms += 999U;
        beat(&st, RR_60_BPM);
    }
    check(!cutoff_fired(&st) && cvns_granted(&st), "live: 60 s at 60 BPM never stale");
}

/* A new CVNS request re-arms: an old baseline cannot grant early, and an old
 * last-edge time cannot trip staleness the moment the request starts. */
static void test_arm_reset_on_new_request(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);

    g_tick_ms += 600000U;                   /* ten minutes between sessions */
    np_cardiac_interlock_arm_reset();
    idle_tick(&st, 1U);
    check(!cutoff_fired(&st), "arm reset: an old edge time does not trip the new request");
    check(!cvns_granted(&st), "arm reset: an old baseline does not grant the new request");
    establish_baseline(&st, RR_60_BPM);
    check(cvns_granted(&st), "arm reset: granted after fresh beats arm it");
}

/* arm_reset re-arms monitoring; it must never clear a live cutoff. */
static void test_arm_reset_keeps_cutoff(void)
{
    np_safety_state_t st;
    reset_all(&st, true, 0U);
    establish_baseline(&st, RR_60_BPM);
    for (uint8_t i = 0U; i < TEST_RR_BUF_SIZE && !cutoff_fired(&st); i++) { beat(&st, RR_120_BPM); }
    check(cutoff_fired(&st), "arm reset: cutoff fired");
    np_cardiac_interlock_arm_reset();
    idle_tick(&st, 1U);
    check(cutoff_fired(&st), "arm reset: a live cutoff survives a new request");
    /* ...and so does its lockout: re-enable is still refused inside 30 s. */
    np_cardiac_interlock_reenable(&st);
    check(cutoff_fired(&st), "arm reset: the 30 s lockout survives a new request");
}

int main(void)
{
    test_prearm_hold_withholds_then_grants();
    test_no_rpeaks_never_granted();
    test_stale_rpeak_cuts_off();
    test_live_rhythm_never_stale();
    test_arm_reset_on_new_request();
    test_arm_reset_keeps_cutoff();

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
    test_cutoff_posts_pending_write();
    test_reenable_posts_clear_write();
    test_restored_cutoff_latent_without_cvns();
    test_restored_cutoff_asserts_on_cvns_request();
    test_restore_false_is_inert();
    test_cardiac_blocks_only_cvns();
    test_user_change_scopes_the_cutoff();
    test_sustained_fall_cut_in_real_time();
    test_fai_cv02_rise_cut_in_real_time();
    test_marginal_fall_cut_in_real_time();
    test_floor_fall_to_40_cut_in_real_time();
    test_floor_two_stage_to_40_cut_in_real_time();
    test_slow_drift_not_cut();
    test_lockout_expiry_reseeds_history();

    if (g_failures == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
