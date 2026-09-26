/*
 * NeurOne Safety MCU — Impedance Cross-Validation Report Host Tests
 * Document: NP-SW-001 §5.1 (SW01-M06), OI-CVNS-HUB-11
 *
 * Host-native tests for the per-electrode cervical VNS impedance report the
 * safety MCU emits in the spare bytes of the heartbeat reply window
 * (np_impedance_check_build_cvns_report).  The hub cross-validates this against
 * its own per-electrode measurement (OI-CVNS-HUB-09).
 *
 * Also the enable gate (np_impedance_check_gate, NP-FMEA-001 FMEA-M05-07,
 * OI-FMEA-12 (d)): a channel is withheld until its check passes this session.
 *
 * These tests exercise np_impedance_check.c with mocked HAL stubs; they do NOT
 * require ARM cross-compilation or SPI hardware.
 *
 * IEC 62304 Class C — SW-01 safety MCU.
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
extern np_safe_status_t np_impedance_check_init(void);
extern void             np_impedance_check_request(uint16_t requested_mask);
extern void             np_impedance_check_poll(np_safety_state_t *state);
extern bool             np_impedance_check_build_cvns_report(np_safety_imp_report_t *out);
extern void             np_impedance_check_reset_session(void);
extern void             np_impedance_check_gate(np_safety_state_t *state);

/* ── Mocked HAL stubs ──────────────────────────────────────────────────────────
 * These DEFINE symbols DECLARED in np_safety_hal.h (included above), which is
 * the point: a signature that drifts from the production contract is now a
 * compile error here rather than a silently mislinked call (OI-SWCI-18).     */
static bool     g_result_ready[16];
static uint32_t g_read_ohm[16];
static uint32_t g_electrode_ohm[NP_SAFETY_IMP_CVNS_ELECTRODES];

uint32_t np_hal_get_tick_ms(void) { return 0U; }
static unsigned g_starts[16];
void     np_hal_impedance_start_test(uint8_t channel)
{
    if (channel < 16U) { g_starts[channel]++; }
}
bool     np_hal_impedance_result_ready(uint8_t channel)
{
    return (channel < 16U) ? g_result_ready[channel] : false;
}
uint32_t np_hal_impedance_read_ohm(uint8_t channel)
{
    return (channel < 16U) ? g_read_ohm[channel] : 0U;
}
uint32_t np_hal_impedance_read_cvns_electrode_ohm(uint8_t electrode)
{
    return (electrode < NP_SAFETY_IMP_CVNS_ELECTRODES) ? g_electrode_ohm[electrode] : 0U;
}

/* ── Harness ─────────────────────────────────────────────────────────────────── */
static int g_failures = 0;
static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

/* CVNS is impedance-check index 3 → HAL channel index 3 (see np_impedance_check.c). */
#define CVNS_CH 3U

static void reset_mocks(void)
{
    memset(g_result_ready, 0, sizeof(g_result_ready));
    memset(g_read_ohm, 0, sizeof(g_read_ohm));
    memset(g_electrode_ohm, 0, sizeof(g_electrode_ohm));
    memset(g_starts, 0, sizeof(g_starts));
}

static uint16_t report_checksum(const np_safety_imp_report_t *r)
{
    uint16_t sum = 0U;
    const uint8_t *b = (const uint8_t *)r;
    for (uint8_t i = 0U; i < (NP_SAFETY_IMP_REPORT_LEN - 2U); i++) { sum += b[i]; }
    return sum;
}

/* ── Tests ───────────────────────────────────────────────────────────────────── */

static void test_report_invalid_before_measurement(void)
{
    reset_mocks();
    np_impedance_check_init();

    np_safety_imp_report_t rep;
    memset(&rep, 0xAB, sizeof(rep));   /* poison to confirm the builder zeroes it */
    check(!np_impedance_check_build_cvns_report(&rep),
          "report: invalid before any CVNS measurement");
    check(rep.magic == 0U, "report: magic cleared when unpopulated");
}

static void test_report_valid_after_cvns_poll(void)
{
    reset_mocks();
    np_impedance_check_init();

    np_safety_state_t state;
    memset(&state, 0, sizeof(state));
    state.fault_slot   = 0xFFU;
    state.granted_mask = NP_SAFETY_EN_CVNS;

    /* Request CVNS impedance check, then make its result ready. */
    np_impedance_check_request(NP_SAFETY_EN_CVNS);
    g_result_ready[CVNS_CH] = true;
    g_read_ohm[CVNS_CH]     = 2000U;    /* in-window aggregate (< 10 kΩ) */
    g_electrode_ohm[0]      = 2100U;    /* left  → 2.10 kΩ → 210 (×100)  */
    g_electrode_ohm[1]      = 3450U;    /* right → 3.45 kΩ → 345 (×100)  */
    np_impedance_check_poll(&state);

    np_safety_imp_report_t rep;
    check(np_impedance_check_build_cvns_report(&rep),
          "report: valid after CVNS poll completes");
    check(rep.magic == NP_SAFETY_IMP_REPORT_MAGIC, "report: magic set");
    check((rep.flags & NP_SAFETY_IMP_FLAG_CVNS_VALID) != 0U, "report: valid flag set");
    check(rep.cvns_kohm_x100[0] == 210U, "report: left electrode 2100 Ω → 210 (kΩ×100)");
    check(rep.cvns_kohm_x100[1] == 345U, "report: right electrode 3450 Ω → 345 (kΩ×100)");
    check(rep.checksum == report_checksum(&rep), "report: checksum matches contents");

    /* The additive-current enable gate is unchanged: CVNS stayed granted. */
    check((state.granted_mask & NP_SAFETY_EN_CVNS) != 0U,
          "report: enable gate unaffected (in-window aggregate stays granted)");
}

