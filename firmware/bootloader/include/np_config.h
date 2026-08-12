/*
 * NeurOne Bootloader — Hardware Configuration
 * Target: NXP i.MX RT1062 (Cortex-M7, 600 MHz)
 * Document: NP-FW-EMMC-001 Rev 1 §8.1
 *
 * All eMMC partition offsets are LBA-addressed (512-byte sectors).
 * Partition layout matches the 9-partition GPT defined in the spec.
 */

#ifndef NP_CONFIG_H
#define NP_CONFIG_H

#include <stdint.h>

/* ── i.MX RT1062 SRAM layout ──────────────────────────────────────────────── */
/* Bootloader executes from OCRAM loaded by ROM.                               */
#define NP_OCRAM_BASE           0x20200000UL    /* On-chip RAM base            */
#define NP_OCRAM_SIZE           (512U * 1024U)  /* 512 KiB                     */
#define NP_SCRATCH_SRAM_BASE    0x20270000UL    /* 64 KiB scratch within OCRAM */
#define NP_SCRATCH_SRAM_SIZE    (64U * 1024U)

/* ── SNVS Low Power General Purpose Register 0 ───────────────────────────── */
/* Survives warm resets; holds boot bank flag and attempt counter.             */
#define NP_SNVS_BASE            0x400D4000UL
#define NP_SNVS_LPGPR0          (*(volatile uint32_t *)(NP_SNVS_BASE + 0x68U))

/* SNVS_LPGPR0 bit field layout:                                               */
/*   [1:0]  BOOT_BANK      0=Bank A, 1=Bank B                                 */
/*   [7:2]  BOOT_ATTEMPTS  incremented each boot of OTA candidate (max 3)     */
/*   [8]    OTA_PENDING    set by app before reset to trigger OTA swap verify  */
/*   [15:9] reserved                                                           */
/*   [16]   DFU_FORCED     set to force DFU recovery regardless of bank state  */
/*   [31:17] reserved                                                          */
#define NP_SNVS_BANK_MASK       0x00000003UL
#define NP_SNVS_BANK_A          0x00000000UL
#define NP_SNVS_BANK_B          0x00000001UL
#define NP_SNVS_ATTEMPTS_SHIFT  2U
#define NP_SNVS_ATTEMPTS_MASK   (0x3FU << NP_SNVS_ATTEMPTS_SHIFT)
#define NP_SNVS_OTA_PENDING     (1UL << 8U)
#define NP_SNVS_DFU_FORCED      (1UL << 16U)
#define NP_BOOT_MAX_ATTEMPTS    3U

/* ── SNVS_LPGPR1 — factory reset in-progress flag ────────────────────────── */
/* Set at factory-reset step R-3, cleared at R-11 (NP-FW-EMMC-002 Rev 1 §B).  */
/* If set on boot, a reset was interrupted by power loss; re-run SANITIZE.    */
#define NP_SNVS_LPGPR1          (*(volatile uint32_t *)(NP_SNVS_BASE + 0x6CU))
#define NP_SNVS_RESET_IN_PROGRESS  (1UL << 0U)

/* ── SNVS_LPGPR2 — anonymization in-progress flag ────────────────────────── */
/* Set while the research anonymization pipeline holds an encrypted extract in */
/* the Scratch partition (NP-FW-EMMC-002 Rev 1 §D).                           */
#define NP_SNVS_LPGPR2          (*(volatile uint32_t *)(NP_SNVS_BASE + 0x70U))
#define NP_SNVS_ANON_IN_PROGRESS   (1UL << 0U)

/* ── eMMC user-area partition map (512-byte LBA sectors) ─────────────────── */
/* All offsets are from LBA 0 of the eMMC user area.                          */
/* GPT header at LBA 1, entries LBA 2–33, first usable LBA 34.               */
/* Partitions are 1 MiB aligned (2048 sector granularity).                    */

#define NP_LBA_PER_MIB          2048UL          /* 512B sectors per MiB       */

/* Partition 1 — Firmware Bank A (128 MiB) */
#define NP_BANK_A_LBA_START     (1UL * NP_LBA_PER_MIB)     /* LBA  2048       */
#define NP_BANK_SIZE_LBA        (128UL * NP_LBA_PER_MIB)   /* 262144 sectors  */
#define NP_BANK_SIZE_BYTES      (128UL * 1024UL * 1024UL)

