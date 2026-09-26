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
 *   R-3  Set SNVS_LPGPR1 bit 0 (reset_in_progress — survives warm reset),
 *        then write the durable marker to Config (survives power loss).  If
 *        the marker cannot be written the reset does not start: the flag is
 *        cleared and NP_RESET_ERR_MARKER returned with nothing erased.
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
 * Interrupted-reset resilience: if the bootloader finds LPGPR1 bit 0 set on
 * boot, a reset was interrupted between R-3 and R-11 by a WARM reset.  The
 * bootloader (or this module via np_factory_reset_resume_after_powerloss())
 * re-runs R-5..R-10 before completing boot.  Those steps are idempotent:
 * re-SANITIZing an already-erased partition and re-deriving salt/token leaves
 * the device in the same clean factory state.
 *
 * Power loss is covered by the durable marker, not by LPGPR1.  LPGPR1 has no
 * VBAT supply and is cleared by a power removal (NP-FW-NVRAM-001 §3.4), so
 * NP-FW-NVRAM-001 §3.4.1 option A (principal decision 2026-09-26,
 * OI-NVRAM-05) adds a marker that lives in the Config partition itself:
 *
 *   - written at R-3, BEFORE the first erase, as a replicated Config file;
 *   - removed only by R-7, which erases the whole partition it lives in;
 *   - so from R-3 to R-7 the marker is present, and from R-7 until R-10
 *     commits Config holds no filesystem at all.
 *
 * np_factory_reset_boot_check() reads that state at bring-up, BEFORE UHDR or
 * SHDR is mounted, and completes the reset when any of the three says one was
 * running: the SNVS flag (warm reset), a marker, or a Config with no
 * filesystem.  The last rule also fires on a Config lost for any other reason.
 * That is accepted, not overlooked (§3.4.1): ukmd.rec lives in Config, so UHDR
 * is already unreadable without it, and the reset mints a new warranty token.
 * The whole R-5..R-10 sequence re-runs, not only R-7..R-10, so an old SHDR
 * history can never be uploaded under the newly minted token.
 *
 * A Config that cannot be READ (an I/O fault) is none of these.  It is
 * reported as NP_FR_BOOT_UNKNOWN and nothing is erased, because unknown is
 * never absence and never permission.
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
 * factory reset is in progress or was interrupted by a warm reset.  false does
 * NOT mean no reset was interrupted: a power loss clears the flag (OI-NVRAM-05).
 * Safe to call on any platform; reads the SNVS register only.
 */
bool np_factory_reset_is_in_progress(void);

/*
 * Boot-time check for an interrupted factory reset (OI-NVRAM-05, option A).
 * Call at bring-up after nothing but the bootloader has run, and BEFORE UHDR
 * or SHDR is mounted.  Reads the SNVS flag and the durable marker
 * (np_factory_reset_hal_marker_state()); if either says a reset was running,
 * or Config holds no filesystem, re-runs R-5..R-10 and clears the flag.
 *
 *   NP_FR_BOOT_NONE           boot normally
 *   NP_FR_BOOT_RESUMED        the reset was completed; boot the factory state
 *   NP_FR_BOOT_RESUME_FAILED  do not mount UHDR/SHDR; the next boot retries
 *   NP_FR_BOOT_UNKNOWN        Config unreadable; do not mount UHDR/SHDR
 */
np_fr_boot_t np_factory_reset_boot_check(void);

/*
 * Resume an interrupted factory reset.  Called by the bootloader when
 * np_factory_reset_is_in_progress() is true, which means a warm reset
 * interrupted it.  A power loss is np_factory_reset_boot_check()'s case, which
 * reads the durable marker as well as the flag (OI-NVRAM-05).  Re-runs the
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
 * R-10: Format the Config partition (R-7 left it with no filesystem), then
 * write the new TRNG salt + warranty token + factory defaults to it.  Until
 * this commits, Config has no filesystem, which is what
 * np_factory_reset_boot_check() reads as "a reset was running".  Both pointers reference NP_FR_TRNG_SALT_LEN /
 * NP_FR_WARRANTY_TOKEN_LEN byte buffers respectively and must be non-NULL.
 * Returns NP_RESET_OK on success, NP_RESET_ERR_CONFIG on failure. */
extern np_reset_status_t np_factory_reset_hal_write_config_defaults(
    const uint8_t *trng_salt, const uint8_t *warranty_token);

/* R-3 durable marker — implemented over np_cfg_store by
 * firmware/hub_control/src/np_reset_marker.c (NP-FW-NVRAM-001 §3.4.1).
 *
 * np_factory_reset_hal_marker_write: write the marker to the mounted Config
 * partition.  NP_RESET_OK only once it is durable; NP_RESET_ERR_MARKER
 * otherwise.
 *
 * np_factory_reset_hal_marker_state: mount Config and report what it says
 * (np_fr_marker_state_t).  Leaves Config mounted when it returns ABSENT or
 * PRESENT.  Must distinguish "no filesystem" (NO_STORE) from "could not
 * read" (UNKNOWN): the first completes a reset, the second must not. */
extern np_reset_status_t    np_factory_reset_hal_marker_write(void);
extern np_fr_marker_state_t np_factory_reset_hal_marker_state(void);

#endif /* NP_FACTORY_RESET_H */
