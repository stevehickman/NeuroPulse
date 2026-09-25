/*
 * NeurOne Safety MCU — SW01-M01 / SW01-M02 cutoff-path host tests
 * Document: NP-FMEA-001 Rev 11 (OI-FMEA-11): FMEA-M01-01, -M01-05, -M02-05
 *
 * The eleventh Class C host-test target.  It exists because NP-FMEA-001 scored
 * three residuals on verification that did not exist: np_gpio_mgr.c (the ONLY
 * writer of the ten stimulation enable lines) and np_spi_watchdog_check() (the
 * heartbeat-loss cutoff) were linked into no host-test target at all.  The
 * platform driver's polarity and boot preset are covered by
 * np_hal_platform_tests; what was not covered is the layer above it:
 *
 *   1. np_gpio_mgr_init() leaves all ten lines disabled         (FMEA-M01-05)
 *   2. np_gpio_mgr_apply() maps each granted bit to exactly its own line, and
 *      disables every line whose bit is clear                    (FMEA-M01-01)
 *   3. apply() rewrites all ten lines on EVERY call, so no pin can hold a
 *      stale enabled state past one main-loop iteration          (FMEA-M01-01)
 *   4. heartbeat tick OVERWRITES granted_mask (0 while any all-channel fault
 *      is active) rather than accumulating it                    (FMEA-M01-01)
 *   5. heartbeat loss clears granted_mask at exactly the timeout, and the next
 *      apply() disables all ten lines                            (FMEA-M02-05)
 *   6. heartbeat sequence gate: a repeated, replayed or backward frame does
 *      not count as a beat, so a stuck hub still trips the watchdog
 *                                                    (FMEA-M02-03, OI-FMEA-12)
 *   7. tick liveness: SysTick stopped, slowed or sped up against TIM2 latches
 *      an all-channel FAULT that nothing but a reset clears
 *                                                    (FMEA-M02-02, OI-FMEA-12)
 *
 * np_hal_gpio_write_pin() is a recording double here, not the real driver:
 * the fake register file keeps only the last BSRR write per port, so it cannot
 * observe nine sequential port-B writes.  The driver's own polarity
 * (1 = HIGH = disabled) is asserted against the real source in
 * np_hal_platform_tests; this target asserts what np_gpio_mgr.c asks it for.
 *
 * NOT covered, by design (NP-FMEA-001 OI-FMEA-11, NP-RISK-004 OI-RISK4-06,
 * NP-RISK-002 OI-RISK2-05): a hung main loop.  apply() and check() both run
 * from it, the safety MCU configures no IWDG, and nothing reads an enable pin
 * back.  No host test can show a backstop that is not built.
 *
 * Build with -DNP_BUILD_TESTS=ON.  No ARM toolchain required.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "np_safety_config.h"
#include "np_safety_hal.h"
#include "np_safety_protocol.h"

int np_test_port_a;
int np_test_port_b;

/* ── Units under test ─────────────────────────────────────────────────────── */
extern np_safe_status_t np_gpio_mgr_init(void);
extern void             np_gpio_mgr_apply(const np_safety_state_t *state);
extern np_safe_status_t np_spi_watchdog_init(void);
extern void             np_spi_watchdog_tick(np_safety_state_t              *state,
                                             const np_safety_rx_ext_frame_t *rx,
                                             np_safety_tx_frame_t           *tx);
extern void             np_spi_watchdog_check(np_safety_state_t *state);
extern bool             np_spi_watchdog_seq_accept(uint8_t seq);
extern void             np_spi_watchdog_tick_liveness(np_safety_state_t *state);

/* ── The ten enable lines and the protocol bit that must drive each ─────────
 * Mirrors np_gpio_mgr_apply() by NAME, not by position: a transposed pair in
 * the unit under test fails test 2 for both lines.                          */