static void test_report_rounds_and_survives_gate_fail(void)
{
    reset_mocks();
    np_impedance_check_init();

    np_safety_state_t state;
    memset(&state, 0, sizeof(state));
    state.fault_slot   = 0xFFU;
    state.granted_mask = NP_SAFETY_EN_CVNS;

    np_impedance_check_request(NP_SAFETY_EN_CVNS);
    g_result_ready[CVNS_CH] = true;
    g_read_ohm[CVNS_CH]     = 12000U;   /* aggregate > 10 kΩ → gate FAILS  */
    g_electrode_ohm[0]      = 1055U;    /* 1.055 kΩ → rounds to 106 (×100) */
    g_electrode_ohm[1]      = 4004U;    /* 4.004 kΩ → rounds to 400 (×100) */
    np_impedance_check_poll(&state);

    /* Gate failed → CVNS dropped from granted_mask and IMPEDANCE flagged. */
    check((state.granted_mask & NP_SAFETY_EN_CVNS) == 0U,
          "report: over-limit aggregate drops CVNS enable (gate unchanged)");
    check((state.status & NP_SAFETY_STATUS_IMPEDANCE) != 0U,
          "report: over-limit sets IMPEDANCE status");

    /* Report is still built (so the hub can cross-check even a marginal reading). */
    np_safety_imp_report_t rep;
    check(np_impedance_check_build_cvns_report(&rep),
          "report: still produced on a failed gate");
    check(rep.cvns_kohm_x100[0] == 106U, "report: 1055 Ω rounds to 106 (kΩ×100)");
    check(rep.cvns_kohm_x100[1] == 400U, "report: 4004 Ω rounds to 400 (kΩ×100)");
}

static void test_report_reinvalidated_on_new_request(void)
{
    reset_mocks();
    np_impedance_check_init();

    np_safety_state_t state;
    memset(&state, 0, sizeof(state));
    state.fault_slot   = 0xFFU;
    state.granted_mask = NP_SAFETY_EN_CVNS;

    /* First measurement completes → report valid. */
    np_impedance_check_request(NP_SAFETY_EN_CVNS);
    g_result_ready[CVNS_CH] = true;
    g_read_ohm[CVNS_CH]     = 2000U;
    g_electrode_ohm[0]      = 2000U;
    g_electrode_ohm[1]      = 2000U;
    np_impedance_check_poll(&state);
    np_safety_imp_report_t rep;
    check(np_impedance_check_build_cvns_report(&rep), "report: valid after first poll");

    /* A fresh CVNS request (new session) must invalidate the stale report until
     * the new measurement completes. */
    np_impedance_check_request(NP_SAFETY_EN_CVNS);
    check(!np_impedance_check_build_cvns_report(&rep),
          "report: invalidated on a fresh CVNS request until re-measured");
}

/* ══ Enable gate (FMEA-M05-07, OI-FMEA-12 (d)) ═════════════════════════════ */

#define TDCS_CH 1U            /* impedance-check index of tDCS */
#define GOOD_OHM 1000U
#define BAD_OHM  (NP_IMPEDANCE_MAX_OHM + 1U)

/* One main-loop iteration as np_safety_main.c orders it: the heartbeat grant
 * (granted = requested, as np_spi_watchdog_tick does with no fault), then the
 * poll, then the gate.  Returns the mask the GPIO write would see. */
static uint16_t iterate(np_safety_state_t *st)
{
    st->granted_mask = st->requested_mask;
    np_impedance_check_poll(st);
    np_impedance_check_gate(st);
    return st->granted_mask;
}

static void gate_session(np_safety_state_t *st, uint16_t mask)
{
    reset_mocks();
    (void)np_impedance_check_init();
    memset(st, 0, sizeof(*st));
    st->requested_mask = mask;
    np_impedance_check_reset_session();
    np_impedance_check_request(mask);
}

