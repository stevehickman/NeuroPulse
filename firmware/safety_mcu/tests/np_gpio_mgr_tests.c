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

int main(void)
{
    test_init_disables_all();
    test_apply_one_hot_mapping();
    test_apply_zero_and_all();
    test_apply_rewrites_every_line_every_call();
    test_tick_overwrites_granted_mask();
    test_heartbeat_loss_cuts_all();
    test_heartbeat_timeout_across_tick_wrap();

    printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "OK", g_failures);
    return g_failures ? 1 : 0;
}
