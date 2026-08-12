/* Document: NP-FW-EMMC-002 Rev 1 §B */
/*
 * NeurOne Device Factory Reset — Host Test HAL Control (HOST TEST ONLY)
 *
 * This header exists ONLY to let host smoke tests force each platform HAL
 * step (SANITIZE, zero, TRNG, config write) to fail on demand, so the
 * abort-safety invariant can be verified: on any HAL failure the reset must
 * leave SNVS_LPGPR1 bit 0 (reset_in_progress) SET, so the next boot re-runs
 * the data wipe and the device never comes up half-erased.
 *
 * The entire file is compiled out unless NPTEST_HOST is defined.  It has no
 * presence in target firmware — on target the real board-layer HAL is linked
 * and this control state does not exist.
 */

#ifndef NP_FACTORY_RESET_TEST_HAL_H
#define NP_FACTORY_RESET_TEST_HAL_H

#ifdef NPTEST_HOST

#include "np_factory_reset_types.h"

/*
 * Per-step failure controls and invocation counters for the host HAL stubs.
 *
 * fail_* semantics:
 *    0  → never fail (default; step returns NP_RESET_OK)
 *   >0  → fail this many times, then succeed (models transient faults +
 *         bounded-retry recovery)
 *   <0  → always fail (models a hard fault; bounded retry gives up)
 *
 * trng_fail_on_call: 1-based index of the TRNG call that should fail; 0 =
 *   never.  R-8 (salt) is call 1, R-9 (token) is call 2 — this lets each be
 *   failed independently.
 *
 * calls_*: incremented on every invocation so tests can assert bounded retry
 *   (e.g. an always-failing SANITIZE is called exactly NP_FR_HAL_MAX_RETRIES
 *   times, never in an unbounded loop).
 */
typedef struct {
    int      fail_sanitize_uhdr;
    int      fail_zero_shdr;
    int      fail_zero_config;
    int      fail_write_config;
    int      trng_fail_on_call;

    uint32_t calls_sanitize_uhdr;
    uint32_t calls_zero_shdr;
    uint32_t calls_zero_config;
    uint32_t calls_write_config;
    uint32_t calls_trng;
} np_fr_host_hal_control_t;

/* Single definition lives in np_factory_reset.c under NPTEST_HOST. */
extern np_fr_host_hal_control_t np_fr_host_hal;

/* Reset all controls and counters to the all-OK default. */
static inline void np_fr_host_hal_reset(void)
{
    np_fr_host_hal_control_t zero = {0};
    np_fr_host_hal = zero;
}

#endif /* NPTEST_HOST */

#endif /* NP_FACTORY_RESET_TEST_HAL_H */
