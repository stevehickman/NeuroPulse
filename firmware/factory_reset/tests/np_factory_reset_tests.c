/* Document: NP-FW-EMMC-002 Rev A §B */
/*
 * NeuroPulse Device Factory Reset — Host Smoke Test
 * Build: -DNPTEST_HOST (host execution, not cross-compiled)
 *
 * This is NOT hardware verification.  It confirms that the SNVS read path and
 * the R-5..R-10 logic run on the host without crashing, using the host-backed
 * LPGPR1 word and the no-op HAL stubs.  Full hardware verification (real
 * eMMC SANITIZE timing, TRNG entropy, power-loss interruption) is bench work
 * on the i.MX RT1062 target.
 */

#include "np_factory_reset.h"

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

int main(void)
{
    /* 1: SNVS read does not crash and reports "not in progress" initially. */
    np_fr_host_snvs_lpgpr1 = 0U;
    check(np_factory_reset_is_in_progress() == false,
          "is_in_progress false when flag clear");

    /* 2: A set flag is reported as in-progress. */
    np_fr_host_snvs_lpgpr1 = NP_FR_RESET_IN_PROGRESS;
    check(np_factory_reset_is_in_progress() == true,
          "is_in_progress true when flag set");

    /* 3: Full execute() returns OK and clears the flag (host reboot is no-op). */
    np_fr_host_snvs_lpgpr1 = 0U;
    check(np_factory_reset_execute() == NP_RESET_OK,
          "execute returns NP_RESET_OK");
    check(np_factory_reset_is_in_progress() == false,
          "execute clears reset_in_progress (R-11)");

    /* 4: Resume path on a set flag completes and clears it. */
    np_fr_host_snvs_lpgpr1 = NP_FR_RESET_IN_PROGRESS;
    np_factory_reset_resume_after_powerloss();
    check(np_factory_reset_is_in_progress() == false,
          "resume_after_powerloss clears flag on success");

    printf("\n%s (%d failure(s))\n",
           g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
           g_failures);
    return g_failures == 0 ? 0 : 1;
}