static const struct {
    void       *port;
    uint16_t    pin;
    uint16_t    bit;
    const char *name;
} k_lines[] = {
    { NP_EN_PBM_CRANIAL_PORT, NP_EN_PBM_CRANIAL_PIN, NP_SAFETY_EN_PBM_CRANIAL, "PBM_CRANIAL" },
    { NP_EN_BES_PORT,         NP_EN_BES_PIN,         NP_SAFETY_EN_BES_TACS,    "BES"         },
    { NP_EN_TDCS_PORT,        NP_EN_TDCS_PIN,        NP_SAFETY_EN_TDCS,        "TDCS"        },
    { NP_EN_VNS_PORT,         NP_EN_VNS_PIN,         NP_SAFETY_EN_VNS_HRV,     "VNS"         },
    { NP_EN_VISUAL_PORT,      NP_EN_VISUAL_PIN,      NP_SAFETY_EN_VISUAL,      "VISUAL"      },
    { NP_EN_INTRANASAL_PORT,  NP_EN_INTRANASAL_PIN,  NP_SAFETY_EN_INTRANASAL,  "INTRANASAL"  },
    { NP_EN_CVNS_PORT,        NP_EN_CVNS_PIN,        NP_SAFETY_EN_CVNS,        "CVNS"        },
    { NP_EN_TMS_PORT,         NP_EN_TMS_PIN,         NP_SAFETY_EN_TMS,         "TMS"         },
    { NP_EN_PBM_1170_PORT,    NP_EN_PBM_1170_PIN,    NP_SAFETY_EN_PBM_1170NM,  "PBM_1170"    },
    { NP_EN_CLIN_STIM_PORT,   NP_EN_CLIN_STIM_PIN,   NP_SAFETY_EN_CLIN_STIM,   "CLIN_STIM"   },
};
#define N_LINES  (sizeof(k_lines) / sizeof(k_lines[0]))

/* ── Recording HAL doubles (signatures from np_safety_hal.h) ──────────────── */
#define UNWRITTEN  (-99)
static int      g_state[N_LINES];     /* last state written, UNWRITTEN if none */
static unsigned g_writes[N_LINES];    /* writes per line                       */
static unsigned g_unknown_writes;     /* writes to a (port,pin) not in k_lines */
static uint32_t g_tick_ms;

void np_hal_gpio_write_pin(void *port, uint16_t pin, int state)
{
    size_t i;
    for (i = 0U; i < N_LINES; i++) {
        if (k_lines[i].port == port && k_lines[i].pin == pin) {
            g_state[i] = state;
            g_writes[i]++;
            return;
        }
    }
    g_unknown_writes++;
}

uint32_t np_hal_get_tick_ms(void) { return g_tick_ms; }

/* TIM2's live 1 MHz count, advanced independently of g_tick_ms so §7 can stop,
 * slow or wrap either one. */
static uint32_t g_tim2_us;
uint32_t np_hal_tim2_now_us(void) { return g_tim2_us; }

static void rec_reset(void)
{
    size_t i;
    for (i = 0U; i < N_LINES; i++) { g_state[i] = UNWRITTEN; g_writes[i] = 0U; }
    g_unknown_writes = 0U;
}

/* Active-LOW, per np_safety_hal.h: 0 = LOW = ENABLED, non-zero = HIGH = disabled. */
static bool line_enabled(size_t i)  { return g_state[i] == 0; }
static bool line_disabled(size_t i) { return g_state[i] != UNWRITTEN && g_state[i] != 0; }

/* ── Harness ──────────────────────────────────────────────────────────────── */
static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

/* ══ 1. init leaves every line disabled (FMEA-M01-05) ═══════════════════════ */
static void test_init_disables_all(void)
{
    size_t i;
    int    ok = 1;

    rec_reset();
    check(np_gpio_mgr_init() == NP_SAFE_OK, "gpio_mgr_init returns NP_SAFE_OK");
    for (i = 0U; i < N_LINES; i++) {
        if (!line_disabled(i)) {
            printf("  %s not written disabled by init\n", k_lines[i].name);
            ok = 0;
        }
    }
    check(ok, "gpio_mgr_init writes all 10 enable lines HIGH (disabled)");
    check(g_unknown_writes == 0U, "gpio_mgr_init writes no pin outside the ten enables");
}

