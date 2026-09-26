/* Document: NP-FW-EMMC-002 Rev 1 §B */
/*
 * NeurOne Device Factory Reset — Implementation
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 *
 * 12-step sequence R-1..R-12 per NP-FW-EMMC-002 Rev 1 §B.3.  See
 * np_factory_reset.h for the step list and the power-loss-resume contract.
 *
 * No heap.  All buffers are stack-allocated and zeroed with memset_explicit()
 * after use so the new salt and warranty token never linger in SRAM.
 */

#include "np_factory_reset.h"

#include <string.h>

/* memset_explicit() is C23 (ISO/IEC 9899:2023).  This module compiles at
 * -std=c11, so on toolchains that do not yet expose it we provide a
 * non-elidable fallback through a volatile function pointer to memset.  The
 * volatile indirection prevents the compiler from optimising the zeroing
 * away as a dead store.  Source code always calls memset_explicit(). */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
/* memset_explicit provided by <string.h> */
#elif defined(__STDC_LIB_EXT1__)
/* memset_s available; expose it under the memset_explicit name. */
static void *np_fr_memset_explicit(void *dst, int ch, size_t len)
{
    (void)memset_s(dst, len, ch, len);
    return dst;
}
#define memset_explicit np_fr_memset_explicit
#else
static void *(*const volatile np_fr_memset_ptr)(void *, int, size_t) = memset;
static void *np_fr_memset_explicit(void *dst, int ch, size_t len)
{
    return np_fr_memset_ptr(dst, ch, len);
}
#define memset_explicit np_fr_memset_explicit
#endif

/* ── SNVS_LPGPR1 flag helpers ─────────────────────────────────────────────── */

static void np_fr_set_in_progress(void)
{
    NP_FR_SNVS_LPGPR1 |= NP_FR_RESET_IN_PROGRESS;   /* R-3 */
}

static void np_fr_clear_in_progress(void)
{
    NP_FR_SNVS_LPGPR1 &= ~NP_FR_RESET_IN_PROGRESS;  /* R-11 */
}

bool np_factory_reset_is_in_progress(void)
{
    return (NP_FR_SNVS_LPGPR1 & NP_FR_RESET_IN_PROGRESS) != 0UL;
}

/* ── Bounded-retry wrapper for HAL operations ─────────────────────────────── */
/* Retries a HAL call up to NP_FR_HAL_MAX_RETRIES times.  Returns the first
 * NP_RESET_OK, or the last error code if every attempt failed.  Bounded so a
 * persistent hardware fault can never spin forever. */
static np_reset_status_t np_fr_retry(np_reset_status_t (*op)(void))
{
    np_reset_status_t st = NP_RESET_ERR_SANITIZE;   /* default if loop body never assigns */
    for (uint32_t attempt = 0U; attempt < NP_FR_HAL_MAX_RETRIES; ++attempt) {
        st = op();
        if (st == NP_RESET_OK) {
            return NP_RESET_OK;
        }
    }
    return st;
}

/* ── R-5..R-10: idempotent data-wipe + re-derivation core ─────────────────── */
/* Shared by np_factory_reset_execute() (forward path) and
 * np_factory_reset_resume_after_powerloss() (recovery path).  Every step here
 * is idempotent, so re-running after an interrupted reset is safe.
 *
 * On success the new salt and warranty token are written to Config (R-10) and
 * both stack buffers are zeroed before return.  On any failure the buffers are
 * still zeroed and the corresponding error code is returned. */
