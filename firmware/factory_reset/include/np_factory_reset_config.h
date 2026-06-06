/* Document: NP-FW-EMMC-002 Rev A §B */
/*
 * NeuroPulse Device Factory Reset — Hardware Configuration
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 *
 * SNVS_LPGPR1 holds the factory-reset-in-progress flag.  It survives warm
 * resets and brief power loss (battery-backed SNVS domain), allowing the
 * bootloader to detect a reset that was interrupted by power loss during
 * steps R-5..R-10 and re-run the SANITIZE/zero sequence before completing
 * boot.
 *
 * SNVS base and register-access pattern match firmware/bootloader/include/
 * np_config.h (NP_SNVS_BASE, *(volatile uint32_t *)address).  LPGPR1 is the
 * register immediately above LPGPR0 (offset 0x6C vs 0x68).
 */

#ifndef NP_FACTORY_RESET_CONFIG_H
#define NP_FACTORY_RESET_CONFIG_H

#include <stdint.h>

/* ── SNVS Low Power General Purpose Register 1 ───────────────────────────── */
/* Survives warm resets and battery-backed power loss.                         */
/* Holds the factory-reset-in-progress flag (bit 0).                           */
#define NP_FR_SNVS_BASE             0x400D4000UL

#ifdef NPTEST_HOST
/* Host test build: the i.MX SNVS MMIO address is not mapped on the host, so a
 * raw read would fault.  Back LPGPR1 with a real RAM word defined in the test
 * harness.  This lets np_factory_reset_is_in_progress() and the flag set/clear
 * helpers exercise their logic on the host without crashing. */
extern volatile uint32_t np_fr_host_snvs_lpgpr1;
#define NP_FR_SNVS_LPGPR1           (np_fr_host_snvs_lpgpr1)
#else
#define NP_FR_SNVS_LPGPR1           (*(volatile uint32_t *)(NP_FR_SNVS_BASE + 0x6CU))
#endif

/* SNVS_LPGPR1 bit field layout:                                               */
/*   [0]     RESET_IN_PROGRESS  set at R-3, cleared at R-11                    */
/*   [31:1]  reserved                                                          */
#define NP_FR_RESET_IN_PROGRESS     (1UL << 0U)

/* ── TRNG output sizes ───────────────────────────────────────────────────── */
/* New TRNG salt and warranty token are both 32 bytes (256-bit).              */
/* Salt feeds UHDR key derivation; token is the opaque SHDR device linkage.   */
#define NP_FR_TRNG_SALT_LEN         32U
#define NP_FR_WARRANTY_TOKEN_LEN    32U

/* ── Retry limits ────────────────────────────────────────────────────────── */
/* HAL operations (SANITIZE, zero, TRNG, config write) are retried a bounded  */
/* number of times before the reset aborts with an error status.  A bounded   */
/* retry guards against transient eMMC/TRNG hardware errors without ever       */
/* looping forever (an unbounded retry on a hard fault would brick the boot). */
#define NP_FR_HAL_MAX_RETRIES       3U

#endif /* NP_FACTORY_RESET_CONFIG_H */