/* ══ 2. apply maps each bit to exactly its own line (FMEA-M01-01) ═══════════ */
static void test_apply_one_hot_mapping(void)
{
    np_safety_state_t st;
    size_t b, i;
    int    ok = 1;

    for (b = 0U; b < N_LINES; b++) {
        memset(&st, 0, sizeof(st));
        st.granted_mask = k_lines[b].bit;
        rec_reset();
        np_gpio_mgr_apply(&st);
        for (i = 0U; i < N_LINES; i++) {
            bool want_enabled = (i == b);
            if (want_enabled ? !line_enabled(i) : !line_disabled(i)) {
                printf("  granting %s: %s is %s\n", k_lines[b].name, k_lines[i].name,
                       line_enabled(i) ? "ENABLED" : "disabled/unwritten");
                ok = 0;
            }
        }
    }
    check(ok, "apply: each granted bit enables exactly its own line and no other");
}

static void test_apply_zero_and_all(void)
{
    np_safety_state_t st;
    size_t i;
    int    all_off = 1;
    int    all_on  = 1;
    int    spare   = 1;

    memset(&st, 0, sizeof(st));
    rec_reset();
    np_gpio_mgr_apply(&st);
    for (i = 0U; i < N_LINES; i++) { if (!line_disabled(i)) { all_off = 0; } }
    check(all_off, "apply(granted_mask = 0) disables all 10 lines");

    st.granted_mask = NP_SAFETY_EN_ALL_MASK;
    rec_reset();
    np_gpio_mgr_apply(&st);
    for (i = 0U; i < N_LINES; i++) { if (!line_enabled(i)) { all_on = 0; } }
    check(all_on, "apply(NP_SAFETY_EN_ALL_MASK) enables all 10 lines");

    /* Unallocated bits must never assert a line. */
    st.granted_mask = (uint16_t)~NP_SAFETY_EN_ALL_MASK;
    rec_reset();
    np_gpio_mgr_apply(&st);
    for (i = 0U; i < N_LINES; i++) { if (!line_disabled(i)) { spare = 0; } }
    check(spare, "apply: unallocated mask bits enable nothing");
    check(g_unknown_writes == 0U, "apply writes no pin outside the ten enables");
}

/* ══ 3. apply rewrites every line on every call (FMEA-M01-01) ═══════════════
 * A change-gated writer ("only write lines whose bit changed") would let a pin
 * that was mis-written once hold its enabled state indefinitely.            */
static void test_apply_rewrites_every_line_every_call(void)
{
    np_safety_state_t st;
    size_t i;
    int    ok = 1;

    memset(&st, 0, sizeof(st));
    rec_reset();
    np_gpio_mgr_apply(&st);
    np_gpio_mgr_apply(&st);
    np_gpio_mgr_apply(&st);
    for (i = 0U; i < N_LINES; i++) { if (g_writes[i] != 3U) { ok = 0; } }
    check(ok, "apply rewrites all 10 lines on every call, including an unchanged mask");
}

/* ══ 4. heartbeat tick overwrites granted_mask (FMEA-M01-01) ════════════════ */
static void test_tick_overwrites_granted_mask(void)
{
    np_safety_state_t st;

    memset(&st, 0, sizeof(st));
    g_tick_ms = 1000U;
    (void)np_spi_watchdog_init();

    st.requested_mask = NP_SAFETY_EN_ALL_MASK;
    np_spi_watchdog_tick(&st, NULL, NULL);
    check(st.granted_mask == NP_SAFETY_EN_ALL_MASK, "tick: no fault grants the requested mask");

    st.status |= NP_SAFETY_STATUS_THERMAL;
    np_spi_watchdog_tick(&st, NULL, NULL);
    check(st.granted_mask == 0U, "tick: an active fault zeroes granted_mask (no stale bits kept)");

    st.status = NP_SAFETY_STATUS_OK;
    st.requested_mask = NP_SAFETY_EN_TDCS;
    st.granted_mask   = NP_SAFETY_EN_ALL_MASK;   /* stale from a prior tick */
    np_spi_watchdog_tick(&st, NULL, NULL);
    check(st.granted_mask == NP_SAFETY_EN_TDCS,
          "tick: granted_mask is replaced by the new request, not OR-accumulated");
}