/* Partition 2 — Firmware Bank B (128 MiB) */
#define NP_BANK_B_LBA_START     (NP_BANK_A_LBA_START + NP_BANK_SIZE_LBA)

/* Partition 3 — Safety MCU FW staging (4 MiB) */
#define NP_SAFETY_MCU_LBA_START (NP_BANK_B_LBA_START + NP_BANK_SIZE_LBA)
#define NP_SAFETY_MCU_SIZE_LBA  (4UL * NP_LBA_PER_MIB)

/* Partition 4 — Config/Calibration (16 MiB, LittleFS) */
#define NP_CONFIG_LBA_START     (NP_SAFETY_MCU_LBA_START + NP_SAFETY_MCU_SIZE_LBA)
#define NP_CONFIG_SIZE_LBA      (16UL * NP_LBA_PER_MIB)

/* Partition 5 — SHDR (512 MiB) */
#define NP_SHDR_LBA_START       (NP_CONFIG_LBA_START + NP_CONFIG_SIZE_LBA)
#define NP_SHDR_SIZE_LBA        (512UL * NP_LBA_PER_MIB)

/* Partition 6 — UHDR (6903 MiB, AES-256-XTS) */
#define NP_UHDR_LBA_START       (NP_SHDR_LBA_START + NP_SHDR_SIZE_LBA)
#define NP_UHDR_SIZE_LBA        (6903UL * NP_LBA_PER_MIB)

/* Partition 7 — Scratch (500 MiB, zeroed on boot, OTA staging workspace) */
#define NP_SCRATCH_LBA_START    (NP_UHDR_LBA_START + NP_UHDR_SIZE_LBA)
#define NP_SCRATCH_SIZE_LBA     (500UL * NP_LBA_PER_MIB)

/* ── Firmware image header layout ────────────────────────────────────────── */
/* Each bank starts with NP_IMAGE_HEADER at offset 0.                         */
/* Firmware vector table + code follows at offset NP_IMAGE_HEADER_SIZE.       */
#define NP_IMAGE_MAGIC          0x4E504657UL    /* "NPFW"                      */
#define NP_IMAGE_HEADER_SIZE    256U            /* Aligned to 256 bytes        */
#define NP_FW_LOAD_ADDR         0x20200000UL    /* OCRAM load target           */
#define NP_FW_MAX_SIZE          (NP_BANK_SIZE_BYTES - NP_IMAGE_HEADER_SIZE)

/* ── Ed25519 public key (manufacturing root — provisioned at test) ────────── */
/* 32-byte curve25519 public key embedded in bootloader at build time.        */
/* Replace with production key before tooling sign-off.                       */
#define NP_ED25519_PUBKEY_SIZE  32U
#define NP_ED25519_SIG_SIZE     64U
#define NP_SHA256_SIZE          32U
#define NP_SHA512_SIZE          64U

/* ── USDHC peripheral (eMMC on USDHC2) ──────────────────────────────────── */
#define NP_USDHC2_BASE          0x402C4000UL
#define NP_EMMC_SECTOR_SIZE     512U
#define NP_EMMC_BOOT_PART_1     1U              /* EXTCSD[179] value for BP1  */
#define NP_EMMC_BOOT_PART_2     2U              /* EXTCSD[179] value for BP2  */
#define NP_EMMC_USER_AREA       0U              /* EXTCSD[179] value for UDA  */

/* ── USB OTG1 (USB-C DFU recovery) ──────────────────────────────────────── */
#define NP_USB1_BASE            0x402E0000UL
#define NP_DFU_VENDOR_ID        0x3A9BU         /* NeurOne USB VID          */
#define NP_DFU_PRODUCT_ID       0x0001U         /* Bootloader DFU PID          */
#define NP_DFU_TRANSFER_SIZE    4096U           /* wTransferSize bytes         */
#define NP_DFU_TIMEOUT_MS       300000U         /* 5-minute DFU session window */

/* ── Watchdog / boot confirmation ────────────────────────────────────────── */
/* Application must call np_ota_confirm_boot() within this window after boot. */
/* If the app does not confirm, the next reset treats this as a boot failure. */
/* This constant is stored in Config partition, not SNVS (survives SNVS wipe).*/
#define NP_OTA_CONFIRM_TIMEOUT_S 30U

/* ── CRC32 polynomial ─────────────────────────────────────────────────────── */
#define NP_CRC32_POLY           0xEDB88320UL    /* IEEE 802.3 reflected        */

#endif /* NP_CONFIG_H */