static void test_gate_pending_then_pass(void)
{
    np_safety_state_t st;
    unsigned i;
    int ok = 1;

    gate_session(&st, NP_SAFETY_EN_TDCS | NP_SAFETY_EN_PBM_CRANIAL | NP_SAFETY_EN_VISUAL);
    for (i = 0U; i < 50U; i++) {
        if ((iterate(&st) & NP_SAFETY_EN_TDCS) != 0U) { ok = 0; }
    }
    check(ok, "gate: withheld while the check is pending (a result that never comes)");
    check((st.granted_mask & (NP_SAFETY_EN_PBM_CRANIAL | NP_SAFETY_EN_VISUAL)) ==
              (NP_SAFETY_EN_PBM_CRANIAL | NP_SAFETY_EN_VISUAL),
          "gate: channels with no impedance check are untouched");
    check(g_starts[TDCS_CH] == 1U, "gate: a pending check is not restarted");
    check(g_starts[0] == 0U && g_starts[2] == 0U && g_starts[CVNS_CH] == 0U,
          "gate: no check is started for a channel nobody requested");

    g_read_ohm[TDCS_CH] = GOOD_OHM; g_result_ready[TDCS_CH] = true;
    check((iterate(&st) & NP_SAFETY_EN_TDCS) != 0U, "gate: granted in the iteration it passes");
    check((iterate(&st) & NP_SAFETY_EN_TDCS) != 0U, "gate: and stays granted");
}

static void test_gate_fail_is_not_regranted(void)
{
    np_safety_state_t st;
    unsigned i;
    int ok = 1;

    gate_session(&st, NP_SAFETY_EN_TDCS);
    g_read_ohm[TDCS_CH] = BAD_OHM; g_result_ready[TDCS_CH] = true;
    (void)iterate(&st);
    check((st.status & NP_SAFETY_STATUS_IMPEDANCE) != 0U, "fail: IMPEDANCE status set");
    for (i = 0U; i < 50U; i++) {           /* 50 heartbeats that each re-grant */
        if ((iterate(&st) & NP_SAFETY_EN_TDCS) != 0U) { ok = 0; }
    }
    check(ok, "fail: withheld on every later heartbeat (the pre-fix code re-granted)");
    check(g_starts[TDCS_CH] == 1U, "fail: a failed channel is not auto-retried");

    /* A new request (the next session, or CVNS re-enable) is the only retry. */
    g_result_ready[TDCS_CH] = false;
    np_impedance_check_request(NP_SAFETY_EN_TDCS);
    check((iterate(&st) & NP_SAFETY_EN_TDCS) == 0U, "fail: re-request withholds until its result");
    g_read_ohm[TDCS_CH] = GOOD_OHM; g_result_ready[TDCS_CH] = true;
    check((iterate(&st) & NP_SAFETY_EN_TDCS) != 0U, "fail: a passing re-check grants");
}

static void test_gate_pass_does_not_carry_over(void)
{
    np_safety_state_t st;

    gate_session(&st, NP_SAFETY_EN_TDCS);
    g_read_ohm[TDCS_CH] = GOOD_OHM; g_result_ready[TDCS_CH] = true;
    check((iterate(&st) & NP_SAFETY_EN_TDCS) != 0U, "carry: passed in session 1");

    /* Session 2 starts WITHOUT tDCS requested; tDCS is added mid-session. */
    g_result_ready[TDCS_CH] = false;
    np_impedance_check_reset_session();
    np_impedance_check_request(0U);
    st.requested_mask = NP_SAFETY_EN_TDCS;
    check((iterate(&st) & NP_SAFETY_EN_TDCS) == 0U,
          "carry: session 1's pass does not enable tDCS in session 2");
    check(g_starts[TDCS_CH] == 2U, "carry: a mid-session first request starts a check");
    (void)iterate(&st);
    check(g_starts[TDCS_CH] == 2U, "carry: ... once");
    g_result_ready[TDCS_CH] = true;
    check((iterate(&st) & NP_SAFETY_EN_TDCS) != 0U, "carry: granted once that check passes");
}

static void test_gate_cvns_reenable_rechecks(void)
{
    np_safety_state_t st;

    gate_session(&st, NP_SAFETY_EN_CVNS);
    g_read_ohm[CVNS_CH] = GOOD_OHM; g_result_ready[CVNS_CH] = true;
    check((iterate(&st) & NP_SAFETY_EN_CVNS) != 0U, "cvns: passed");
    g_result_ready[CVNS_CH] = false;
    np_impedance_check_request(NP_SAFETY_EN_CVNS);      /* cardiac re-enable */
    check((iterate(&st) & NP_SAFETY_EN_CVNS) == 0U,
          "cvns: the re-enable's repeat impedance check withholds CVNS until it passes");
}

int main(void)
{
    test_report_invalid_before_measurement();
    test_report_valid_after_cvns_poll();
    test_report_rounds_and_survives_gate_fail();
    test_report_reinvalidated_on_new_request();
    test_gate_pending_then_pass();
    test_gate_fail_is_not_regranted();
    test_gate_pass_does_not_carry_over();
    test_gate_cvns_reenable_rechecks();

    if (g_failures == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
