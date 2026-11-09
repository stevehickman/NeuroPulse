/*
 * NeurOne Hub Control — Cross-Check Non-Finite Reading Host Tests
 * OI-SIMFW-29 / NP-FW-HUB-001 §8.3.1, REQ-FWHUB-41; NP-FMEA-001 §3.3 FMEA-M03-02
 *
 * REQ-FWHUB-41's cross-check counts a non-finite delivered-current reading
 * (NaN, +/-infinity: an absent or failed DAC) as an excess, because it cannot
 * show delivery is within bounds.  The driver wrote that as
 * `!isfinite(delivered) || fabsf(delivered) > bound + margin`.  The hub is built
 * with -ffast-math, which implies -ffinite-math-only: the compiler may assume no
 * NaN or infinity exists, so isfinite() is folded to true (arm-none-eabi-gcc
 * 13.2.1 emits no test for it) and the NaN outcome rests on whichever condition
 * code the compiler picked for the magnitude comparison.  GCC 13.2.1 picks one
 * that flags NaN; clang for Cortex-M7 picks one that does not (the run is reset
 * as in-bound).  The check was right on the shipped toolchain by accident.  It
 * now tests the float's exponent bits.
 *
 * THIS TARGET IS BUILT WITH THE DEVICE'S FLOATING-POINT FLAGS (-O2 -ffast-math
 * -fno-math-errno -ffreestanding; CMakeLists.txt), unlike np_stim_xcheck_tests
 * (-O0), so it is the build that can show the folding.  The guard below fails
 * the compile if the flags are lost.  On a host x86 gcc or clang the unfixed
 * driver happens to pass these tests too (the comparison there is unordered-safe);
 * the failing build is Cortex-M7 clang, run under qemu-system-arm (NP-FW-HUB-001
 * §8.3.1, OI-SIMFW-29).  What this test pins on every host is the behaviour.
 *
 * Non-finite values are built from bits: NAN and INFINITY are undefined under
 * -ffinite-math-only, and a compiler may constant-fold them away.
 *
 * No FreeRTOS, no hardware.  IEC 62304 Class B — SW-02 hub control.
 */
/* @verifies-file REQ-FWHUB-41 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../include/np_stim_xcheck.h"

#ifndef __FINITE_MATH_ONLY__
#error "np_stim_xcheck_nonfinite_tests must be built with -ffast-math, as the hub is"
#elif !__FINITE_MATH_ONLY__
#error "np_stim_xcheck_nonfinite_tests must be built with -ffinite-math-only (-ffast-math), as the hub is"
#endif

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

static float make_float(uint32_t bits)
{
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static np_telem_record_t rec(np_hub_mod_type_t type, float delivered_ua)
{
    np_telem_record_t r;
    memset(&r, 0, sizeof r);
    r.mod_type = type;
    r.data.stim.current_ua = delivered_ua;
    return r;
}

/* Observe the same reading n times, 1 s apart from t0; count flags raised. */
static int observe_n(np_hub_mod_type_t type, float delivered_ua,
                     uint32_t t0, unsigned n)
{
    np_telem_record_t r = rec(type, delivered_ua);
    int raised = 0;
    for (unsigned i = 0U; i < n; i++) {
        if (np_stim_xcheck_observe(&r, t0 + i * 1000U)) {
            raised++;
        }
    }
    return raised;
}

static float just_over(uint16_t commanded_ua)
{
    float margin = (float)commanded_ua * (float)NP_STIM_XCHECK_TOL_PCT / 100.0f;
    if (margin < (float)NP_STIM_XCHECK_FLOOR_UA) {
        margin = (float)NP_STIM_XCHECK_FLOOR_UA;
    }
    return (float)commanded_ua + margin + 1.0f;
}

static float just_under(uint16_t commanded_ua)
{
    return just_over(commanded_ua) - 2.0f;
}

typedef struct {
    uint32_t    bits;
    const char *name;
} nonfinite_case_t;

static const nonfinite_case_t NONFINITE_CASES[] = {
    { 0x7FC00000UL, "quiet NaN"            },
    { 0xFFC00000UL, "negative quiet NaN"   },
    { 0x7F800001UL, "signalling NaN"       },
    { 0x7F800000UL, "+infinity"            },
    { 0xFF800000UL, "-infinity"            },
};
#define NONFINITE_COUNT (sizeof(NONFINITE_CASES) / sizeof(NONFINITE_CASES[0]))

/* A non-finite read-back is an excess on both cross-checked channels: after
 * NP_STIM_XCHECK_CONSECUTIVE of them, exactly one flag, and the channel latches. */
