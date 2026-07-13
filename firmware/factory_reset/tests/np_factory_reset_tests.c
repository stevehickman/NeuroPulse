/* Document: NP-FW-EMMC-002 Rev A §B */
/*
 * NeurOne Device Factory Reset — Host Smoke + Failure-Path Test
 * Build: -DNPTEST_HOST (host execution, not cross-compiled)
 *
 * This is NOT hardware verification.  It confirms that the SNVS read path and
 * the R-5..R-10 logic run on the host without crashing, AND that the
 * abort-safety invariant holds under injected HAL failures:
 *
 *   On ANY failed reset step, reset_in_progress (SNVS_LPGPR1 bit 0) must stay
 *   SET so the next boot re-runs the data wipe — the device must never come up
 *   with user data half-erased.
 *
 * It also verifies bounded-retry behaviour (an always-failing retried step is
 * called exactly NP_FR_HAL_MAX_RETRIES times, never in an unbounded loop) and
 * retry recovery (a transient fault that clears within the retry budget lets
 * the reset complete and clear the flag).
 *
 * Full hardware verification (real eMMC SANITIZE timing, TRNG entropy, live
 * power-loss interruption) is bench work on the i.MX RT1062 target.
 */

#include "np_factory_reset.h"
#include "np_factory_reset_test_hal.h"

#include <stdint.h>
#include <stdio.h>

/* Host-side backing for SNVS_LPGPR1 (see np_factory_reset_config.h). */
volatile uint32_t np_fr_host_snvs_lpgpr1 = 0U;

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

/* Reset both the SNVS flag and the HAL control state to a clean pre-reset
 * baseline before each test.  The flag starts clear; np_factory_reset_execute()
 * sets it at R-3. */
static void begin_case(void)
{
    np_fr_host_snvs_lpgpr1 = 0U;
    np_fr_host_hal_reset();
}