static np_reset_status_t np_fr_wipe_and_rederive(void)
{
    np_reset_status_t st;

    /* R-5: SANITIZE UHDR (NIST SP 800-88 Purge). */
    st = np_fr_retry(np_factory_reset_hal_sanitize_uhdr);
    if (st != NP_RESET_OK) {
        return NP_RESET_ERR_SANITIZE;
    }

    /* R-6: Zero SHDR. */
    st = np_fr_retry(np_factory_reset_hal_zero_shdr);
    if (st != NP_RESET_OK) {
        return NP_RESET_ERR_SANITIZE;
    }

    /* R-7: Zero Config. */
    st = np_fr_retry(np_factory_reset_hal_zero_config);
    if (st != NP_RESET_OK) {
        return NP_RESET_ERR_SANITIZE;
    }

    /* R-8 / R-9: generate new TRNG salt and warranty token. */
    uint8_t new_salt[NP_FR_TRNG_SALT_LEN];
    uint8_t new_token[NP_FR_WARRANTY_TOKEN_LEN];

    st = np_factory_reset_hal_trng_generate(new_salt, sizeof(new_salt));   /* R-8 */
    if (st != NP_RESET_OK) {
        memset_explicit(new_salt, 0, sizeof(new_salt));
        return NP_RESET_ERR_TRNG;
    }

    st = np_factory_reset_hal_trng_generate(new_token, sizeof(new_token)); /* R-9 */
    if (st != NP_RESET_OK) {
        memset_explicit(new_salt, 0, sizeof(new_salt));
        memset_explicit(new_token, 0, sizeof(new_token));
        return NP_RESET_ERR_TRNG;
    }

    /* R-10: write salt + token + factory defaults to Config. */
    st = np_factory_reset_hal_write_config_defaults(new_salt, new_token);

    /* Zero the sensitive buffers regardless of write outcome. */
    memset_explicit(new_salt, 0, sizeof(new_salt));
    memset_explicit(new_token, 0, sizeof(new_token));

    if (st != NP_RESET_OK) {
        return NP_RESET_ERR_CONFIG;
    }

    return NP_RESET_OK;
}

/* ── R-12: reboot ─────────────────────────────────────────────────────────── */
/* On target firmware this triggers a system reset (NVIC_SystemReset()).  In
 * NPTEST_HOST builds it is a no-op so the test harness keeps running. */
static void np_fr_reboot(void)
{
#ifdef NPTEST_HOST
    /* Host build: do not reboot the test process. */
#else
    /* ARMv7-M Application Interrupt and Reset Control Register (AIRCR).
     * Write VECTKEY (0x05FA) | SYSRESETREQ to request a system reset. */
    volatile uint32_t *const AIRCR = (volatile uint32_t *)0xE000ED0CUL;
    __asm volatile("dsb" ::: "memory");
    *AIRCR = (0x05FAUL << 16U) | (1UL << 2U);   /* SYSRESETREQ */
    __asm volatile("dsb" ::: "memory");
    for (;;) {
        __asm volatile("wfi");
    }
#endif
}

/* ── Public API ───────────────────────────────────────────────────────────── */

np_reset_status_t np_factory_reset_execute(void)
{
    /* R-1: caller already verified the Ed25519-signed FACTORY_RESET command.  */

    /* R-2: module-level authenticity check.  Real cryptographic validation is
     * performed at the hub layer before this function is called; this module
     * accepts unconditionally and documents the assumption. */

    /* R-3: set reset_in_progress (warm reset), then write the durable marker
     * (power loss — NP-FW-NVRAM-001 §3.4.1 option A, OI-NVRAM-05).  The marker
     * MUST be durable before the first erase: a reset whose start is not on
     * the medium could be cut by a power loss and never resumed.  So a reset
     * that cannot write it does not start — nothing has been erased yet, and
     * the flag is cleared so a warm reset does not resume what never began. */
    np_fr_set_in_progress();
    if (np_factory_reset_hal_marker_write() != NP_RESET_OK) {
        np_fr_clear_in_progress();
        return NP_RESET_ERR_MARKER;
    }

    /* R-4: suspend sessions — no-op.  This function is only invoked when no
     * stimulation session is active (enforced by the hub layer), so there is
     * nothing to suspend.  Kept as an explicit step for spec traceability. */

    /* R-5..R-10: wipe data partitions and re-derive salt + warranty token. */
    np_reset_status_t st = np_fr_wipe_and_rederive();
    if (st != NP_RESET_OK) {
        /* Leave reset_in_progress SET so the next boot re-runs the wipe.
         * The device must never come up with user data half-erased. */
        return st;
    }

    /* R-11: clear reset_in_progress — reset completed successfully. */
    np_fr_clear_in_progress();

    /* R-12: reboot into the freshly reset device. */
    np_fr_reboot();

    /* Reached only on the host build (np_fr_reboot is a no-op there). */
    return NP_RESET_OK;
}