/* ══ 5. heartbeat loss → cutoff → all lines disabled (FMEA-M02-05) ══════════ */
static void test_heartbeat_loss_cuts_all(void)
{
    np_safety_state_t st;
    size_t i;
    int    ok = 1;

    memset(&st, 0, sizeof(st));
    g_tick_ms = 5000U;
    (void)np_spi_watchdog_init();
    st.requested_mask = NP_SAFETY_EN_ALL_MASK;
    np_spi_watchdog_tick(&st, NULL, NULL);          /* last good beat at 5000 */

    g_tick_ms = 5000U + NP_SAFETY_WDG_TIMEOUT_MS - 1U;
    np_spi_watchdog_check(&st);
    check(st.granted_mask == NP_SAFETY_EN_ALL_MASK, "check: no cutoff 1 ms before the timeout");

    g_tick_ms = 5000U + NP_SAFETY_WDG_TIMEOUT_MS;
    np_spi_watchdog_check(&st);
    check(st.granted_mask == 0U, "check: granted_mask cleared at exactly the timeout");
    check((st.status & (NP_SAFETY_STATUS_WATCHDOG | NP_SAFETY_STATUS_CUTOFF))
              == (NP_SAFETY_STATUS_WATCHDOG | NP_SAFETY_STATUS_CUTOFF),
          "check: WATCHDOG and CUTOFF status set on timeout");

    rec_reset();
    np_gpio_mgr_apply(&st);                          /* same main-loop iteration */
    for (i = 0U; i < N_LINES; i++) { if (!line_disabled(i)) { ok = 0; } }
    check(ok, "heartbeat loss: the next apply disables all 10 lines");

    /* A late grant cannot re-enable while no heartbeat arrives: check() is
     * edge-triggered, so the mask stays at the 0 it wrote. */
    g_tick_ms += 10000U;
    np_spi_watchdog_check(&st);
    check(st.granted_mask == 0U, "check: mask stays cleared while heartbeats stay absent");
}

static void test_heartbeat_timeout_across_tick_wrap(void)
{
    np_safety_state_t st;

    memset(&st, 0, sizeof(st));
    g_tick_ms = 0xFFFFFF00U;                         /* 256 ms before wrap */
    (void)np_spi_watchdog_init();
    st.requested_mask = NP_SAFETY_EN_ALL_MASK;
    np_spi_watchdog_tick(&st, NULL, NULL);

    g_tick_ms = 0xFFFFFF00U + NP_SAFETY_WDG_TIMEOUT_MS;   /* wraps past 0 */
    np_spi_watchdog_check(&st);
    check(st.granted_mask == 0U, "check: timeout still fires across the 32-bit tick wrap");
}

/* ══ 6. heartbeat sequence gate (FMEA-M02-03, OI-FMEA-12 (a)) ══════════════ */

/* Feed one well-formed heartbeat at g_tick_ms the way np_safety_main.c does:
 * the tick (watchdog reset + grant) runs only when the counter is accepted. */
static bool beat(np_safety_state_t *st, uint8_t seq)
{
    bool live = np_spi_watchdog_seq_accept(seq);
    if (live) { np_spi_watchdog_tick(st, NULL, NULL); }
    np_spi_watchdog_check(st);
    return live;
}

static void test_seq_startup_and_run(void)
{
    np_safety_state_t st;
    uint8_t i;
    int ok = 1;

    memset(&st, 0, sizeof(st));
    g_tick_ms = 100U;
    (void)np_spi_watchdog_init();
    st.requested_mask = NP_SAFETY_EN_ALL_MASK;

    check(!beat(&st, 5U), "seq: first frame after init is not accepted");
    check(!beat(&st, 6U), "seq: second (one forward step) is not accepted");
    check(beat(&st, 7U) && st.granted_mask == NP_SAFETY_EN_ALL_MASK,
          "seq: third (two forward steps) is accepted and grants");
    for (i = 0U; i < 20U; i++) {
        if (!beat(&st, (uint8_t)(i & 7U))) { ok = 0; }    /* 7 -> 0 wraps */
    }
    check(ok, "seq: a live counter stays accepted across the 7 -> 0 wrap");
    check(beat(&st, (uint8_t)(0xF8U | 4U)), "seq: bits above the 3-bit counter are ignored (3 -> 4)");
}