/* @verifies REQ-FWHUB-41 */
static void test_a_non_finite_reading_is_flagged_on_both_channels(void)
{
    static const struct {
        np_hub_mod_type_t type;
        uint8_t           ch;
        const char       *label;
    } CHANNELS[] = {
        { NP_MOD_TDCS,    NP_SAFETY_CH_TDCS,    "tDCS"    },
        { NP_MOD_BES_TACS, NP_SAFETY_CH_BES_TACS, "BES/tACS" },
    };
    for (unsigned c = 0U; c < 2U; c++) {
        for (unsigned i = 0U; i < NONFINITE_COUNT; i++) {
            char msg[112];
            np_stim_xcheck_clear();
            np_stim_xcheck_commanded(CHANNELS[c].ch, 1000U, 0U);
            const int raised = observe_n(CHANNELS[c].type, make_float(NONFINITE_CASES[i].bits),
                                         0U, NP_STIM_XCHECK_CONSECUTIVE);
            snprintf(msg, sizeof msg, "%s, %s: flagged exactly once after the consecutive run",
                     CHANNELS[c].label, NONFINITE_CASES[i].name);
            check(raised == 1 && np_stim_xcheck_latched(CHANNELS[c].ch), msg);
        }
    }
}

/* The debounce is unchanged: one non-finite sample is not a flag, and an
 * in-bound sample between non-finite ones resets the run, as it does for a
 * numeric excess.  A non-finite reading also counts while a ramp-down hold
 * is running, and after a numeric excess in the same run. */
/* @verifies REQ-FWHUB-41 */
static void test_a_non_finite_reading_counts_like_any_excess(void)
{
    const float nan_v = make_float(0x7FC00000UL);

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1000U, 0U);
    check(observe_n(NP_MOD_TDCS, nan_v, 0U, NP_STIM_XCHECK_CONSECUTIVE - 1U) == 0 &&
          !np_stim_xcheck_latched(NP_SAFETY_CH_TDCS),
          "debounce: fewer than the consecutive run of non-finite readings does not flag");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1000U, 0U);
    int raised = observe_n(NP_MOD_TDCS, nan_v, 0U, NP_STIM_XCHECK_CONSECUTIVE - 1U);
    raised += observe_n(NP_MOD_TDCS, 1000.0f, 10000U, 1U);
    raised += observe_n(NP_MOD_TDCS, nan_v, 20000U, NP_STIM_XCHECK_CONSECUTIVE - 1U);
    check(raised == 0 && !np_stim_xcheck_latched(NP_SAFETY_CH_TDCS),
          "debounce: an in-bound reading between non-finite ones resets the run");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1000U, 0U);
    raised  = observe_n(NP_MOD_TDCS, just_over(1000U), 0U, 1U);
    raised += observe_n(NP_MOD_TDCS, nan_v, 1000U, NP_STIM_XCHECK_CONSECUTIVE - 1U);
    check(raised == 1, "debounce: a non-finite reading extends a run of numeric excesses");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 2000U, 0U);
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 500U, 30000U);
    check(observe_n(NP_MOD_TDCS, nan_v, 0U, NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "ramp-down hold: a non-finite reading is an excess against the held level too");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1000U, 0U);
    check(observe_n(NP_MOD_TDCS, nan_v, 0U, 10U) == 1,
          "once per session: a persistent non-finite reading raises one flag, not one per snapshot");
}

/* The fix refuses nothing it used to admit and admits nothing it used to flag. */
/* @verifies REQ-FWHUB-41 */
static void test_finite_readings_are_judged_as_before(void)
{
    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1500U, 0U);
    check(observe_n(NP_MOD_TDCS, 1500.0f, 0U, 10U) == 0, "finite: delivered == commanded is not flagged");
    check(observe_n(NP_MOD_TDCS, -1500.0f, 20000U, 10U) == 0, "finite: reverse polarity at the bound is not flagged");
    check(observe_n(NP_MOD_TDCS, just_under(1500U), 40000U, 10U) == 0,
          "finite: delivered at the edge of tolerance is not flagged");
    check(observe_n(NP_MOD_TDCS, 0.0f, 60000U, 10U) == 0, "finite: zero delivery is not flagged");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1500U, 0U);
    check(observe_n(NP_MOD_TDCS, just_over(1500U), 0U, NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "finite: delivered just over tolerance is flagged");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_TDCS, 1500U, 0U);
    check(observe_n(NP_MOD_TDCS, -just_over(1500U), 0U, NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "finite: reverse-polarity excess is flagged");

    np_stim_xcheck_clear();
    np_stim_xcheck_commanded(NP_SAFETY_CH_BES_TACS, 800U, 0U);
    check(observe_n(NP_MOD_BES_TACS, 1.0e9f, 0U, NP_STIM_XCHECK_CONSECUTIVE) == 1,
          "finite: a very large finite reading is flagged");
}

int main(void)
{
    test_a_non_finite_reading_is_flagged_on_both_channels();
    test_a_non_finite_reading_counts_like_any_excess();
    test_finite_readings_are_judged_as_before();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", g_failures);
    return 1;
}