np_fr_boot_t np_factory_reset_boot_check(void)
{
    /* Read the marker FIRST, so Config's state is known whatever the flag
     * says.  Then decide.  Any one of three signs means a reset was running:
     *
     *   SNVS flag     a warm reset interrupted R-3..R-11
     *   PRESENT       a power loss (or any reset) between R-3 and R-7
     *   NO_STORE      a power loss between R-7 and R-10's commit — or a
     *                 Config lost some other way, which is accepted (§3.4.1):
     *                 UHDR is already unreadable without ukmd.rec
     *
     * UNKNOWN is not one of them.  An unreadable Config is never taken as
     * permission to erase, and never as proof that nothing was running. */
    np_fr_marker_state_t m = np_factory_reset_hal_marker_state();
    bool flag = np_factory_reset_is_in_progress();

    if (m == NP_FR_MARKER_UNKNOWN) {
        return NP_FR_BOOT_UNKNOWN;
    }
    if (!flag && m == NP_FR_MARKER_ABSENT) {
        return NP_FR_BOOT_NONE;
    }

    /* Re-run from R-5 even when only Config is missing: R-5 and R-6 are
     * idempotent, and running them means an old SHDR history can never be
     * uploaded under the warranty token R-9 is about to mint. */
    np_fr_set_in_progress();
    if (np_fr_wipe_and_rederive() != NP_RESET_OK) {
        return NP_FR_BOOT_RESUME_FAILED;   /* flag stays set: next boot retries */
    }
    np_fr_clear_in_progress();
    return NP_FR_BOOT_RESUMED;
}

void np_factory_reset_resume_after_powerloss(void)
{
    /* Re-run the idempotent R-5..R-10 core.  If it succeeds, clear the flag
     * (R-11).  If any step fails, leave the flag set so the next boot retries
     * rather than booting with a partially wiped device. */
    if (np_fr_wipe_and_rederive() == NP_RESET_OK) {
        np_fr_clear_in_progress();
    }
}

/* ── Platform HAL stubs (host test builds only) ────────────────────────────── */
/* On target firmware the board layer provides the real implementations and
 * these stubs are excluded.  In NPTEST_HOST builds they let the module link
 * and run on the host without eMMC/TRNG hardware.
 *
 * Each stub is failure-injectable via np_fr_host_hal (np_factory_reset_test_hal.h)
 * so the test suite can force any step to fail and verify the abort-safety
 * invariant — reset_in_progress stays SET on failure — plus bounded-retry
 * recovery.  By default every control is zero, so the stubs succeed and the
 * happy-path behaviour is unchanged. */
#ifdef NPTEST_HOST

#include "np_factory_reset_test_hal.h"

/* Single definition of the host HAL control state. */
np_fr_host_hal_control_t np_fr_host_hal = {0};

/* Shared step evaluator: bumps the call counter, then honours fail_remaining
 * (>0 fail-then-recover, <0 always-fail, 0 succeed). */
static np_reset_status_t np_fr_host_step(int *fail_remaining, uint32_t *call_count,
                                         np_reset_status_t err)
{
    (*call_count)++;
    if (*fail_remaining != 0) {
        if (*fail_remaining > 0) {
            (*fail_remaining)--;
        }
        return err;
    }
    return NP_RESET_OK;
}

static void np_fr_host_trace(char c)
{
    if (np_fr_host_hal.trace_len + 1U < sizeof(np_fr_host_hal.trace)) {
        np_fr_host_hal.trace[np_fr_host_hal.trace_len++] = c;
        np_fr_host_hal.trace[np_fr_host_hal.trace_len] = '\0';
    }
}