static void test_seq_stuck_buffer_trips_watchdog(void)
{
    np_safety_state_t st;
    uint32_t t0;
    int ok = 1;

    memset(&st, 0, sizeof(st));
    g_tick_ms = 1000U;
    (void)np_spi_watchdog_init();
    st.requested_mask = NP_SAFETY_EN_ALL_MASK;
    (void)beat(&st, 0U); (void)beat(&st, 1U);
    check(beat(&st, 2U), "stuck: live before the hang");
    t0 = g_tick_ms;

    /* The hub hangs and its SPI re-sends the last buffer every 200 ms. */
    while (g_tick_ms < t0 + NP_SAFETY_WDG_TIMEOUT_MS - 200U) {
        g_tick_ms += 200U;
        if (beat(&st, 2U)) { ok = 0; }
    }
    check(ok, "stuck: a repeated counter is never accepted");
    check(st.granted_mask == NP_SAFETY_EN_ALL_MASK, "stuck: no cutoff before the timeout");
    g_tick_ms = t0 + NP_SAFETY_WDG_TIMEOUT_MS;
    (void)beat(&st, 2U);
    check(st.granted_mask == 0U && (st.status & NP_SAFETY_STATUS_WATCHDOG) != 0U,
          "stuck: the watchdog fires at the timeout despite well-formed frames");
    g_tick_ms += 10000U;
    (void)beat(&st, 2U);
    check(st.granted_mask == 0U, "stuck: and stays cut while the replay continues");
}

static void test_seq_two_buffer_replay_never_accepted(void)
{
    np_safety_state_t st;
    uint8_t i;
    int ok = 1;

    memset(&st, 0, sizeof(st));
    g_tick_ms = 1000U;
    (void)np_spi_watchdog_init();
    (void)beat(&st, 3U); (void)beat(&st, 4U);
    check(beat(&st, 5U), "ping-pong: live before the hang");
    for (i = 0U; i < 40U; i++) {                 /* 4, 5, 4, 5, ... */
        g_tick_ms += 200U;
        if (beat(&st, (uint8_t)(4U + (i & 1U)))) { ok = 0; }
    }
    check(ok, "ping-pong: a two-buffer replay is never accepted (steps +7, +1)");
    check(st.granted_mask == 0U, "ping-pong: the watchdog has cut everything");
}

static void test_seq_lost_frames_and_steps(void)
{
    g_tick_ms = 1000U;
    (void)np_spi_watchdog_init();
    (void)np_spi_watchdog_seq_accept(0U); (void)np_spi_watchdog_seq_accept(1U);
    check(np_spi_watchdog_seq_accept(2U), "steps: live run");
    check(np_spi_watchdog_seq_accept(4U), "steps: one lost frame (+2) stays accepted");
    check(np_spi_watchdog_seq_accept(7U), "steps: two lost frames (+3) stay accepted");
    check(!np_spi_watchdog_seq_accept(3U), "steps: three lost frames (+4) restart the run");
    check(!np_spi_watchdog_seq_accept(4U), "steps: ... one step after the restart is not enough");
    check(np_spi_watchdog_seq_accept(5U), "steps: ... two are");
    check(!np_spi_watchdog_seq_accept(4U), "steps: a backward step (+7) restarts the run");
    check(!np_spi_watchdog_seq_accept(4U), "steps: a repeat (+0) restarts the run");
    (void)np_spi_watchdog_seq_accept(5U);
    check(np_spi_watchdog_seq_accept(6U), "steps: a hub reset recovers after two forward steps");
    (void)np_spi_watchdog_init();
    check(!np_spi_watchdog_seq_accept(7U), "steps: init forgets the previous counter");
    /* After init no counter is known, so the first frame is no step at all:
     * 1 then 2 is one forward step, not two, whatever init left in last. */
    (void)np_spi_watchdog_init();
    (void)np_spi_watchdog_seq_accept(1U);
    check(!np_spi_watchdog_seq_accept(2U),
          "steps: the first frame after init starts a run, it does not extend one");
}