int main(void)
{
    /* ── Happy path ──────────────────────────────────────────────────────── */

    /* 1: SNVS read does not crash and reports "not in progress" initially. */
    begin_case();
    check(np_factory_reset_is_in_progress() == false,
          "is_in_progress false when flag clear");

    /* 2: A set flag is reported as in-progress. */
    np_fr_host_snvs_lpgpr1 = NP_FR_RESET_IN_PROGRESS;
    check(np_factory_reset_is_in_progress() == true,
          "is_in_progress true when flag set");

    /* 3: Full execute() returns OK and clears the flag (host reboot is no-op). */
    begin_case();
    check(np_factory_reset_execute() == NP_RESET_OK,
          "execute returns NP_RESET_OK");
    check(np_factory_reset_is_in_progress() == false,
          "execute clears reset_in_progress (R-11)");

    /* 4: Resume path on a set flag completes and clears it. */
    begin_case();
    np_fr_host_snvs_lpgpr1 = NP_FR_RESET_IN_PROGRESS;
    np_factory_reset_resume_after_powerloss();
    check(np_factory_reset_is_in_progress() == false,
          "resume_after_powerloss clears flag on success");

    /* ── Abort-safety: flag STAYS SET on any HAL failure ─────────────────── */

    /* 5: R-5 SANITIZE UHDR always fails → ERR_SANITIZE, flag stays set. */
    begin_case();
    np_fr_host_hal.fail_sanitize_uhdr = -1;   /* always fail */
    check(np_factory_reset_execute() == NP_RESET_ERR_SANITIZE,
          "execute returns ERR_SANITIZE when R-5 fails");
    check(np_factory_reset_is_in_progress() == true,
          "flag STAYS SET when R-5 (UHDR SANITIZE) fails");

    /* 6: R-5 always fails → retried exactly NP_FR_HAL_MAX_RETRIES times (bounded). */
    check(np_fr_host_hal.calls_sanitize_uhdr == NP_FR_HAL_MAX_RETRIES,
          "R-5 bounded retry: called exactly NP_FR_HAL_MAX_RETRIES times");

    /* 7: R-6 zero SHDR always fails → ERR_SANITIZE, flag stays set. */
    begin_case();
    np_fr_host_hal.fail_zero_shdr = -1;
    check(np_factory_reset_execute() == NP_RESET_ERR_SANITIZE,
          "execute returns ERR_SANITIZE when R-6 fails");
    check(np_factory_reset_is_in_progress() == true,
          "flag STAYS SET when R-6 (SHDR zero) fails");

    /* 8: R-7 zero Config always fails → ERR_SANITIZE, flag stays set. */
    begin_case();
    np_fr_host_hal.fail_zero_config = -1;
    check(np_factory_reset_execute() == NP_RESET_ERR_SANITIZE,
          "execute returns ERR_SANITIZE when R-7 fails");
    check(np_factory_reset_is_in_progress() == true,
          "flag STAYS SET when R-7 (Config zero) fails");

    /* 9: R-8 TRNG salt (first TRNG call) fails → ERR_TRNG, flag stays set. */
    begin_case();
    np_fr_host_hal.trng_fail_on_call = 1;
    check(np_factory_reset_execute() == NP_RESET_ERR_TRNG,
          "execute returns ERR_TRNG when R-8 (salt) fails");
    check(np_factory_reset_is_in_progress() == true,
          "flag STAYS SET when R-8 (TRNG salt) fails");

    /* 10: R-9 TRNG token (second TRNG call) fails → ERR_TRNG, flag stays set. */
    begin_case();
    np_fr_host_hal.trng_fail_on_call = 2;
    check(np_factory_reset_execute() == NP_RESET_ERR_TRNG,
          "execute returns ERR_TRNG when R-9 (token) fails");
    check(np_factory_reset_is_in_progress() == true,
          "flag STAYS SET when R-9 (TRNG token) fails");
    check(np_fr_host_hal.calls_trng == 2U,
          "R-9 failure means both TRNG calls ran (salt ok, token fail)");

    /* 11: R-10 config-defaults write always fails → ERR_CONFIG, flag stays set. */
    begin_case();
    np_fr_host_hal.fail_write_config = -1;
    check(np_factory_reset_execute() == NP_RESET_ERR_CONFIG,
          "execute returns ERR_CONFIG when R-10 fails");
    check(np_factory_reset_is_in_progress() == true,
          "flag STAYS SET when R-10 (Config write) fails");

    /* ── Retry recovery: transient fault within budget completes the reset ── */

    /* 12: R-5 fails twice then succeeds → execute completes, flag cleared. */
    begin_case();
    np_fr_host_hal.fail_sanitize_uhdr = 2;    /* fail twice, succeed on 3rd */
    check(np_factory_reset_execute() == NP_RESET_OK,
          "execute recovers when R-5 fails twice then succeeds");
    check(np_factory_reset_is_in_progress() == false,
          "flag cleared after retry recovery");
    check(np_fr_host_hal.calls_sanitize_uhdr == NP_FR_HAL_MAX_RETRIES,
          "R-5 retry recovery: called exactly NP_FR_HAL_MAX_RETRIES times");

    /* ── Resume path honours the same invariant ──────────────────────────── */

    /* 13: Resume with an always-failing step leaves the flag SET (next boot retries). */
    begin_case();
    np_fr_host_snvs_lpgpr1 = NP_FR_RESET_IN_PROGRESS;
    np_fr_host_hal.fail_sanitize_uhdr = -1;
    np_factory_reset_resume_after_powerloss();
    check(np_factory_reset_is_in_progress() == true,
          "resume leaves flag SET when wipe fails (next boot retries)");

    /* 14: Resume with a transient fault that clears within budget clears the flag. */
    begin_case();
    np_fr_host_snvs_lpgpr1 = NP_FR_RESET_IN_PROGRESS;
    np_fr_host_hal.fail_zero_shdr = 1;         /* fail once, then succeed */
    np_factory_reset_resume_after_powerloss();
    check(np_factory_reset_is_in_progress() == false,
          "resume clears flag when transient fault recovers");

    printf("\n%s (%d failure(s))\n",
           g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
           g_failures);
    return g_failures == 0 ? 0 : 1;
}