np_reset_status_t np_factory_reset_hal_marker_write(void)
{
    np_reset_status_t st = np_fr_host_step(&np_fr_host_hal.fail_marker_write,
                                           &np_fr_host_hal.calls_marker_write,
                                           NP_RESET_ERR_MARKER);
    if (st == NP_RESET_OK) {
        np_fr_host_trace('M');
        np_fr_host_hal.marker_state = NP_FR_MARKER_PRESENT;
    }
    return st;
}

np_fr_marker_state_t np_factory_reset_hal_marker_state(void)
{
    np_fr_host_hal.calls_marker_state++;
    return np_fr_host_hal.marker_state;
}

np_reset_status_t np_factory_reset_hal_sanitize_uhdr(void)   /* OI-RESET-01 */
{
    np_reset_status_t st = np_fr_host_step(&np_fr_host_hal.fail_sanitize_uhdr,
                                           &np_fr_host_hal.calls_sanitize_uhdr,
                                           NP_RESET_ERR_SANITIZE);
    if (st == NP_RESET_OK) {
        np_fr_host_trace('U');
    }
    return st;
}

np_reset_status_t np_factory_reset_hal_zero_shdr(void)       /* OI-RESET-02 */
{
    np_reset_status_t st = np_fr_host_step(&np_fr_host_hal.fail_zero_shdr,
                                           &np_fr_host_hal.calls_zero_shdr,
                                           NP_RESET_ERR_SANITIZE);
    if (st == NP_RESET_OK) {
        np_fr_host_trace('S');
    }
    return st;
}

np_reset_status_t np_factory_reset_hal_zero_config(void)     /* OI-RESET-03 */
{
    np_reset_status_t st = np_fr_host_step(&np_fr_host_hal.fail_zero_config,
                                           &np_fr_host_hal.calls_zero_config,
                                           NP_RESET_ERR_SANITIZE);
    if (st == NP_RESET_OK) {
        /* R-7 erases the partition the marker lives in. */
        np_fr_host_trace('C');
        np_fr_host_hal.marker_state = NP_FR_MARKER_NO_STORE;
    }
    return st;
}

np_reset_status_t np_factory_reset_hal_trng_generate(uint8_t *buf, size_t len) /* OI-RESET-04 */
{
    np_fr_host_hal.calls_trng++;
    if (buf == NULL || len == 0U) {
        return NP_RESET_ERR_TRNG;
    }
    /* Fail the requested call index (R-8 = call 1, R-9 = call 2). */
    if (np_fr_host_hal.trng_fail_on_call != 0 &&
        (uint32_t)np_fr_host_hal.trng_fail_on_call == np_fr_host_hal.calls_trng) {
        return NP_RESET_ERR_TRNG;
    }
    /* Host stub: deterministic non-zero fill (NOT cryptographic — host only). */
    for (size_t i = 0U; i < len; ++i) {
        buf[i] = (uint8_t)(0xA5U ^ (uint8_t)i);
    }
    return NP_RESET_OK;
}

np_reset_status_t np_factory_reset_hal_write_config_defaults( /* OI-RESET-05 */
    const uint8_t *trng_salt, const uint8_t *warranty_token)
{
    if (trng_salt == NULL || warranty_token == NULL) {
        return NP_RESET_ERR_CONFIG;
    }
    np_reset_status_t st = np_fr_host_step(&np_fr_host_hal.fail_write_config,
                                           &np_fr_host_hal.calls_write_config,
                                           NP_RESET_ERR_CONFIG);
    if (st == NP_RESET_OK) {
        /* R-10 formats Config and commits the defaults: no marker. */
        np_fr_host_trace('W');
        np_fr_host_hal.marker_state = NP_FR_MARKER_ABSENT;
    }
    return st;
}

#endif /* NPTEST_HOST */