/* ══ 7. tick liveness: SysTick against TIM2 (FMEA-M02-02, OI-FMEA-12 (b)) ═══ */

static void live_init(np_safety_state_t *st, uint32_t ms, uint32_t us)
{
    memset(st, 0, sizeof(*st));
    st->fault_slot = NP_FAULT_SLOT_NONE;
    g_tick_ms = ms;
    g_tim2_us = us;
    (void)np_spi_watchdog_init();
    st->requested_mask = NP_SAFETY_EN_ALL_MASK;
    st->granted_mask   = NP_SAFETY_EN_ALL_MASK;
}

static bool tick_faulted(const np_safety_state_t *st)
{
    return st->granted_mask == 0U &&
           (st->status & (NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF)) ==
               (NP_SAFETY_STATUS_FAULT | NP_SAFETY_STATUS_CUTOFF) &&
           st->fault_slot == NP_FAULT_SLOT_TICK;
}

static void test_liveness_agreeing_clocks(void)
{
    np_safety_state_t st;
    uint32_t i;
    int ok = 1;

    /* TIM2 starts 1 s before its 32-bit wrap, so the run crosses it. */
    live_init(&st, 50000U, 0xFFFFFFFFU - 1000000U);
    for (i = 0U; i < 5000U; i++) {               /* 5 s in 1 ms loop steps */
        g_tick_ms += 1U;
        g_tim2_us += 1000U;
        np_spi_watchdog_tick_liveness(&st);
        if (st.granted_mask != NP_SAFETY_EN_ALL_MASK) { ok = 0; }
    }
    check(ok, "liveness: agreeing clocks never fault, across the TIM2 wrap");

    /* A 40 ms loop stall (flash erase) advances both alike. */
    g_tick_ms += 40U;
    g_tim2_us += 40000U;
    np_spi_watchdog_tick_liveness(&st);
    check(st.granted_mask == NP_SAFETY_EN_ALL_MASK, "liveness: a long iteration is not a fault");
}

static void test_liveness_frozen_systick(void)
{
    np_safety_state_t st;

    live_init(&st, 1000U, 0U);
    g_tim2_us = NP_SAFETY_TICK_CHECK_MS * 1000U - 1U;
    np_spi_watchdog_tick_liveness(&st);
    check(st.granted_mask == NP_SAFETY_EN_ALL_MASK,
          "frozen SysTick: nothing before a full window of TIM2");
    g_tim2_us = NP_SAFETY_TICK_CHECK_MS * 1000U;
    np_spi_watchdog_tick_liveness(&st);
    check(tick_faulted(&st), "frozen SysTick: FAULT + CUTOFF + TICK slot after one window");
}

static void test_liveness_frozen_tim2(void)
{
    np_safety_state_t st;

    live_init(&st, 1000U, 777U);
    g_tick_ms = 1000U + NP_SAFETY_TICK_CHECK_MS;
    np_spi_watchdog_tick_liveness(&st);
    check(tick_faulted(&st), "frozen TIM2: FAULT after one window of SysTick");
}

