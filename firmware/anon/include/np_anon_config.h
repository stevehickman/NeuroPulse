/*
 * NeuroPulse Research Anonymisation — Scratch Encryption Configuration
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 * Document: NP-FW-EMMC-002 Rev A §D
 *
 * Defines the AES-256-CTR scratch-partition encryption constants for the
 * on-device research anonymisation pipeline.  Decrypted UHDR data is staged
 * only in the Scratch partition as ciphertext; the key lives in SRAM and is
 * destroyed on completion or lost on power-off, so a power-loss event leaves
 * nothing recoverable.
 *
 * Scratch partition LBA geometry is owned by the bootloader storage map
 * (NP-FW-EMMC-001 Rev A §4).  This header re-exports it from np_config.h so
 * the anonymisation module and the bootloader agree on the same region.
 */

#ifndef NP_ANON_CONFIG_H
#define NP_ANON_CONFIG_H

#include <stdint.h>

/* Scratch partition geometry (NP-FW-EMMC-001 Rev A §4).                       */
/* np_config.h defines NP_SCRATCH_LBA_START and NP_SCRATCH_SIZE_LBA in LBA    */
/* (512-byte sector) units.  Both the bootloader and this module reference the */
/* identical region so a recovery SANITIZE wipes exactly the staged ciphertext.*/
#include "np_config.h"

/* ── AES-256-CTR parameters (NP-FW-EMMC-002 Rev A §D.2) ──────────────────── */

/* Block size for scratch read/write staging — matches the eMMC sector size  */
/* so each plaintext block maps to exactly one ciphertext sector.             */
#define NP_ANON_SCRATCH_BLOCK_SIZE  512U

/* AES-256 key length in bytes.                                                */
#define NP_ANON_KEY_LEN             32U

/* CTR nonce length in bytes.  See OI-ANON-AES-02 for the counter block        */
/* construction and the disjoint-window requirement below.                     */
#define NP_ANON_NONCE_LEN           16U

/* Number of 16-byte AES blocks per scratch block.  Each scratch block must    */
/* consume a DISJOINT window of CTR counter values so adjacent blocks never     */
/* reuse keystream; the HAL counter base for a scratch block is                 */
/* block_offset * NP_ANON_AES_BLOCKS_PER_SCRATCH_BLOCK.  See OI-ANON-AES-02.    */
#define NP_ANON_AES_BLOCKS_PER_SCRATCH_BLOCK  (NP_ANON_SCRATCH_BLOCK_SIZE / 16U)

/* ── SNVS_LPGPR2 — anonymisation in-progress flag (NP-FW-EMMC-002 §D.6) ──── */
/*                                                                            */
/* SNVS general-purpose registers survive warm reset but not power-off.  The  */
/* flag is set before the first scratch write and cleared only after the      */
/* post-run SANITIZE completes.  If the bootloader finds it set on boot, an   */
/* anonymisation run was interrupted (power loss) and the Scratch partition   */
/* is SANITIZE'd before the flag is cleared.                                  */
/*                                                                            */
/* NOTE: NP_SNVS_BASE (0x400D4000) is defined in np_config.h.  LPGPR2 lives   */
/* at offset 0x70.  These symbols are defined locally here AND in np_main.c   */
/* to avoid colliding with Unit 1's parallel edit to np_config.h.  When the   */
/* np_config.h definitions land, the merge reviewer must delete these local   */
/* definitions and keep the np_config.h versions (single source of truth).    */
/*                                                                            */
/* On the host (NPTEST_HOST) the MMIO address is not mapped, so LPGPR2 is      */
/* backed by a real RAM variable instead of a fixed-address dereference; this  */
/* lets the host test exercise the flag set/clear logic without a segfault.    */
#ifndef NP_SNVS_ANON_IN_PROGRESS
#define NP_SNVS_ANON_IN_PROGRESS    (1UL << 0U)
#endif

#ifndef NP_SNVS_LPGPR2
#ifdef NPTEST_HOST
extern volatile uint32_t np_anon_host_lpgpr2;
#define NP_SNVS_LPGPR2 (np_anon_host_lpgpr2)
#else
#define NP_SNVS_LPGPR2 \
    (*(volatile uint32_t *)(NP_SNVS_BASE + 0x70U))
#endif
#endif

#endif /* NP_ANON_CONFIG_H */
