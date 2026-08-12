/* Document: NP-FW-EMMC-002 Rev 1 §B */
/*
 * NeurOne Device Factory Reset — Public API and Platform HAL
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 *
 * Implements the 12-step device factory reset procedure (R-1..R-12) from
 * NP-FW-EMMC-002 Rev 1 §B.3:
 *
 *   R-1  Caller has already verified signed FACTORY_RESET command (Ed25519).
 *   R-2  Verify command authenticity (module stub: accept; real validation at
 *        hub layer before np_factory_reset_execute() is called).
 *   R-3  Set SNVS_LPGPR1 bit 0 (reset_in_progress — survives warm reset).
 *   R-4  Suspend sessions (no-op: only called when no session is active).
 *   R-5  eMMC SANITIZE on UHDR partition (NIST SP 800-88 "Purge").
 *   R-6  Zero SHDR partition.
 *   R-7  Zero Config partition.
 *   R-8  Generate new 32-byte TRNG salt.
 *   R-9  Generate new 32-byte warranty token.
 *   R-10 Write new TRNG salt + warranty token + factory defaults to Config.
 *   R-11 Clear SNVS_LPGPR1 bit 0 (reset_in_progress = 0).
 *   R-12 Reboot (no-op in host builds; NVIC_SystemReset() in real firmware).
 *
 * Power-loss resilience: if the bootloader finds LPGPR1 bit 0 set on boot,
 * a reset was interrupted between R-3 and R-11.  The bootloader (or this
 * module via np_factory_reset_resume_after_powerloss()) re-runs R-5..R-10
 * before completing boot.  Those steps are idempotent: re-SANITIZing an
 * already-erased partition and re-deriving salt/token leaves the device in
 * the same clean factory state.
 */

#ifndef NP_FACTORY_RESET_H
#define NP_FACTORY_RESET_H

#include "np_factory_reset_types.h"
#include "np_factory_reset_config.h"

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * Execute the full factory reset sequence R-1..R-12.
 *
 * Preconditions:
 *   - Caller (hub layer) has already verified the Ed25519-signed
 *     FACTORY_RESET command (R-1).
 *   - No stimulation session is active (R-4 is a no-op).
 *
 * Returns NP_RESET_OK on success.  On any HAL failure the sequence aborts
 * and returns the corresponding error code WITHOUT clearing the
 * reset_in_progress flag — so the next boot re-runs the data-wipe steps and
 * the device cannot come up in a half-reset state with user data still
 * present.  On the no-reboot host build this function returns NP_RESET_OK
 * after R-11; on target firmware R-12 reboots and the function never returns.
 */
np_reset_status_t np_factory_reset_execute(void);

/*
 * Returns true if SNVS_LPGPR1 bit 0 (reset_in_progress) is set, indicating a
 * factory reset is in progress or was interrupted by power loss.  Safe to call
 * on any platform; reads the SNVS register only.
 */
bool np_factory_reset_is_in_progress(void);

/*
 * Resume an interrupted factory reset after power loss.  Called by the
 * bootloader when np_factory_reset_is_in_progress() is true.  Re-runs the
 * idempotent data-wipe and re-derivation steps R-5..R-10, then clears the
 * reset_in_progress flag (R-11).  Does NOT reboot — the bootloader continues
 * its normal boot flow afterwards and the application detects the factory
 * state from the cleared Config partition.
 *
 * If any HAL step fails, the flag is left set so the next boot retries.
 */
void np_factory_reset_resume_after_powerloss(void);

/* ── Platform HAL (implemented by the platform/board layer) ──────────────── */
/* In NPTEST_HOST builds these are provided as no-op stubs in the .c file.    */

/* OI-RESET-01 — platform HAL stub.
 * R-5: eMMC SANITIZE on the UHDR partition (NIST SP 800-88 "Purge").  On
 * target this issues the eMMC SANITIZE command scoped to the UHDR LBA range.
 * Returns NP_RESET_OK on success, NP_RESET_ERR_SANITIZE on failure. */
extern np_reset_status_t np_factory_reset_hal_sanitize_uhdr(void);

/* OI-RESET-02 — platform HAL stub.
 * R-6: Zero the SHDR partition.  Returns NP_RESET_OK / NP_RESET_ERR_SANITIZE. */
extern np_reset_status_t np_factory_reset_hal_zero_shdr(void);

/* OI-RESET-03 — platform HAL stub.
 * R-7: Zero the Config/Calibration partition.  Returns NP_RESET_OK /
 * NP_RESET_ERR_SANITIZE. */
extern np_reset_status_t np_factory_reset_hal_zero_config(void);

/* OI-RESET-04 — platform HAL stub.
 * R-8/R-9: Fill buf[0..len-1] with hardware TRNG bytes.  buf must be non-NULL
 * and len must be > 0.  Returns NP_RESET_OK on success, NP_RESET_ERR_TRNG on
 * failure or invalid argument. */
extern np_reset_status_t np_factory_reset_hal_trng_generate(uint8_t *buf, size_t len);

/* OI-RESET-05 — platform HAL stub.
 * R-10: Write the new TRNG salt + warranty token + factory defaults to the
 * Config partition.  Both pointers reference NP_FR_TRNG_SALT_LEN /
 * NP_FR_WARRANTY_TOKEN_LEN byte buffers respectively and must be non-NULL.
 * Returns NP_RESET_OK on success, NP_RESET_ERR_CONFIG on failure. */
extern np_reset_status_t np_factory_reset_hal_write_config_defaults(
    const uint8_t *trng_salt, const uint8_t *warranty_token);

#endif /* NP_FACTORY_RESET_H */