static void test_liveness_tolerance_edges(void)
{
    np_safety_state_t st;

    /* SysTick slow by exactly the tolerance: accepted. */
    live_init(&st, 0U, 0U);
    g_tim2_us = NP_SAFETY_TICK_CHECK_MS * 1000U;
    g_tick_ms = NP_SAFETY_TICK_CHECK_MS - NP_SAFETY_TICK_TOL_MS;
    np_spi_watchdog_tick_liveness(&st);
    check(st.granted_mask == NP_SAFETY_EN_ALL_MASK, "slow SysTick: exactly the tolerance passes");

    /* One more millisecond slow: fault. */
    live_init(&st, 0U, 0U);
    g_tim2_us = NP_SAFETY_TICK_CHECK_MS * 1000U;
    g_tick_ms = NP_SAFETY_TICK_CHECK_MS - NP_SAFETY_TICK_TOL_MS - 1U;
    np_spi_watchdog_tick_liveness(&st);
    check(tick_faulted(&st), "slow SysTick: tolerance + 1 ms faults");

    /* SysTick fast by tolerance + 1: fault (a fast tick shortens the lockout). */
    live_init(&st, 0U, 0U);
    g_tick_ms = NP_SAFETY_TICK_CHECK_MS + NP_SAFETY_TICK_TOL_MS + 1U;
    g_tim2_us = NP_SAFETY_TICK_CHECK_MS * 1000U;
    np_spi_watchdog_tick_liveness(&st);
    check(tick_faulted(&st), "fast SysTick: tolerance + 1 ms faults");

    /* Fast by exactly the tolerance: accepted. */
    live_init(&st, 0U, 0U);
    g_tick_ms = NP_SAFETY_TICK_CHECK_MS + NP_SAFETY_TICK_TOL_MS;
    g_tim2_us = NP_SAFETY_TICK_CHECK_MS * 1000U;
    np_spi_watchdog_tick_liveness(&st);
    check(st.granted_mask == NP_SAFETY_EN_ALL_MASK, "fast SysTick: exactly the tolerance passes");
}

static void test_liveness_is_a_rate(void)
{
    np_safety_state_t st;
    uint32_t i;
    int ok = 1;

    /* SysTick 5 % slow for 10 s: inside the 10 % tolerance in every window.
     * The check re-references each window, so the error does not accumulate
     * (10 s would otherwise be 500 ms apart). */
    live_init(&st, 0U, 0U);
    for (i = 1U; i <= 10000U; i++) {
        g_tim2_us += 1000U;
        g_tick_ms  = (i * 95U) / 100U;
        np_spi_watchdog_tick_liveness(&st);
        if (st.granted_mask != NP_SAFETY_EN_ALL_MASK) { ok = 0; }
    }
    check(ok, "rate: a 5 % slow SysTick never faults over 10 s (tolerance is per window)");
}

static void test_liveness_latch(void)
{
    np_safety_state_t st;

    live_init(&st, 1000U, 0U);
    g_tim2_us = NP_SAFETY_TICK_CHECK_MS * 1000U;
    np_spi_watchdog_tick_liveness(&st);                 /* SysTick frozen: fault */
    check(tick_faulted(&st), "latch: faulted");

    /* SysTick recovers, and another module clears FAULT and re-grants. */
    g_tick_ms += 5000U;
    g_tim2_us += 5000000U;
    st.status       = NP_SAFETY_STATUS_OK;
    st.fault_slot   = NP_FAULT_SLOT_SIG_FAIL;
    st.granted_mask = NP_SAFETY_EN_ALL_MASK;
    np_spi_watchdog_tick_liveness(&st);
    check(tick_faulted(&st), "latch: re-asserted every call, even after the clocks agree again");

    np_spi_watchdog_tick(&st, NULL, NULL);
    check(st.granted_mask == 0U, "latch: a heartbeat tick grants nothing while latched");

    live_init(&st, 1000U, 0U);
    g_tick_ms += NP_SAFETY_TICK_CHECK_MS;
    g_tim2_us += NP_SAFETY_TICK_CHECK_MS * 1000U;
    np_spi_watchdog_tick_liveness(&st);
    check(st.granted_mask == NP_SAFETY_EN_ALL_MASK, "latch: only init (a reset) clears it");
}

int main(void)
{
    test_init_disables_all();
    test_apply_one_hot_mapping();
    test_apply_zero_and_all();
    test_apply_rewrites_every_line_every_call();
    test_tick_overwrites_granted_mask();
    test_heartbeat_loss_cuts_all();
    test_heartbeat_timeout_across_tick_wrap();
    test_seq_startup_and_run();
    test_seq_stuck_buffer_trips_watchdog();
    test_seq_two_buffer_replay_never_accepted();
    test_seq_lost_frames_and_steps();
    test_liveness_agreeing_clocks();
    test_liveness_frozen_systick();
    test_liveness_frozen_tim2();
    test_liveness_tolerance_edges();
    test_liveness_is_a_rate();
    test_liveness_latch();

    printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "OK", g_failures);
    return g_failures ? 1 : 0;
}
